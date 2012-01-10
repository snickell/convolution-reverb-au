/*
 *  ConvolverState.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverState_h__
#define _ConvolverState_h__

#include <stdint.h>
#include <pthread.h>
#include <errno.h>

namespace Convolver {
	class State;
	class FrameRequest;
	typedef uint64_t FrameNum;	
}

#include "ConvolverTypes.h"
#include "ConvolverKernel.h"
#include "LockFreeQueue.h"
#include "ConvolverStateTypes.h"
#include <sstream>
#include "DebugSettings.h"
#include <boost/foreach.hpp>

namespace Convolver {
	using std::cerr;
	extern uint32_t lastInputBlockSize;
	
	class State {
	public:
		State(Kernel &convolver, boost::shared_ptr<BlockPattern> &blockPattern);		
		~State();
		
		void setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern);
		
		/* new stuff */
		
		// FIXME: these two should be members, not shared_ptrs
		boost::shared_ptr<FrameBuffer> frameBuffer;		
		boost::shared_ptr<FrameRequests> frameRequests;
		FrameNum currentFrameNum;
		
		FreqBlock* getAccumulator(uint32_t framesFromNow, uint32_t accumulatorSize);
		
		// FIXME: can FrameRequest be a pointer instead of a shared_ptr ? the worker thread owns it...
		typedef boost::tuple<boost::shared_ptr<TimeBlock>, boost::shared_ptr<FrameRequest>, FrameNum> WorkItem;
		
		inline bool tryToLockAccumulators(const char * who) {
			if (releaseAccumulatorLockOnPop) return true;
			int result = pthread_mutex_trylock(&accumulatorMutex);
			switch(result) {
				case 0: {
					#if DEBUG_CONVOLVE_THREADS
					std::stringstream foo;
					foo << "State(" << (uint32_t)this << ") "<< "Accumulator trylocked by " << who << endl;
					cout << foo.str();
					#endif
					return true;
				}
				case EBUSY:
					return false;
				default:
					cerr << "UNEXPECTED RESULT " << result << " TRYING TO LOCK ACCUMULATORS in State::tryToLockAccumulators" << endl;
					cerr << "Called by " << who << endl;
					assert(false);
			}
		}
		inline void lockAccumulators(const char * who) {
			int result = pthread_mutex_lock(&accumulatorMutex);
			switch(result) {
				case 0: {
					#if DEBUG_CONVOLVE_THREADS
					std::stringstream foo;
					foo << "State(" << (uint32_t)this << ") "<< "Accumulator locked by " << who << endl;
					cout << foo.str();					
					#endif
					return;
				}
				case EDEADLK:
					cerr << "Deadlock reported locking by " << who << endl;
					return;
				default:
					cerr << "UNEXPECTED RESULT " << result << " LOCKING ACCUMULATORS in State::tryToLockAccumulators" << endl;
					cerr << "Called by " << who << endl;
					assert(false);
			}			
		}
		inline void unlockAccumulators(const char * who) {
			int result = pthread_mutex_unlock(&accumulatorMutex);
			switch(result) {
				case 0: {
					#if DEBUG_CONVOLVE_THREADS
					std::stringstream foo;
					foo << "State(" << (uint32_t)this << ") "<< "Accumulator unlocked by " << who << endl;
					cout << foo.str();
					#endif
					return;
				}
				case EPERM: {
					std::stringstream foo;
					foo << "State(" << (uint64_t)this << ") "<< "Mutex not held by this thread, called by " << who << endl;
					cerr << foo.str();		
					return;
				}
				default:
					cerr << "UNEXPECTED RESULT " << result << " LOCKING ACCUMULATORS in State::tryToLockAccumulators" << endl;
					cerr << "Called by " << who << endl;
					assert(false);
			}		
		}
		inline void setReleaseAccumulatorLockOnPop(bool value) {
			releaseAccumulatorLockOnPop |= value;
		}
		inline void setSignalThreadWorkAvailable(bool value) {
			signalWorkerThreadOnPop |= value;
		}
		inline void push(shared_ptr<TimeBlock> &frame) {
			assert(frame->size() == frameSize);
			lastInputBlockSize = frame->size();
			if (lastInputBlockSize != frameSize) {
				cerr << "ConvolverState::push() WARNING block being pushed has size " << frame->size() << ", but we were initialized with frame size " << frameSize << endl;
			}
			this->currentFrameNum = this->frameBuffer->addFrame(frame);
		}
		inline uint32_t getFrameSize() {
			return frameSize;
		}
		bool queueFrameRequestForWorkThread(WorkItem *item) {
			BOOST_FOREACH(ConvolutionOp &op, item->get<1>()->getLazyConvolutions()) {
				workThreadFrameStatus[op.outputConvolutionStartingAt]++;
			}

			return workItems->push(item);
		}
		inline void signalFrameDone() {
			pthread_cond_signal(&this->frameFinallyDone);
		}
		inline void sorryFinallydoneWithCurrentFrame() {
			pthread_cond_signal(&this->frameFinallyDone);
		}
		
		void alertUnderrun();
		
		typedef map<uint32_t, shared_ptr<FreqBlock> > FreqAccumulator;
		typedef deque<FreqAccumulator> FreqAccumulators;	
		
		class TimeAccumulator : public deque<shared_ptr<TimeBlock> > {
		public:
			TimeAccumulator(uint32_t frameSize) : frameSize(frameSize) { 
				#if DEBUG
				std::cout << "TimeAccumulator::TimeAccumulator() Initialized to frame size " << frameSize << std::endl; 
				#endif
			}
			
			void setFrameSize(uint32_t frameSize);
			void accumulate(boost::shared_ptr<TimeBlock> timeBlock);
			boost::shared_ptr<TimeBlock> pop();
			
			uint32_t frameSize;
		};
		
		uint32_t frameSize;
		boost::shared_ptr<TimeBlock> pop(uint32_t size);
		inline FrameNum getCurrentFrameNum() { return currentFrameNum; }
		
		void reset();	
		void print(float scale);

		uint32_t count;		

		void workThreadLoop();
		unordered_map<FrameNum, int> workThreadFrameStatus;

	protected:
		shared_ptr<LocklessQueue<WorkItem *> > workItems;

		bool workThreadRunning;
		bool releaseAccumulatorLockOnPop;
		bool signalWorkerThreadOnPop;
		
		Kernel &convolver;
		
		pthread_t workThread;
		pthread_mutex_t accumulatorMutex;
		pthread_mutex_t workAvailableMutex;
		pthread_cond_t  workAvailable;
		pthread_cond_t  frameFinallyDone;
		
		// Checked
		FreqAccumulators freqAccumulators;	
		TimeAccumulator timeAccumulator;
		
		FrameNum frameNum;
		
		void startWorkThread();
		inline void signalThreadWorkAvailable() {
			if (!workThreadRunning) startWorkThread();
			pthread_cond_signal(&workAvailable);
		}
		
	};
}

#endif