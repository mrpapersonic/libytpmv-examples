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

/*template<class T> void swap(T& a, T& b) {
	T tmp = a;
	a = b;
	b = a;
}*/

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	string buf = get_file_contents("songs/moonflight2.mod");
	
	// "do" has pitch 2.64/30
	addSource("angryasuna", "sources/angryasuna.mkv", "sources/angryasuna.mkv", 1.93/30, 1.5, 1.5);
	addSource("agiri_lead", "sources/agiri_lead.wav", "sources/agiri_lead.mkv", 2.57/30, 1., 1.);
	addSource("agiriwater", "sources/agiriwater.wav", "sources/agiriwater.mp4", 2.45/30, 1., 1.);
	addSource("agiriwater_2", "sources/agiriwater_2.mkv", "sources/agiriwater_2.mkv", 3.46/30, 1., 1.);
	addSource("daah", "sources/daah.mp4", "sources/daah.mp4", 2.75/30, 1., 1.);
	addSource("horn", "sources/horn.mp4", "sources/horn.mp4", 1.83/30, 1., 1.);
	addSource("forkbend2", "sources/forkbend2.wav", "sources/forkbend2.mkv", 2.3/30, 1., 1.);
	addSource("aaaaa", "sources/aaaaa.wav", "sources/aaaaa.mkv", 2.35/2/30, 1./2, 1./2);
	addSource("time2eat", "sources/time2eat.wav", "sources/time2eat.mkv", 2.85/30, 2., 2.);
	addSource("drink", "sources/drink.mp4",
					"sources/drink.mp4", 2.37/30, 1., 1.);
	
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 1., 1.);
	addSource("kick1", "sources/kick1.wav",
					"sources/crash2.mkv", 2., 2., 2.);
	addSource("drumkick", "sources/drumkick.mp4", "sources/drumkick.mp4", 1., 1., 1.);
	addSource("kick3", "sources/kick3.wav", "sources/kick3.mkv", 1., 1., 1.);
	addSource("sigh", "sources/sigh.mp4", "sources/sigh.mp4", 1., 3., 3.);
	addSource("kick", "sources/kick.wav", "sources/kick.mkv", 1., 1., 1.);
	addSource("shootthebox", "sources/shootthebox.wav", "sources/shootthebox.mkv", 1., 1., 1.);
	
	//trimSource("aaaaa", 0.05);
	trimSourceAudio("time2eat", 0.15);
	trimSource("forkbend2", 0.1);
	trimSource("horn", 0.05);
	
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
		float minSize = userParams[0];
		float sizeScale = userParams[1];
		float aspect = resolution.x/resolution.y;
		float t = secondsRel*sizeScale;
		
		coords.z *= 0.1;
		
		gl_Position = vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = texPos;
		
		uv += vec2(0.2,0.2) * secondsAbs;
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
	void main() {
		float transpKeyEnable = userParams[2];
		vec3 transpKey = vec3(30,255,5)/255.0 * transpKeyEnable;
		
		vec2 texCoords = mod(uv, 1.0);
		vec4 result = vec4(texture2D(image, texCoords*0.98+vec2(0.01,0.01)).rgb, 1.);
		result.a = length(result.rgb - transpKey);
		result.a = result.a*result.a*result.a*3;
		result.a = clamp(result.a,0.0,1.0);
		color = result;
	}
	)aaaaa";
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm*1.2;
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
		double fade = 0.;
		int videoPosition = 0;
		
		
		//if(n.start.seq < 4) continue;
		//if(n.start.seq > 7) break;
		
		n.pitchSemitones -= 6.2;
		
		// select source
		if(n.channel == 2) {
			src = getSource("kick3");
			n.pitchSemitones = -12;
			n.amplitudeDB += 8;
			n.keyframes.clear();
			delay = -0.08;
		} else if(n.channel == 0) {
			// lead
			src = getSource("time2eat");
			n.pitchSemitones += 24;
			n.amplitudeDB += 5;
			pan = 0.5;
			
			if(n.durationRows() > 2)
				fade = 0.05;
		} else if(n.channel == 1) {
			// bass
			if((n.start.row % 16) == 0) {
				src = getSource("horn");
				n.pitchSemitones += 24;
				n.amplitudeDB += 18;
				n.end.absRow = n.start.absRow + 16;
				delay = -0.03;
				{
					AudioSegment as(n, src->audio, bpm);
					as.startSeconds += delay;
					segments.push_back(as);
				}
				n.pitchSemitones -= 12;
			} else {
				src = getSource("drink");
				n.amplitudeDB += 13;
				delay = -0.1;
				if((n.start.row % 16) == 12)
					n.end.absRow = n.start.absRow + 4;
			}
			pan = 0.5;
		} else if(n.channel == 3) {
			// high lead
			src = getSource("aaaaa");
			n.pitchSemitones += 12;
			n.amplitudeDB -= 3;
			pan = 0.4;
			//delay = -0.03;
			// add reverb
			{
				AudioSegment as(n, src->audio, bpm);
				as.amplitude[0] *= (1-pan)*2;
				as.amplitude[1] *= (pan)*2;
				as.startSeconds += delay;
				for(int j=0; j<5; j++) {
					swap(as.amplitude[0], as.amplitude[1]);
					as.amplitude[0] *= 0.6;
					as.amplitude[1] *= 0.6;
					as.startSeconds += 0.05;
					as.endSeconds += 0.05;
					segments.push_back(as);
				}
			}
		} else continue;
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		as.startSeconds += delay;
		//as.endSeconds += delay;
		if(fade > 0.) {
			double a = as.keyframes.front().amplitude[0];
			as.keyframes.push_back(AudioKeyFrame{0., {a, 0.}, 0.});
			as.keyframes.push_back(AudioKeyFrame{fade, {a*1.3, a*1.4}, 0.});
			as.keyframes.push_back(AudioKeyFrame{fade*2, {a, a}, 0.});
		}
		if(n.channel == 0 && n.durationRows() >= 6) as.tempo = 1.;
		segments.push_back(as);
		
		
		VideoSegment vs(n, src->video, bpm);
		
		// set video clip position and size
		//							minSize,	sizeScale,	transpKeyEn
		vector<float> shaderParams = {0.8,		1.,			-10.};
		switch(n.channel) {
			case 1:	vs.vertexes = genRectangle(-1, -1, 0, 0); break;
			case 0: vs.vertexes = genRectangle(0, -1, 1, 0); break;
			case 2:	vs.vertexes = genRectangle(-1, 0, 0, 1); break;
			case 3: vs.vertexes = genRectangle(0, 0, 1, 1); shaderParams[2] = 1.; break;
			default: continue;
		}
		vs.startSeconds += delay;
		//vs.offsetSeconds -= delay;
		vs.vertexShader = &vertexShader;
		vs.fragmentShader = &shader;
		vs.shaderParams = shaderParams;
		videoSegments.push_back(vs);
	}
	
	// find song duration
	double songStart = 1e9, songEnd = -1e9;
	for(auto& vs: videoSegments) {
		if(vs.startSeconds < songStart) songStart = vs.startSeconds;
		if(vs.endSeconds > songEnd) songEnd = vs.endSeconds;
	}

	// add green screen
	/*Image greenImage = {1, 1, "\x00\xff\x00\xff"s};
	ImageSource greenImageSource(&greenImage);
	
	VideoSegment vs;
	vs.startSeconds = songStart;
	vs.endSeconds = songEnd;
	vs.source = &greenImageSource;
	vs.zIndex = -1000;
	videoSegments.push_back(vs);*/
	
	defaultSettings.volume = 1./4;
	//defaultSettings.skipToSeconds = 22;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

