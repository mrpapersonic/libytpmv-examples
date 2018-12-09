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
	//srand(index);
	//i = random();
	//fprintf(stderr, "%d => %d\n", index, (i%arr.size()));
	//i = arr.size() - i - 1;
	return arr[i % arr.size()];
}

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	
	// "do" has pitch 2.6/30
	double bp = 2.6/30;
	setSourceDir("sources/spherical/");
	addSource2("ding2.mkv", 	pow(2,1./12) * bp,		1., 0., 0.1);
	addSource2("ding3.mkv", 	pow(2,-2./12) * bp,		1., 0., 0.1);
	addSource2("ding4.mkv", 	pow(2,-4./12) * bp,		1., 0., 0.05);
	addSource2("ding5.mkv", 	pow(2,-7./12) * bp,		1., 0., 0.11);
	addSource2("ding6.mkv", 	pow(2,-3.7/12) * bp,	1., 0., 0.);
	addSource2("harmonica.mkv", pow(2,0.) * bp,			1./2, 0., 0.5);
	addSource2("harmonica2.mkv", pow(2,0.2/12) * bp,	1., 0., 0.2);
	addSource2("meow2.mkv", 	pow(2,0./12) * bp,		1., 0., 0.03);
	
	addSource2("kick.mkv",	1.,	1., 0., 0.04);
	addSource2("kick2.mkv", 1.,	1., 0., 0.);
	addSource("kick_filtered", "kick_filtered.wav",
					"kick.mkv", 1., 1., 1.);
	trimSource("kick_filtered", 0.04);
	
	dupSource("kick", "slap", 1.5, 2);
	dupSource("kick2", "slap2", 1.5, 2);
	
	addSource2("snare.mkv",		1.,	1., 0., 0.01);
	addSource2("snare2.mkv", 	1.,	1., 0., 0.056);
	addSource2("snare3.mkv", 	1.,	1., 0., 0.);
	addSource2("snare4.mkv", 	1.,	1., 0., 0.013);
	
	addSource2("tss.mkv", 	1.,		1., 0., 0.02);
	addSource2("tss2.mkv", 	1.,		1., 0., 0.);
	addSource2("hihat.mkv", 1.5,	1., 0., 0.01);
	
	addSource2("woodstick.mkv", 0.2,	0.5, 13., 0.035);
	addSource2("woodstick2.mkv", 1.,	1., 0., 0.04);
	addSource2("woodstick3.mkv", 0.5,	0.5, 15., 0.038);
	addSource2("woodstick4.mkv", 1.,	1., 0., 0.027);
	
	
	categories["lead"] = {"ding3", "ding5", "meow2", "ding6"};
	categories["highlead"] = {"ding3", "ding5", "ding4", "ding6"};
	categories["wind"] = {"harmonica2"};
	categories["bass"] = {"harmonica2"};
	categories["kick"] = {"kick_filtered", "kick"};
	categories["snare"] = {"snare2", "snare", "snare3", "snare4"};
	categories["hihat"] = {"tss2", "tss", "hihat"};
	categories["slap"] = {"slap", "woodstick3"};
	
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
	
	string buf = get_file_contents("songs/edzes-inside_beeks_mind_1.mod");
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastSeg[16] = {};
	int lastNote[16] = {};
	for(int i=0;i<(int)notes.size();i++) {
		Note& origNote = notes[i];
		Note n = origNote;
		Source* src = nullptr;
		double pan = 0.5;
		double delay = 0.;
		Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		bool forceVisible = false;
		
		// select source
		string srcName;
		int gridPos = n.channel;
		switch(n.instrument) {
			case 14: srcName = selectSource("kick", 0); gridPos = 0; n.pitchSemitones = -3; break;
			case 15: srcName = selectSource("slap", 0); gridPos = 0; n.pitchSemitones = 3; n.amplitudeDB -= 3; break;
			case 16: srcName = selectSource("hihat", 0); gridPos = 0; n.pitchSemitones = 0; n.amplitudeDB -= 15; break;
			case 2: srcName = selectSource("bass", 0); gridPos = 1; n.pitchSemitones -= 12; n.amplitudeDB += 1; break;
			// do xi so
			case 5: srcName = selectSource("wind", 0); gridPos = 3; n.pitchSemitones += 12; n.amplitudeDB -= 2; break;
			case 4:
				srcName = selectSource("lead", 0); n.amplitudeDB += 4;
				if(n.channel == 2 || n.channel == 7) { // main melody
					gridPos = n.channel == 2 ? 4 : 7;
					n.end.absRow += 2;
				} else {
					gridPos = n.channel - 4 + 8;
				}
				break;
			// main melody part 2
			case 1:
				gridPos = 5;
				srcName = selectSource("lead", 2); n.amplitudeDB += 4;
				break;
			case 6:
				if(n.channel == 13) { // do xi so mi
					gridPos = 7;
					srcName = selectSource("lead", 1);
					if(n.start.absRow % 2) continue;
					n.end.absRow++;
					n.amplitudeDB += 8;
				} else if(n.channel < 8) { // melody hint
					gridPos = n.channel == 2 ? 5 : 6;
					srcName = selectSource("wind", 0);
					n.pitchSemitones += 12;
					n.amplitudeDB -= 3;
					forceVisible = true;
				} else { // chords
					gridPos = n.channel - 9 + 12;
					srcName = selectSource("wind", 0);
					n.pitchSemitones += 12;
					n.amplitudeDB -= 10;
				}
				break;
			// do re mi so do so mi re
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
				if(n.start.absRow % 2) continue;
				gridPos = 2;
				n.end.absRow++;
				n.pitchSemitones += 12;
				n.amplitudeDB -= 3;
				srcName = selectSource("highlead", 1);
				break;
			default: goto skip;
		}
		src = getSource(srcName);
		
		{
			int gridN = 4;
			int gridSize = gridN*gridN;
			VideoSegment vs(n, src, bpm);
			
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			1.0,};
			shaderParams[0] = float(gridPos%gridN)/gridN + 1./gridN/2;
			shaderParams[1] = float(gridPos/gridN)/gridN + 1./gridN/2;
			shaderParams[2] = 1./gridN;
			shaderParams[3] = 1./gridN;
			shaderParams[4] = 0.8;
			
			AudioSegment as(n, src, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += delay;
			segments.push_back(as);
			
			// bass
			if(n.instrument == 2) {
				n.pitchSemitones += 12;
				n.amplitudeDB -= 4;
				{AudioSegment as(n, src, bpm);
				segments.push_back(as);}
				n.pitchSemitones += 12;
				n.amplitudeDB += 1;
				{AudioSegment as(n, src, bpm);
				segments.push_back(as);}
				n.pitchSemitones += 12;
				n.amplitudeDB -= 8;
				{AudioSegment as(n, src, bpm);
				segments.push_back(as);}
			}
			// chords
			if(n.channel >= 9 && n.channel <= 12) {
				n.pitchSemitones += 12;
				n.amplitudeDB -= 1;
				AudioSegment as(n, src, bpm);
				segments.push_back(as);
			}
			
			if(gridPos == -1) continue;
			// if the amplitude is quiet compared to a recently played note on this channel,
			// do not display video for it
			if(lastSeg[n.channel] != 0 && !forceVisible) {
				double ampl = origNote.amplitudeDB - notes.at(lastNote[n.channel]).amplitudeDB;
				int t = origNote.start.absRow - notes.at(lastNote[n.channel]).start.absRow;
				if(ampl < -3 && t < 8) {
					videoSegments.at(lastSeg[n.channel]).endSeconds = vs.endSeconds;
					continue;
				}
			}
			lastNote[n.channel] = i;
			
			vs.vertexShader = &vertexShader;
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			vs.zIndex = n.channel;
			lastSeg[n.channel] = (int)videoSegments.size();
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	
	defaultSettings.volume = 0.22;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

