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

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	
	string buf = get_file_contents("songs/micronat_-_flying.mod");
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08*2/30);
	addSource("hum", "sources/bass1.mkv",
					"sources/bass1.mkv", 2.8*8/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5*4/30, 2., 2.);
	addSource("gab1", "sources/gab1.mkv",
					"sources/gab1.mkv", 2.55*4/30, 1./2, 1./2);
	addSource("gab2", "sources/gab2.mkv",
					"sources/gab2.mkv", 3.15*2/30, 1./2, 1./2);
	addSource("gab3", "sources/gab3.mkv",
					"sources/gab3.mkv", 2.5*2/30, 1./2, 1./2);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1./5, 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1./5, 1., 1.);
	trimSource("hum", 0.1);
	trimSource("gab1", 0.1);
	trimSource("gab3", 0.1);
	
	string shader =
		"float sizeScale = clamp(param(5) * secondsRel + param(4), 0.0, 1.0);\n\
		vec2 mypos = vec2(param(0), param(1));\n\
		vec2 mysize = vec2(param(2),param(3))*sizeScale;\n\
		mypos -= mysize*0.5;\n\
		vec2 myend = mypos+mysize;\n\
		vec2 relpos = (pos-mypos)/mysize;\n\
		if(pos.x>=mypos.x && pos.y>=mypos.y \n\
			&& pos.x<myend.x && pos.y<myend.y) \n\
			return vec4(texture2D(image, relpos).rgb, 1.);\n\
		return vec4(0,0,0,0);\n";

	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	ImageSource imgSource(&img1);
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastGridPos = 0;
	for(int i=0;i<(int)notes.size();i++) {
		Note& n = notes[i];
		Source* src = nullptr;
		double pan = 0.5;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		
		// select source
		switch(n.channel) {
			case 0: src = getSource("gab1"); n.amplitudeDB += 3; break;
			case 1: goto skip;
			case 2: src = getSource("hum"); n.pitchSemitones+=12; n.amplitudeDB += 5; break;
			case 3: src = getSource("gab2"); pan=0.7; n.amplitudeDB += 6; break;
			default: goto skip;
		}
		n.pitchSemitones -= 2;
		
		if(n.channel == 0) n.amplitudeDB += 9;
		if(n.channel == 2 && (n.instrument == 8 || n.instrument == 9))
			n.amplitudeDB += 6;
		
		{
			int gridN = 4;
			int gridSize = gridN*gridN;
			VideoSegment vs(n, src->hasVideo()?src->video:&imgSource, bpm);
			if(int(videoSegments.size()) >= gridSize) {
				videoSegments[videoSegments.size()-gridSize].endSeconds = vs.startSeconds-0.01;
			}
			
			int gridPos = (lastGridPos++) % gridSize;
			
			// set video clip position and size
			//							centerX		centerY	w		h		minSizeScale,	sizeScale
			vector<float> shaderParams = {0.5,		0.5,	1.,		1.,		0.8,			1.0,};
			shaderParams[0] = float(gridPos%gridN)/gridN + 1./gridN/2;
			shaderParams[1] = float(gridPos/gridN)/gridN + 1./gridN/2;
			shaderParams[2] = 1./gridN;
			shaderParams[3] = 1./gridN;
			shaderParams[4] = 0.8;
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			segments.push_back(as);
			
			vs.shader = &shader;
			vs.shaderParams = shaderParams;
			vs.zIndex = n.channel;
			videoSegments.push_back(vs);
		}
	skip: ;
	}
	defaultSettings.volume = 1./3;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

