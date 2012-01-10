/*
 *  ConvolverFilter.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverFilter.h"
#include "ConvolverInternal.h"

#include "FilterLab.h"


#define bufferSize sampleSize + 1
#define fftSize sampleSize * 2

using std::vector;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using std::cout;
using std::endl;
using std::cerr;

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

namespace Convolver {

	IR::IR(Kernel &kernel, shared_ptr<BlockPattern> blockPattern, std::vector<const float *> &filterSignals, 
		   uint32_t filterSignalLength, bool normalize, std::string description)
	{
		initialize(kernel, blockPattern, filterSignals, filterSignalLength, normalize, description);
	}
		
	void IR::setBlockPattern(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern) {
#if DEBUG
		cout << "IR::setBlockPattern(): scaling filters by " << filtersScaledBy << endl;
#endif		
		foreach(shared_ptr<Filter> &filter, filters) {
			filter->setBlockPattern(kernel, blockPattern);
			filter->modify_Scale(filtersScaledBy);
		}
	}
	
	void IR::initialize(Kernel &kernel, shared_ptr<BlockPattern> blockPattern, std::vector<const TimeSample *> &filterSignals, 
						uint32_t filterSignalLength, bool normalize, std::string description)
	{
		#if DEBUG
		cout << "IR::IR(): " << filterSignals.size() << " channels of " << filterSignalLength << " samples" << endl;
		#endif
		
		filters.reserve(filterSignals.size());
		
		float maxGain = 0.0f;
		
		uint32_t filterSignalsSize = filterSignals.size();
		for(uint32_t channelNum=0; channelNum < filterSignalsSize; channelNum++) {
			const TimeSample *filterSignal = filterSignals[channelNum];

			std::stringstream channelStringStream;
			channelStringStream << channelNum;
			std::string channelDescription = description + "[" + channelStringStream.str() + "]";
			
			shared_ptr<Filter> filterPtr(new Filter(kernel, blockPattern, filterSignal, filterSignalLength, channelDescription));
			filters.push_back(filterPtr);

			if (normalize) {
				// Find the channel with the highest gain
				float gain = filterPtr->measureGain();
				if (gain > maxGain) maxGain = gain;
			}
		}

		if (normalize) {
			filtersScaledBy = 1.0f / maxGain;
		
			#if DEBUG
			cout << "IR::IR(): scaling filters by " << filtersScaledBy << endl;
			#endif		
		
			foreach(shared_ptr<Filter> filter, filters) {
				filter->modify_Scale(filtersScaledBy);
			}
		} else {
			filtersScaledBy = 1.0f;
		}
	}

	vector<shared_ptr<Filter> > &IR::getFilters()
	{
		return filters;
	}

	UnitIR::UnitIR(Kernel &kernel, boost::shared_ptr<BlockPattern> blockPattern) {
		// We initialize with the unit impulse
		float *unitFilterBuffer = new float[unitIRLength]();
		unitFilterBuffer[0] = 1.0f;

		std::vector<const float *> signals(1, unitFilterBuffer);
		
		bool normalize = true;
		initialize(kernel, blockPattern, signals, unitIRLength, true, "UnitImpulse");
		
		delete[] unitFilterBuffer;
	}
	
	
	
	Filter::Filter(Kernel &kernel, boost::shared_ptr<BlockPattern> blockPattern, const float *samplesTimeDomain, uint32_t numSamples, std::string description)
		: Signal(kernel, blockPattern, samplesTimeDomain, numSamples), description(description), numSamples(numSamples)
	{
		samples = new TimeSample[numSamples];
		std::copy(samplesTimeDomain, samplesTimeDomain + numSamples, samples);
	}
	
	void Filter::setBlockPattern(Kernel &kernel, boost::shared_ptr<BlockPattern> &blockPattern) {
		Signal::initialize(kernel, blockPattern, samples, numSamples);
	}
	
	float Filter::measureGain() {
		Kernel labConvolver(blockPattern);
		FilterLab lab(this, labConvolver);
		
		float avgGain = lab.measureGain();
		float peakGain = lab.measurePeakGain();
		
		// Use whichever gain is larger, reduce chance of clipping
		float gain = std::max(avgGain, peakGain);	
		
		// FIXME: we aren't measuring the frequency response
		//frequencyResponse = lab.measureFrequencyResponse(kMyNumberOfResponseFrequencies, scale);
				
		return gain;
	}
}

#if OLD_CODE

namespace Convolver {
	namespace DungHeap {
		
		Filter::Filter(Kernel *convolver, uint32_t filterSignalLength, const float *filterSignal, std::string description) 
			: description(description), convolver(convolver), unpaddedFilterSignalLength(filterSignalLength), 
			  frequencyResponse(NULL), filterSignal(NULL), useTreatedFilters(false)
		{
			int numFilters = (int)ceil((float)filterSignalLength / sampleSize);
			
			for(int i=0; i < numFilters; i++) {
				Buffer filter = new kiss_fft_cpx[this->bufferSize]();
				this->rawFilters.push_back(filter);
			}
			
			this->filterSignal = new float[unpaddedFilterSignalLength];		
			
			this->set(filterSignal);
			
		#if DEBUG_PRINT_FILTER
			cout << endl << "Raw filter:" << endl;
			this->print(1.0f);
			cout << endl << endl;
		#endif

		}


		Filter::~Filter() {
		#if DEBUG_MEMORY
			cout << "Convolver::Filter::~Filter()" << endl;
		#endif
			
			for(vector<Buffer>::iterator i = rawFilters.begin(); i != rawFilters.end(); ++i) {
				delete *i;
			}
			freeTreatedFilters();
			
			delete frequencyResponse;
			delete filterSignal;
		}

		void Filter::freeTreatedFilters() {
		#if DEBUG_MEMORY
			cout << "Convolver::Filter::freeTreatedFilters()" << endl;
		#endif
			
			for(vector<Buffer>::iterator i = treatedFilters.begin(); i != treatedFilters.end(); ++i) {
				delete[] *i;
			}
			treatedFilters.erase(treatedFilters.begin(), treatedFilters.end());
		}

		/*
		 void Convolver::Filter::setTreatment(bool normalize, bool reverse) {
		 bool useTreatedFilters = normalize || reverse;	
		 bool theresChange = (this->treamentNormalized != normalize) || (this->treatmentReversed != reverse);
		 
		 if (theresChange) {
		 this->useTreatedFilters = useTreatedFilters;
		 
		 freeTreatedFilters();
		 
		 if (useTreatedFilters) {
		 // Treatments are supposed to be on
		 }
		 
		 
		 }
		 }
		 */

		void Filter::print(float scale) {
			printf("Contents of filter:\n");
			int count=0;
			vector<Buffer> &filters = this->get();
			
			for(vector<Buffer>::iterator i = filters.begin(); i != filters.end(); ++i) {
				printf("\tFreq Domain sub-filter #%d:\n\t   0: ", count++);
				Buffer filter = *i;
				for (uint32_t j=0; j < bufferSize; j++) {
					char sign = filter[j].i >= 0.0f ? '+' : '-';
					printf("%5.2f%c%5.2f \t", filter[j].r*scale, sign, fabs(filter[j].i*scale));
					if ((j+1) % 10 == 0) printf("\n\t%4d: ", j+1);	
				}
				printf("\n");
			}
			printf("\n\n=============================\n\n\n\n\n\n");	
		}

		void Filter::set(const float *filterSignal)
		{
			memcpy(this->filterSignal, filterSignal, unpaddedFilterSignalLength * sizeof(float));
			
			uint32_t filterSignalLength = this->rawFilters.size() * this->sampleSize;
			
			/*
			 printf("here's the filter:\n\t");
			 for (int i=0; i < this->unpaddedFilterSignalLength; i++) {
			 printf("%6.2f\t", this->filterSignal[i]);
			 if (i % 10 == 0) printf("\n\t");	
			 }
			 printf("\n\n");
			 */
			
			// Pad the filter out to the proper length
			float *paddedFilter = Kernel::zeroPad(filterSignal, this->unpaddedFilterSignalLength, filterSignalLength);
			
			kiss_fftr_cfg fftrCfg = kiss_fftr_alloc(fftSize, 0, NULL, NULL);
			
			// Partition the padded filter
			for(uint32_t i=0; i < rawFilters.size(); i++) {
				// Now zero pad the end of each filter, so they "ring out" in convolution
				float *paddedSubFilter = Kernel::zeroPad(&paddedFilter[i*this->sampleSize], this->sampleSize, this->fftSize);
				
				/*
				 printf("Time domain sub-filter #%d:\n   0:", i);
				 for (int j=0; j < this->fftSize; j++) {
				 printf("%6.2f \t", paddedSubFilter[j]);
				 if ((j+1) % 10 == 0) printf("\n%4d: ", j+1);	
				 }
				 printf("\n\n\n");
				 */
				kiss_fftr(fftrCfg, paddedSubFilter, this->rawFilters[i]);
				
				delete paddedSubFilter;
			}
			
			kiss_fftr_free(fftrCfg);
			
			delete paddedFilter;
		}

		vector<Buffer> &Filter::get() {
			if (this->useTreatedFilters) {
				return this->treatedFilters;
			} else {
				return this->rawFilters;
			}
		}
			
		float Filter::measureGain() {
			bool wasUsingTreated = this->useTreatedFilters;
			
			this->useTreatedFilters = false;	
			
			uint32_t filterSize = rawFilters.size() * sampleSize;
			Kernel labConvolver(sampleSize);
			FilterLab lab(this, labConvolver, sampleSize, filterSize);
			
			float avgGain = lab.measureGain();
			float peakGain = lab.measurePeakGain();
			
			// Use whichever gain is larger, reduce chance of clipping
			float gain = std::max(avgGain, peakGain);	
			
			// FIXME: we aren't measuring the frequency response
			//frequencyResponse = lab.measureFrequencyResponse(kMyNumberOfResponseFrequencies, scale);
			
			this->useTreatedFilters = wasUsingTreated;
			
			return gain;
		}

		// Create a new treated filter with raw multiplied by scale, turn on treated filters
		// FIXME: this should really start from the current filter, but that's a little hard
		void Filter::multiplyBy(float scale) {	
			freeTreatedFilters();
			treatedFilters.reserve(rawFilters.size());
			
			for(vector<Buffer>::iterator i = rawFilters.begin(); i != rawFilters.end(); ++i) {
				Buffer filter = *i;
				
				Buffer normalizedFilter = new kiss_fft_cpx[this->bufferSize]();
				
				for (uint32_t i=0; i < bufferSize; i++) {		
					normalizedFilter[i].r = filter[i].r * scale;
					normalizedFilter[i].i = filter[i].i * scale;
				}
				
				this->treatedFilters.push_back(normalizedFilter);
			}
			assert(treatedFilters.size() == rawFilters.size());	
			this->useTreatedFilters = true;
		}

		const float *Filter::getSamples() {
			/*
			 uint32_t filterSignalLength = this->getNumSamples();
			 
			 float *filter = new float[filterSignalLength];
			 float *filterPtr = filter;
			 
			 for(vector<Buffer>::iterator i = filters.begin(); i != filters.end(); ++i) {
			 memcpy(filterPtr, *i, this->sampleSize);
			 filterPtr += sampleSize;
			 }*/
			return filterSignal;
		}

		uint32_t Filter::getNumSamples() {
			//return filters.size() * this->sampleSize;
			return unpaddedFilterSignalLength;
		}

		const FrequencyResponse *Filter::getFrequencyResponse() {
			return frequencyResponse;
		}

	}
}

#endif