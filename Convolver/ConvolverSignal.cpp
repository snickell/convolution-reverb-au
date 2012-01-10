/*
 *  ConvolverSignal.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverSignal.h"
#include "ConvolverInternal.h"

#include <iostream>

using std::vector;
using boost::shared_ptr;
using std::cout;
using std::endl;

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

namespace Convolver {

	Signal::Signal(Convolver::Kernel &kernel, shared_ptr<BlockPattern> &blockPattern, 
				   const TimeSample *samplesTimeDomain, uint32_t numSamples)
	{	
		initialize(kernel, blockPattern, samplesTimeDomain, numSamples);
	}
	
	void Signal::initialize(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern, 
							const TimeSample *samplesTimeDomain, uint32_t numSamples)
	{
		this->blockPattern = blockPattern;
		rawBlocks.clear();
		
		// THERE'S A BUG IN HERE... what size are things supposed to be? do we want
		// fftSize to be 2x timeSize?
		
		uint32_t block = 0;
		uint32_t samplesHandled = 0;
		bool reserved = false;
		timeSize = 0;
		
		while(samplesHandled < numSamples) {
			uint32_t blockSize = blockPattern->sizeForTimeBlock(block);
			timeSize += blockSize;
			uint32_t fftSize = blockSize * 2;
			
			// If this is our first time through, reserve a bunch of blocks
			// This is well suited to the Fixed & GrowingSize allocation schemes
			if (!reserved) {
				rawBlocks.reserve(ceil((double)numSamples / (double)blockSize));
				reserved = true;
			}
			
			// Zero pad the input signal (to the fftsize, but also for the last roundoff
			uint32_t numSamplesLeft = numSamples - samplesHandled;
			uint32_t numRealSamplesInBlock = std::min(blockSize, numSamplesLeft);
			float *inputTimeDomainPadded = Convolver::Kernel::zeroPad(&samplesTimeDomain[samplesHandled], numRealSamplesInBlock, fftSize);
			
			// Do the FFT
			FFT &fft = kernel.getFFT(fftSize);
			shared_ptr<FreqBlock> freqBlock(fft.fftr(inputTimeDomainPadded, fftSize));
			rawBlocks.push_back(freqBlock);
			
			
			delete[] inputTimeDomainPadded;			
			
			samplesHandled += blockSize;
			block++;
		}
		blocks = rawBlocks;		
	}

	
	void setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern) {
		
	}
	
	Signal::~Signal() {
		
	}
	
	vector<shared_ptr<FreqBlock> > &Signal::getBlocks() {
		return blocks;
	}

	void Signal::modify_Scale(float scale) {
		foreach(shared_ptr<FreqBlock> &block, blocks) {
			block->scale(scale);
		}
	}
	
	void Signal::print (float scale) {
		throw std::string("Not Implemented");
	}

}