/*
 *  ConvolverKernel.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/7/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverKernel.h"

#include "SSEConvolution.h"

#include <math.h>
#include <iostream>
#include <stdio.h>
#include <boost/shared_ptr.hpp>

using std::endl;
using std::cout;
using std::vector;
using std::list;
using std::map;
using boost::shared_ptr;

#define USE_SSE3 1

#include "ConvolverInternal.h"
#include "ConvolverState.h"

#include <boost/unordered_map.hpp>
#include <boost/circular_buffer.hpp>
#include <deque>

using std::deque;
using boost::circular_buffer;

namespace Convolver {

	
	static uint32_t numTicks = 0;
	static time_t t1 = 0;
	static time_t t2;
	uint32_t lastInputBlockSize = 0;
	
	uint32_t testCount[2] = {0, 0};
	uint32_t numUnderruns = 0;
	

	shared_ptr<TimeBlock>  Kernel::convolve(shared_ptr<TimeBlock> &frame, Filter &filter, State &state) {
		state.push(frame);
		

		schedule_convolution(filter, state);
		
		bool useThread = false;		
		process_convolutions(state, useThread);
		
		return state.pop(frame->size());
	}

	void Kernel::schedule_convolution(Filter &filter, State &state) {
		vector< boost::shared_ptr<FreqBlock> > &blocks = filter.getBlocks();
		uint32_t numBlocks = blocks.size();
		
		// Only do big blocks if we're at the start of a state
		bool doSizeOneBlocks = state.count == 0;
		uint32_t timeFrameSize = state.getFrameSize();		
		FrameNum currentFrameNum = state.getCurrentFrameNum();
		FrameRequests &frameRequests = *state.frameRequests;
		
		uint32_t delay = 0;
		for(uint32_t i=0; i < numBlocks; i++) {
			//if (useThread && !accumulatorsLocked) accumulatorsLocked = state.tryToLockAccumulators("Kernel::convolve()");
			
			boost::shared_ptr<FreqBlock> block = blocks[i];
			
			if ((block->size() == sizeOne)) {
				if(!doSizeOneBlocks)
					break;
			}
			
			uint32_t numSamplesInBlock = FFT::getTimeDomainSize(block->size()) / 2;
			
			if ((numSamplesInBlock % timeFrameSize) > 0) {
				cout << "Block size " << numSamplesInBlock << " is not a multiple of " << timeFrameSize << endl;
				assert(numSamplesInBlock % timeFrameSize == 0);
			}
			
			uint32_t numFrames = numSamplesInBlock / timeFrameSize;		
			FrameNum endFrame = currentFrameNum + numFrames;
			
			shared_ptr<FrameRequest> frameRequest(frameRequests.request(currentFrameNum, endFrame));
			FrameNum outputConvolutionStartingAt = currentFrameNum + delay;
			
#if DEBUG_CONVOLVE
			cout << "\t\tscheduling ConvolutionOp (" /*<< (uint32_t)&(*convolutionOp)*/ << "): F" << i << " x [" << currentFrameNum << ", " << endFrame << "]"; 
			cout << " -> " << outputConvolutionStartingAt << endl;			
#endif			
			frameRequest->lazilyConvolveWith(block, outputConvolutionStartingAt);
			
			delay += numFrames;
		}
	}
	
	void Kernel::process_convolutions(State &state, bool useThread) {
		bool accumulatorsLocked = false;
		if (useThread && !accumulatorsLocked) accumulatorsLocked = state.tryToLockAccumulators("Kernel::convolve()");
		
		// Compute the sizes of different frame types
		uint32_t timeFrameSize = state.getFrameSize();
		
		#if DEBUG_CONVOLVE_STATS
		numTicks++;
		time(&t2);
		if (t2 - t1 >= 1) {
			t1 = t2;
			cout << "Frames/s = " << numTicks * timeFrameSize << endl;
			cout << "convolveAccumulates on frame size " << sizeZero << ": " << testCount[0] << endl;
			cout << "convolveAccumulates on frame size " << sizeOne << ": " << testCount[1] << endl;
			cout << "last input block size " << lastInputBlockSize << endl;
			cout << "number of underruns " << numUnderruns << endl;
			cout << endl;
			testCount[0] = 0;
			testCount[1] = 0;
			numTicks = 0;
			numUnderruns = 0;
		}
		#endif		
		
		FrameBuffer &frameBuffer = *state.frameBuffer;
		FrameRequests &frameRequests = *state.frameRequests;
		
		// Schedule the input to be convolved with the filter blocks
		FrameNum currentFrameNum = state.getCurrentFrameNum();
		
#if DEBUG_CONVOLVE
		cout << endl << "PROCESSING FRAME: " << currentFrameNum << endl;
		cout << "\tScheduling:" << endl;
#endif
		
		bool doSizeOneBlocks = false;
		if (state.count == 0) {
			doSizeOneBlocks = true;
			state.count++;
		} else if (state.count == (sizeOne-1)/(sizeZero-1) - 1) {
			state.count = 0;
		} else {
			state.count++;
		}

		list<shared_ptr<FrameRequest> >	requests = frameRequests.advanceToFrame(currentFrameNum);

		if (useThread && !accumulatorsLocked) {
			state.lockAccumulators("Kernel::convolve()");
			accumulatorsLocked = true;
		}
		
		#if DEBUG_CONVOLVE
		cout << "\tProcessing " << requests.size() << " requests for frame: " << currentFrameNum << endl;
		#endif	
		bool workForWorkerThread = false;
		
		// Process due frame requests
		foreach(shared_ptr<FrameRequest> frameRequest, requests)  {
			#if DEBUG_CONVOLVE
			cout << "\t\tprocessing requests for [" << frameRequest->id.first << ", " << frameRequest->id.second << "], " << endl;
			#endif
			bool padBuffer = true;
			shared_ptr<TimeBlock> frames = frameBuffer.fulfill(*frameRequest, timeFrameSize, padBuffer);
			uint32_t frameSize = frames->size();

			if(useThread && FFT::getFreqDomainSize(frameSize) == sizeOne) {
				#if DEBUG_CONVOLVE
					cout << "\t\t\tenqueing work item" << endl;
				#endif
				//cout << "Adding work item" << endl;
				State::WorkItem *item = new State::WorkItem(frames, frameRequest, currentFrameNum);
				bool dataAdded = state.queueFrameRequestForWorkThread(item);
				if (!dataAdded) {
					cerr << "QUEUE WAS FULL" << endl;
					assert(false);
				}
				workForWorkerThread = true;
			} else {
				bool lockAccumulators = false;
				convolve(state, *frames, *frameRequest, currentFrameNum, lockAccumulators);
			}
		}
		
		FrameNum endFrame = frameRequests.getOldestFrameRequested();
		#if DEBUG_CONVOLVE
		cout << "\tFlushing frames before: " << endFrame << endl;		
		#endif
		frameBuffer.flushFramesBefore(endFrame);
		#if DEBUG_CONVOLVE	
		cout << "\tDone Convolving, returning" << endl;
		#endif
		state.setSignalThreadWorkAvailable(workForWorkerThread);
		state.setReleaseAccumulatorLockOnPop(accumulatorsLocked);
	}
	
	
	
	void Kernel::convolve(State &state, TimeBlock &timeBlock, FrameRequest &frameRequest, FrameNum currentFrameNum, bool inWorkThread) {
		uint32_t frameSize = timeBlock.size();
		
		FFT &fft = getFFT(frameSize);
		shared_ptr<FreqBlock> signalBlockPtr = fft.fftr(timeBlock);
		FreqBlock &signalBlock = *signalBlockPtr;
		uint32_t signalBlockSize = signalBlock.size();
		
		// Perform each ConvolutionOp
		foreach(ConvolutionOp &op, frameRequest.getLazyConvolutions()) {
			FreqBlock &filterBlock = *op.block;
			
			uint32_t outputConvolutionFramesFromNow = op.outputConvolutionStartingAt - currentFrameNum;
			
			if (inWorkThread) state.lockAccumulators("Kernel::convolve(thread)"); {
				FrameNum currentFrame = state.getCurrentFrameNum();
				if (currentFrame > op.outputConvolutionStartingAt) {
					state.alertUnderrun();
					if (inWorkThread) state.unlockAccumulators("Kernel::convolve(thread) underrun");
					break;
				}
				
				FreqBlock *accumulator = state.getAccumulator(outputConvolutionFramesFromNow, signalBlockSize);
			
				#if DEBUG_CONVOLVE
				cout << "\t\t\tdoing ConvolutionOp (" << (uint32_t)&(op) << "): outputting at frame " << op.outputConvolutionStartingAt << " from Accumulator(" << (uint32_t)&(*accumulator) << ")" << endl;				
				#endif				
			
				assert(signalBlock.size() == filterBlock.size());
				assert(filterBlock.size() == accumulator->size());
			
			
				convolveAccumulate(signalBlock, filterBlock, *accumulator, signalBlockSize);

				if (inWorkThread) {
					int numFramesLeft = --state.workThreadFrameStatus[op.outputConvolutionStartingAt];
					assert(numFramesLeft >= 0);
					
					if (currentFrame == op.outputConvolutionStartingAt && numFramesLeft == 0) {
						state.sorryFinallydoneWithCurrentFrame();
					}					
				}
			} if (inWorkThread) state.unlockAccumulators("Kernel::convolve(thread)");
		}
	}
		
}

static void printComplex(Buffer buffer, int size, float scale) {
	for (int j=0; j < size; j++) {
		char sign = buffer[j].i >= 0.0f ? '+' : '-';
		printf("%7.4f%c%7.4f \t", buffer[j].r*scale, sign, fabs(buffer[j].i*scale));
		if ((j+1) % 7 == 0) printf("\n\t%4d: ", j+1);	
	}
}

#include <typeinfo>

void Convolver::Kernel::setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern) {
	if (blockPattern != NULL) {
		if (dynamic_cast<TwoSizeBlockPattern *>(&*blockPattern) != NULL) {
#if DEBUG
			cout << "Kernel::Kernel() Its a twoSizeBlockPattern" << endl;
#endif
			sizeZero = FFT::getFreqDomainSize(blockPattern->minimumBlockSize()*2);
			sizeOne = FFT::getFreqDomainSize(blockPattern->maximumBlockSize()*2);
		} else if (dynamic_cast<FixedSizeBlockPattern *> (&*blockPattern) != NULL) {
#if DEBUG
			cout << "Kernel::Kernel() its a FixedSizeBlockPattern" << endl;
#endif
			sizeZero = FFT::getFreqDomainSize(blockPattern->minimumBlockSize()*2);
			sizeOne = 0;
		} else {
			// We don't support other patterns, yet.
			assert(false);
		}
	} else {
		sizeZero = 0;
		sizeOne = 0;
	}	
}


Convolver::Kernel::Kernel(shared_ptr<BlockPattern> blockPattern)
{
	setBlockPattern(blockPattern);
	
#if DEBUG_MEMORY
	cout << "Kernel::Kernel(): allocating" << endl;
#endif
	timer.restart();
	
#if DEBUG
	cout << "Kernel::Kernel(): ";
#if USE_SSE3
	cout << "SSE3 enabled" << endl;
#else
	cout << "SSE3 disabled" << endl;
#endif
#endif
}

Convolver::Kernel::~Kernel() {
#if DEBUG_MEMORY
	cout << "Kernel::~Kernel()" << endl;
#endif
}


float * Convolver::Kernel::zeroPad(const float *signal, uint32_t signalLength, uint32_t targetLength) {
	assert(targetLength >= signalLength);
	float *output = new float[targetLength];
	memcpy(output, signal, signalLength * sizeof(float));
	if (signalLength != targetLength) {
		memset(&output[signalLength], 0, (targetLength - signalLength) * sizeof(float));
	}
	
	return output;
}

namespace Convolver {
#if USE_APPLE_ACCELERATE
	
	inline static void convolveAccumulateAppleAccelerate(DSPSplitComplex *input, DSPSplitComplex *filter, 
														 DSPSplitComplex *accumulator, int numComplex) {
		vDSP_zvcma(input, 1, filter, 1, accumulator, 1, accumulator, 1, numComplex);
		//vDSP_zvadd(input, 1, accumulator, 1, accumulator, 1, numComplex);
		//accumulator->realp[30] = 1.0;
	}
	
#else
	
    typedef FreqSample * FreqSamplePtr;
    
inline static void convolveAccumulateSSE3(const FreqSample* input, const FreqSample* filter, __restrict__ FreqSamplePtr accumulator, int numSamples) {
	int startAt;
	
	
#if USE_SSE3
	// FIXME: do we have an off-by-one error here?
	startAt = multiply_complex_SSE3((float*)input, (float*)filter, (float*)accumulator, numSamples);
#else
	startAt = 0;
#endif
	
	// Finish off samples not done by vector unit
	for (int i=startAt; i < numSamples; i++) {
		accumulator[i].r += input[i].r * filter[i].r - input[i].i * filter[i].i;
		accumulator[i].i += input[i].r * filter[i].i + input[i].i * filter[i].r;
	}
	
	
	// FIXME: debug, compare SSE to non-SSE
	// Finish off samples not done by vector unit
	/*
	 for (int i=0; i < size - 1; i++) {
	 if ((fabs(accumulatorSSE[i].r - accumulator[i].r) > 0.0001) || (fabs(accumulatorSSE[i].i - accumulator[i].i) > 0.0001)) {
	 printf("%11.8f+%11.8f * %11.8f+%11.8f = \t\t%7.4f+%7.4f\t\t%7.4f+%7.4f\n", input[i].r, input[i].i, filter[i].r, filter[i].i, accumulatorSSE[i].r, accumulatorSSE[i].i, accumulator[i].r, accumulator[i].i);
	 printf ("MISMATCH!\n");
	 }
	 }	
	 printf("\n\n");*/
}
	
#endif
	
}


void Convolver::Kernel::convolveAccumulate(FreqBlock &input, FreqBlock &filter, FreqBlock &accumulator/*__restrict__ FreqSample* accumulator*/, int numSamples)
{
	#if DEBUG_CONVOLVE_STATS
	if (numSamples == sizeZero) {
			testCount[0]++;
	} else if (numSamples == sizeOne) {
			testCount[1]++;
	}
	#endif
	
#if USE_APPLE_ACCELERATE
	assert(input.splitComplexNumComplex == numSamples - 1);
	convolveAccumulateAppleAccelerate(input.dspSplitComplex(), filter.dspSplitComplex(), accumulator.dspSplitComplex(), numSamples - 1);
#else
	convolveAccumulateSSE3(input.cArrayUnpacked(), filter.cArrayUnpacked(), accumulator.cArrayUnpacked(), numSamples);	
#endif
}


Convolver::FFT &Convolver::Kernel::getFFT(uint32_t timeDomainSize) {
	map<uint32_t, shared_ptr<FFT> >::iterator result = fftSizeToFFT.find(timeDomainSize);
	
	if (result == fftSizeToFFT.end()) {
		// FFT not found, allocate a new one
		shared_ptr<FFT> newFFT(new FFT(timeDomainSize));
		fftSizeToFFT[timeDomainSize] = newFFT;
		return *newFFT;
	} else {
		return *((*result).second);
	}
}

Convolver::FFT &Convolver::Kernel::getFFTI(uint32_t freqDomainSize) {
	return getFFT(FFT::getTimeDomainSize(freqDomainSize)); 
}	
