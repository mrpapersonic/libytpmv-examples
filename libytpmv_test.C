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
	string buf = get_file_contents("songs/aix_-_plastic_pop_2.mod");
	
	addSource("o", "./sources/o_35000_fade.wav", "./sources/o.mkv", 3.5/30);
	addSource("lol", "./sources/lol_20800.wav", "./sources/lol.mkv", 2.08/30);
	addSource("hum", "./sources/agiriice_hpf3.wav", "./sources/agiriice.mp4", pow(2,0.1/12));
	addSource("hum2", "sources/bass1_3.wav", "", 2.8/30, 1., 1.);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	addSource("kick1", "sources/kick1.wav",
					"sources/crash2.mkv", 2., 2., 2.);
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
		vec3 velocity = vec3(userParams[2], userParams[3], 0.0);
		mat3 transforms = rotationMatrix(vec3(0.0,0.0,1.0), 0.2);
		transforms = rotationMatrix(vec3(1.0,0.0,0.0), -1.3) * transforms;
		float aspect = 1280.0/720.0;
		coords.x *= aspect;
		
		coords.x += (gl_InstanceID % 24)*2*aspect;
		coords.y -= (gl_InstanceID / 24)*2;
		coords = transforms * (coords + velocity*secondsAbs);
		coords.z -= 5;
		
		gl_Position = proj*vec4(coords,1.0);
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
		
		float b = 0.05;
		float bd1 = min(abs(pos.x-b), abs(1-b-pos.x));
		if(pos.y<b || pos.y>1-b) bd1 = b;
		float bd2 = min(abs(pos.y-b), abs(1-b-pos.y));
		if(pos.x<b || pos.x>1-b) bd2 = b;
		float bd = min(bd1, bd2);
		
		float d1 = min(length(pos-vec2(b,b)), length(pos-vec2(b,1-b)));
		float d2 = min(length(pos-vec2(1-b,b)), length(pos-vec2(1-b,1-b)));
		float d = min(d1,d2);
		bd = min(bd, d);
		
		float c1 = 1.0-(0.05/(bd+0.08));
		vec4 borderColor = vec4(c1,c1,c1,1.0);
		if(pos.x < b || pos.x > (1-b)) return borderColor;
		if(pos.y < b || pos.y > (1-b)) return borderColor;
		
		
		return result;
		)aaaaa";
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes, true);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	
	vector<int> prevAudioSegments(32, -1);
	vector<int> prevVideoSegments(4, -1);
	
	double vx = -0.1, vy = 0.5;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.;
		int videoPosition = 0;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		//n.pitchSemitones -= 7;
		n.pitchSemitones -= 12;
		
		// select source
		if(n.instrument == 7) {
			// kick
			src = getSource("kick1"); n.pitchSemitones = 0; n.amplitudeDB += 8;
			videoPosition = 0;
		} else if(n.instrument == 9) {
			// snare
			src = getSource("perc1"); n.pitchSemitones = 0; n.amplitudeDB += 7;
			videoPosition = 0;
		} /* else if(n.instrument == 8) {
			// hi-hat
			src = getSource("perc1"); n.pitchSemitones = 0; n.amplitudeDB += 5;
			videoPosition = 0;
		} */ else if(n.instrument == 1) {
			// lead
			src = getSource("lol"); n.pitchSemitones += 32; n.amplitudeDB += 14;
			videoPosition = 1;
		} else if(n.instrument == 3) {
			// bass
			src = getSource("o"); n.pitchSemitones += 13; n.amplitudeDB += 6;
			videoPosition = 2;
		} else if(n.instrument == 5) {
			/* arps */
			src = getSource("hum"); n.pitchSemitones -= 10.5; n.amplitudeDB += 6;
			videoPosition = 0;
			delay = 0.;
		} else continue;
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		as.startSeconds += delay;
		if(prevAudioSegments.at(n.channel) != -1)
			segments.at(prevAudioSegments.at(n.channel)).endSeconds = as.startSeconds;
		prevAudioSegments.at(n.channel) = (int)segments.size();
		segments.push_back(as);
		
		
		VideoSegment vs(n, src->video, bpm);
		if(src->name == "kick1") vs.offsetSeconds = 1.0;
		if(src->name == "lol") vs.startSeconds -= 0.15;
		vs.endSeconds = vs.startSeconds + 1;
		
		if(prevVideoSegments.at(videoPosition) != -1)
			videoSegments.at(prevVideoSegments.at(videoPosition)).endSeconds = vs.startSeconds;
		prevVideoSegments.at(videoPosition) = (int)videoSegments.size();
		
		// set video clip position and size
		//							opacity		radius,	vx,			vy,			sizeScale	opacityScale	transpKey (r,g,b)
		vector<float> shaderParams = {1.0,		1000.,	(float)vx,	(float)vy,	0.1,		0.,				-1., -1., -1.};
		switch(videoPosition) {
			case 1:	vs.vertexes = genRectangle(-1, -1, 0, 0); break;
			case 0: vs.vertexes = genRectangle(0, -1, 1, 0); break;
			case 2:	vs.vertexes = genRectangle(-1, 0, 0, 1); break;
			case 3: vs.vertexes = genRectangle(0, 0, 1, 1); break;
			default: continue;
		}
		
		vs.instances = 24*24;
		
		int pX = 10 + (int)round(vx*vs.startSeconds/2)*2*(1280./720.);
		int pY = -6 + (int)round(vy*vs.startSeconds/2)*2;
		auto& v = vs.vertexes;
		for(int k=0; k<(int)v.size(); k+=5) {
			v[k] -= pX;
			v[k+1] -= pY;
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

