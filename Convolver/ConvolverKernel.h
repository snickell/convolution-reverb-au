/*
 *  ConvolverKernel.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverKernel_h__
#define _ConvolverKernel_h__

namespace Convolver {
	class Kernel;
}

#include "ConvolverTypes.h"
#include "ConvolverState.h"
#include "ConvolverFilter.h"
#include "ConvolverSignal.h"
#include "ConvolverFFT.h"

#include <list>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/timer.hpp>

namespace Convolver {
	using boost::shared_ptr;
	
	class Kernel {
	public:
		Kernel(shared_ptr<BlockPattern> blockPattern = shared_ptr<BlockPattern>());
		~Kernel();
		
		void setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern);
		
		// Easy to use function: inFrame -> outFrame
		shared_ptr<TimeBlock> convolve(shared_ptr<TimeBlock> &frame, Filter &filter, State &state);
		
		void schedule_convolution(Filter &filter, State &state);		
		// More sophisticated function designed for manual handling of channel state
		void process_convolutions(State &state, bool useThread=false);
		
				
		FFT &getFFT(uint32_t timeDomainSize);
		FFT &getFFTI(uint32_t freqDomainSize);
		
		static float *zeroPad(const float *signal, uint32_t signalLength, uint32_t targetLength);
		
	protected:
		// Thread-safe function, not for general use, for use by State threads
		void convolve(State &state, TimeBlock &block, FrameRequest &request, FrameNum currentFrameNum, bool lockAccumulators=false);
		friend class State;
		
	private:
		inline void convolveAccumulate(FreqBlock &input, FreqBlock &filter, FreqBlock &accumulator/*__restrict__ FreqSample* accumulator*/, int numSamples);
		boost::timer timer;
		std::map<uint32_t, boost::shared_ptr<FFT> > fftSizeToFFT;
		
		// FIXME: we're hardcoded to two block sizes here, that sortof sucks
		uint32_t sizeZero;
		uint32_t sizeOne;
	};
};


#endif