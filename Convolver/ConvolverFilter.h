/*
 *  ConvolverFilter.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverFilter_h__
#define _ConvolverFilter_h__

namespace Convolver {
	class Filter;
}

#include "ConvolverKernel.h"
#include "ConvolvotronProperties.h"
#include "ConvolverSignal.h"

#include "kiss_fftr.h"

#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

typedef kiss_fft_cpx *Buffer;

namespace Convolver {	
	class IR {
	public:
		IR(Kernel &ckernel, boost::shared_ptr<BlockPattern> blockPattern, std::vector<const TimeSample *> &filterSignals, 
		   uint32_t filterSignalLength, bool normalize, std::string description="");
		
		std::vector<boost::shared_ptr<Filter> > &getFilters();
		void setBlockPattern(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern);
	protected:
		IR() {};
		void initialize(Kernel &ckernel, boost::shared_ptr<BlockPattern> blockPattern, std::vector<const float *> &filterSignals, uint32_t filterSignalLength, bool normalize, std::string description);
	private:
		float measureGain();
		
		std::vector<boost::shared_ptr<Filter> > filters;
		float filtersScaledBy;
	};
	
	class UnitIR : public IR {
	public:
		UnitIR(Kernel &kernel, boost::shared_ptr<BlockPattern> blockPattern);
	private:
		static const uint32_t unitIRLength = 5;
	};

	class Filter : public Signal {
	public:
		Filter(Kernel &kernel, boost::shared_ptr<BlockPattern> blockPattern, const TimeSample *samplesTimeDomain, uint32_t numSamples, std::string description="");

		float measureGain();
		
		// FIXME: implement
		const FrequencyResponse *getFrequencyResponse() { return NULL; };
		void setBlockPattern(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern);		
		
		std::string description;
				
	public:
		// DO NOT USE THIS IN CONVOLVER CODE
		const TimeSample *getSamples() { return samples; };	
		uint32_t getNumSamples() { return numSamples; };		
	private:
		// DO NOT USE THESE IN CONVOLVER CODE
		uint32_t numSamples;
		TimeSample *samples;		
	};
	/*
	
	namespace DungHeap {
		class Filter {
		public:		
			~Filter();
			
			void set(const float *filterSignal);
			void print(float scale);
			const FrequencyResponse *getFrequencyResponse();
			
			const float *getSamples();
			uint32_t getNumSamples();
			
			//void setTreatment(bool volumeCompensation, bool reverse);
			void multiplyBy(float factor);
			float measureGain();
			
			std::vector<Buffer> &get();
			
			uint32_t size() { return get().size() * sampleSize; }
			
			float scale;
			std::string description;
		protected:
			Kernel *convolver;
			
			uint32_t unpaddedFilterSignalLength;
			FrequencyResponse *frequencyResponse;		
			float *filterSignal;		
			uint32_t sampleSize;
			
			std::vector<Buffer> rawFilters;
			std::vector<Buffer> treatedFilters;
			
			bool useTreatedFilters;
			bool treatmentNormalized;
			bool treatmentReversed;
			
		private:
			void freeTreatedFilters();		
			Filter(Kernel *convolver, uint32_t filterSignalLength, const float *filterSignal, std::string description="");

			friend class Convolver;
			friend class IR;
		};
	}
	 */
}

#endif
