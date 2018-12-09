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
	
	string buf = get_file_contents("songs/mado_-_sad.mod");
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08*4/30);
	addSource("hum", "sources/bass1.mkv",
					"sources/bass1.mkv", 2.8*8/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5*8/30, 1., 1.);
	addSource("gab1", "sources/gab1.wav",
					"sources/gab1.mkv", 2.55*4/30, 1./2, 1./2);
	addSource("gab2", "sources/gab2.mkv",
					"sources/gab2.mkv", 3.15*2/30, 1./2, 1./2);
	addSource("gab3", "sources/gab3.mkv",
					"sources/gab3.mkv", 2.5*2/30, 1./2, 1./2);
	addSource("ya", "sources/drink.mp4",
					"sources/drink.mp4", 2.45*2/30, 1., 1.);
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
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = texPos;
	}
	)aaaaa";
	
	string shader = R"aaaaa(
		float transpKey = param(6);
		float alpha = param(7);
		vec2 velocity = vec2(param(8), param(9));
		vec4 result = vec4(texture2D(image, pos*0.98+vec2(0.01,0.01)).rgb, 1.);
		result.a = length(result.rgb - vec3(transpKey,transpKey,transpKey));
		result.a = result.a*result.a*result.a*1000;
		result.a = clamp(result.a,0.0,1.0);
		result.a = clamp(result.a*alpha,0.0,1.0);
		return result;
		)aaaaa";

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
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double timeOffsetSeconds = 0.;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones + 41;
		
		// select source
		switch(n.instrument) {
			case 1:
				src = getSource("gab1");
				n.amplitudeDB += 10;
				break;
			case 2:
				n.pitchSemitones += 7.;
				src = getSource("hum"); n.amplitudeDB += 5;
				break;
			case 3:
				src = getSource("aaaa");
				n.amplitudeDB -= 1;
				n.pitchSemitones -= 5.;
				break;
			case 10:
				src = getSource("aaaa");
				n.pitchSemitones += 3 + 12;
				n.amplitudeDB += 6;
				n.end.absRow += 4;
				timeOffsetSeconds = -0.06;
				break;
			case 4:
				src = getSource("perc1");
				n.pitchSemitones = 0;
				n.amplitudeDB /= 2;
				n.amplitudeDB -= 3;
				timeOffsetSeconds = -0.05;
				break;
			case 5:
				n.amplitudeDB -= 13;
				segments.push_back(AudioSegment(n, ins, bpm));
				goto skip;
			default: goto skip;
		}
		if(n.channel == 5) goto skip;
		
		{
			VideoSegment vs(n, src->hasVideo()?src->video:&imgSource, bpm);
			vs.zIndex = n.channel;
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale	transpKey,	alpha
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			3.0,		-1.0,		1.0};
			
			switch(n.channel) {
				// main melody 1
				case 0: shaderParams = {0.2, 0.2, 0.4, 0.4, 0.8, 1.0, -1.0, 1.0}; break;
				case 1: shaderParams = {0.6, 0.2, 0.4, 0.4, 0.8, 1.0, -1.0, 1.0}; break;
				// bass
				case 2: shaderParams = {0.5, 0.5, 1.0, 1.0, 1.0, 0.0, -1.0, 1.0}; vs.zIndex = -100; break;
				// bass 2
				case 3: shaderParams = {0.6, 0.8, 0.4, 0.4, 0.8, 1.0, -1.0, 1.0}; break;
				// high melody 1
				case 4: shaderParams = {0.1, 0.6, 0.3, 0.3, 0.6, 5.0, 241./255., 1.0}; break;
				case 5: shaderParams = {0.3, 0.6, 0.3, 0.3, 0.6, 5.0, 241./255., 1.0}; break;
				case 6: shaderParams = {0.4, 0.6, 0.3, 0.3, 0.6, 5.0, 241./255., 1.0}; break;
				// percussion
				case 7: shaderParams = {0.8, 0.6, 0.2, 0.2, 0.8, 3.0, -1.0, 1.0}; break;
				case 8: shaderParams = {0.8, 0.6, 0.3, 0.3, 0.8, 3.0, -1.0, 1.0}; break;
				// main melody
				default: shaderParams = {0.1, 0.9, 0.2, 0.2, 0.8, 3.0, -1.0, 1.0}; break;
			}
			
			// channel 13 to 18 is main melody
			if(n.channel >= 13 && n.channel <= 18) {
				shaderParams[0] = 0.07 + (n.channel-13) * 0.16;
				shaderParams[6] = 241./255.;
			}
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += timeOffsetSeconds;
			segments.push_back(as);
			
			
			// channels 4,5,6 has tremolo applied; apply transparency effect to video
			if(n.channel == 4 || n.channel == 5 || n.channel == 6) {
				for(auto& kf: as.keyframes) {
					VideoKeyFrame vkf;
					vkf.relTimeSeconds = kf.relTimeSeconds;
					vkf.shaderParams = shaderParams;
					vkf.shaderParams[7] = (kf.amplitude[0] > 0.8) ? 1.0 : 0.5;
					vs.keyframes.push_back(vkf);
				}
			}
			
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	defaultSettings.volume = 0.25;
	//defaultSettings.skipToSeconds = 75.;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

