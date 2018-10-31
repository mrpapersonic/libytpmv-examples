#include <stdio.h>
#include <ytpmv/modparser.H>
#include <ytpmv/common.H>
#include <ytpmv/simple.H>
#include <ytpmv/mmutil.H>
#include <math.h>
#include <unistd.h>
#include <assert.h>

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
layout(location = 1) in vec3 objCenter;
layout(location = 2) in vec2 texPos;
uniform vec2 coordBase;
uniform mat2 coordTransform;
uniform float secondsRel;
uniform float secondsAbs;
uniform float userParams[16];
uniform mat4 proj;
smooth out vec2 uv;

void main() {
	vec3 coords = myPos;
	float sizeScale = userParams[4] * secondsRel;
	
	coords.z = -1.0;
	
	gl_Position = vec4(coords,1.0);
	gl_Position.xy = coordBase + gl_Position.xy * coordTransform;
	
	uv = texPos;
}
)aaaaa";
string shader = R"aaaaa(
	float opacityScale = param(0) + param(5) * secondsRel;
	opacityScale = clamp(opacityScale,0.0,1.0);
	float radius = param(1);
	vec2 aspect = vec2(resolution.x/resolution.y, 1.0);
	float dist = length((pos-vec2(0.5,0.5))*aspect)*2;
	float opacity = clamp(radius-pow(dist,5), 0.0, 1.0);
	return vec4(texture2D(image, pos).rgb, opacityScale*opacity);
	)aaaaa";

string shader_withborder = R"aaaaa(
	float opacityScale = param(0) + param(5) * secondsRel;
	opacityScale = clamp(opacityScale,0.0,1.0);
	float radius = param(1);
	vec2 aspect = vec2(resolution.x/resolution.y, 1.0);
	float dist = length((pos-vec2(0.5,0.5))*aspect)*2;
	
	// outside region
	if(dist > radius) {
		// add shadow
		float distToBorder = dist-radius;
		distToBorder += 0.3;
		float opacity = clamp(1./pow(distToBorder*5,2), 0.0, 1.0);
		return vec4(0.0,0.0,0.0,opacity);
	}
	if(radius-dist < 0.05)
		return vec4(0.7,0.7,0.7,opacityScale*0.8);
	return vec4(texture2D(image, pos).rgb, opacityScale);
	)aaaaa";


int readAll(int fd,void* buf, int len) {
	uint8_t* buf1=(uint8_t*)buf;
	int off=0;
	int r;
	while(off<len) {
		if((r=read(fd,buf1+off,len-off))<=0) break;
		off+=r;
	}
	return off;
}

void addBackingVideo(vector<VideoSegment>& videoSegments, double songEnd) {
	int backVideoPipe[2];
	assert(pipe(backVideoPipe) == 0);
	loadVideoToFD("/persist/home_alone-1.ogv", backVideoPipe[1]);
	
	VideoSegment vs;
	vs.shader = &shader;
	vs.zIndex = -100;
	vs.shaderParams = {1.,		1000.,		0.,	0.,	0.1,		0.};
	vs.startSeconds = -2.3;
	vs.endSeconds = songEnd;
	vs.speed = 1.;
	vs.source = nullptr;
	uint32_t texture = createTexture();
	
	double lastFrameTime = 0.;
	vs.getTexture = [texture, backVideoPipe, lastFrameTime]
					(const VideoSegment& seg, double timeSeconds) mutable {
		double frameDuration = 1./23.75;
		int w = 1354, h = 768;
		int stride = (w*3 + 3)/4*4;
		string data;
		while((timeSeconds - lastFrameTime) >= frameDuration) {
			data.resize(stride*h);
			readAll(backVideoPipe[0], &data[0], (int)data.length());
			lastFrameTime += frameDuration;
		}
		if(data.size() > 0) setTextureImage(texture, &data[0], w, h);
		return texture;
	};
	
	vs.vertexes = genRectangle(-1, -1, 1, 1);
	vs.vertexVarSizes[0] = 3;
	vs.vertexVarSizes[1] = 2;
	vs.vertexVarSizes[2] = 0;
	videoSegments.push_back(vs);
}

int main(int argc, char** argv) {
	ytpmv::parseOptions(argc, argv);
	string buf = get_file_contents("songs/home_alone.mod");
	
	addSource("o", "sources/o_35000.wav", "", 3.5/30);
	addSource("lol", "sources/lol_20800.wav", "sources/lol.mkv", 2.08/30);
	addSource("ha", "sources/ha2_21070.wav", "", 2.107/30);
	addSource("hum", "sources/bass1_4.wav",
					"sources/bass1.mkv", 2.8/30, 2., 1.);
	addSource("hum2", "sources/bass1_3.wav", "", 2.8/30, 1., 1.);
	addSource("ya", "sources/drink.mp4",
					"sources/drink.mp4", 2.45/30, 1., 1.);
	addSource("aaaa", "sources/aaaa.wav",
					"sources/aaaa.mp4", 1.5/30, 2., 2.);
	addSource("perc1", "sources/perc1.mkv",
					"sources/perc1.mkv", 1., 1., 1.);
	addSource("perc2", "sources/perc2.mkv",
					"sources/perc2.mkv", 1., 4., 1.);
	
	getSource("hum2")->video = getSource("hum")->video;
	getSource("hum2")->hasVideo = true;
	
	
	SongInfo inf;
	vector<Instrument> instr;
	vector<Note> notes;
	parseMod((uint8_t*)buf.data(), buf.length(), inf, instr, notes);
	
	double bpm = inf.bpm;
	//double bpm = inf.bpm*0.8;
	
	// convert to audio and video segments
	vector<AudioSegment> segments;
	vector<VideoSegment> videoSegments;
	int lastSegment[4] = {-1, -1, -1, -1};
	bool lastOrientation[4] = {false, false, false, false};
	
	double songEnd = 0;
	for(int i=0;i<(int)notes.size();i++) {
		double t = notes[i].end.absRow*60./bpm;
		if(t>songEnd) songEnd = t;
	}
	fprintf(stderr, "SONG END: %f\n", songEnd);
	
	
	for(int i=0;i<(int)notes.size();i++) {
		Note& originalNote = notes[i];
		Note n = originalNote;
		Source* src = nullptr;
		double pan = 0.5;
		//Instrument& ins = instr.at(n.instrument==0?0:(n.instrument-1));
		n.pitchSemitones -= 3;
		
		// select source
		switch(n.instrument) {
			case 1:
				src = getSource("hum2");
				n.amplitudeDB += 3;
				n.pitchSemitones += 24;
				break;
			case 2:
				src = getSource("hum"); n.pitchSemitones+=24; n.amplitudeDB += 5;
				break;
			case 7:
				src = getSource("lol"); n.pitchSemitones+=24; n.amplitudeDB += 26;
				n.amplitudeDB -= (n.pitchSemitones)/3.;
				break;
			case 5: src = getSource("perc1"); n.pitchSemitones=-1; n.amplitudeDB += 5; break;
			case 4: src = getSource("perc1"); n.pitchSemitones=0; n.amplitudeDB += 0; break;
			case 3: src = getSource("perc2"); n.pitchSemitones=12; n.amplitudeDB += 0; break;
			default: continue;
		}
		
		// add harmonics to the bass sample
		if(n.instrument == 1 || n.instrument == 2) {
			for(int harm = 2; harm<6; harm++) {
				AudioSegment as(n, src->audio, bpm);
				as.pitch *= harm;
				//as.tempo *= harm;
				for(int ch=0;ch<CHANNELS;ch++)
					as.amplitude[ch] *= 1./(harm*harm);
				segments.push_back(as);
			}
		}
		// add bass
		{
			AudioSegment as(n, src->audio, bpm);
			as.pitch *= 0.5;
			for(int ch=0;ch<CHANNELS;ch++)
				as.amplitude[ch] *= .2;
			segments.push_back(as);
		}
		
		AudioSegment as(n, src->audio, bpm);
		as.amplitude[0] *= (1-pan)*2;
		as.amplitude[1] *= (pan)*2;
		segments.push_back(as);
		
		
		VideoSegment vs(n, src->video, bpm);
		vs.vertexShader = &vertexShader;
		vs.shader = &shader_withborder;
		vs.vertexVarSizes[0] = 3;
		vs.vertexVarSizes[1] = 3;
		vs.vertexVarSizes[2] = 2;
		vs.vertexVarSizes[3] = 0;
		vs.zIndex = n.start.absRow + 1;
		
		// set video clip position and size
		//							opacity		radius,	vx,	vy,	sizeScale	opacityScale
		vector<float> shaderParams = {1.0,		1.,		0.,	0.,	0.1,		0.};
		switch(n.channel) {
			case 0:	// percussion
				vs.vertexes = genRectangleWithCenter(-0.8, -2, 2.2, 1, -1., -1., 2., 2.);
				break;
			case 1:	// main melody 1
				vs.vertexes = genRectangleWithCenter(-0.8, -1, 2.2, 2, -1., -1., 2., 2.);
				break;
			case 2:	// main melody 2
				vs.vertexes = genRectangleWithCenter(-2.2, -1, 0.8, 2, -1., -1., 2., 2.);
				break;
			case 3:	// bass
				vs.vertexes = genRectangleWithCenter(-2.2, -2, 0.8, 1, -1., -1., 2., 2.);
				break;
			default: continue;
		}
		
		if(n.channel == 1 || n.channel == 2) {
			// this is a quiet note
			if(originalNote.amplitudeDB < -7) {
				// extend the last loud note
				if(lastSegment[n.channel] >= 0)
					videoSegments.at(lastSegment[n.channel]).endSeconds = vs.endSeconds;
				
				//vs.zIndex = 5;
				shaderParams[0] = 0.5;
				for(int j=0; j<(int)vs.vertexes.size(); j+=8) {
					vs.vertexes.at(j) += 0.05;
					vs.vertexes.at(j+1) += 0.05;
				}
			} else {
				// extend the last loud note
				if(lastSegment[n.channel] >= 0)
					videoSegments.at(lastSegment[n.channel]).endSeconds = vs.startSeconds;
				//vs.endSeconds += 1;
				lastSegment[n.channel] = (int)videoSegments.size();
				
				// flip horizontally
				lastOrientation[n.channel] = !lastOrientation[n.channel];
				if(lastOrientation[n.channel]) {
					for(int j=0; j<(int)vs.vertexes.size(); j+=8) {
						vs.vertexes.at(j+6) = 1.0 - vs.vertexes.at(j+6);
					}
				}
			}
		}
		
		vs.shaderParams = shaderParams;
		videoSegments.push_back(vs);
	}
	
	// add the background video
	addBackingVideo(videoSegments, songEnd);
	
	defaultSettings.volume = 1./4;
	//defaultSettings.skipToSeconds = 15;
	ytpmv::run(argc, argv, segments, videoSegments);
	return 0;
}

