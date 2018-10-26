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
	
	string buf = get_file_contents("songs/hehj.mod");
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08*2/30);
	addSource("hum", "sources/bass1.mkv",
					"sources/bass1.mkv", 2.8*4/30, 1., 0.2);
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
	trimSource("gab1", 0.1);
	trimSource("gab3", 0.1);
	
	string shader =
		"float sizeScale = param(8) * secondsRel;\n\
		float opacityScale = param(4) + param(9) * secondsRel;\n\
		opacityScale = clamp(opacityScale,0.0,1.0);\n\
		float radius = param(5);\n\
		vec2 aspect = vec2(resolution.x/resolution.y, 1.0);\n\
		vec2 mypos = vec2(param(0)-sizeScale, param(1)-sizeScale);\n\
		vec2 mysize = vec2(param(2)+sizeScale*2,param(3)+sizeScale*2);\n\
		vec2 velocity = vec2(param(6), param(7));\n\
		mypos += velocity * secondsRel;\n\
		vec2 myend = mypos+mysize;\n\
		vec2 relpos = (pos-mypos)/mysize;\n\
		float dist = length((relpos-vec2(0.5,0.5))*aspect)*2;\n\
		float opacity = clamp(radius-pow(dist,5), 0.0, 1.0);\n\
		if(pos.x>=mypos.x && pos.y>=mypos.y \n\
			&& pos.x<myend.x && pos.y<myend.y) \n\
			return vec4(texture2D(image, relpos).rgb, opacityScale*opacity);\n\
		return vec4(0,0,0,0);\n";

	string img1data = get_file_contents("fuck.data");
	Image img1 = {480, 371, img1data};
	
	VideoSource imgSource = {"source 1", {img1}, 1.0};
	
	
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
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		
		// select source
		switch(n.instrument) {
			case 1: src = getSource("gab1"); n.amplitudeDB += 16; break;
			case 2: src = getSource("hum"); break;
			case 3: src = getSource("lol"); n.pitchSemitones+=12; n.amplitudeDB += 10; break;
			case 4: src = getSource("gab2"); n.pitchSemitones += -7 + 12; pan=0.7; n.amplitudeDB += 6; break;
			case 6: src = getSource("gab2"); n.pitchSemitones += -7 + 12; n.amplitudeDB += 6; break;
			case 8: src = getSource("perc1"); n.amplitudeDB += 5; break;
			case 9: src = getSource("perc2"); n.amplitudeDB += 5; break;
			default: goto skip;
		}
		n.pitchSemitones -= 2;
		n.keyframes.clear();
		
		if(n.channel == 0) n.amplitudeDB += 9;
		if(n.channel == 2 && (n.instrument == 8 || n.instrument == 9))
			n.amplitudeDB += 6;
		
		{
			VideoSegment vs(n, src->hasVideo?src->video:imgSource, bpm);
			
			// set video clip position and size
			//								x		y		w		h		opacity		radius,	vx,	vy,	sizeScale	opacityScale
			vector<float> shaderParams = {0.,		0.,		1.,		1.,		0.7,		100.,	0.,	0.,	0.1,		0.};
			if(n.channel >= 1 && n.channel <= 9) {
				shaderParams[0] = float((n.channel-1)/3)/3. + 1./3/2 - 0.4/2;
				shaderParams[1] = float((n.channel-1)%3)/3. + 1./3/2 - 0.4/2;
				shaderParams[2] = 0.4;
				shaderParams[3] = 0.4;
				shaderParams[4] = 1.;
				shaderParams[5] = 1.;
			}
			if(n.channel == 8) {
				shaderParams[0] = 2./3 + 1./3/2 - 0.4/2;
				shaderParams[1] = float(n.start.row-56)/8.;
				if(n.start.row == 0) {
					shaderParams[0] = 0.;
					shaderParams[1] = 0.;
					shaderParams[2] = 1.;
					shaderParams[3] = 1.;
					shaderParams[4] = 0.8;
					shaderParams[5] = 100.;
				}
			}
			if(n.channel == 9) {
				vs.endSeconds += 1.;
				shaderParams[6] = 0.;	// velocity x
				shaderParams[7] = -1.8;	// velocity y
				shaderParams[8] = 0.;	// size scaling per time
				shaderParams[9] = -2.;	// opacity change per time
			}
			
			AudioSegment as(n, src->audio, bpm);
			as.amplitude[0] *= (1-pan)*2;
			as.amplitude[1] *= (pan)*2;
			
			// percussion samples have some delay
			if(n.instrument == 8 || n.instrument == 9) {
				as.startSeconds -= 0.03;
			}
			
			segments.push_back(as);
			
			// sample 4 is a chord of do and so
			if(n.instrument == 4) {
				AudioSegment as2(n, src->audio, bpm);
				as2.pitch *= pow(2,-5./12);
				segments.push_back(as2);
			}
			// sample 6 is a chord of do and la
			if(n.instrument == 6) {
				AudioSegment as2(n, src->audio, bpm);
				as2.pitch *= pow(2,-3./12);
				segments.push_back(as2);
			}
			
			vs.shader = shader;
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

