/*
 *  ConvolverFFT.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */


#ifndef _ConvolverFFT_h__
#define _ConvolverFFT_h__

namespace Convolver {
	class FFT;
}

#include "ConvolverTypes.h"
#include "ConvolverFFTType.h"
#include <boost/shared_ptr.hpp>

namespace Convolver {
	class FFT {
	public:
		FFT(uint32_t size);
		~FFT();
		
		boost::shared_ptr<FreqBlock> fftr(const TimeSample *samples, uint32_t numSamples);
		boost::shared_ptr<FreqBlock> fftr(TimeBlock &block);
		boost::shared_ptr<TimeBlock> fftri(FreqBlock &block);
		
		static uint32_t getFreqDomainSize(uint32_t timeDomainSize);
		static uint32_t getTimeDomainSize(uint32_t freqDomainSize);
		
		uint32_t timeDomainSize;	
		uint32_t freqDomainSize;
		float scalar;
		uint32_t sizeLog2n;
	private:
		#if USE_APPLE_ACCELERATE		
		FFTSetup fftSetup; 		
		#else
		kiss_fftr_cfg fftrCfg;	
		kiss_fftr_cfg fftriCfg;	
		#endif
	};
};

#endif