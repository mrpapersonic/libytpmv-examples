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
	coords.y -= gl_InstanceID*2;
	
	mat3 transforms = rotationMatrix(vec3(0.0,0.0,1.0), 0.2);
	transforms = rotationMatrix(vec3(1.0,0.0,0.0), -1.3) * transforms;
	
	coords = transforms * (coords + velocity*secondsAbs);
	coords.z -= 5;
	
	gl_Position = proj*vec4(coords,1.0);
	gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
	uv = texPos;
}
)aaaaa";

string vertexShader2 = R"aaaaa(
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
	vec2 velocity = vec2(userParams[2], userParams[3]);
	mat3 transforms = rotationMatrix(vec3(0.0,0.0,1.0), 0.5);
	transforms = rotationMatrix(vec3(1.0,0.0,0.0), -1.3)*transforms;
	
	vec2 pos = round(-velocity*secondsAbs/2)*2;
	pos.x -= 10;
	//pos.y -= 5;
	pos.y -= (gl_InstanceID % 24)*2;
	pos.x += (gl_InstanceID / 24)*2;
	int X = int(pos.x);
	int Y = int(pos.y);
	
	// model transformations
	coords /= 5.0;
	float rx = sin(secondsAbs + X*2 + Y*2) + (X ^ (X*3)) + 0.9;
	float ry = cos(secondsAbs + X*3 + Y) + (Y ^ (X*3)) + 0.2;
	
	coords = rotationMatrix(vec3(1.0,0.0,0.0), rx*5) * coords;
	coords = rotationMatrix(vec3(0.0,1.0,0.0), ry*5) * coords;
	
	// world transformations
	pos += velocity*secondsAbs;
	coords.x += cos(secondsAbs*0.5 + X/2 + (Y ^ (X*3)));
	coords.z += 1.2 + sin(secondsAbs + X/2 + (Y ^ (X*3)));
	coords.xy += pos;
	
	coords = transforms * (coords);
	coords.z -= 5;
	
	gl_Position = proj*vec4(coords,1.0);
	gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
	uv = texPos;
}
)aaaaa";


string vertexShader3 = R"aaaaa(
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
	vec2 velocity = vec2(userParams[2], userParams[3]);
	mat3 transforms = rotationMatrix(vec3(0.0,0.0,1.0), 0.2);
	transforms = rotationMatrix(vec3(1.0,0.0,0.0), -1.3) * transforms;
	
	vec2 pos = vec2(-2,-6);
	pos.y -= (gl_InstanceID % 24)*2;
	pos.x += (gl_InstanceID / 24)*2;
	int X = int(pos.x);
	int Y = int(pos.y);
	
	// model transformations
	coords /= 2;
	coords *= 0.5 + clamp(secondsRel*1.5, 0.0, 0.7);
	float rx = sin(secondsAbs/5 + X*2 + Y*2) + (X ^ (X*3)) + 0.9;
	float ry = cos(secondsAbs/5 + X*3 + Y) + (Y ^ (X*3)) + 0.2;
	
	coords = rotationMatrix(vec3(1.0,0.0,0.0), rx*5) * coords;
	coords = rotationMatrix(vec3(0.0,1.0,0.0), ry*5) * coords;
	
	// world transformations
	coords.z += 1.0 + sin(secondsAbs + X*2 + Y*2)*2;
	coords.xy += pos;
	
	coords = transforms * (coords);
	coords.z -= 5;
	
	gl_Position = proj*vec4(coords,1.0);
	gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
	uv = texPos;
}
)aaaaa";

string shader = R"aaaaa(
	float opacityScale = param(0) + param(5) * (secondsRel);
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
	vec4 borderColor = vec4(c1,c1,c1,opacityScale);
	if(pos.x < b || pos.x > (1-b)) return borderColor;
	if(pos.y < b || pos.y > (1-b)) return borderColor;
	
	
	return result;
	)aaaaa";

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	string buf = get_file_contents("songs/skifa2.mod");
	
	// "do" has pitch 2.64/30
	addSource("o", "sources/o_35000_fade.wav", "sources/o.mkv", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "sources/ha.mkv", 2.107/30);
	addSource("angryasuna", "sources/angryasuna.mkv", "sources/angryasuna.mkv", 1.92/30, 1.5, 1.5);
	addSource("agiri_lead", "sources/agiri_lead.wav", "sources/agiri_lead.mkv", 2.64/30, 1., 1.);
	addSource("agiriwater_2", "sources/agiriwater_2.mkv", "sources/agiriwater_2.mkv", 3.46/30, 1., 1.);
	addSource("daah", "sources/daah.mp4", "sources/daah.mp4", 2.75/30, 1., 1.);
	addSource("horn", "sources/horn.mp4", "sources/horn.mp4", 1.9/30, 1., 1.);
	
	addSource("hum", "sources/bass1_4.wav",
					"sources/bass1.mkv", 2.8/30, 2., 1.);
	addSource("hum2", "sources/bass1_3.wav", "", 2.8/30, 1., 1.);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	addSource("kick1", "sources/kick1.wav",
					"sources/crash2.mkv", 2., 2., 2.);
	addSource("drumkick", "sources/drumkick.mp4", "sources/drumkick.mp4", 1., 1., 1.);
	addSource("kick3", "sources/kick3.mkv", "sources/kick3.mkv", 1., 1., 1.);
	addSource("sigh", "sources/sigh.mp4", "sources/sigh.mp4", 1., 3., 3.);
	getSource("hum2")->video = getSource("hum")->video;
	
	//trimSource("agiri_lead", 0.05);
	trimSource("horn", 0.05);
	//trimSource("lightswitch", 0.61);
	
	
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
	
	double vx = 0.0, vy = 1.;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.;
		double origVolumeDB = n.amplitudeDB;
		int videoPosition = 0;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		//n.pitchSemitones -= 7;
		n.keyframes.clear();
		
		// select source
		if(n.channel == 1) {
			if(n.amplitudeDB < -3) continue;
			if(n.instrument == 5) {
				src = getSource("drumkick");
				n.pitchSemitones = 0;
				n.amplitudeDB += 20;
				delay -= 0.04;
			} else {
				src = getSource("kick3");
				n.pitchSemitones = 0;
				n.amplitudeDB += 5;
				delay -= 0.05;
			}
		} else if(n.channel == 4) {
			// hi-hat
			src = getSource("sigh");
			n.pitchSemitones = 10;
			n.amplitudeDB = 2;
		} else if(n.channel == 0) {
			// high lead
			src = getSource("horn");
			n.pitchSemitones += 24;
			n.amplitudeDB += 17;
			pan = 0.5;
		} else if(n.channel == 2) {
			// lead
			src = getSource("daah");
			n.pitchSemitones += 12;
			n.amplitudeDB += 8;
			pan = 0.5;
		} else if(n.channel == 3) {
			// bass
			src = getSource("angryasuna");
			n.amplitudeDB += 8;
			n.pitchSemitones -= 12;
			{
				AudioSegment as(n, src->audio, bpm);
				as.pitch *= 2;
				for(int k=0;k<CHANNELS;k++) as.amplitude[k] *= 0.4;
				segments.push_back(as);
			}
		} else continue;
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		as.startSeconds += delay;
		segments.push_back(as);
		
		
		VideoSegment vs(n, src->video, bpm);
		if(src->name == "kick1") vs.offsetSeconds = 1.0;
		if(src->name == "lol") vs.startSeconds -= 0.15;
		vs.instances = 24*24;
		vs.vertexShader = &vertexShader;
		vs.shader = &shader;
		
		// set video clip position and size
		//							opacity		radius,	vx,			vy,			sizeScale	opacityScale	transpKey (r,g,b)
		vector<float> shaderParams = {1.0,		1000.,	(float)vx,	(float)vy,	0.1,		0.,				-1., -1., -1.};
		switch(n.channel) {
			case 9:
				vs.vertexes = genParallelpiped();
				vs.vertexShader = &vertexShader3;
				shaderParams[5] = -2.;
				vs.zIndex = 9;
				break;
			case 3: vs.vertexes = genRectangle(0, -1, 1, 0); shaderParams[5] = -4.; break;
			case 1:	vs.vertexes = genRectangle(-1, 0, 0, 1); break;
			case 4: vs.vertexes = genRectangle(0, 0, 1, 1); break;
			case 2:	vs.vertexes = genRectangle(-1, -1, 0, 0); shaderParams[5] = -2.; break;
			case 0:
				vs.vertexes = genRectangle(-1, 0, 1,
											1, 0, 1,
											1, 0, -1);
				vs.vertexShader = &vertexShader2;
				shaderParams[0] = pow((n.pitchSemitones+20)/100,4);
				vs.zIndex = 10;
				break;
			default: continue;
		}
		
		
		vs.shaderParams = shaderParams;
		videoSegments.push_back(vs);
	}
	defaultSettings.volume = 1./4;
	//defaultSettings.skipToSeconds = 22;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

