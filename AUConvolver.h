/*
 *  AUConvolver.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _AUConvolver_h__
#define _AUConvolver_h__

#include "Convolver.h"
#include "AUEffectBase.h"

#include <boost/shared_ptr.hpp>

namespace Convolver {
	class AUConvolver : public Convolver {
	public:
		AUConvolver(AUEffectBase &au, boost::shared_ptr<BlockPattern> blockPattern);
		
		void setFilters(Filters &filters, float stereoSeparation);
		
		virtual void convolve(const AudioBufferList & inBuffer, 
							  AudioBufferList & outBuffer,
							  UInt32 inFramesToProcess,
							  float dryGain,
							  float wetGain);
	protected:
		AUEffectBase &au;

	};
}

#endif