/*
 *  main.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 7/11/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include <iostream>
#include <stdio.h>
#include <list>
#include <algorithm>
#include <math.h>

#include <sndfile.h>

#include "Convolver.h"

using std::cerr;
using std::cout;
using std::endl;

using boost::shared_ptr;

// FIXME: optimize for performance on the server
#define SAMPLE_SIZE 2048

void normalize_signal(float *signal, uint32_t numFloats) {
	float max_float = 0.0f;
	uint32_t max_float_index = 0;
	
	float frame;
	for (uint32_t i=0; i < numFloats; i++) {
		frame = abs(signal[i]);
		if (frame > max_float) {
			max_float_index = i;
			max_float = frame;
		}
	}

	double gain = 0.95f / max_float;	

	for (uint32_t i=0; i < numFloats; i++) {
		signal[i] *= gain;
	}
}

uint32_t getSampleRate(const char *filename) {
	SF_INFO sfinfo;
	sfinfo.format = 0;
	SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);
	sf_close(file);
	return sfinfo.samplerate;
}

// FIXME: currently we only read a mono audio file, and toss the rest
float *readAudioFile(const char *filename, uint32_t *bufferSize, uint32_t *sampleRate) {
	SF_INFO sfinfo;
	sfinfo.format = 0;
	SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);
	
	float *buffer = new float[sfinfo.frames * sfinfo.channels];
	sf_count_t num_read = sf_readf_float(file, buffer, sfinfo.frames);
	assert(num_read == sfinfo.frames);
	
	*bufferSize = num_read;
	*sampleRate = sfinfo.samplerate;
	
	sf_close(file);	
	
	
	if(sfinfo.channels == 1) return buffer;
	
	// Otherwise, we conver this to mono
	float *monoBuffer = new float[sfinfo.frames];
	for (uint32_t i=0; i < sfinfo.frames; i++) {
		monoBuffer[i] = buffer[i * sfinfo.channels];
	}
		
	delete buffer;
	
	return monoBuffer;
}

char *resample(const char *filename, uint32_t target_rate) {
	char *buffer = new char[L_tmpnam];
	tmpnam (buffer);
	
	const char *command_schema = "sndfile-resample -to %u %s %s";
	char *command = new char[strlen(command_schema) + strlen(filename) + L_tmpnam + 1024];
	sprintf(command, "sndfile-resample -to %u %s %s", target_rate, filename, buffer);
	
	int retcode = system(command);

	if (retcode != 0) {
		std::cerr << "Problem calling command..." << std::endl;
		std::cerr << "Calling: " << command << std::endl << "failed." << std::endl << "Are you sure sndfile-resample is on the PATH?" << std::endl;
		assert(retcode == 0);
	}

	return buffer;
}

int main(int argc, char *argv[]) {
	uint32_t sampleSize = SAMPLE_SIZE;
		
	if (argc != 4) {
		std::cerr << "Proper usage:\n" << std::endl << argv[0] << " signalFile irFile outputFile.wav" << std::endl << std::endl;
		return 2;
	}
	
	const char *signalFilename = argv[1];
	const char *irFilename = argv[2];
	const char *outFilename = argv[3];
	
	
	// Read the signals to be convolved
	uint32_t signalBufferSize;
	uint32_t irBufferSize;
	
	uint32_t signalSampleRate;
	uint32_t irSampleRate;
	
	float *signalBuffer;
	float *irBuffer;
	
	irBuffer = readAudioFile(irFilename, &irBufferSize, &irSampleRate);
	signalSampleRate = getSampleRate(signalFilename);
	if (signalSampleRate == irSampleRate) {
		signalBuffer = readAudioFile(signalFilename, &signalBufferSize, &signalSampleRate);
	} else {		
		cout << "Signal is at " << signalSampleRate << "hz, but IR is at " << irSampleRate << "hz. Resampling..." << endl;
		char *resampledFilename = resample(signalFilename, irSampleRate);
		cout << "Resampled to: " << resampledFilename << endl << endl;		
		signalBuffer = readAudioFile(resampledFilename, &signalBufferSize, &signalSampleRate);

		remove(resampledFilename);
		delete resampledFilename;
	}
	
	assert(irSampleRate == signalSampleRate);
	
	// Open a file for output
	SF_INFO outInfo;
	outInfo.samplerate = irSampleRate;
	outInfo.channels = 1; // FIXME: support multi-channel
	outInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	outInfo.sections = 1;
	outInfo.seekable = 1;
	
	SNDFILE *outFile = sf_open(outFilename, SFM_WRITE, &outInfo);
	
	
	// Initialize the Convolver Kernel which will do the work
	Convolver::Kernel convolver;
	
	uint32_t small = SAMPLE_SIZE;
	uint32_t big = small * 4; // 4096
	uint32_t numSmall = 4;
	// Choose the block pattern we'll use (e.g. 1024, 2048 4096, 4096, 4096, 4096, ...)
	//shared_ptr<Convolver::BlockPattern> pattern(new Convolver::FixedSizeBlockPattern (sampleSize));	
	shared_ptr<Convolver::BlockPattern> pattern(new Convolver::TwoSizeBlockPattern(small, big, numSmall));
	
	
	// Each channel has a state to it
	Convolver::State state(convolver, pattern);
	
	// Load a filter (single channel IR)
	Convolver::Filter filter(convolver, pattern, irBuffer, irBufferSize);
	float gain = filter.measureGain();
	filter.modify_Scale(1.0f / gain);
	
	int outputBufferSize = signalBufferSize + irBufferSize + sampleSize;
	float *outputBuffer = new float[outputBufferSize];
	
	// Do the Convolution in sampleSize blocks
	for(int i=0; i < signalBufferSize + irBufferSize; i += sampleSize) {
	  float *result;
		
	  shared_ptr<Convolver::TimeBlock> input;
	  if (i + sampleSize > signalBufferSize) {
	    // Feed the last samples & start ringing out
	    float *lastSampleBuffer = new float[sampleSize]();
	    if (i <= signalBufferSize) memcpy(lastSampleBuffer, &signalBuffer[i], (signalBufferSize - i) * sizeof(float));
		input = shared_ptr<Convolver::TimeBlock>(new Convolver::TimeBlock(lastSampleBuffer, lastSampleBuffer + sampleSize));
		delete lastSampleBuffer;
	  } else {
		input = shared_ptr<Convolver::TimeBlock>(new Convolver::TimeBlock(&signalBuffer[i], &signalBuffer[i] + sampleSize));
	  }

	  shared_ptr<Convolver::TimeBlock> output = convolver.convolve(input, filter, state);

	  // Copy the result to our output buffer		
	  std::copy(output->begin(), output->end(), &outputBuffer[i]);		  
	}
	
	//normalize_signal(outputBuffer, outputBufferSize);
	
	sf_count_t num_written = sf_writef_float(outFile, outputBuffer, outputBufferSize);
	
	if (num_written != outputBufferSize) {
		cerr << "ERROR: output buffer was " << outputBufferSize << " frames, but soundfile only wrote " << num_written << " frames" << endl;
		assert(num_written == outputBufferSize);
	}
	
	sf_close(outFile);
	
	cout << "Success!!!" << endl;
	
	return 0;
}
