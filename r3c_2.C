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
	
	string buf = get_file_contents("songs/robocop3_collab_2.mod");
	
	// "do" has pitch 2.6/30
	addSource("angryasuna", "sources/angryasuna.mkv", "sources/angryasuna.mkv", 1.88/30, 1.5, 1.5);
	addSource("agiri_lead", "sources/agiri_lead.wav", "sources/agiri_lead.mkv", 2.64/30, 1., 1.);
	addSource("agiriwater", "sources/agiriwater.wav", "sources/agiriwater.mp4", 2.45/30, 1., 1.);
	addSource("agiriwater_2", "sources/agiriwater_2.mkv", "sources/agiriwater_2.mkv", 3.46/30, 1., 1.);
	addSource("daah", "sources/daah.mp4", "sources/daah.mp4", 2.75*2/30, 2., 2.);
	addSource("horn", "sources/horn.mp4", "sources/horn.mp4", 1.83/30, 1., 1.);
	addSource("forkbend2", "sources/forkbend2.wav", "sources/forkbend2.mkv", 2.3/30, 1., 1.);
	addSource("aaaaa", "sources/aaaaa.wav", "sources/aaaaa.mkv", 2.35/2/30, 1./2, 1./2);
	addSource("time2eat", "sources/time2eat.wav", "sources/time2eat.mkv", 2.85/30, 1., 1.);
	
	addSource("gab5", "sources/gab5.wav",
					"sources/gab5.mkv", 3.3*4/30, 1./2, 1./2);
	addSource("eee", "sources/eee1.wav", "sources/eee.mp4", 1.97*2/30, 1., 1.);
	
	addSource("perc1", "sources/perc1_short.wav",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	addSource("kick1", "sources/kick1.wav",
					"sources/crash2.mkv", 2., 2., 2.);
	addSource("drumkick", "sources/drumkick.wav", "sources/drumkick.mp4", 1., 1., 1.);
	addSource("kick3", "sources/kick3.wav", "sources/kick3.mkv", 1., 1., 1.);
	addSource("sigh", "sources/sigh.mp4", "sources/sigh.mp4", 1., 3., 3.);
	addSource("kick", "sources/kick.wav", "sources/kick.mkv", 1., 1., 1.);
	addSource("shootthebox", "sources/shootthebox.wav", "sources/shootthebox.mkv", 1., 1., 1.);
	
	trimSource("forkbend2", 0.1);
	trimSource("horn", 0.05);
	trimSource("eee", 0.06);
	//trimSource("kick3", 0.084);
	
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
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = texPos;
	}
	)aaaaa";
	
	string shader =
		"return vec4(texture2D(image, pos).rgb, 1.);";

	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastSeg[16] = {};
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		if(n.start.seq < 54) continue;
		n.pitchSemitones -= 9;
		n.pitchSemitones -= 7;
		
		// select source
		switch(n.instrument) {
			// bass
			case 4:
				n.pitchSemitones -= 2.8;
				if(n.channel == 0) {
					src = getSource("angryasuna");
					n.amplitudeDB += 1;
				} else {
					src = getSource("horn");
					n.amplitudeDB += 15;
					n.pitchSemitones += 12;
					{
						AudioSegment as(n, src->audio, bpm);
						segments.push_back(as);
					}
					n.pitchSemitones -= 12;
					n.amplitudeDB -= 5;
				}
				break;
			// ding
			case 5: src = getSource("daah"); n.amplitudeDB += 12; n.pitchSemitones += 12; n.end.absRow += 1; break;
			// kick
			case 2: src = getSource("drumkick"); n.amplitudeDB += 10; n.pitchSemitones = -3; delay = -0.04; break;
			// tss
			case 8: src = getSource("sigh"); n.amplitudeDB += 5; n.pitchSemitones = 3; break;
			// slap
			case 3: src = getSource("perc1"); n.amplitudeDB += 2; n.pitchSemitones = -3; delay = -0.03; break;
			// high tss
			case 7: src = getSource("sigh"); n.amplitudeDB -= 3; n.pitchSemitones = 8; break;
			// strings
			case 11: src = getSource("eee"); n.amplitudeDB += 2; n.pitchSemitones += 19.1; break;
			case 12: src = getSource("eee"); n.amplitudeDB += 2; n.pitchSemitones += 16.1; break;
			case 13: src = getSource("eee"); n.amplitudeDB += 2; n.pitchSemitones += 16.1; break;
			default: goto skip;
		}
		
		{
			int gridN = 4;
			int gridSize = gridN*gridN;
			VideoSegment vs(n, src->video, bpm);
			
			int gridPos = 0;
			switch(n.channel) {
				case 0: gridPos = 0; break;
				case 1: gridPos = 1; break;
				case 2: gridPos = 4; break;
				case 3: gridPos = 5; break;
				case 4: gridPos = 6; break;
				case 5: gridPos = 8; break;
				case 6: gridPos = 9; break;
				case 7: gridPos = 10; break;
				case 10: gridPos = 13; break;
				case 11: gridPos = 14; break;
				case 12: gridPos = 12; break;
				case 13: gridPos = 15; break;
				default: gridPos = 15; break;
			}
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			1.0,};
			shaderParams[0] = float(gridPos%gridN)/gridN + 1./gridN/2;
			shaderParams[1] = float(gridPos/gridN)/gridN + 1./gridN/2;
			shaderParams[2] = 1./gridN;
			shaderParams[3] = 1./gridN;
			shaderParams[4] = 0.8;
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += delay;
			segments.push_back(as);
			
			if(n.instrument == 3) { // slap
				if(n.amplitudeDB < 0) {
					videoSegments.at(lastSeg[n.channel]).endSeconds = vs.startSeconds;
					continue;
				}
			}
			
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			vs.zIndex = n.channel;
			lastSeg[n.channel] = (int)videoSegments.size();
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	
	// find song duration
	double songStart = 1e9, songEnd = -1e9;
	for(auto& vs: videoSegments) {
		if(vs.startSeconds < songStart) songStart = vs.startSeconds;
		if(vs.endSeconds > songEnd) songEnd = vs.endSeconds;
	}
	
	// add chroma key screen
	Image colorImage = {1, 1, "\xff\x00\x00\xff"s};
	ImageSource colorImageSource(&colorImage);
	
	VideoSegment vs;
	vs.startSeconds = songStart;
	vs.endSeconds = songEnd;
	vs.source = &colorImageSource;
	vs.zIndex = -1000;
	videoSegments.push_back(vs);
	
	defaultSettings.volume = 1./3;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

