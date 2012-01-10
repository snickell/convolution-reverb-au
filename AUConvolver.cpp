/*
 *  AUConvolver.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "AUConvolver.h"

#include <iostream>

using std::cerr;
using std::cout;
using std::endl;
using boost::shared_ptr;

using namespace Convolver;

AUConvolver::AUConvolver(AUEffectBase &au, shared_ptr<BlockPattern> blockPattern) 
	: Convolver(blockPattern, true), au(au)
{
	//au.GetMaxFramesPerSlice()
	
}

void AUConvolver::setFilters(Filters &filters, float stereoSeparation)
{
	uint32_t numChannels = (UInt32)au.GetNumberOfChannels();
	bool changeOutChannels = false;
	
	assert((numChannels == 1) || (numChannels == 2));
	
	#if DEBUG
	cout << "AUConvolver::setFilters() on " << filters.size() << " IRs" << endl;
	#endif	
	
	if (numChannels == 1) {
		setupMonoIn(filters, changeOutChannels, numChannels);
	} else if (numChannels == 2) {
		Convolver::setupStereoIn(filters, changeOutChannels, numChannels, stereoSeparation);
	}
}

void AUConvolver::convolve(const AudioBufferList & inBuffer, 
						   AudioBufferList & outBuffer,
						   UInt32 inFramesToProcess,
						   float dryGain,
						   float wetGain)
{
	assert(sizeof(AudioSampleType) == sizeof(TimeSample));
	
	uint32_t size = inBuffer.mNumberBuffers;
	std::vector<const TimeSample *> in(size);
	for (uint32_t i=0; i < size; i++) {
		in[i] = (const TimeSample *)inBuffer.mBuffers[i].mData;
	}
	
	size = outBuffer.mNumberBuffers;
	std::vector<TimeSample *> out(size);
	for (uint32_t i=0; i < size; i++) {
		#if DEBUG_CONVOLVE
		cout << "AUConvolver::convolve() out channel: " << i << " pointer(" << (uint32_t)outBuffer.mBuffers[i].mData << ")" << endl;
		#endif	
		
		out[i] = (TimeSample *)outBuffer.mBuffers[i].mData;
	}
	
	Convolver::convolve(in, out, inFramesToProcess, dryGain, wetGain);
}