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

int getPianoKey(int semitones) {
	int res = 0;
	while(semitones < 0) {
		semitones += 12;
		res -= 7;
	}
	while(semitones > 12) {
		semitones -= 12;
		res += 7;
	}
	static const int noteMappings[] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 8, 9, 10, 10, 11, 11, 12, 12};
	return res + noteMappings[semitones];
}

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	string buf = get_file_contents("songs/xaxaxa_-_untitled.mod");
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
	trimSource("hum", 0.1);
	trimSource("gab3", 0.1);
	trimSource("aaaa", 0.05);
	
	string shader =
		"float sizeScale = clamp(param(5) * secondsRel + param(4), 0.0, 1.0);\n\
		vec2 mypos = vec2(param(0), param(1));\n\
		vec2 mysize = vec2(param(2),param(3))*sizeScale;\n\
		float transpKey = param(6);\n\
		float alpha = param(7);\n\
		vec2 velocity = vec2(param(8), param(9));\n\
		mypos -= mysize*0.5;\n\
		mypos += velocity*secondsRel;\n\
		vec2 myend = mypos+mysize;\n\
		vec2 relpos = (pos-mypos)/mysize;\n\
		vec4 result;\n\
		if(pos.x>=mypos.x && pos.y>=mypos.y \n\
			&& pos.x<myend.x && pos.y<myend.y) { \n\
			result = vec4(texture2D(image, relpos*0.98+vec2(0.01,0.01)).rgb, 1.);\n\
			result.a = length(result.rgb - vec3(transpKey,transpKey,transpKey));\n\
			result.a = result.a*result.a*result.a*1000;\n\
			result.a = clamp(result.a,0.0,1.0);\n\
			result.a = clamp(result.a*alpha,0.0,1.0);\n\
			return result;\n\
		}\n\
		return vec4(0,0,0,0);\n";
	
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
				src = getSource("hum");
				n.amplitudeDB += 4;
				break;
			case 2:
				src = getSource("gab1");
				n.pitchSemitones -= 12;
				n.amplitudeDB += 6;
				break;
			case 3:
				src = getSource("aaaa");
				n.amplitudeDB += 1;
				n.pitchSemitones -= 12;
				break;
			default: goto skip;
		}
		
		{
			VideoSegment vs(n, src->video, bpm);
			vs.zIndex = n.channel;
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale	transpKey,	alpha,	vx,	vy
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			3.0,		-1.0,		1.0,	0.,	0.};
			
			switch(n.channel) {
				// main melody
				case 0:
				{
					// upper "do" has pitch 19
					int note = (int)n.pitchSemitones - 19;
					int note2 = getPianoKey(note);
					shaderParams = {0.85, 0.2, 0.3, 0.3, 0.8, 1.0, -1.0, 1.0, -0.2, 0.0};
					shaderParams[1] = (0.15 - note2 * 0.08);
					vs.endSeconds = vs.startSeconds + 5.;
					vs.zIndex = n.start.absRow + 1;
					break;
				}
				// bass
				case 2: shaderParams = {0.5, 0.5, 1.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 0.0}; vs.zIndex = -100; break;
				// high melody 1
				case 1:
				{
					// "do" has pitch 7
					int note = (int)n.pitchSemitones - 7;
					int note2 = getPianoKey(note);
					
					shaderParams = {0.2, 0.9, 0.3, 0.3, 0.8, 3.0, 241./255., 1.0, 0.0, 0.0};
					shaderParams[0] = 0.07 + (note2 % 6) * 0.16;
					shaderParams[1] = 0.6 + (note2/6) * 0.2;
					vs.zIndex = n.start.absRow + 1;
					break;
				}
				default: shaderParams = {0.1, 0.9, 0.2, 0.2, 0.8, 3.0, -1.0, 1.0, 0.0, 0.0}; break;
			}
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			as.startSeconds += timeOffsetSeconds;
			segments.push_back(as);
			
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	defaultSettings.volume = 0.3;
	//defaultSettings.skipToSeconds = 15.;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

