/*
 *  ConvolverFFT.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverFFT.h"
#include "ConvolverInternal.h"
#include <math.h>

using std::vector;
using std::cout;
using std::endl;
using boost::shared_ptr;

static void printComplex(Convolver::FreqSample *buffer, int size, float scale) {
	for (int j=0; j < size; j++) {
		char sign = buffer[j].i >= 0.0f ? '+' : '-';
		printf("%7.4f%c%7.4f \t", buffer[j].r*scale, sign, fabs(buffer[j].i*scale));
		if ((j+1) % 7 == 0) printf("\n\t%4d: ", j+1);	
	}
}

#if USE_APPLE_ACCELERATE


Convolver::FFT::FFT(uint32_t size) 
: timeDomainSize(size), freqDomainSize(getFreqDomainSize(size)), scalar(1.0f / sqrt(timeDomainSize))
{
	bool isPowerOfTwo = (size & (size - 1)) == 0;
	if (!isPowerOfTwo) {
		cerr << "FFT size must be a power of two, try changing the block size" << endl;
		assert(isPowerOfTwo);
	}
	
	this->sizeLog2n = (uint32_t)round(log2(size));
	
	assert((2 << (this->sizeLog2n - 1)) == size);
	
	this->fftSetup = vDSP_create_fftsetup(sizeLog2n, kFFTRadix2);
}

Convolver::FFT::~FFT() {
	vDSP_destroy_fftsetup(this->fftSetup);
}

namespace Convolver {
	
	static DSPSplitComplex *doFFT(FFTSetup setup, const TimeSample *data, uint32_t numSamples, uint32_t sizeLog2n) {
		assert(numSamples % 2 == 0);
		uint32_t numComplexSamples = numSamples / 2;
		assert(sizeof(TimeSample) == sizeof(float));
		
		DSPSplitComplex *input = new DSPSplitComplex;
		input->realp = new float[numComplexSamples];
		input->imagp = new float[numComplexSamples];
		
		// FIXME: is this cast to const DSPComplex * valid ??? Is that really how it expects input data?
		/* Split the complex (interleaved) data into two arrays */
		vDSP_ctoz((const DSPComplex *)data,2,input,1,numComplexSamples);
		
		vDSP_fft_zrip(setup, input, 1, sizeLog2n, FFT_FORWARD);
		
		/* Unpack the end */
		/*output[numSamples-2] = output[1];
		output[numSamples-1] = 0.0f;
		output[1] = 0.0f;*/
		
		return input;
	}

	static void doFFTI(FFTSetup setup, TimeBlock &outBlock, FreqBlock &inBlock, uint32_t sizeLog2n) {
		uint32_t numSamples = outBlock.size();
		assert(numSamples % 2 == 0);
		uint32_t numComplexSamples = numSamples / 2;
		assert(sizeof(TimeSample) == sizeof(float));
		
		DSPSplitComplex *input = inBlock.dspSplitComplex();
		vDSP_fft_zrip(setup, input, 1, sizeLog2n, FFT_INVERSE);
		
		vDSP_ztoc(input,1,(DSPComplex *)outBlock.cArray(),2,numComplexSamples);
	}

}

shared_ptr<Convolver::FreqBlock> Convolver::FFT::fftr(const TimeSample *samples, uint32_t numSamples) {
	assert(numSamples == timeDomainSize);
#if DEBUG_CONVOLVE	
	cout << "FFT::fftr(): timeDomainSize=" << timeDomainSize << ", freqDomainSize=" << freqDomainSize << endl;
#endif
	
	DSPSplitComplex *splitComplex = doFFT(this->fftSetup, samples, numSamples, sizeLog2n);
	shared_ptr<FreqBlock> outBlock(new FreqBlock(freqDomainSize, splitComplex));
	
	// FIXME: we scale here because the filter needs it, but the filter should really call this
	outBlock->scale(scalar*scalar);	
	
	return outBlock;
}

shared_ptr<Convolver::FreqBlock> Convolver::FFT::fftr(TimeBlock &inBlock) {
	assert(inBlock.size() == timeDomainSize);
#if DEBUG_CONVOLVE
	cout << "\t\t\tDoing FFT...size " << timeDomainSize << endl;	
#endif
	
	DSPSplitComplex *splitComplex = doFFT(this->fftSetup, inBlock.cArray(), inBlock.size(), sizeLog2n);
	shared_ptr<FreqBlock> outBlock(new FreqBlock(freqDomainSize, splitComplex));
	
	// kiss_fftr(fftrCfg, inBlock.cArray(), outBlock->cArray());
	
	return outBlock;
}

shared_ptr<Convolver::TimeBlock> Convolver::FFT::fftri(FreqBlock &inBlock) {
	assert(inBlock.size() == freqDomainSize);
#if DEBUG_CONVOLVE
	cout << "\t\t\tDoing FFTI...size " << timeDomainSize << endl;	
#endif	
	shared_ptr<TimeBlock> outBlock(new TimeBlock(timeDomainSize));
	
	doFFTI(this->fftSetup, *outBlock, inBlock, sizeLog2n);
	
	// kiss_fftri(fftriCfg, inBlock.cArray(), outBlock->cArray());
	
	return outBlock;
}

uint32_t Convolver::FFT::getFreqDomainSize(uint32_t timeDomainSize) {
	return timeDomainSize / 2 + 1;
}

uint32_t Convolver::FFT::getTimeDomainSize(uint32_t freqDomainSize) {
	return (freqDomainSize - 1) * 2;
}



#else

Convolver::FFT::FFT(uint32_t size) 
	: timeDomainSize(size), freqDomainSize(getFreqDomainSize(size)), scalar(1.0f / sqrt(timeDomainSize))
{
	this->fftrCfg = kiss_fftr_alloc(size, 0, NULL, NULL);
	this->fftriCfg = kiss_fftr_alloc(size, 1, NULL, NULL);
}

Convolver::FFT::~FFT()
{
	kiss_fftr_free(this->fftrCfg);
	kiss_fftr_free(this->fftriCfg);
}

shared_ptr<Convolver::FreqBlock> Convolver::FFT::fftr(const TimeSample *samples, uint32_t numSamples) {
	assert(numSamples == timeDomainSize);
#if DEBUG_CONVOLVE	
	cout << "FFT::fftr(): timeDomainSize=" << timeDomainSize << ", freqDomainSize=" << freqDomainSize << endl;
#endif
	shared_ptr<FreqBlock> outBlock(new FreqBlock(freqDomainSize));
	
	kiss_fftr(fftrCfg, samples, outBlock->cArrayUnpacked());
	
	// FIXME: debug scaling
	outBlock->scale(scalar*scalar);	
	
	return outBlock;
}

shared_ptr<Convolver::FreqBlock> Convolver::FFT::fftr(TimeBlock &inBlock) {
	assert(inBlock.size() == timeDomainSize);
#if DEBUG_CONVOLVE
	cout << "\t\t\tDoing FFT...size " << timeDomainSize << endl;	
#endif
	shared_ptr<FreqBlock> outBlock(new FreqBlock(freqDomainSize));

	kiss_fftr(fftrCfg, inBlock.cArray(), outBlock->cArrayUnpacked());

	// FIXME: debug scaling
	//outBlock->scale(scalar);		
	
	return outBlock;
}

shared_ptr<Convolver::TimeBlock> Convolver::FFT::fftri(FreqBlock &inBlock) {
	assert(inBlock.size() == freqDomainSize);
#if DEBUG_CONVOLVE
	cout << "\t\t\tDoing FFTI...size " << timeDomainSize << endl;	
#endif	
	shared_ptr<TimeBlock> outBlock(new TimeBlock(timeDomainSize));
	
	kiss_fftri(fftriCfg, inBlock.cArrayUnpacked(), outBlock->cArray());
	
	// FIXME: debug scaling
	//outBlock->scale(scalar);		
	
	return outBlock;
}

uint32_t Convolver::FFT::getFreqDomainSize(uint32_t timeDomainSize) {
	return timeDomainSize / 2 + 1;
}

uint32_t Convolver::FFT::getTimeDomainSize(uint32_t freqDomainSize) {
	return (freqDomainSize - 1) * 2;
}

#endif
