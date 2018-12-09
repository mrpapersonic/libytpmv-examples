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
	vec2 offsPos = vec2(userParams[6], userParams[7]);
	float aspect = resolution.x/resolution.y;
	float t = secondsRel*sizeScale;
	float magnification = minSize + sqrt(sqrt(abs(t)))*0.2;
	
	coords.x *= aspect;
	center.x *= aspect;
	coords = center + (coords-center) * magnification;
	
	vec2 pos = vec2(-28*aspect, 14);
	pos.y -= (gl_InstanceID / 28)*2;
	pos.x += (gl_InstanceID % 28)*2*aspect;
	pos.x += mod(offsPos.x, aspect*2);
	pos.y += mod(offsPos.y, 2);
	
	coords += vec3(pos,0);
	
	coords.z -= 2.0;
	coords = rotationMatrix(vec3(1,0,0), rotationX) * coords;
	coords.z += 2.0;
	
	coords.z -= 3.0;
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
		//result = vec4(texture2D(image, vec2(0.5,0.5)).rgb, 1.);
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
		
		gl_FragDepth = gl_FragCoord.z;
		if(p.x < cornerRadius && p.y < cornerRadius && cornerRadius > 0.01)
			if(distToCorner > cornerRadius) {
				color = vec4(0,0,0,0);
				gl_FragDepth = 100.0;
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

// flip the image on a video segment
void flip(vector<float>& vertexes) {
	for(int i=0; i<(int)vertexes.size(); i+=8) {
		vertexes[i+6] = 1.0 - vertexes[i+6];
	}
}

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	string buf = get_file_contents("songs/firebird.mod");
	// "do" has pitch 2.6/30
	addSource("angryasuna", "sources/angryasuna.mkv", "sources/angryasuna.mkv", 1.88/30, 1.5, 1.5);
	addSource("agiri_lead", "sources/agiri_lead.wav", "sources/agiri_lead.mkv", 2.64/30, 1., 1.);
	addSource("agiriwater", "sources/agiriwater.wav", "sources/agiriwater.mp4", 2.45/30, 1., 1.);
	addSource("agiriwater_2", "sources/agiriwater_2.mkv", "sources/agiriwater_2.mkv", 3.46/30, 1., 1.);
	addSource("daah", "sources/daah.mp4", "sources/daah.mp4", 2.75/30, 1., 1.);
	addSource("horn", "sources/horn.mp4", "sources/horn.mp4", 1.83/30, 1., 1.);
	addSource("forkbend2", "sources/forkbend2.wav", "sources/forkbend2.mkv", 2.3/30, 1., 1.);
	addSource("aaaaa", "sources/aaaaa.wav", "sources/aaaaa.mkv", 2.35/2/30, 1./2, 1./2);
	addSource("time2eat", "sources/time2eat.wav", "sources/time2eat.mkv", 2.85/30, 1., 1.);
	
	addSource("gab5", "sources/gab5.wav",
					"sources/gab5.mkv", 3.3*4/30, 1./2, 1./2);
	addSource("eee", "sources/eee.wav", "sources/eee.mp4", 1.97*2/30, 2., 2.);
	
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
	trimSource("eee", 0.05);
	//trimSource("kick3", 0.084);
	
	
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
	
	bool dir[16] = {};
	
	for(int i=0;i<(int)notes.size();i++) {
		int vsStart = (int)videoSegments.size();
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.0;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones += ins.tuningSemitones + 41;
		
		//if(n.start.seq < 15) continue;
		
		auto genVertices = [](double centerX, double centerY, double z) {
			return genRectangleWithCenter(centerX-0.7, centerY-0.7, centerX+0.7, centerY+0.7, 0,0,1,1, z);
		};
		
		auto placeNote = [&](string source, int id) -> VideoSegment& {
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
			vs.instances = 28*34;
			
			static const int idMap[] = {3,5,6,2,4,7,1,0,8,9};
			id = idMap[id];
			
			double centerX, centerY;
			centerX = ((id/3)/3. + 0.5)*(2.) - 1. + id*.2;
			centerY = ((id%3)/3. + 0.5)*(2.) - 1. + id*.2;
			
			vs.vertexes.clear();
			srand48(123567);
			for(int j=0; j<9; j++) {
				auto v = genVertices(centerX - drand48()*10, centerY - drand48()*10, (j+1)*0.15 + id*0.04);
				vs.vertexes.insert(vs.vertexes.end(),v.begin(),v.end());
			}
			
			
			
			//vs.startSeconds += delay;
			//vs.endSeconds += delay;
			vs.offsetSeconds -= delay;
			//				minSizeScale,	sizeScale	transpKeyEnable	cornerRadius,	rotationX,	zoom,	offX,	offY
			vs.shaderParams = {0.2,			1.0,		-1.0,			0.3,			0.,			0.,		0,	0};
			
			double startTime = vs.startSeconds;
			vs.interpolateKeyframes = [bpm, startTime, id](vector<float>& out,
						const vector<float>& kf1, const vector<float>& kf2,
						double t1, double t2, double t) {
				double a = (t1==t2) ? 0. : clamp((t-t1)/(t2-t1), 0., 1.);
				double b = 1. - a;
				for(int i=0; i<(int)out.size(); i++)
					out[i] = b*kf1.at(i) + a*kf2.at(i);
				
				double tAbs = t + startTime;
				double trans1 = (64*4+15)*60./bpm;
				if(tAbs > trans1) {
					out[6] += (tAbs-trans1)*0.2;
					out[7] += (tAbs-trans1)*0.5;
				}
				double trans2 = trans1+(64*11 + 32 + 15)*60./bpm;
				if(tAbs > trans2) {
					// rotationX
					out[4] = clamp((tAbs-trans2)*2., 0., 1.) * -0.9;
					// zoom
					//out[5] = clamp((tAbs-trans2)*2., 0., 1.) * -3.;
				}
			};
			
			videoSegments.push_back(vs);
			return videoSegments.back();
		};
		
		dir[n.channel] = !dir[n.channel];
		
		// select source
		if(n.channel == 4) {
			if(n.instrument == 14) {
				// kick
				n.pitchSemitones = 3;
				n.amplitudeDB += 7;
				delay -= 0.04;
				VideoSegment& vs = placeNote("drumkick", 0);
				//vs.vertexes = genVertices(0., 0., 4.);
				//vs.zIndex = -100;
			}
		} else if(n.channel == 5) {
			if(n.instrument == 8) {
				// snare
				n.pitchSemitones = 0;
				n.amplitudeDB -= 2;
				delay -= 0.035;
				VideoSegment& vs = placeNote("perc1", 0);
				//vs.vertexes = genVertices(0., 0., 4.);
				//vs.zIndex = -100;
			}
		} else if(n.channel == 3) {
			// high lead
			if(n.start.seq >= 18)
				n.pitchSemitones += 24;
			else n.pitchSemitones += 12;
			
			n.amplitudeDB -= 3;
			pan = 0.3;
			delay = -0.03;
			VideoSegment& vs = placeNote("gab5", 2);
			vs.shaderParams[1] = 0.;
			
			pan = 0.7;
			delay += 0.05;
			VideoSegment& vs2 = placeNote("gab5", 3);
			vs2.shaderParams[1] = 0.;
			//vs2.vertexes = genVertices(0.5, 1., 3.5);
		} else if(n.channel == 0) {
			// lead
			if(n.instrument == 1 && n.start.seq >= 5) {
				// second part
				n.pitchSemitones += 12;
				n.amplitudeDB -= 3;
				delay = -0.03;
				
				pan = 0.6;
				n.amplitudeDB += 3;
				VideoSegment& vs3 = placeNote("eee", 4);
				n.amplitudeDB -= 3;
				if(dir[n.channel]) flip(vs3.vertexes);
				
				if(n.start.seq < 17)
					n.pitchSemitones -= 12;
				pan = 0.4;
				VideoSegment& vs2 = placeNote("forkbend2", 5);
				if(dir[n.channel]) flip(vs2.vertexes);
				
				for(int harm = 2; harm < 5; harm++) {
					AudioSegment as(n, src->audio, bpm);
					as.pitch *= harm;
					as.tempo *= harm;
					double scale = 1./pow(harm,2);
					for(int k=0;k<CHANNELS;k++) as.amplitude[k] *= scale;
					segments.push_back(as);
				}
				
				pan = 0.6;
				n.amplitudeDB -= 5;
				delay += -0.15;
				VideoSegment& vs = placeNote("time2eat", 6);
				if(dir[n.channel]) flip(vs.vertexes);
			} else {
				n.amplitudeDB -= 4;
				
				if(n.start.seq < 5) n.pitchSemitones += 12;
				n.amplitudeDB += 5;
				VideoSegment& vs3 = placeNote("eee", 4);
				if(dir[n.channel]) flip(vs3.vertexes);
				n.amplitudeDB -= 5;
				n.pitchSemitones -= 12;
				
				pan = 0.4;
				VideoSegment& vs2 = placeNote("forkbend2", 5);
				if(dir[n.channel]) flip(vs2.vertexes);
			}
		} else if(n.channel == 1) {
			// lead chord
			n.pitchSemitones += 12;
			pan = 0.6;
			n.amplitudeDB -= 8;
			delay = -0.15;
			VideoSegment& vs3 = placeNote("time2eat", 7);
		} else if(n.channel == 6) {
			// temp lead
			pan = 0.4;
			n.amplitudeDB -= 9;
			n.pitchSemitones -= 12;
			VideoSegment& vs = placeNote("forkbend2", 8);
			//n.pitchSemitones -= 12;
			n.amplitudeDB += 10;
			delay = -0.15;
			VideoSegment& vs3 = placeNote("time2eat", 1);
			//vs3.vertexes = genVertices(1., 1., 1.0);
			//vs3.zIndex = n.start.absRow + 1;
		} else if(n.channel == 2) {
			// bass
			n.amplitudeDB += 12;
			{
				AudioSegment as(n, getSource("horn")->audio, bpm);
				segments.push_back(as);
			}
			n.amplitudeDB += 3;
			n.pitchSemitones += 12;
			VideoSegment& vs1 = placeNote("horn", 9);
			vs1.vertexes = genRectangleWithCenter(-1, -1, 1, 1, 0,0,1,1, 0.);
			vs1.zIndex = 200;
			vs1.shaderParams[0] = 1.0;
			vs1.shaderParams[1] = 0.0;
			vs1.shaderParams[3] = 0.0;
			
			for(int harm = 2; harm < 20; harm++) {
				AudioSegment as(n, src->audio, bpm);
				as.pitch *= harm;
				as.tempo *= harm;
				
				double scale = 1./pow(harm,1.5);
				//if(harm < 3) scale = 1./pow(3,1.5);
				if((harm%2) == 0) scale *= 0.5;
				
				for(int k=0;k<CHANNELS;k++) as.amplitude[k] *= scale;
				segments.push_back(as);
				
				if(harm % 2) continue;
				double inset = harm/25.0;
				VideoSegment vs(n, src->video, bpm);
				vs.instances = 28*34;
				vs.zIndex = 200 - harm;
				vs.vertexes = genRectangleWithCenter(-1, -1, 1, 1, 0,0,1,1, harm*0.001);
				vs.vertexShader = &vertexShader;
				vs.fragmentShader = &shader;
				//				minSizeScale,	sizeScale	transpKeyEnable		cr		rot		zoom,	
				vs.shaderParams = {0.8-inset,	2.0+harm/5.,-1.0,				0.0,	0.,		0.,		0,0};
				vs.vertexVarSizes[0] = 3;
				vs.vertexVarSizes[1] = 3;
				vs.vertexVarSizes[2] = 2;
				vs.vertexVarSizes[3] = 0;
				vs.interpolateKeyframes = videoSegments.back().interpolateKeyframes;
				videoSegments.push_back(vs);
			}
		} else continue;
	}
	defaultSettings.volume = 0.3;
	//defaultSettings.skipToSeconds = 10.;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

