/*
 *  ConvolverStateTypes.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/16/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverStateTypes_h__
#define _ConvolverStateTypes_h__

#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>

#include <deque>
#include <utility>
#include <list>
#include <map>
#include <limits> 
#include <iostream>

namespace Convolver {
	using std::pair;
	using std::deque;
	using boost::shared_ptr;
	using boost::unordered_map;
	using std::list;
	using std::vector;
	using std::map;
	
	
	typedef pair<FrameNum,FrameNum> FrameRequestID;
	
	typedef struct ConvolutionOp {
		shared_ptr<FreqBlock> block;
		FrameNum outputConvolutionStartingAt;
	} ConvolutionOp;		
	
	class FrameRequest {
	public:
		static uint32_t maxRequests;
		FrameRequest(FrameRequestID id) : id(id) { convolutionOps.reserve(maxRequests); }
		
		void lazilyConvolveWith(shared_ptr<FreqBlock> &block, FrameNum frameNum);
		vector<ConvolutionOp> &getLazyConvolutions();
		
		vector<ConvolutionOp> convolutionOps;
		FrameRequestID id;
	};	
	
	class FrameBuffer {
	public:
		FrameBuffer() : NEGATIVE_BUFFER_INDEX(std::numeric_limits<uint32_t>::max()), bufferOffset(0), frameNum(0) {}
		
		typedef deque<shared_ptr<TimeBlock> > Buffer;
		
		void flushFramesBefore(FrameNum oldestFrameToKeep);
		shared_ptr<TimeBlock> fulfill(FrameRequest &frameRequest, uint32_t frameSize, bool pad);
		FrameNum getFrameNum() { return frameNum; }
		
		void test();		
	protected:
		FrameNum addFrame(shared_ptr<TimeBlock> &frame);
		
	private:
		uint32_t NEGATIVE_BUFFER_INDEX; 
		inline uint32_t frameNumToBufferIndex(FrameNum frameNum) {
			if (frameNum < bufferOffset) {
				return NEGATIVE_BUFFER_INDEX;
			} else {
				return frameNum - bufferOffset;
			}
		}
		
		Buffer buffer;
		FrameNum bufferOffset;
		FrameNum frameNum;
		
		friend class State;
	};
	
	class FrameRequests {
	public:
		FrameRequests(FrameNum initialFrame) : frameNum(initialFrame) {}
		
		// FIXME: we technically only need one map here, we can overrride the hash function
		// to only use the endFrame, and then access the buckets when we want to
		// retrieve an item
		typedef unordered_map<FrameRequestID, shared_ptr<FrameRequest> > IDToRequestMap;
		typedef unordered_map<FrameNum, list<FrameRequestID> > EndFrameToIDMap;
		
		shared_ptr<FrameRequest> &request(FrameNum startFrame, FrameNum endFrame);
		list<shared_ptr<FrameRequest> > advanceToFrame(FrameNum frameNum);
		FrameNum getOldestFrameRequested();
	private:
		EndFrameToIDMap endFrameToIDs;
		IDToRequestMap idsToRequests;
		FrameNum frameNum;
		
		// Just for debugging
		friend class State;
	};
}	

#endif