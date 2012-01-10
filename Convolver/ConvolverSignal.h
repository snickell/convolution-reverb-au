/*
 *  ConvolverSignal.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverSignal_h__
#define _ConvolverSignal_h__

namespace Convolver {
	class Signal;
	class Kernel;
}

#include "ConvolverTypes.h"
#include <vector>
#include <boost/shared_ptr.hpp>

namespace Convolver {
	class Signal {
	public:
		Signal(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern, 
			   const TimeSample *samplesTimeDomain, uint32_t numSamples);
		~Signal();

		void			modify_Scale	(float factor);
		boost::shared_ptr<BlockPattern> &getBlockPattern() { return blockPattern; }
		virtual void	print		(float scale);
		virtual std::vector< boost::shared_ptr<FreqBlock> > &getBlocks();
		virtual uint32_t getTimeSize() { return timeSize; }
		
	protected:
		void initialize(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern, 
						const TimeSample *samplesTimeDomain, uint32_t numSamples);
		
		
		std::vector< boost::shared_ptr<FreqBlock> > blocks;
		std::vector< boost::shared_ptr<FreqBlock> > rawBlocks;		
		
		boost::shared_ptr<BlockPattern> blockPattern;
		
		uint32_t timeSize;
	};
};

// Avoid circularity
#include "ConvolverKernel.h"

#endif