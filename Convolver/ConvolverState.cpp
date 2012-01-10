/*
 *  ConvolverState.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/6/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "ConvolverState.h"
#include "ConvolverInternal.h"

#include <mach/thread_policy.h>

using std::vector;
using std::list;
using boost::shared_ptr;

namespace Convolver {
	static void *workerThreadEntry(void *arguments) {
		State &state = *(State *)arguments;
		state.workThreadLoop();
		#if DEBUG
		cout << "workerThreadEntry() THREAD EXITING" << endl;
		#endif
		return NULL;
	}
	
	void State::workThreadLoop() {
		Kernel kernel;

		State::WorkItem *item;
		while (this->workThreadRunning) {
			bool gotData;
			timespec foo = {0, 666666};			
			
			gotData = this->workItems->pop(item);
			if (gotData) {
				//cout << "Got data" << endl;
				#if DEBUG_CONVOLVE
				cout << "Worker thread got data, processing:" << endl;
				#endif
				
				shared_ptr<TimeBlock> &timeBlock = item->get<0>();
				shared_ptr<FrameRequest> &request = item->get<1>();
				FrameNum &frameNum = item->get<2>();
				
				bool lockAccumulators = true;
				kernel.convolve(*this, *timeBlock, *request, frameNum, lockAccumulators);
				
				delete item;
				
				#if DEBUG_CONVOLVE
				cout << "Done worker thread work" << endl;
				#endif
			} else {
				pthread_cond_wait(&workAvailable, &workAvailableMutex);
				// yield
				//nanosleep(&foo, NULL);
				//sched_yield();
			}
		}
		cout << "State::workerThread(): thread exiting" << endl;
	}
}

void Convolver::State::setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern) {
	frameSize = blockPattern->minimumBlockSize();
	timeAccumulator.setFrameSize(frameSize);
}

Convolver::State::State(Convolver::Kernel &convolver, shared_ptr<BlockPattern> &blockPattern) 
	:	frameBuffer(new FrameBuffer()),
		frameRequests(new FrameRequests(frameBuffer->getFrameNum()-1)),
		currentFrameNum(frameBuffer->getFrameNum()),
		frameSize(blockPattern->minimumBlockSize()), count(0), 
		// FIXME: we hardcode workItems here, we shouldn't
		workItems(new LocklessQueue<WorkItem *>(400)), 
		workThreadRunning(false), releaseAccumulatorLockOnPop(false), signalWorkerThreadOnPop(false),
		 convolver(convolver), timeAccumulator(frameSize), frameNum(0)
{
	pthread_mutex_init(&this->accumulatorMutex, NULL);
	pthread_mutex_init(&this->workAvailableMutex, NULL);
	pthread_cond_init(&this->workAvailable, NULL);
	pthread_cond_init(&this->frameFinallyDone, NULL);
}


void Convolver::State::startWorkThread() {
	int result;
	
	pthread_attr_t threadAttr;
	result = pthread_attr_init(&threadAttr);
	assert(result == 0);
	
	int schedPolicy;
	result = pthread_attr_getschedpolicy(&threadAttr, &schedPolicy);
	assert(result==0);
	
	sched_param schedParam;
	result = pthread_attr_getschedparam(&threadAttr, &schedParam);
	assert(result==0);
	
	#if DEBUG_CONVOLVE_THREADS
	cout << "State::startWorkThread() default priority is " << schedParam.sched_priority << endl;
	#endif
	
	schedParam.sched_priority = sched_get_priority_max(schedPolicy);
	
	#if DEBUG_CONVOLVE_THREADS
	cout << "State::startWorkThread() MAX PRIORITY IS " << schedParam.sched_priority << endl;
	#endif
	
	result = pthread_attr_setschedparam(&threadAttr, &schedParam);
	assert(result ==0);
	
	// int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
	// Set the scheduling parameter attribute in a thread attributes object.
	
	// int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
	// Set the scheduling policy attribute in a thread attributes object.
	
	workThreadRunning = true;	
	result = pthread_create(&this->workThread, &threadAttr, &workerThreadEntry, this);	
	assert(result == 0);
	
	#if DEBUG_CONVOLVE_THREADS
	cout << "State::startWorkThread() Worker thread created" << endl;
	#endif
}

Convolver::State::~State() {
	#if DEBUG_CONVOLVE_THREADS
	cout << "Unlocking mutex from State::~State" << endl;
	#endif
	// Unlock this in case the thread is waiting on it
	pthread_mutex_unlock(&this->accumulatorMutex);
	
	if (workThreadRunning) {
		workThreadRunning = false;

		#if DEBUG_CONVOLVE_THREADS
		cout << "State::~State(): waiting for worker to join" << endl;
		#endif
		// Kick the thread so it wakes up,
		// trick it, tell it we have a tasty treat for it
		// then,
		// kill it.
		pthread_cond_signal(&workAvailable);
		
		pthread_join(workThread, NULL);
		#if DEBUG_CONVOLVE_THREADS
		cout << "State::~State(): worker joined" << endl;
		#endif
	}
	
	pthread_mutex_destroy(&this->accumulatorMutex);
	pthread_mutex_destroy(&this->workAvailableMutex);
	pthread_cond_destroy(&this->workAvailable);
	pthread_cond_destroy(&this->frameFinallyDone);
}

/*
Convolver::State::State(const Convolver::State& s) {
	printf("NOTICE: Copy called on STATE\n");
	blockSize = s.blockSize;
	
	for(vector<Buffer>::const_iterator i = s.accumulators.begin(); i != s.accumulators.end(); ++i) {
		Buffer newAccumulator = new kiss_fft_cpx[bufferSize];
		memcpy(newAccumulator, *i, bufferSize);
		accumulators.push_back(newAccumulator);
	}
	
	sampleAccumulator = new float[blockSize]();
	memcpy(sampleAccumulator, s.sampleAccumulator, blockSize);
}
 */


void Convolver::State::print(float scale) {
	/*
	printf("Contents of STATE:\n");
	
	// Print the sample accumulator
	printf("\tsample accumulator:\n\t   0:");
	for (uint32_t i=0; i < blockSize; i++) {
		printf("%6.2f \t", sampleAccumulator[i]);
		if ((i+1) % 10 == 0) printf("\n\t%4d: ", i+1);	
	}
	printf("\n\n");
	
	// Print the filter accumulators
	int count=0;
	for(vector<Buffer>::iterator i = accumulators.begin(); i != accumulators.end(); ++i) {
		printf("\tAccumulator #%d:\n\t   0: ", count++);
		Buffer accumulator = *i;
		for (uint32_t j=0; j < bufferSize; j++) {
			char sign = accumulator[j].i >= 0.0f ? '+' : '-';
			printf("%10.7f%c%10.7f \t", accumulator[j].r*scale, sign, fabs(accumulator[j].i*scale));
			if ((j+1) % 5 == 0) printf("\n\t%4d: ", j+1);	
		}
		printf("\n\n");
	}
	printf("\n\n=============================\n\n\n\n\n\n");	
	 */
}

namespace Convolver {

	void State::TimeAccumulator::setFrameSize(uint32_t frameSize) {
		uint32_t oldFrameSize = this->frameSize;
		this->frameSize = frameSize;
		
		// First, copy all the existing samples to a single buffer
		uint32_t numSamples = oldFrameSize * this->size();
		TimeBlock samples(numSamples);
		TimeBlock::iterator samplesIter = samples.begin();
		for (uint32_t i=0; i < size(); i++) {
			shared_ptr<TimeBlock> &fromBlock = this->at(i);
			std::copy(fromBlock->begin(), fromBlock->end(), samplesIter);
			samplesIter += fromBlock->size();
		}

		// Now erase everything, and copy the mono-buffer to a bunch of buffers
		this->clear();
		uint32_t numNewAccumulators = (uint32_t)ceil((float)numSamples / (float)this->frameSize);

		samplesIter = samples.begin();
		for(uint32_t i=0; i < numNewAccumulators; i++) {
			shared_ptr<TimeBlock> newBlock(new TimeBlock(frameSize));
			std::copy(samplesIter, samplesIter+frameSize, newBlock->begin());
			this->push_back(newBlock);
			samplesIter += frameSize;
		}
	}
	
	void State::TimeAccumulator::accumulate(boost::shared_ptr<TimeBlock> timeBlock) {
		assert(timeBlock->size() % frameSize == 0);
		uint32_t numIterations = timeBlock->size() / frameSize;
		
		while (this->size() <= numIterations) {
			this->push_back(shared_ptr<TimeBlock>(new TimeBlock(frameSize)));
		}
		
		TimeBlock::iterator startSection = timeBlock->begin();
		
		for(uint32_t i=0; i < numIterations; i++) {
			this->at(i)->accumulate(startSection, startSection + frameSize);
			startSection += frameSize;
		}
	}
	
	shared_ptr<TimeBlock> State::TimeAccumulator::pop() {
		shared_ptr<TimeBlock> block = front();
		pop_front();
		return block;
	}
}


namespace Convolver {
	extern int numUnderruns;
}

shared_ptr<Convolver::TimeBlock> Convolver::State::pop(uint32_t timeBlockSize) {
	unordered_map<FrameNum, int>::iterator iter = workThreadFrameStatus.find(currentFrameNum);
	if (iter != workThreadFrameStatus.end()) {
		if (iter->second > 0) {
			timespec waitTime;
			waitTime.tv_sec = 0;
			waitTime.tv_nsec = 200000000;

			assert(workThreadRunning);
			assert(releaseAccumulatorLockOnPop);
			
			cerr << "State::pop() underrun, waiting on a thread\n";		
			#if DEBUG_CONVOLVE_STATS
			numUnderruns++;
			#endif
			
			pthread_cond_wait(&this->frameFinallyDone, &this->accumulatorMutex);
			//int result = pthread_cond_timedwait(&this->frameFinallyDone, &this->accumulatorMutex, &waitTime);
			//assert(result != ETIMEDOUT);
			assert(workThreadFrameStatus[currentFrameNum] == 0);
		}
		workThreadFrameStatus.erase(iter);
	}
	
	
	
	assert(freqAccumulators.size() > 0);
	
	// Now get the IFFT for the top buffer on the accumulator
	FreqAccumulator &accumulator = freqAccumulators.front();
	#if DEBUG_CONVOLVE
	cout << "OUTPUTTING " << accumulator.size() <<  " FREQ ACCUMULATORS" << endl;
	#endif
	FreqAccumulator::iterator i;
	for(i=accumulator.begin(); i != accumulator.end(); ++i) {
		shared_ptr<FreqBlock> freqBlock = i->second;
		
	#if DEBUG_CONVOLVE
	cout << "\toutputting Accumulator( " << (size_t)&(*freqBlock) << ")" << endl;
	cout << "\t\tdoing IFFT\n" << endl;
	#endif		
		
		FFT &fft = convolver.getFFTI(freqBlock->size());
		shared_ptr<TimeBlock> localAccumulator = fft.fftri(*freqBlock);
		timeAccumulator.accumulate(localAccumulator);
	}
	
	#if	DEBUG_CONVOLVE
	cout << "Popping timeAccumulator, before size: " << timeAccumulator.size() << endl;
	#endif
	shared_ptr<TimeBlock> toReturn = timeAccumulator.pop();
	
	assert(toReturn->size() == timeBlockSize);
	
	// Drop the current frame off the accumulators
	freqAccumulators.pop_front();

	this->currentFrameNum = frameBuffer->getFrameNum();
	
	#if DEBUG_MEMORY
	//cout << "Number of FreqBlocks: " << FreqBlock::numFreqBlocks << endl;
	//cout << "Number of TimeBlocks: " << TimeBlock::numTimeBlocks << endl;
	#endif
	
	#if DEBUG_CONVOLVE
	cout << endl;
	#endif
	
	if (releaseAccumulatorLockOnPop) {
		unlockAccumulators("State::pop");
		releaseAccumulatorLockOnPop = false;
	}
	if (signalWorkerThreadOnPop) {
		signalThreadWorkAvailable();
		signalWorkerThreadOnPop = false;
	}
	
	return toReturn;
}

void Convolver::State::alertUnderrun() {
	cerr << "State(" << (size_t)this << "WARNING: thread underrun!" << endl;
}


void Convolver::State::reset() {
	freqAccumulators.erase(freqAccumulators.begin(), freqAccumulators.end());
	timeAccumulator.erase(timeAccumulator.begin(), timeAccumulator.end());
}

Convolver::FreqBlock *Convolver::State::getAccumulator(uint32_t framesFromNow, uint32_t accumulatorSize) {
	if (freqAccumulators.size() <= framesFromNow) {
		uint32_t newSize = framesFromNow + 1;
		freqAccumulators.resize(newSize);
	}
	
	FreqAccumulator &accumulatorsAtFrame = freqAccumulators.at(framesFromNow);
	FreqAccumulator::iterator existingAccumulator = accumulatorsAtFrame.find(accumulatorSize);
	if (existingAccumulator==accumulatorsAtFrame.end()) {
		pair<uint32_t, shared_ptr<FreqBlock> > mapValue(accumulatorSize, shared_ptr<FreqBlock>(new FreqBlock(accumulatorSize)));
		accumulatorsAtFrame.insert(mapValue);
		return &*mapValue.second;
	} else {
		return &*(*existingAccumulator).second;
	}
}

namespace Convolver {
	using boost::unordered_map;
	
	uint32_t FrameRequest::maxRequests = 0;
	
	void FrameRequest::lazilyConvolveWith(shared_ptr<FreqBlock> &block, FrameNum frameNum) {
		ConvolutionOp op = {block, frameNum};
		convolutionOps.push_back(op);
		uint32_t numOps = convolutionOps.size();
		if (maxRequests < numOps) maxRequests = numOps;
	}
	
	vector<ConvolutionOp> &FrameRequest::getLazyConvolutions() {
		return convolutionOps;
	}
	
	
	FrameNum FrameBuffer::addFrame(shared_ptr<TimeBlock> &frame) {
		buffer.push_back(frame);
		return frameNum++;
	}
	
	// 		FrameNum bufferOffset;
	void FrameBuffer::flushFramesBefore(FrameNum oldestFrameToKeep) {
		uint32_t oldestIndexToKeep = frameNumToBufferIndex(oldestFrameToKeep);
		
		// We're already flushed
		if (oldestIndexToKeep == NEGATIVE_BUFFER_INDEX) {
			cerr << "Asked to flush before a negative buffer, skipping" << endl;
			return;
		} else if (oldestIndexToKeep == 0) {
			return;
		}
		
		bufferOffset += oldestIndexToKeep;
		
		Buffer::iterator bufferStart = buffer.begin();
		buffer.erase(bufferStart, bufferStart + oldestIndexToKeep);
	}
	
	shared_ptr<TimeBlock> FrameBuffer::fulfill(FrameRequest &frameRequest, uint32_t frameSize, bool pad) {
		uint32_t requestStartIndex = frameNumToBufferIndex(frameRequest.id.first);
		uint32_t requestEndIndex   = frameNumToBufferIndex(frameRequest.id.second);
#if 0
		cout << "FrameBuffer::fulfill[idx:" << requestStartIndex << ", idx:" << requestEndIndex << ")" << endl;
		cout << "FrameBuffer::fulfull: buffer.size() = " << buffer.size() << endl;
#endif
		assert(requestStartIndex != NEGATIVE_BUFFER_INDEX);
		assert(requestEndIndex <= buffer.size());
		assert(requestEndIndex > requestStartIndex);
		
		uint32_t numFrames = requestEndIndex - requestStartIndex;
		
#if 0
		cout << "FrameBuffer::fulfill: numFrames=" << numFrames << endl;
#endif
		
		uint32_t paddingFactor = pad ? 2 : 1;
		shared_ptr<TimeBlock> output(new TimeBlock(numFrames * frameSize * paddingFactor));
		
#if 0
		cout << "FrameBuffer::fulfill: created a timeblock of size=" << output->size() << endl;
#endif
		
		TimeBlock::iterator outputIter = output->begin();
		
		Buffer::iterator start = buffer.begin() + requestStartIndex;
		Buffer::iterator end = buffer.begin() + requestEndIndex;		
		
		Buffer::iterator i;
		for(i = start; i != end; ++i) {
			shared_ptr<TimeBlock> &timeBlock = *i;
			assert(timeBlock->size() == frameSize);
			std::copy(timeBlock->begin(), timeBlock->end(), outputIter);
			outputIter += frameSize;
		}
		
		return output;
	}
	
	
	shared_ptr<FrameRequest> &FrameRequests::request(FrameNum startFrame, FrameNum endFrame) {
		FrameRequestID id(startFrame, endFrame);
		IDToRequestMap::iterator requestIter = idsToRequests.find(id);
		if (requestIter == idsToRequests.end()) {
			shared_ptr<FrameRequest> frameRequest(new FrameRequest(id));
			
			// Insert the new item into our maps
			std::pair<IDToRequestMap::iterator, bool> result = idsToRequests.insert(IDToRequestMap::value_type(id, frameRequest));
			endFrameToIDs[endFrame].push_back(id);
			requestIter = result.first;
		}
		
		return requestIter->second;
	}
	
	// Pop all the frames that are ready
	list<shared_ptr<FrameRequest> > FrameRequests::advanceToFrame(FrameNum frameNum) {
		// We don't support big jumps yet
		assert(this->frameNum + 1 == frameNum);
		
		this->frameNum = frameNum;
		
		EndFrameToIDMap::iterator iter = endFrameToIDs.find(frameNum+1);
		assert(iter != endFrameToIDs.end());
		list<FrameRequestID> &ids = (*iter).second;
		
		list<shared_ptr<FrameRequest> > toReturn;
		// FIXME: If it was a vector... toReturn.reserve(ids.size()); would that be faster?
		
		foreach(FrameRequestID &id, ids) {
			IDToRequestMap::iterator requestIter = idsToRequests.find(id);
			assert(requestIter != idsToRequests.end());
			toReturn.push_back((*requestIter).second);
			idsToRequests.erase(requestIter);
		}
		
		endFrameToIDs.erase(iter);
		
		return toReturn;
	}
	
	// FIXME: slow
	FrameNum FrameRequests::getOldestFrameRequested() {
		FrameNum oldestFrame = frameNum;
#if DEBUG_CONVOLVE2
		cout << "FrameRequests::getEndiestFrameRequested():" << endl;		
#endif
		
		foreach(IDToRequestMap::value_type &entry, idsToRequests) {
#if DEBUG_CONVOLVE2
			cout << "[" << entry.first.first << ", " << entry.first.second << ")";
#endif
			if (entry.first.first < oldestFrame) {
				oldestFrame = entry.first.first;
#if DEBUG_CONVOLVE2
				cout << " *" << endl;
#endif
			}
#if DEBUG_CONVOLVE2
			cout << endl;
#endif
		}
		return oldestFrame;
	}	
}
