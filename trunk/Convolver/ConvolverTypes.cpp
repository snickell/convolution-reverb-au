/*
 *  ConvolverTypes.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverTypes.h"
#include "ConvolverFFT.h"
#include "ConvolverInternal.h"

#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::cerr;

uint32_t Convolver::TimeBlock::numTimeBlocks = 0;

static void printComplex(Convolver::FreqSample *buffer, int size, float scale) {
	for (int j=0; j < size; j++) {
		char sign = buffer[j].i >= 0.0f ? '+' : '-';
		printf("%7.4f%c%7.4f \t", buffer[j].r*scale, sign, fabs(buffer[j].i*scale));
		if ((j+1) % 7 == 0) printf("\n\t%4d: ", j+1);	
	}
}

std::string Convolver::BlockPattern::toString() {
	std::stringstream stringStream;
	stringStream << "BlockPattern(";	
	uint32_t numBlocks = 9;
	for(uint32_t i=0; i < numBlocks; i++) {
		stringStream << sizeForTimeBlock(i);
		stringStream << ", ";
	}
	stringStream << "...)";
	
	return stringStream.str();
}


uint32_t Convolver::BlockPattern::sizeForFreqBlock(uint32_t blockNum) {
	return FFT::getFreqDomainSize(this->sizeForTimeBlock(blockNum));
}

#if USE_APPLE_ACCELERATE

namespace Convolver {
	FreqBlock::FreqBlock(uint32_t size)
	: mSize(size), splitComplexNumComplex(size - 1)
	{
		splitComplex = new DSPSplitComplex;
		splitComplex->realp = new float[splitComplexNumComplex]();
		splitComplex->imagp = new float[splitComplexNumComplex]();
	}
	
	FreqBlock::FreqBlock(uint32_t size, DSPSplitComplex *splitComplex)
	: mSize(size), splitComplex(splitComplex), splitComplexNumComplex(size - 1)
	{
		
	}
	
	FreqBlock::~FreqBlock() {
		delete splitComplex->realp;
		delete splitComplex->imagp;
		delete splitComplex;
		
#if DEBUG_MEMORY
		//cout << "FreqBlock::~FreqBlock(" << size() << ")" << endl;			
		numFreqBlocks--;
#endif
	}
	
	
	uint32_t FreqBlock::size() { 
		return mSize; 
	}

	void Convolver::FreqBlock::scale(float by) {
		// FIXME: could use
		/*void vDSP_vsmul (const float input1[],
						 vDSP_Stride stride1,
						 const float * input2,
						 float result[],
						 vDSP_Stride strideResult,
						 vDSP_Length size);
		*/
		for(uint32_t i=0; i < splitComplexNumComplex; i++) {
			splitComplex->realp[i] *= by;
			splitComplex->imagp[i] *= by;
		}
	}
	
}
#else
namespace Convolver {

	FreqBlock::FreqBlock(uint32_t size) : block(size) { 
	}
	
	FreqBlock::~FreqBlock() {
		#if DEBUG_MEMORY
		//cout << "FreqBlock::~FreqBlock(" << size() << ")" << endl;			
		//numFreqBlocks--;
		#endif
	}
	
	uint32_t FreqBlock::size() { 
		return block.size(); 
	}
	

	
	void Convolver::FreqBlock::scale(float by) {
		foreach(FreqSample &sample, this->block) {
			sample.r *= by;
			sample.i *= by;
		}
	}

}
#endif



void Convolver::FreqBlock::print() {
	assert(false);
	/*
	cout << endl << "FreqBlock, size=" << size() << endl;
	printComplex(cArray(), size(), 1.0);
	cout << endl << endl;
	 */
}



void Convolver::TimeBlock::scale(float by) {
	foreach(TimeSample &sample, *this) {
		sample *= by;
	}
}

void Convolver::TimeBlock::accumulate(TimeBlock::iterator start, TimeBlock::iterator end) {
	TimeBlock::iterator us;
	for(us = this->begin(); us != this->end(); ++us) {
		*us += *start;
		start++;
		if (start == end) break;
	}
}

void Convolver::TimeBlock::accumulate(TimeBlock &anotherBlock) {
	uint32_t blockSize = anotherBlock.size();
	assert(size() >= blockSize);
	
	// FIXME: is there a faster (SSE?) way to do this?
	for (uint32_t i=0; i < blockSize; i++) {
		(*this)[i] += anotherBlock[i];
	}	
}
