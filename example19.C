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


map<string, vector<string> > categories;

SongInfo inf;
vector<Instrument> instr;
vector<Note> notes;
double bpm;

// outputs
vector<AudioSegment> segments;
vector<VideoSegment> videoSegments;

int lastSeg_[128] = {};
int lastNote_[128] = {};
bool flip[128] = {};
string nextSource[128];


string selectSource(string category, int index) {
	vector<string>& arr = categories[category];
	if(arr.empty()) throw logic_error("source category \""+category+"\" empty!");
	
	int i = index;
	return arr[i % arr.size()];
}

void generateArps(AudioSegment& as, double bpm, int index) {
	double rowDurationSeconds = 60./bpm;
	double volScale = as.keyframes.at(0).amplitude[0];
	
	for(int i=0; i<50; i++) {
		double t = i*rowDurationSeconds/3.;
		double vol = 0.;
		if((i % 3) == index) vol = 1.;
		
		vol *= volScale;
		as.keyframes.push_back(AudioKeyFrame{t,	{vol, vol}, 0.});
	}
}
bool hasLastNote(int ch) {
	return (lastNote_[ch] != 0);
}
Note& lastNote(int ch) {
	return notes.at(lastNote_[ch]-1);
}
bool hasLastSeg(int ch) {
	return (lastSeg_[ch] != 0);
}
VideoSegment& lastSeg(int ch) {
	return videoSegments.at(lastSeg_[ch]-1);
}
void extendLastVideo(int ch, double t) {
	if(hasLastSeg(ch)) lastSeg(ch).endSeconds = t;
}


string shader = R"aaaaa(
	vec4 result = vec4(texture2D(image, pos).rgb, 1.);
	result.a = length(result.rgb - vec3(0.0,1.0,0.0));
	result.a = result.a*result.a*result.a*1000;
	result.a = clamp(result.a,0.0,1.0);
	return result;
)aaaaa";

string shaderWithShadow = R"aaaaa(
	vec2 aspect = vec2(resolution.x/resolution.y, 1.0);
	vec4 result = vec4(texture2D(image, pos).rgb, 1.0);
	
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
	
	float opac = pow((1.0 - (bd/b)),2)*0.5;
	vec4 borderColor = vec4(0,0,0,opac);
	if(pos.x < b || pos.x > (1-b)) return borderColor;
	if(pos.y < b || pos.y > (1-b)) return borderColor;
	
	
	return result;
	)aaaaa";

void placeNote(Note& n, Source* src,
				double pan, double delay, double fade) {
	VideoSegment vs(n, src, bpm);
	
	AudioSegment as(n, src, bpm);
	as.amplitude[0] *= (1-pan)*2;
	as.amplitude[1] *= (pan)*2;
	as.startSeconds += delay;
	
	// apply fadeout to audio
	if(fade < 1e9) {
		double a = as.keyframes.front().amplitude[0];
		as.keyframes.push_back(AudioKeyFrame{fade,	{a,		a}, 0.});
		as.keyframes.push_back(AudioKeyFrame{fade+0.2,{a*0.5, a*0.5}, 0.});
		as.keyframes.push_back(AudioKeyFrame{fade+0.4,{a*0.3, a*0.3}, 0.});
		as.keyframes.push_back(AudioKeyFrame{fade+0.8,{a*0.2, a*0.2}, 0.});
		as.keyframes.push_back(AudioKeyFrame{fade+1.6,{0., 	0.}, 0.});
	}
	segments.push_back(as);
	
	videoSegments.push_back(vs);
	lastSeg_[n.channel] = (int)videoSegments.size();
}

void applyReverb(AudioSegment as, double reverbWet) {
	// apply reverb to audio
	as.amplitude[0] *= reverbWet;
	as.amplitude[1] *= reverbWet;
	as.startSeconds += 0.02;
	as.endSeconds += 0.02;
	for(int j=0; j<8; j++) {
		AudioSegment as2 = as;
		double d = 0.04*(j+1);
		as2.startSeconds += d;
		as2.endSeconds += d;
		as2.amplitude[0] *= (j%2) ? 0.1 : 1.0;
		as2.amplitude[1] *= (j%2) ? 1.0 : 0.1;
		segments.push_back(as2);
		
		if(!(j%2)) {
			as.amplitude[0] *= 0.7;
			as.amplitude[1] *= 0.7;
		}
	}
}

void scene1Note(Note& n, Instrument& ins, string nCat) {
	static string vertexShader = R"aaaaa(
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
	mat3 rotationMatrix(vec3 axis, float angle) {
		axis = normalize(axis);
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		
		return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
					oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
					oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);
	}
	float asymp(float x) {
		return 1.0 - 1.0/(1.0+x);
	}
	void main() {
		float aspect = resolution.x/resolution.y;
		vec3 coords = myPos;
		float tmp = 1.0/(secondsRel*param(5)+1.0);
		float sizeScale = clamp(tmp*(1.0-param(4))+param(4), 0.0, 1.0);
		float zoomScale = 1.0/(secondsRel*param(8)+1.0);
		zoomScale = 1.01 + 0.2*zoomScale;
		vec2 mypos = vec2(param(0), param(1));
		vec2 mysize = vec2(param(2),param(3))*sizeScale;
		float rotationY = param(9);
		vec2 offsPos = vec2(0.0,0.0);
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		
		// transitions
		vec3 origCoords = coords;
		float trans0 = secondsAbs;
		float trans1 = clamp(secondsAbs-5,0,1000);
		
		// trans0
		offsPos.y += trans0;
		
		// model transforms
		coords.x *= aspect;
		coords = rotationMatrix(vec3(1,0.,0), rotationY) * coords;
		
		// tiling
		coords.y += (20 - (gl_InstanceID / 28))*2;
		coords.x += (-5 + (gl_InstanceID % 28))*2*aspect;
		coords.x += mod(offsPos.x, aspect*2);
		coords.y += mod(offsPos.y, 2);
		
		coords.z += param(7);
		
		
		// trans1
		coords = rotationMatrix(vec3(0,0,1), trans1*0.15) * coords;
		coords = rotationMatrix(vec3(1,0.5,0), -1.) * coords;
		coords.z -= asymp(trans1*0.15)*8.0;
		
		
		coords.z -= 2.5;

		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";

	Note origNote = n;
	Source* src = nullptr;
	double pan = 0.5, delay = 0., fade = 1e9, reverbWet = 0.3;
	double x=0., y=0., sz = 0.2, z = 0., rot = 0., sizeScale = 2.;
	int zIndex = 0;
	string srcName;
	
	if(nCat == "bass") {
		srcName = selectSource("bass", 1);
		reverbWet = 0.25;
		
		n.adjustAmplitude(7);
		if(n.start.seq < 2) n.adjustAmplitude(-3);
		
		n.pitchSemitones += 24;
		// make higher pitch notes quieter
		n.adjustAmplitude(-(n.pitchSemitones-35.)*0.25);
		
		x = 0.5; y = 0.5; sz = 1.0; sizeScale = 0.; z = -0.02;
		if(flip[n.channel]) sz *= -1;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "kick") {
		srcName = selectSource("kick", 0); n.pitchSemitones = 0; n.amplitudeDB += 5;
		reverbWet = 0.1;
		n.end.absRow += 1;
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = 0.; y = 0.5; sz = 0.5; z = 0.5; rot = M_PI/2;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "snare") {
		srcName = selectSource("slap", 0); n.pitchSemitones = 0; n.adjustAmplitude(5);
		reverbWet = 0.1;
		n.end.absRow += 1;
		
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = 0.; y = 0.5; sz = 0.5; z = 0.5; rot = M_PI/2;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "chords") {
		srcName = selectSource("wind", 1);
		fade = 0.;
		reverbWet = 0.2;
		pan = 0.6;
		
		sz = 1./5 * 0.95;
		y = 1. - sz/2*1.1;
		x = (n.channel-3-1.5)*sz + 0.5;
		sz *= 0.85;
	} else if(nCat == "arps") {
		srcName = selectSource("wind", 2); n.adjustAmplitude(-5); //n.pitchSemitones += 12;
		
		if(n.channel == 101) { selectSource("wind", 0); }
		if(n.channel == 102) { selectSource("wind", 0); }
		reverbWet = 0.1;
		int tmp = n.channel - 100 - 1;
		pan = 0.6;
		
		x = 0.5 + tmp*0.3;
		y = 0.2;
	} else return;
	
	src = getSource(srcName);
	placeNote(n, src, pan, delay, fade);
	
	applyReverb(segments.back(), reverbWet);
	
	// set video clip position and size
	//							centerX		centerY	w		h		minSizeScale,	sizeScale,	alpha,	z,	zoomScale, rotationY
	vector<float> shaderParams = {x,		y,		sz,		sz,		0.4,			sizeScale,	1.0,	z,	1.0,		rot};
	VideoSegment& vs = videoSegments.back();
	vs.vertexShader = &vertexShader;
	vs.shader = &shader;
	vs.shaderParams = shaderParams;
	vs.instances = 28*28;
	vs.zIndex = zIndex;
}


void scene2Note(Note& n, Instrument& ins, string nCat) {
	static string vertexShader = R"aaaaa(
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
	smooth out vec2 modelXY;
	float param(int i) { return userParams[i]; }
	mat3 rotationMatrix(vec3 axis, float angle) {
		axis = normalize(axis);
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		
		return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
					oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
					oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);
	}
	float asymp(float x) {
		return 1.0 - 1.0/(1.0+x);
	}
	void main() {
		float aspect = resolution.x/resolution.y;
		vec3 coords = myPos;
		float tmp = 1.0/(secondsRel*param(5)+1.0);
		float sizeScale = clamp(tmp*(1.0-param(4))+param(4), 0.0, 1.0);
		float zoomScale = 1.0/(secondsRel*param(8)+1.0);
		zoomScale = 1.01 + 0.2*zoomScale;
		if(param(8) == 0) zoomScale = 1.0;
		vec2 mypos = vec2(param(0), param(1));
		vec2 mysize = vec2(param(2),param(3))*sizeScale;
		float rotationY = param(9);
		vec2 offsPos = vec2(0.0,0.0);
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		
		// transitions
		vec3 origCoords = coords;
		float trans1 = clamp(secondsAbs-14.5,0,1);
		float trans2 = clamp(secondsAbs-29.5,0,1000);
		float trans3 = clamp(secondsAbs-90,0,1);
		float trans4 = clamp(secondsAbs-92.2,0,1);
		float trans4_1 = clamp(secondsAbs-92.2,0,1000);
		float trans5 = clamp(secondsAbs-121.5,0,1000);
		
		// trans1
		coords *= trans1;
		
		// trans4
		offsPos += vec2(-0.7,0.3) * trans4_1;
		
		// model transforms
		coords.x *= aspect;
		coords.z += param(7);
		
		// trans5
		coords.z *= (asymp(trans5*0.1)*150 + 1);
		
		// tiling
		coords.y += (5 - (gl_InstanceID / 28))*2;
		coords.x += (-5 + (gl_InstanceID % 28))*2*aspect;
		coords.x += mod(offsPos.x, aspect*2);
		coords.y += mod(offsPos.y, 2);
		
		modelXY = coords.xy;
		
		// trans2
		coords = rotationMatrix(vec3(0,0,1), asymp(trans2*0.05)*1.) * coords;
		coords = rotationMatrix(vec3(1,0,0), -asymp(trans2*0.05)*1.6) * coords;
		coords.z -= asymp(trans2*0.1)*15.0;
		
		// trans3
		coords = rotationMatrix(vec3(0,0,1), trans3*3) * coords;
		coords.z -= 10*trans3;
		
		// trans4
		coords = rotationMatrix(vec3(0,0,1), -trans4*3) * coords;
		coords.z += 10*trans4;
		
		
		coords.z -= 2.5;
		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";
	
	static string fragmentShader_wavy = R"aaaaa(
		#version 330 core
		in vec4 gl_FragCoord;
		smooth in vec2 uv;
		smooth in vec2 modelXY;
		out vec4 color; 
		uniform vec2 resolution;
		uniform float secondsAbs;
		uniform float secondsRel; 
		uniform sampler2D image;
		uniform float userParams[16];
		
		void main() {
			vec2 pos = uv;
			// apply sine wave effect to y
			pos.y = pos.y*1.14 - (sin(modelXY.x*3 + secondsAbs*10) + 1)*0.07;
			color = vec4(texture2D(image, pos).rgb, 1.);
			// scale x accordingly to maintain aspect ratio
			pos.x = (pos.x-0.5) * 1.14 + 0.5;
			
			/*
			if(pos.y < 0 || pos.y > 1)
				color.a = 0.0;*/
				
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
			
			float opac = pow((1.0 - (bd/b)),2)*0.5;
			vec4 borderColor = vec4(0,0,0,opac);
			if(pos.x < b || pos.x > (1-b) || pos.y < b || pos.y > (1-b)) {
				color = borderColor;
			}
		}
	)aaaaa";
	
	static string vertexShader2 = R"aaaaa(
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
		float aspect = resolution.x/resolution.y;
		vec3 coords = myPos;
		float tmp = 1.0/(secondsRel*param(5)+1.0);
		float sizeScale = clamp(tmp*(1.0-param(4))+param(4), 0.0, 1.0);
		float zoomScale = 1.0/(secondsRel*param(8)+1.0);
		zoomScale = 1.01 + 0.2*zoomScale;
		if(param(8) == 0) zoomScale = 1.0;
		vec2 mypos = vec2(param(0), param(1));
		vec2 mysize = vec2(param(2),param(3))*sizeScale;
		float rotationY = param(9);
		vec2 offsPos = vec2(0.0,0.0);
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
				
		// transitions
		vec3 origCoords = coords;
		float trans1 = clamp(secondsAbs-14.5,0,1);
		
		// model transforms
		coords.z -= param(7);
		
		// trans1
		coords *= trans1;

		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";

	Note origNote = n;
	Source* src = nullptr;
	double pan = 0.5, delay = 0., fade = 1e9, reverbWet = 0.3;
	double x=0., y=0., sz = 0.2, z = 0., rot = 0., sizeScale = 2., zScale = 0.;
	int zIndex = 0;
	string srcName;
	
	if(nCat == "bass") {
		srcName = selectSource("bass", 1);
		reverbWet = 0.25;
		
		n.adjustAmplitude(7);
		n.pitchSemitones += 24;
		// make higher pitch notes quieter
		n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
		
		
		sz = 1./4;
		y = 1. - sz/2;
		x = (3-1.5)*sz + 0.5;
		z = 0.02;
		sz *= 0.85;
		sizeScale = 0.;
		if(flip[n.channel]) sz *= -1;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "kick") {
		srcName = selectSource("kick", 0); n.pitchSemitones = 0; n.amplitudeDB += 5;
		reverbWet = 0.1;
		n.end.absRow += 1;
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = 0.5; y = 0.5; sz = 1.0; sizeScale = 0.; z = -0.02; zScale = 1.0; zIndex = -10;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "snare") {
		srcName = selectSource("slap", 0); n.pitchSemitones = 0; n.adjustAmplitude(5);
		reverbWet = 0.1;
		n.end.absRow += 1;
		
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = 0.5; y = 0.5; sz = 1.0; sizeScale = 0.; z = -0.02; zScale = 1.0; zIndex = -10;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "chords") {
		srcName = selectSource("wind", 1); n.adjustAmplitude(-2);
		fade = 0.;
		reverbWet = 0.2;
		pan = 0.6;
		
		sz = 1./4;
		z = 0.02;
		y = 1. - sz/2;
		x = (n.channel-3-1.5)*sz + 0.5;
		sz *= 0.85;
		sizeScale = 0.;
	} else if(nCat == "main") {
		static int k = 9;
		srcName = selectSource("lead", 0);
		if(n.start.seq >= 6 && n.start.seq <= 8) {
			srcName = selectSource("lead", 5);
		}
		if(n.start.seq >= 16 && n.start.seq <= 19) {
			srcName = selectSource("lead", 1);
			reverbWet = 0.5;
		}
		if(n.instrument == 10) {
			//srcName = selectSource("lead", 2);
			srcName = selectSource("lead2", k);
			if(n.durationRows() > 1) k++;
			n.adjustAmplitude(1);
			//n.keyframes.clear();
			//zScale = 1.0;
		}
		
		n.adjustAmplitude(6);
		
		x = 0.5; y = 0.5; sz = 0.5; z = sz; rot = M_PI/2;
		if(n.start.seq >= 30)
			x = 0.25;
		if(flip[n.channel]) sz *= -1;
		zIndex = 100;
	} else if(nCat == "counter") {
		static int k = 9;
		srcName = selectSource("lead2", k);
		if(n.durationRows() > 1) k++;
		if(n.instrument == 10) {
			n.adjustAmplitude(-1);
		}
		n.adjustAmplitude(5);
		
		//x = 0.5; y = 0.5; sz = 0.3; z = 0.02;
		x = 0.75; y = 0.5; sz = 0.5; z = sz; rot = M_PI/2;
		if(flip[n.channel]) sz *= -1;
		zIndex = 100;
	} else if(nCat == "mountain") {
		srcName = selectSource("lead", 1);
		n.adjustAmplitude(-1);
		
		// make higher pitch notes quieter
		n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
		
		n.pitchSemitones += 12;
		pan = 0.4;
		
		x = 0.2; y = 0.5; sz = 0.3; z = 0.02;
	} else if(nCat == "arps") {
		srcName = selectSource("wind", 2); n.adjustAmplitude(-5); //n.pitchSemitones += 12;
		if((n.start.seq >= 8 && n.start.seq <= 11) ||
			(n.start.seq >= 26)) {
			srcName = selectSource("wind", 0);
			n.adjustAmplitude(-3);
		}
		
		if(n.channel == 101) { selectSource("wind", 0); }
		if(n.channel == 102) { selectSource("wind", 0); }
		reverbWet = 0.1;
		int tmp = n.channel - 100 - 1;
		pan = 0.6;
		
		x = 0.5 + tmp*0.3;
		y = 0.15;
		z = 0.02;
		zIndex = 10;
	} else return;
	
	src = getSource(srcName);
	placeNote(n, src, pan, delay, fade);
	
	
	// apply arps
	if(n.channel >= 100 && n.durationRows() > 2) {
		generateArps(segments.back(), bpm, n.channel-100);
	}
	
	applyReverb(segments.back(), reverbWet);
	
	// set video clip position and size
	//							centerX		centerY	w		h		minSizeScale,	sizeScale,	alpha,	z,	zoomScale, rotationY
	vector<float> shaderParams = {x,		y,		sz,		abs(sz),0.4,			sizeScale,	1.0,	z,	zScale,		rot};
	VideoSegment& vs = videoSegments.back();
	vs.vertexShader = &vertexShader;
	vs.shader = &shaderWithShadow;
	vs.shaderParams = shaderParams;
	vs.zIndex = zIndex;
	vs.instances = 28*28;
	if(nCat == "main" || nCat == "counter") {
		vs.vertexShader = &vertexShader2;
		vs.instances = 1;
	}
	if(nCat == "kick" || nCat == "snare")
		vs.shader = &shader;
	if(nCat == "chords" || nCat == "bass")
		vs.fragmentShader = &fragmentShader_wavy;
	if(srcName == "Done")
		vs.shader = &shader;
}


void scene3Note(Note& n, Instrument& ins, string nCat) {
	static string vertexShader = R"aaaaa(
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
	mat3 rotationMatrix(vec3 axis, float angle) {
		axis = normalize(axis);
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		
		return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
					oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
					oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);
	}
	// result approaches 1 as x approaches inf
	float asymp(float x) {
		return 1.0 - 1.0/(1.0+x);
	}
	void main() {
		float aspect = resolution.x/resolution.y;
		vec3 coords = myPos;
		float tmp = 1.0/(secondsRel*param(5)+1.0);
		float sizeScale = clamp(tmp*(1.0-param(4))+param(4), 0.0, 1.0);
		float zoomScale = 1.0/(secondsRel*param(8)+1.0);
		//zoomScale = 1.01 + 0.2*zoomScale;
		zoomScale = 1.0;
		vec2 mypos = vec2(param(0), param(1));
		vec2 mysize = vec2(param(2),param(3))*sizeScale;
		float nCat = param(9);
		vec3 offsPos = vec3(0.0,0.0,0.0);
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		
		// transitions
		vec3 origCoords = coords;
		float trans1 = secondsAbs;
		float trans2 = clamp(secondsAbs-75.2, 0, 1000);
		float trans3 = clamp(secondsAbs-92.2, 0, 1000);
		offsPos += vec3(0.1,0.2,0.5) * trans1;
		if(nCat >= 3) {
			offsPos += vec3(0.3,-0.8*sin(nCat),0.2*cos(nCat)) * trans1;
		}
		offsPos += vec3(-2.7,-1.7,-1.5) * trans3;
		
		// tiling
		vec3 pos = vec3(0,0,0);
		int id = gl_InstanceID % 144;
		pos.z -= (6 - (gl_InstanceID / 144))*2;
		pos.y += (4 - (id / 12))*2;
		pos.x += (-5 + (id % 12))*2*aspect;
		pos.x += mod(offsPos.x, aspect*2);
		pos.y += mod(offsPos.y, 2);
		pos.z += mod(offsPos.z, 2);
		
		pos.x += sin(pos.z*0.2)*2*asymp(trans2);
		pos.y += cos(pos.z*0.3)*2*asymp(trans2);
		
		// trans1
		/*if(nCat >= 3) {
			coords = rotationMatrix(vec3(1,0,0), sin(pos.x*2 + pos.y*4)*0.3) * coords;
			coords = rotationMatrix(vec3(0,1,0), sin(pos.y*3 + pos.z*2)*0.3) * coords;
		}*/
		
		
		
		// model transforms
		coords.x *= aspect;
		coords.z += param(7);
		
		// trans3
		vec3 pos3 = pos;
		pos3.y += pos.z * 20;
		pos3.z = 0;
		float pan3 = asymp(trans3*0.5);
		pos = pos*(1-pan3) + pos3*pan3;
		
		
		// set world position
		coords += pos;
		
		
		// trans3
		//coords = rotationMatrix(vec3(1,0.2,0), -asymp(trans2*0.5)*0.8) * coords;
		//coords.z -= asymp(trans2*0.5)*7;
		
		coords.z -= 7;
		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";
	static string vertexShader2 = R"aaaaa(
	#version 330 core
	layout(location = 0) in vec3 myPos;
	layout(location = 1) in vec2 texPos;
	uniform vec2 coordBase;
	uniform mat2 coordTransform;
	uniform float secondsRel, secondsAbs;
	smooth out vec2 uv;
	void main(){
		float zoomScale = 1.0/(secondsRel*3.0+1.0);
		zoomScale = 1.01 + 0.2*zoomScale;
		
		// tiling
		vec2 offsPos = vec2(-0.2,-0.3)*secondsAbs;
		vec2 pos = vec2(0,0);
		pos.y += (4 - (gl_InstanceID / 8))*2;
		pos.x += (-4 + (gl_InstanceID % 8))*2;
		pos.x += mod(offsPos.x, 2);
		pos.y += mod(offsPos.y, 2);
		
		gl_Position.xy = coordBase + (myPos.xy + pos)*0.8 * coordTransform;
		gl_Position.z = 0.99;
		gl_Position.w = 1.0;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";

	Note origNote = n;
	Source* src = nullptr;
	double pan = 0.5, delay = 0., fade = 1e9, reverbWet = 0.3;
	double x=0., y=0., sz = 0.2, z = 0., rot = 0., sizeScale = 2.;
	int zIndex = 0;
	int nCat1 = 1;
	string srcName;
	
	if(nCat == "bass") {
		srcName = selectSource("bass", 1);
		reverbWet = 0.25;
		
		n.adjustAmplitude(7);
		n.pitchSemitones += 24;
		// make higher pitch notes quieter
		n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
		
		
		x = 0.5; y = -0.5; z = -0.5;
		if(flip[n.channel]) sz *= -1;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "kick") {
		srcName = selectSource("kick", 0); n.pitchSemitones = 0; n.amplitudeDB += 5;
		reverbWet = 0.1;
		n.end.absRow += 1;
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = -0.5; y = 0.0; z = 0.0;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "snare") {
		srcName = selectSource("slap", 0); n.pitchSemitones = 0; n.adjustAmplitude(5);
		reverbWet = 0.1;
		n.end.absRow += 1;
		
		if(n.start.seq == 25) n.adjustAmplitude(-3);
		
		x = -0.5; y = 0.0; z = 0.0;
		extendLastVideo(n.channel, n.start.toSeconds(bpm));
	} else if(nCat == "chords") {
		srcName = selectSource("wind", 1); n.adjustAmplitude(-2);
		fade = 0.;
		reverbWet = 0.2;
		pan = 0.6;
		
		x = -0.5; y = 0.5; z = 0.0;
		z = (n.channel-3-1.5)*sz + 0.5;
	} else if(nCat == "main") {
		srcName = selectSource("lead", 6);
		if(n.start.seq >= 14 && n.start.seq <= 15) {
			srcName = selectSource("lead", 8);
		}
		if(n.start.seq >= 16 && n.start.seq <= 19) {
			n.pitchSemitones += 0.3;
			srcName = selectSource("lead", 1);
			reverbWet = 0.5;
			nCat1 = 2;
		}
		if(n.start.seq >= 21) {
			srcName = selectSource("lead", 5);
		}
		if(n.start.seq >= 23) {
			srcName = selectSource("lead", 9);
		}
		if(n.instrument == 10) {
			n.adjustAmplitude(-1);
		}
		n.adjustAmplitude(6);
		
		x = 0.0; y = 0.5; z = 0.0;
		sz = 0.5;
	} else if(nCat == "counter") {
		srcName = selectSource("lead", 7);
		if(n.instrument == 10) {
			n.adjustAmplitude(-1);
		}
		n.adjustAmplitude(5);
		
		x = 0.0; y = 0.5; z = 0.5;
	} else if(nCat == "mountain") {
		srcName = selectSource("lead", 1);
		n.adjustAmplitude(-1);
		n.pitchSemitones += 0.3;
		
		// make higher pitch notes quieter
		n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
		
		n.pitchSemitones += 12;
		pan = 0.4;
		
		x = 0.5; y = 0.0; z = 0.5;
	} else if(nCat == "arps") {
		srcName = selectSource("wind", 2); n.adjustAmplitude(-5); //n.pitchSemitones += 12;
		if((n.start.seq >= 8 && n.start.seq <= 11) ||
			(n.start.seq >= 26)) {
			srcName = selectSource("wind", 0);
			n.adjustAmplitude(-3);
		}
		
		if(n.channel == 101) { selectSource("wind", 0); }
		if(n.channel == 102) { selectSource("wind", 0); }
		reverbWet = 0.1;
		int tmp = n.channel - 100 - 1;
		pan = 0.6;
		
		x = 0.5; y = 0.5; z = tmp;
	} else return;
	
	if(n.start.seq >= 21)
		nCat1 = 3 + n.channel;
	
	src = getSource(srcName);
	placeNote(n, src, pan, delay, fade);
	
	
	// apply arps
	if(n.channel >= 100 && n.durationRows() > 2) {
		generateArps(segments.back(), bpm, n.channel-100);
	}
	
	applyReverb(segments.back(), reverbWet);
	
	// set video clip position and size
	//							centerX		centerY	w		h		minSizeScale,	sizeScale,	alpha,	z,	zoomScale, nCat
	vector<float> shaderParams = {x,		y,		sz,		sz,		0.4,			sizeScale,	1.0,	z,	0.0,		nCat1};
	VideoSegment& vs = videoSegments.back();
	vs.vertexShader = &vertexShader;
	vs.shader = &shader;
	vs.shaderParams = shaderParams;
	vs.zIndex = zIndex;
	vs.instances = 12*12*8;
	if(nCat1 == 2) {
		vs.vertexShader = &vertexShader2;
		vs.instances = 8*8;
		vs.zIndex = -100;
	}
}


int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	// "do" has pitch 2.6/30
	double bp = 2.6/30;
	setSourceDir("sources/");
	srand(12345);
	
	//			name			pitch					tempo	vol	offs
	addSource2("agiriice.mp4", 	pow(2,0.1/12) * bp,		1., 3., 0.042);
	addSource2("agiri_lead.mkv", pow(2,-12./12) * bp,	1., 9., 0.02);
	addSource2("agirisample.mp4", pow(2,-3./12) * bp,	1., 1., 0.);
	addSource2("lead4.mp4",		pow(2,-9.5/12) * bp,	1., 1., 0.09);
	addSource2("ao.mkv",		pow(2,6./12) * bp,		1., 7., 0.043);
	addSource2("unmasked.mp4",	pow(2,-0.8/12) * bp,	1., 3., 0.198);
	addSource2("weeen.mp4", 	pow(2,-6.2/12) * bp,	1., 3., 0.18);
	addSource2("Done.mp4", 		pow(2,-1.85/12) * bp,	1., 2., 0.047);
	addSource2("drumkick.mp4", 	1.,						1., 6., 0.043);
	addSource2("sigh.mp4",		1.,		 				3., 2., 0.052);
	addSource2("smack.mp4",		1.,		 				1., 1., 0.086);
	addSource2("ayaya.mkv",		pow(2,-19.05/12) * bp,	0.5, 8., 0.012);
	addSource2("luckystar1.mkv",pow(2,3./12) * bp,		1., 1., 0.082);
	addSource2("luckystar2.mkv",pow(2,-9.1/12) * bp,	1., 3., 0.083);
	addSource2("luckystar3.mkv",pow(2,4./12) * bp,		1., 4., 0.094);
	addSource2("renge1.mkv",	pow(2,-8.4/12) * bp,	1., 2., 0.116);
	//addSource2("gab9.mkv",		pow(2,-9./12) * bp,		1., 2., 0.123);
	//addSource2("gab10.mkv",		pow(2,0.7/12) * bp,		1., 2., 0.0);
	//addSource2("gab11.mkv",		pow(2,-5.3/12) * bp,	1., 2., 0.02);
	//addSource2("gab12.mkv",		pow(2,-5./12) * bp,		1., 2., 0.02);
	addSource2("gab13.mkv",		pow(2,-6.7/12) * bp,	0.5, 5., 0.06);
	addSource2("gab14.mkv",		pow(2,-0.5/12) * bp,	1., 6., 0.293);
	addSource2("gab16.mkv",		pow(2,-6.3/12) * bp,	1., 4., 0.072);
	addSource2("aaaa.mp4",		pow(2,-9.2/12) * bp,		1., 6., 0.1);
	addSource2("forkbend2.mkv",	pow(2,-13.4/12) * bp,	1., -1., 0.1);
	
	
	addSource2("spherical/aaaaa2.mkv", 	pow(2,-1.35/12) * bp,		1., 4., 0.0);
	
	//addSource2("ytpmv1.mp4", 	pow(2,4./12) * bp,		1., 3., 0.0);
	
	addSource("ytpmv1", "ytpmv1_1.wav", "ytpmv1.mkv", 	pow(2,4./12) * bp,		1., 1.);
	getSource("ytpmv1")->amplitudeDB += 2;
	
	addSource("eee", "eee6.wav", "eee.mkv", 	pow(2,8./12) * bp,		1., 1.);
	trimSource("eee", 0.065);
	getSource("eee")->amplitudeDB += 1;
	
	dupSource("agiriice", "agiriice2", 1., 1., 0.);
	dupSource("agiriice", "agiriice3", 1., 1., 0.);
	getSource("agiriice")->amplitudeDB += 5;
	dupSource("ao", "ao_bass", 1., 1., 0.);
	
	loadSource(*getSource("agiriice2"), "agiriice_hpf3.wav", "", pow(2,0.2/12) * bp, 1., 1.);
	trimSourceAudio("agiriice2", 0.042);
	
	loadSource(*getSource("agiriice3"), "agiriice_hpf2.wav", "", pow(2,0.2/12) * bp, 1., 1.);
	trimSourceAudio("agiriice3", 0.042);
	
	loadSource(*getSource("ao_bass"), "ao_bass.wav", "", pow(2,18./12) * bp, 1., 1.);
	trimSourceAudio("ao_bass", 0.043);
	
	
	dupSource("drumkick", "slap", 4., 4., 5.);
	
	//                          0                 1        2      3       4          5         6              7            8	9
	categories["lead"] = {"spherical/aaaaa2", "ytpmv1", "Done", "ao", "gab14", "ayaya", "luckystar2", "luckystar3", "renge1", "gab16"};
	categories["lead2"] = {"spherical/aaaaa2", "forkbend2", "aaaa", "ayaya", "luckystar1", "weeen", "luckystar2", "luckystar3", "renge1", "gab13", "gab14", "gab16"};
	//categories["lead2"] = {"spherical/aaaaa2", "forkbend2"};
	categories["highlead"] = {"agiri_lead"};
	categories["wind"] = {"agiriice", "agiriice2", "agiriice3"};
	categories["bass"] = {"agirisample", "eee"};
	categories["kick"] = {"drumkick"};
	categories["snare"] = {"slap"};
	categories["hihat"] = {"sigh"};
	categories["slap"] = {"smack"};
	
	random_shuffle(categories["lead2"].begin(), categories["lead2"].end());
	
	string buf = get_file_contents("songs/Saturday Morning.mod");
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	bpm = inf.bpm;
	//bpm = inf.bpm*0.8;
	
	// perform some editing of the module file
	int N = (int)notes.size();
	for(int i=0;i<N;i++) {
		Note& n = notes[i];
		// split chords into separate notes and assign them channels 100 to 102
		if(n.channel == 7 && n.chordCount == 2) {
			Note newNote = n;
			newNote.channel = 100;
			notes.push_back(newNote);
			
			newNote.pitchSemitones = n.pitchSemitones + n.chord1;
			newNote.channel = 101;
			notes.push_back(newNote);
			
			newNote.pitchSemitones = n.pitchSemitones + n.chord2;
			newNote.channel = 102;
			notes.push_back(newNote);
		}
	}
	
	int scene = 1;
	std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
		return (a.start.absRow+a.start.rowOffset) < (b.start.absRow+b.start.rowOffset);
	});
	for(int i=0;i<(int)notes.size();i++) {
		Note& origNote = notes[i];
		Note n = origNote;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
	
		// correct for pitch differences between samples
		if(ins.period != 0) {
			// assume period of 50 is pitch 1.0
			double p = 50./ins.period;
			n.pitchSemitones += log2(p)*12.;
		}
		n.pitchSemitones += 12;
		
		// select source
		string nCat;
		
		if(n.channel == 6) {
			nCat = "bass";
		} else if(n.instrument == 4) {
			nCat = "kick";
		} else if(n.instrument == 5) {
			nCat = "snare";
		} else if(n.channel >= 3 && n.channel <= 5) {
			nCat = "chords";
		} else if(n.channel == 9) {
			nCat = "main";
		} else if(n.channel == 10) {
			nCat = "counter";
		} else if(n.channel == 8) {
			nCat = "mountain";
		} else if(n.channel >= 100) {
			nCat = "arps";
		} else continue;
		
		int lastScene = scene;
		double extendScene = 0.0;
		if(n.start.seq < 4) {
			scene = 1;
		} else if(n.start.seq < 12) {
			scene = 2;
			extendScene = 1.0;
		} else if(n.start.seq < 26) {
			scene = 3;
		} else {
			scene = 4;
		}
		//if(n.start.seq > 24) continue;
		
		if(scene != lastScene) {
			double t = n.start.toSeconds(bpm);
			for(int ch=0; ch<128; ch++) {
				if(hasLastSeg(ch) && lastSeg(ch).endSeconds > (t-0.3))
					extendLastVideo(ch, t + extendScene);
				lastSeg_[ch] = 0;
			}
		}
		switch(scene) {
			case 1: scene1Note(n, ins, nCat); break;
			case 2: scene2Note(n, ins, nCat); break;
			case 3: scene3Note(n, ins, nCat); break;
			case 4: scene2Note(n, ins, nCat); break;
		}
		
		flip[n.channel] = !flip[n.channel];
		lastNote_[n.channel] = i+1;
	}
	
	defaultSettings.volume = 0.24;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

