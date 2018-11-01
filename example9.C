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

using namespace std;
using namespace ytpmv;


int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	string buf = get_file_contents("songs/ohayou_trololo.mod");
	
	addSource("o", "sources/o_35000.wav", "sources/o.mkv", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "sources/ha.mkv", 2.107/30);
	addSource("hum", "sources/bass1_4.wav",
					"sources/bass1.mkv", 2.8/30, 2., 1.);
	addSource("hum2", "sources/bass1_3.wav", "", 2.8/30, 1., 1.);
	
	getSource("hum2")->video = getSource("hum")->video;
	
	string vertexShader = R"aaaaa(
	#version 330 core
	layout(location = 0) in vec3 myPos;
	layout(location = 1) in vec2 texPos;
	uniform vec2 coordBase;
	uniform mat2 coordTransform;
	uniform float secondsRel;
	uniform float secondsAbs;
	uniform float userParams[16];
	uniform mat4 proj;
	smooth out vec2 uv;
	
	void main() {
		vec3 coords = myPos;
		float sizeScale = userParams[4] * secondsRel;
		
		coords.z = -1.0;
		
		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		
		uv = texPos;
	}
	)aaaaa";
	string shader = R"aaaaa(
		float opacityScale = param(0) + param(5) * (secondsRel-0.2);
		vec3 transpKey = vec3(param(6),param(7),param(8));
		opacityScale = clamp(opacityScale,0.0,1.0);
		float radius = param(1);
		vec2 aspect = vec2(resolution.x/resolution.y, 1.0);
		float dist = length((pos-vec2(0.5,0.5))*aspect)*2;
		float opacity = clamp(radius-pow(dist,5), 0.0, 1.0);
		vec4 result = vec4(texture2D(image, pos).rgb, 1.0);
		
		result.a = length(result.rgb - transpKey);
		result.a = result.a*result.a*result.a*1000;
		result.a = clamp(result.a,0.0,1.0);
		result.a = clamp(result.a*opacityScale*opacity,0.0,1.0);
		return result;
		)aaaaa";
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones -= 8;
		
		// select source
		switch(n.instrument) {
			case 1:
				src = getSource("o"); n.pitchSemitones += 3; n.amplitudeDB += 7;
				{
					AudioSegment as(n, src->audio, bpm);
					segments.push_back(as);
				}
				n.pitchSemitones -= 12;
				n.amplitudeDB -= 7;
				break;
			case 2: src = getSource("lol"); n.pitchSemitones += 12; n.amplitudeDB += 5; break;
			case 3: src = getSource("ha"); n.pitchSemitones += 12; n.amplitudeDB += 7; break;
			default: continue;
		}
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		segments.push_back(as);
		
		
		VideoSegment vs(n, src->video, bpm);
		
		// set video clip position and size
		//							opacity		radius,	vx,	vy,	sizeScale	opacityScale	transpKey (r,g,b)
		vector<float> shaderParams = {1.0,		6.,		0.,	0.,	0.1,		0.,				-1., -1., -1.};
		switch(n.channel) {
			case 0:	// main
				if(n.instrument == 3) {
					vs.vertexes = genRectangle(-1, 0, 0, 1);
					shaderParams[6] = 149./255;
					shaderParams[7] = 117./255;
					shaderParams[8] = 57./255;
				} else {
					vs.vertexes = genRectangle(0, -1, 1, 0);
					shaderParams[6] = 153./255;
					shaderParams[7] = 121./255;
					shaderParams[8] = 66./255;
				}
				shaderParams[5] = -0.5;
				if((n.start.seq == 3 || n.start.seq == 4)
						&& n.start.row < 8) {
					double offs = double(n.start.row)/6;
					vs.vertexes = genRectangle(-1, -offs, 0, 1.-offs);
					shaderParams[5] = -1.;
					vs.endSeconds += 1;
				}
				vs.zIndex = n.start.absRow + 1;
				break;
			case 1:	// bass
				vs.vertexes = genRectangle(-1, -1, 1, 1);
				shaderParams[1] = 1000;
				vs.zIndex = -100;
				break;
			default: continue;
		}
		
		vs.vertexShader = &vertexShader;
		vs.shader = &shader;
		vs.shaderParams = shaderParams;
		videoSegments.push_back(vs);
	}
	defaultSettings.volume = 1./4;
	//defaultSettings.skipToSeconds = 22;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

