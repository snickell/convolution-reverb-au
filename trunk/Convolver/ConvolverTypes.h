/*
 *  ConvolverTypes.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */
#ifndef _ConvolverTypes_h__
#define _ConvolverTypes_h__

namespace Convolver {
	class FreqBlock;
	class TimeBlock;
}

#include "ConvolverFFTType.h"
#include <vector>
#include <iostream>
#include <boost/array.hpp>
#include <math.h>
#include <boost/shared_ptr.hpp>

// Help GDB out
#if DEBUG
typedef float TimeSample;
#endif

namespace Convolver {
	using std::cout;
	using std::endl;
	using boost::shared_ptr;
	
	typedef float TimeSample;
	
	class BlockPattern {
	public:
		virtual uint32_t sizeForTimeBlock(uint32_t blockNum) = 0;
		virtual uint32_t minimumBlockSize() = 0;
		virtual uint32_t maximumBlockSize() = 0;
		
		// Defaults to using sizeForTimeBlock, converting size w/ FFT
		virtual uint32_t sizeForFreqBlock(uint32_t blockNum);
		
		virtual std::string toString();
	};
	
	class FixedSizeBlockPattern : public BlockPattern {
	public:
		FixedSizeBlockPattern(uint32_t timeBlockSize) : timeBlockSize(timeBlockSize) {};
		
		
		virtual uint32_t minimumBlockSize() { return timeBlockSize; }
		virtual uint32_t maximumBlockSize() { return timeBlockSize; }
		
		virtual uint32_t sizeForTimeBlock(uint32_t blockNum) { return timeBlockSize; };
		uint32_t timeBlockSize;
	};
	
	class TwoSizeBlockPattern : public BlockPattern {
	public:
		TwoSizeBlockPattern(uint32_t smallSize, uint32_t bigSize, uint32_t numSmall) 
		: smallSize(smallSize), bigSize(bigSize), numSmall(numSmall) {};
        
		virtual uint32_t minimumBlockSize() { return smallSize; }
		virtual uint32_t maximumBlockSize() { return bigSize; }
		
		virtual uint32_t sizeForTimeBlock(uint32_t blockNum) { 
			if (blockNum < numSmall) {
				return smallSize;
			} else {
				return bigSize;
			}
            
		};
		uint32_t smallSize;
		uint32_t bigSize;
		uint32_t numSmall;
	};	
    
	class ThreeSizeBlockPattern : public BlockPattern {
	public:
		ThreeSizeBlockPattern(uint32_t smallSize, uint32_t medSize, uint32_t bigSize, uint32_t numSmall, uint32_t numMed) 
		: smallSize(smallSize), medSize(medSize), bigSize(bigSize), numSmall(numSmall), numMed(numMed) {};
		
		virtual uint32_t minimumBlockSize() { return smallSize; }
		virtual uint32_t maximumBlockSize() { return bigSize; }
		
		virtual uint32_t sizeForTimeBlock(uint32_t blockNum) { 
			if (blockNum < numSmall) {
				return smallSize;
			} else if (blockNum < numMed + numSmall) {
				return medSize;
			} else {
				return bigSize;
			}
			
		};
		uint32_t smallSize;
		uint32_t medSize;
		uint32_t bigSize;
		uint32_t numSmall;
		uint32_t numMed;
	};		
	
	class DoublingBlockPattern : public BlockPattern {
	public:
		DoublingBlockPattern(uint32_t startSize, uint32_t endSize) 
		: startSize(startSize), endSize(endSize) {};
		
		virtual uint32_t minimumBlockSize() { return startSize; }
		virtual uint32_t maximumBlockSize() { return endSize; }
		
		virtual uint32_t sizeForTimeBlock(uint32_t blockNum) {
			if (blockNum < 20) {
				uint64_t size = startSize << blockNum / 2;
				if (size < endSize) return size;
			}
			return endSize;
		};
		
		uint32_t startSize;
		uint32_t endSize;
	};		
	
	
	
	class FreqBlock {
	public:
		~FreqBlock();
		
		uint32_t size();
		void scale(float by);
		void print();
		
#if USE_APPLE_ACCELERATE
	public:
		FreqBlock(uint32_t size, DSPSplitComplex *splitComplex);
		FreqBlock(uint32_t size);		
		inline DSPSplitComplex* dspSplitComplex() {
			return splitComplex;
		}
	private:
		uint32_t mSize;
		DSPSplitComplex *splitComplex;
	public:
		uint32_t splitComplexNumComplex;
#else
	public:
		FreqBlock(uint32_t size);
		inline FreqSample* cArrayUnpacked() {
			return &block.front();
		}
	private:
		std::vector<FreqSample> block;
#endif
	};
	
	class TimeBlock : public std::vector<TimeSample> {
	public:
		static uint32_t numTimeBlocks;
		inline void init() {
#if DEBUG_MEMORY
			cout << "TimeBlock::TimeBlock(" << size() << ")" << endl;
			numTimeBlocks++;
#endif
		}
		TimeBlock(uint32_t size) : std::vector<TimeSample>(size) { init(); };
		TimeBlock(uint32_t size, const TimeSample* data, uint32_t dataSize) : std::vector<TimeSample>(size) {
			std::copy(data, data+dataSize, cArray());
			init();
		};
		TimeBlock(const TimeSample* dataStart, const TimeSample* dataEnd) : std::vector<TimeSample>(dataStart, dataEnd) { 
			init();
		}
		
		~TimeBlock() {
#if DEBUG_MEMORY
			cout << "TimeBlock::~TimeBlock(" << size() << ")" << endl;
			numTimeBlocks--;
#endif
		}
        
		inline TimeSample* cArray() {
			return &front();
		}
		
		void scale(float by);
		
		void accumulate(TimeBlock &anotherBlock);
		void accumulate(TimeBlock::iterator start, TimeBlock::iterator end);	
	};
	
	// Allow pointers to stack allocated objects by having smart_ptr<T> foo(&stackThing, nodelete())
	struct nodelete {
		template <typename T>
		void operator()( T * ) {}
	};
};

#endif