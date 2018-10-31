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
#include <array>
#include <assert.h>

using namespace std;
using namespace ytpmv;

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	string buf = get_file_contents("songs/skugg.mod");
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08*4/30);
	addSource("hum", "sources/bass1.wav",
					"sources/bass1.mkv", 2.8*8/30, 1., 2.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5*8/30, 1., 1.);
	addSource("gab1", "sources/gab1.wav",
					"sources/gab1.mkv", 2.55*4/30, 1., 1.);
	addSource("gab2", "sources/gab2.mkv",
					"sources/gab2.mkv", 3.15*2/30, 1./2, 1./2);
	addSource("gab3", "sources/gab3.wav",
					"sources/gab3.mkv", 2.55*2/30, 1., 1.);
	addSource("gab5", "sources/gab5.wav",
					"sources/gab5.mkv", 3.4*4/30, 1./2, 1./2);
	addSource("gab7", "sources/gab7.mkv",
					"sources/gab7.mkv", 1.7*4/30, 1./2, 1./2);
	addSource("gab8", "sources/gab8.wav",
					"sources/gab8.mkv", 2.7*2/30, 1./2, 1./5);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 2., 1.);
	addSource("crash1", "sources/crash1.wav",
					"sources/crash2.mkv", 1., 1., 1.);
	trimSource("hum", 0.1);
	trimSource("aaaa", 0.05);
	//trimSource("gab3", 0.1);
	trimSource("gab7", 0.3);
	
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
	mat3 rotationMatrix(vec3 axis, float angle) {
		axis = normalize(axis);
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		
		return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
					oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
					oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);
	}
	
	void main() {
		vec3 coords = myPos;
		vec3 velocity = vec3(userParams[4], userParams[5], userParams[6]);
		float rotation = userParams[7];
		float rotationTime = clamp((secondsAbs - userParams[8])*5.0 + 1.0, 0.0, 1.0);
		//float sizeScale = clamp(pow(rotationTime*2.0-1.0,4)*2+0.5, 0.0, 1.0);
		rotation += rotationTime * 3.142;
		
		coords.xy *= 1.3;
		coords += velocity*secondsRel;
		coords = rotationMatrix(vec3(0.0,1.0,0.0), rotation) * coords;
		coords.z -= 3.0;
		
		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		
		//float texZoom = 1.0 - clamp(secondsRel*10.0, 0.0, 0.1);
		
		uv = texPos; //(texPos-vec2(0.5,0.5))*texZoom + vec2(0.5,0.5);
	}
	)aaaaa";
	string shader = R"aaaaa(
		float transpKey = param(2);
		float fade = param(3);
		vec4 result;
		
		float alpha = clamp((fade-secondsRel)*0.5,0.0,1.0);
		
		result = vec4(texture2D(image, uv*0.98+vec2(0.01,0.01)).rgb, 1.);
		result.a = length(result.rgb - vec3(transpKey,transpKey,transpKey));
		result.a = result.a*result.a*result.a*1000;
		result.a = clamp(result.a,0.0,1.0);
		result.a = clamp(result.a*alpha,0.0,1.0);
		return result;
		)aaaaa";
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	double songEnd = 0;
	for(int i=0;i<(int)notes.size();i++) {
		double t = notes[i].end.absRow*60./bpm;
		if(t>songEnd) songEnd = t;
	}
	fprintf(stderr, "SONG END: %f\n", songEnd);
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastGridPos = 0;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double audioDelay = 0., videoDelay = 0.;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones + 41;
		
		// select source
		switch(n.channel) {
			case 1:
				// bass
				src = getSource("hum");
				n.pitchSemitones -= 12;
				n.amplitudeDB += 7;
				// add two audio layers, one an octave higher
				{
					AudioSegment as(n, src->audio, bpm);
					segments.push_back(as);
				}
				n.pitchSemitones -= 12;
				n.amplitudeDB -= 5;
				break;
			case 0:
			{
				// main melody
				bool part2 = false;
				if(n.start.seq >= 3 && n.start.seq <= 6) part2 = true;
				if(n.start.seq >= 11 && n.start.seq <= 14) part2 = true;
				src = getSource("gab8");
				n.pitchSemitones -= 24;
				n.amplitudeDB -= 1;
				//n.end.absRow += 4;
				if(n.channel == 1) n.amplitudeDB -= 5;
				break;
			}
			case 2:
				// high melody
				src = getSource("gab3");
				n.amplitudeDB += 10;
				n.pitchSemitones -= 12;
				break;
			default: goto skip;
		}
		
		{
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += audioDelay;
			segments.push_back(as);
		}
		
		{
			VideoSegment vs(n, src->video, bpm);
			vs.zIndex = n.channel;
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			
			//							minSizeScale,	sizeScale	transpKey,	fade,	vx,	vy	vz	rotation	rotateTime
			vector<float> shaderParams = {0.8,			3.0,		-1.0,		5.0,	0.,	0.,	0.,	0.,			1e9};
			
			switch(n.channel) {
				// main melody
				case 0:
				{
					int note = (int)n.pitchSemitones - 22;
					
					shaderParams = {0.8, 1.0, -1.0, 2.0, 0.0, 0.0, 0., 0., 0.0};
					vs.vertexes = genRectangle(0, -1, 1, 0);
					
					vs.zIndex = n.start.absRow + 1;
					break;
				}
				// bass
				case 1:
					shaderParams = {1.0, 0.0, -1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0};
					vs.vertexes = genRectangle(-1, -1, 0, 0);
					
					vs.zIndex = -100;
					break;
				// high melody 1
				case 2:
				{
					shaderParams = {0.8, 3.0, -1., 2.0, 0.0, 0.0, 0.0, 0.0, 0.0};
					vs.vertexes = genRectangle(0, 0, 1, 1);
					
					vs.zIndex = -100;
					break;
				}
				default: goto skip;
			}
			
			int nextRotate = int(n.start.absRow/8)*8+4;
			if(n.start.absRow >= nextRotate) nextRotate += 8;
			shaderParams[8] = double(nextRotate)*60./bpm;
			if((((n.start.absRow+4)/8) % 2) == 1) shaderParams[7] = 3.142;
			
			vs.shaderParams = shaderParams;
			vs.startSeconds += videoDelay;
			if(vs.endSeconds > songEnd) {
				vs.endSeconds = songEnd + 0.5;
				vs.shaderParams[3] = songEnd - vs.startSeconds;
			}
			videoSegments.push_back(vs);
			
			// the original mod lacks percussion; add it every 8 rows of the bass
			if(n.channel == 1 && (n.start.row % 8) == 4) {
				src = getSource("perc2");
				n.pitchSemitones = 0;
				n.start.rowOffset = -0.15;
				segments.push_back(AudioSegment(n, src->audio, bpm));
				VideoSegment vs(n, src->video, bpm);
				vs.vertexShader = &vertexShader;
				vs.shader = &shader;
				vs.shaderParams = shaderParams;
				vs.vertexes = genRectangle(-1, 0, 0, 1);
				videoSegments.push_back(vs);
			}
		}
	skip: ;
	}
	defaultSettings.volume = 0.3;
	//defaultSettings.skipToSeconds = 10.;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

