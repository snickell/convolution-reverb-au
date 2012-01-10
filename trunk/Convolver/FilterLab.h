/*
 *  FilterTester.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 6/5/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "Convolver.h"

#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

namespace Convolver {
	
	class FilterLab {
	public:
		FilterLab(Filter *filter, Kernel &convolver);
		~FilterLab();
		
		void saveTestSignal(const char *filename, float scalar=1.0f);
		void saveResultSignal(const char *filename, float scalar=1.0f);
		void save(const char *filename, const float *buffer, uint32_t size, float scalar=1.0f);
		
		float measureGain();
		float measurePeakGain();
		FrequencyResponse *measureFrequencyResponse(int numFrequencies, float scaleAfter);
		
	private:
		void fillWithWhiteNoise(float *buffer, uint32_t size);
		void fillWithPinkNoise(float *buffer, uint32_t size);
		
		float *loadTestSignal(const char *name, uint32_t size);
		
		double measureEnergy(const float *samples, uint32_t size);
		
		float *testSignal;
		float *testSignalConvolved;
		
		uint32_t filterSize;
		uint32_t bufferSize;
		uint32_t measureAreaOffset;
		uint32_t measureAreaSize;
	};
}