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

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	// "do" has pitch 2.6/30
	double bp = 2.6/30;
	setSourceDir("sources/");
	
	//			name			pitch					tempo	vol	offs
	addSource2("agiriice.mp4", 	pow(2,0.1/12) * bp,		1., 3., 0.042);
	addSource2("agiri_lead.mkv", pow(2,-12./12) * bp,	1., 9., 0.02);
	addSource2("agirisample.mp4", pow(2,-3./12) * bp,	1., 1., 0.);
	addSource2("lead4.mp4",		pow(2,-9.5/12) * bp,	1., 1., 0.09);
	addSource2("ao.mkv",		pow(2,6./12) * bp,		1., 7., 0.043);
	addSource2("unmasked.mp4",	pow(2,-1.1/12) * bp,	1., 3., 0.121);
	addSource2("weeen.mp4", 	pow(2,-6.2/12) * bp,	1., 0., 0.15);
	addSource2("Done.mp4", 		pow(2,-1.8/12) * bp,	1., 2., 0.047);
	addSource2("drumkick.mp4", 	1.,						1., 6., 0.043);
	addSource2("sigh.mp4",		1.,		 				3., 2., 0.052);
	addSource2("smack.mp4",		1.,		 				1., 1., 0.086);
	
	addSource2("spherical/aaaaa2.mkv", 	pow(2,-1.4/12) * bp,		1., 3., 0.0);
	
	//addSource2("ytpmv1.mp4", 	pow(2,4./12) * bp,		1., 3., 0.0);
	
	addSource("ytpmv1", "ytpmv1_1.wav", "ytpmv1.mkv", 	pow(2,4.3/12) * bp,		1., 1.);
	getSource("ytpmv1")->amplitudeDB += 2;
	
	addSource("eee", "eee5.wav", "eee.mp4", 	pow(2,8./12) * bp,		1., 1.);
	trimSource("eee", 0.065);
	getSource("eee")->amplitudeDB += 5;
	
	dupSource("agiriice", "agiriice2", 1., 1., 0.);
	dupSource("agiriice", "agiriice3", 1., 1., 0.);
	getSource("agiriice")->amplitudeDB += 5;
	dupSource("ao", "ao_bass", 1., 1., 0.);
	
	loadSource(*getSource("agiriice2"), "agiriice_hpf.wav", "", pow(2,0.2/12) * bp, 1., 1.);
	trimSourceAudio("agiriice2", 0.042);
	
	loadSource(*getSource("agiriice3"), "agiriice_hpf2.wav", "", pow(2,0.2/12) * bp, 1., 1.);
	trimSourceAudio("agiriice3", 0.042);
	
	loadSource(*getSource("ao_bass"), "ao_bass.wav", "", pow(2,18./12) * bp, 1., 1.);
	trimSourceAudio("ao_bass", 0.043);
	
	
	dupSource("drumkick", "slap", 4., 4., 5.);
	
	categories["lead"] = {"spherical/aaaaa2", "ytpmv1", "Done", "ao", "unmasked"};
	categories["highlead"] = {"agiri_lead"};
	categories["wind"] = {"agiriice", "agiriice2", "agiriice3"};
	categories["bass"] = {"agirisample", "eee"};
	categories["kick"] = {"drumkick"};
	categories["snare"] = {"slap"};
	categories["hihat"] = {"sigh"};
	categories["slap"] = {"smack"};
	
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
		
		coords.xy = (coords.xy) * mysize - vec2(1,1);
		coords.xy += mypos*2.0;
		
		
		// transitions
		vec3 origCoords = coords;
		float trans1 = clamp(secondsAbs + 6,0,1000);
		
		// model transforms
		coords.x *= aspect;
		coords = rotationMatrix(vec3(1,0.,0), rotationY) * coords;
		
		// tiling
		vec2 offsPos = vec2(0.0,0.0);
		coords.y += (20 - (gl_InstanceID / 28))*2;
		coords.x += (-5 + (gl_InstanceID % 28))*2*aspect;
		coords.x += mod(offsPos.x, aspect*2);
		coords.y += mod(offsPos.y, 2);
		
		coords.z += param(7);
		
		
		
		// trans1
		coords = rotationMatrix(vec3(0,0,1), trans1*0.15) * coords;
		coords = rotationMatrix(vec3(1,0.5,0), -asymp(trans1)) * coords;
		coords.z -= asymp(trans1*0.1)*5.0;
		
		
		coords.z -= 2.5;
	
		gl_Position = proj*vec4(coords,1.0);
		gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
		uv = (texPos-vec2(0.5,0.5))/zoomScale + vec2(0.5,0.5);
	}
	)aaaaa";
	
	string shader = R"aaaaa(
		vec4 result = vec4(texture2D(image, pos).rgb, 1.);
		result.a = length(result.rgb - vec3(0.0,1.0,0.0));
		result.a = result.a*result.a*result.a*1000;
		result.a = clamp(result.a,0.0,1.0);
		return result;
	)aaaaa";

	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	
	string buf = get_file_contents("songs/Saturday Morning.mod");
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastSeg[128] = {};
	int lastNote[128] = {};
	bool flip[128] = {};
	
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
	
	for(int i=0;i<(int)notes.size();i++) {
		Note& origNote = notes[i];
		Note n = origNote;
		
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		bool forceVisible = false;
		double fade = 1e9;
		double reverbWet = 0.3;
		double tempo = 1.0;
		
		double x=0., y=0., sz = 0.2, z = 0.;
		double rot = 0.;
		double sizeScale = 2.;
		int zIndex = 0;
		
		// correct for pitch differences between samples
		if(ins.period != 0) {
			// assume period of 50 is pitch 1.0
			double p = 50./ins.period;
			n.pitchSemitones += log2(p)*12.;
		}
		n.pitchSemitones += 12;
		
		// select source
		string srcName;
		int gridPos = n.channel;
		if(n.channel == 6) {
			// bass
			
			srcName = selectSource("bass", 1);
			reverbWet = 0.25;
			
			n.adjustAmplitude(6);
			n.pitchSemitones += 24;
			// make higher pitch notes quieter
			n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
			
			x = 0.5;
			y = 0.5;
			sz = 1.0;
			sizeScale = 0.;
			z = -0.02;
			if(flip[n.channel]) sz *= -1;
			if(lastSeg[n.channel] != 0) {
				videoSegments.at(lastSeg[n.channel]).endSeconds = n.start.toSeconds(bpm);
			}
		} else if(n.instrument == 4) {
			// kick
			srcName = selectSource("kick", 0); n.pitchSemitones = 0; gridPos = 9; n.amplitudeDB += 5;
			reverbWet = 0.1;
			n.end.absRow += 1;
			if(n.start.seq == 25) n.adjustAmplitude(-3);
			
			x = 0.;
			y = 0.5;
			sz = 0.5;
			z = 0.5;
			rot = M_PI/2;
			if(lastSeg[n.channel] != 0) {
				videoSegments.at(lastSeg[n.channel]).endSeconds = n.start.toSeconds(bpm);
			}
		} else if(n.instrument == 5) {
			// snare
			srcName = selectSource("slap", 0); n.pitchSemitones = 0; n.adjustAmplitude(5);
			reverbWet = 0.1;
			n.end.absRow += 1;
			
			if(n.start.seq == 25) n.adjustAmplitude(-3);
			
			x = 0.;
			y = 0.5;
			sz = 0.5;
			z = 0.5;
			rot = M_PI/2;
			if(lastSeg[n.channel] != 0) {
				videoSegments.at(lastSeg[n.channel]).endSeconds = n.start.toSeconds(bpm);
			}
		} else if(n.channel >= 3 && n.channel <= 5) {
			// chords
			srcName = selectSource("wind", 1); n.adjustAmplitude(-2);
			fade = 0.;
			reverbWet = 0.2;
			pan = 0.6;
			
			sz = 1./5 * 0.95;
			y = 1. - sz/2*1.1;
			x = (n.channel-3-1.5)*sz + 0.5;
			sz *= 0.85;
		} else if(n.channel == 9) {
			// main melody
			srcName = selectSource("lead", 0);
			if(n.start.seq >= 16 && n.start.seq <= 19) {
				srcName = selectSource("lead", 1);
				reverbWet = 0.5;
			}
			if(n.instrument == 10) {
				n.adjustAmplitude(-1);
			}
			n.adjustAmplitude(6);
			
			//n.keyframes.clear();
			//fade = 0.;
			
			x = 0.5;
			y = 0.5;
			sz = 0.5;
			z = sz;
			rot = M_PI/2;
		} else if(n.channel == 10) {
			// counter melody
			srcName = selectSource("lead", 0);
			if(n.instrument == 10) {
				n.adjustAmplitude(-1);
			}
			n.adjustAmplitude(5);
			
			x = 0.5;
			y = 0.5;
			sz = 0.3;
			z = 0.02;
		} else if(n.channel == 8) {
			// mountain melody
			srcName = selectSource("lead", 1);
			n.adjustAmplitude(-1);
			
			// make higher pitch notes quieter
			n.adjustAmplitude(-(n.pitchSemitones-35.)*0.4);
			
			n.pitchSemitones += 12;
			pan = 0.4;
			
			x = 0.2;
			y = 0.5;
			sz = 0.3;
			z = 0.02;
		} else if(n.channel >= 100) {
			// arps
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
			y = 0.2;
		} else goto skip;
		
		src = getSource(srcName);
		
		{
			VideoSegment vs(n, src, bpm);
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale,	alpha,	z,	zoomScale, rotationY
			vector<float> shaderParams = {x,		y,		sz,		sz,		0.4,			sizeScale,	1.0,	z,	1.0,		rot};
			
			
			AudioSegment as(n, src, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += delay;
			
			// apply arps
			if(n.channel >= 100 && n.durationRows() > 2) {
				generateArps(as, bpm, n.channel-100);
			}
			
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
			
			// if the amplitude is quiet compared to a recently played note on this channel,
			// do not display video for it
			if(lastSeg[n.channel] != 0 && !forceVisible) {
				double ampl = origNote.amplitudeDB - notes.at(lastNote[n.channel]).amplitudeDB;
				int t = origNote.start.absRow - notes.at(lastNote[n.channel]).start.absRow;
				if(ampl < -6 && t < 8) {
					videoSegments.at(lastSeg[n.channel]).endSeconds = vs.endSeconds;
					continue;
				}
			}
			
			flip[n.channel] = !flip[n.channel];
			lastNote[n.channel] = i;
			
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			vs.instances = 28*28;
			vs.zIndex = zIndex;
			lastSeg[n.channel] = (int)videoSegments.size();
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	
	defaultSettings.volume = 0.26;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

