#include <stdio.h>
#include <ytpmv/modparser.H>
#include <ytpmv/common.H>
#include <ytpmv/simple.H>
#include <math.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include <algorithm>
#include <map>
#include <assert.h>

using namespace std;
using namespace ytpmv;

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	string buf = get_file_contents("songs/castle.mod");
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08*2/30);
	addSource("hum", "sources/bass1.mkv",
					"sources/bass1.mkv", 2.8*8/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5*4/30, 2., 2.);
	addSource("gab1", "sources/gab1.wav",
					"sources/gab1.mkv", 2.55*4/30, 1./2, 1./2);
	addSource("gab2", "sources/gab2.mkv",
					"sources/gab2.mkv", 3.15*2/30, 1./2, 1./2);
	addSource("gab3", "sources/gab3.mkv",
					"sources/gab3.mkv", 2.5*2/30, 1./2, 1./2);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	trimSource("hum", 0.1);
	trimSource("gab3", 0.1);
	
	
	string vertexShader = R"aaaaa(
	#version 330 core
	layout(location = 0) in vec3 myPos;
	layout(location = 1) in vec2 texPos;
	uniform vec2 coordBase;
	uniform mat2 coordTransform;
	uniform float secondsRel;
	uniform float secondsAbs;
	uniform float userParams[16];
	uniform vec2 resolution;
	uniform mat4 proj;
	smooth out vec2 uv;
	float param(int i) { return userParams[i]; }
	void main() {
		vec3 coords = myPos;
		float sizeScale = clamp(param(5) * secondsRel + param(4), 0.0, 1.0);
		vec2 mypos = vec2(param(0), param(1));
		vec2 mysize = vec2(param(2),param(3))*sizeScale;
		
		mypos += vec2(0.0,-0.5) * secondsRel;
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = texPos;
	}
	)aaaaa";
	
	string shader =
		"return vec4(texture2D(image, pos).rgb, 1.);";

	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	ImageSource imgSource(&img1);
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastGridPos = 0;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones + 41;
		
		// select source
		switch(n.instrument) {
			case 1:
				src = getSource("gab1");
				n.pitchSemitones -= 12;
				n.amplitudeDB += 8;
				if(n.channel == 1) n.amplitudeDB -= 4;
				break;
			case 2:
				n.pitchSemitones -= 5.5;
				if(n.channel == 2) {
					src = getSource("hum"); n.amplitudeDB += 3;
				} else {
					src = getSource("gab2"); n.amplitudeDB -= 1;
				}
				break;
			case 3: src = getSource("perc2"); n.pitchSemitones = 0; n.amplitudeDB -= 5; break;
			case 4: src = getSource("perc1"); n.pitchSemitones = 0; n.amplitudeDB -= 5; break;
			default: goto skip;
		}
		
		{
			int gridN = 8;
			int gridSize = gridN*gridN;
			VideoSegment vs(n, src->hasVideo()?src->video:&imgSource, bpm);
			if(int(videoSegments.size()) >= gridSize) {
				videoSegments[videoSegments.size()-gridSize].endSeconds = vs.startSeconds-0.01;
			}
			// set length to 2 seconds by default
			vs.endSeconds = vs.startSeconds + 2.;
			
			int gridPos = (lastGridPos++) % gridSize;
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			3.0,};
			shaderParams[0] = float(gridPos%gridN)/gridN + 1./gridN/2;
			shaderParams[1] = 1. - 0.25/gridN; //float(gridPos/gridN)/gridN + 1./gridN/2;
			shaderParams[2] = 1./gridN;
			shaderParams[3] = 1./gridN;
			shaderParams[4] = 0.3;
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			// percussion samples have some delay
			if(n.instrument == 3 || n.instrument == 4) {
				as.startSeconds -= 0.05;
			}
			segments.push_back(as);
			
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			vs.zIndex = n.channel;
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	defaultSettings.volume = 0.7;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

