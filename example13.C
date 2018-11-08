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
	
	string buf = get_file_contents("songs/diablo2oo2.mod");
	// "do" has pitch 2.64/30
	addSource("angryasuna", "sources/angryasuna.mkv", "sources/angryasuna.mkv", 1.88/30, 1.5, 1.5);
	addSource("agiri_lead", "sources/agiri_lead.wav", "sources/agiri_lead.mkv", 2.64/30, 1., 1.);
	addSource("agiriwater", "sources/agiriwater.wav", "sources/agiriwater.mp4", 2.45/30, 1., 1.);
	addSource("agiriwater_2", "sources/agiriwater_2.mkv", "sources/agiriwater_2.mkv", 3.46/30, 1., 1.);
	addSource("daah", "sources/daah.mp4", "sources/daah.mp4", 2.75/30, 1., 1.);
	addSource("horn", "sources/horn.mp4", "sources/horn.mp4", 1.83/30, 1., 1.);
	addSource("forkbend2", "sources/forkbend2.wav", "sources/forkbend2.mkv", 2.3/30, 1., 1.);
	addSource("aaaaa", "sources/aaaaa.wav", "sources/aaaaa.mkv", 2.35/2/30, 1./2, 1./2);
	addSource("time2eat", "sources/time2eat.wav", "sources/time2eat.mkv", 2.9/30, 1., 1.);
	
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	addSource("kick1", "sources/kick1.wav",
					"sources/crash2.mkv", 2., 2., 2.);
	addSource("drumkick", "sources/drumkick.mp4", "sources/drumkick.mp4", 1., 1., 1.);
	addSource("kick3", "sources/kick3.mkv", "sources/kick3.mkv", 1., 1., 1.);
	addSource("sigh", "sources/sigh.mp4", "sources/sigh.mp4", 1., 3., 3.);
	addSource("kick", "sources/kick.wav", "sources/kick.mkv", 1., 1., 1.);
	addSource("shootthebox", "sources/shootthebox.wav", "sources/shootthebox.mkv", 1., 1., 1.);
	
	trimSource("forkbend2", 0.1);
	trimSource("horn", 0.05);
	
	string vertexShader = R"aaaaa(
	#version 330 core
	layout(location = 0) in vec3 myPos;
	layout(location = 1) in vec3 myCenter;
	layout(location = 2) in vec2 texPos;
	uniform vec2 coordBase;
	uniform mat2 coordTransform;
	uniform float secondsRel;
	uniform float secondsAbs;
	uniform float userParams[16];
	uniform vec2 resolution;
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
		vec3 center = myCenter;
		float minSize = userParams[0];
		float sizeScale = userParams[1];
		float rotationX = userParams[4];
		float zoom = userParams[5];
		float transTime = userParams[6];
		float transTime2 = userParams[7];
		float aspect = resolution.x/resolution.y;
		float t = secondsRel*sizeScale;
		float magnification = minSize + sqrt(abs(t))*0.2;
		
		// only apply zoom and rotation after transTime
		zoom *= clamp((secondsAbs - transTime + 2) * 0.5, 0.0, 1.0);
		rotationX *= clamp((secondsAbs - transTime) * 2.5, 0.0, 1.0);
		
		float timeAfterTrans2 = clamp((secondsAbs - transTime2) * 0.05, 0.0, 1.0);
		zoom -= timeAfterTrans2*15;
		
		coords.x *= aspect;
		coords.z *= 0.1;
		center.z *= 0.1;
		coords = center + (coords-center) * magnification;
		
		
		
		vec2 offsPos = vec2(0, clamp(secondsAbs - transTime, 0.0, 1e6) * 2.);
		//offsPos.x += clamp(secondsAbs - transTime2, 0.0, 1e6) * 0.5;
		
		vec2 pos = vec2(-28*aspect, 16);
		pos.y -= (gl_InstanceID / 28)*2;
		pos.x += (gl_InstanceID % 28)*2*aspect;
		pos.x += mod(offsPos.x, aspect*2);
		pos.y += mod(offsPos.y, 2);
		
		coords += vec3(pos,0);
		
		coords = rotationMatrix(vec3(1,0,0), rotationX) * coords;
		
		
		//float rotationZ = 3.142 * 2 * timeAfterTrans2;
		
		//coords = rotationMatrix(vec3(0,0,1), rotationZ) * coords;
		
		coords.z -= 2.3;
		coords.z += zoom;
		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = texPos;
	}
	)aaaaa";
					
	string shader = R"aaaaa(
		#version 330 core
		in vec4 gl_FragCoord;
		smooth in vec2 uv;
		out vec4 color; 
		uniform vec2 resolution;
		uniform float secondsAbs;
		uniform float secondsRel; 
		uniform sampler2D image;
		uniform float userParams[16];
		float param(int i){ 
			return userParams[i];
		}
		// return 1 if v inside the box, return 0 otherwise
		float insideBox(vec2 v, vec2 bottomLeft, vec2 topRight) {
			vec2 s = step(bottomLeft, v) - step(topRight, v);
			return s.x * s.y;   
		}

		void main() {
			float transpKeyEnable = param(2);
			float cornerRadius = param(3);
			vec4 result;
			
			float aspect = resolution.x/resolution.y;
			
			
			float alpha = 1.0; //clamp((fade-secondsRel)*0.5,0.0,1.0);
			
			result = vec4(texture2D(image, uv*0.98+vec2(0.01,0.01)).rgb, 1.);
			if(uv.x < -0.01 || uv.y < -0.01 || uv.x > 1.01 || uv.y > 1.01) {
				color = vec4(0,0,0,0);
				return;
			}
			
			vec2 mirroredUV = uv-vec2(0.5,0.5);
			mirroredUV.x = abs(mirroredUV.x);
			mirroredUV.y = abs(mirroredUV.y);
			
			
			vec2 cr = vec2(cornerRadius, cornerRadius);
			vec2 p = vec2(1,1) - mirroredUV*2;
			p.x *= aspect;
			float distToCorner = length(p-vec2(cornerRadius,cornerRadius));
			
			if(p.x < cornerRadius && p.y < cornerRadius && cornerRadius > 0.01)
				if(distToCorner > cornerRadius) {
					color = vec4(0,0,0,0);
					return;
				}
			
			vec3 transpKey = vec3(62,255,5)/255.0 * transpKeyEnable;
			result.a = length(result.rgb - transpKey);
			result.a = result.a*result.a*result.a*3;
			result.a = clamp(result.a,0.0,1.0);
			result.a = clamp(result.a*alpha,0.0,1.0);
			color = result;
		}
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
	
	
	for(int i=0;i<(int)notes.size();i++) {
		int vsStart = (int)videoSegments.size();
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.0;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones + 41;
		
		auto placeNote = [&](string source) -> VideoSegment& {
			src = getSource(source);
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += delay;
			segments.push_back(as);
			
			VideoSegment vs(n, src->video, bpm);
			vs.zIndex = n.channel;
			vs.vertexShader = &vertexShader;
			vs.fragmentShader = &shader;
			vs.vertexVarSizes[0] = 3;
			vs.vertexVarSizes[1] = 3;
			vs.vertexVarSizes[2] = 2;
			vs.vertexVarSizes[3] = 0;
			//vs.startSeconds += delay;
			//vs.endSeconds += delay;
			vs.offsetSeconds -= delay;
			//				minSizeScale,	sizeScale	transpKeyEnable	cornerRadius,	rotationX,	zoom,	transTime,	transTime2
			vs.shaderParams = {0.6,			3.0,		-1.0,			0.3,			0.,			0.,		1e6,	1e6};
			videoSegments.push_back(vs);
			return videoSegments.back();
		};
		
		// select source
		if(n.channel == 3) {
			if(n.amplitudeDB < -3) continue;
			if(n.instrument == 3) {
				n.pitchSemitones = 0;
				n.amplitudeDB += 5;
				delay -= 0.057;
				VideoSegment& vs = placeNote("kick");
				vs.vertexes = genRectangleWithCenter(-1, 0, 0, 1, 0,0,1,1, 1.);
				vs.zIndex = -100;
			} else {
				// snare
				n.pitchSemitones = -5;
				n.amplitudeDB += 10;
				delay -= 0.06;
				VideoSegment& vs = placeNote("shootthebox");
				vs.vertexes = genRectangleWithCenter(-1, 0, 0, 1, 0,0,1,1, 1.);
				vs.zIndex = -100;
			}
		} else if(n.channel == 2) {
			// high lead
			n.pitchSemitones += 12;
			n.amplitudeDB -= 2;
			pan = 0.5;
			VideoSegment& vs = placeNote("agiriwater");
			vs.vertexes = genRectangleWithCenter(0, 0, 1, 1, 0,0,1,1, 1.);
			vs.zIndex = -100;
		} else if(n.channel == 0) {
			// lead
			n.pitchSemitones += 12;
			if(n.start.seq < 2) {
				n.amplitudeDB += 2;
				VideoSegment& vs = placeNote("aaaaa");
				vs.vertexes = genRectangleWithCenter(-1, -1, 1, 1,
													0, 0, 1.4, 1.4, 1.5);
				vs.zIndex = n.start.absRow + 1;
				vs.shaderParams[2] = 1.0;
			} else if(n.start.seq == 3) {
				delay = -0.12;
				VideoSegment& vs = placeNote("time2eat");
				vs.vertexes = genRectangleWithCenter(-1, -1, 0, 0, 0,0,1,1, 1.);
				vs.zIndex = n.start.absRow + 1;
			} else if((n.start.seq >= 6 && n.start.seq <= 11)
					 || n.start.seq >= 16) {
				VideoSegment& vs2 = placeNote("forkbend2");
				vs2.vertexes = genRectangleWithCenter(0, -1, 1, 0, 0,0,1,1, 1.);
				vs2.zIndex = n.start.absRow + 1;
				
				n.amplitudeDB -= 3;
				delay = -0.15;
				VideoSegment& vs = placeNote("time2eat");
				vs.vertexes = genRectangleWithCenter(-1, -1, 0, 0, 0,0,1,1, 1.);
				vs.zIndex = n.start.absRow + 1;
			} else if(n.start.seq >= 12) {
				VideoSegment& vs2 = placeNote("forkbend2");
				vs2.vertexes = genRectangleWithCenter(0, -1, 1, 0, 0,0,1,1, 1.);
				vs2.zIndex = n.start.absRow + 1;
				
				n.amplitudeDB += 3;
				VideoSegment& vs = placeNote("aaaaa");
				vs.vertexes = genRectangleWithCenter(-1, -1, 0, 0, 0.2,0,1.2,1, 1.);
				vs.zIndex = n.start.absRow + 1;
				vs.shaderParams[2] = 1.0;
			} else {
				VideoSegment& vs = placeNote("forkbend2");
				vs.vertexes = genRectangleWithCenter(0, -1, 1, 0, 0,0,1,1, 1.);
				vs.zIndex = n.start.absRow + 1;
			}
			pan = 0.5;
		} else if(n.channel == 1) {
			// bass
			n.amplitudeDB += 18;
			n.pitchSemitones += 12;
			VideoSegment& vs = placeNote("horn");
			vs.vertexes = genRectangleWithCenter(-1, -1, 1, 1, 0,0,1,1, 0.);
			vs.zIndex = -200;
			vs.shaderParams[0] = 1.0;
			vs.shaderParams[1] = 0.0;
			vs.shaderParams[3] = 0.0;
			
			for(int harm = 2; harm < 20; harm++) {
				AudioSegment as(n, src->audio, bpm);
				as.pitch *= harm;
				as.tempo *= harm;
				for(int k=0;k<CHANNELS;k++) as.amplitude[k] *= 1./pow(harm,1.7);
				segments.push_back(as);
				if(harm%2) continue;
				double inset = harm/25.0;
				VideoSegment vs(n, src->video, bpm);
				vs.zIndex = -200 - harm;
				vs.vertexes = genRectangleWithCenter(-1, -1, 1, 1, 0,0,1,1, harm*0.001);
				vs.vertexShader = &vertexShader;
				vs.fragmentShader = &shader;
				//				minSizeScale,	sizeScale	transpKeyEnable		cr		rot		zoom,	transTime
				vs.shaderParams = {0.8-inset,	2.0+harm/5.,-1.0,				0.0,	0.,		0.,		1e6,1e6};
				vs.vertexVarSizes[0] = 3;
				vs.vertexVarSizes[1] = 3;
				vs.vertexVarSizes[2] = 2;
				vs.vertexVarSizes[3] = 0;
				videoSegments.push_back(vs);
			}
		} else continue;
		
		for(int j=vsStart; j<(int)videoSegments.size(); j++) {
			videoSegments[j].shaderParams[4] = -1.2;
			videoSegments[j].shaderParams[5] = -2.5;
			// time of first transition
			videoSegments[j].shaderParams[6] = 10*64*60./bpm;
			// second transition
			videoSegments[j].shaderParams[7] = 14*64*60./bpm;
			videoSegments[j].instances = 28*34;
		}
	}
	defaultSettings.volume = 0.3;
	//defaultSettings.skipToSeconds = 10.;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

