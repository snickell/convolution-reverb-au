/*
 *  Convolver.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 5/27/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef CONVOLVER_H
#define CONVOLVER_H

#include <assert.h>
#include <vector>
#include <utility>
#include <list>

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>

#include <AudioToolbox/AudioToolbox.h>

namespace Convolver {
	class Convolver;
}

#include "ConvolverTypes.h"
#include "ConvolverKernel.h"
#include "ConvolverFilter.h"
#include "ConvolverState.h"

// Help GDB out
#if DEBUG
typedef std::vector<std::list<std::pair<uint32_t, float> > > InputMixMap;
typedef boost::tuple<uint32_t, boost::shared_ptr<Convolver::Filter>, uint32_t> ConvolutionOp;
typedef std::vector<boost::shared_ptr<Convolver::Signal> > Inputs;
typedef std::vector<boost::tuple<Convolver::TimeSample *, bool> > Outputs;
typedef std::vector<boost::shared_ptr<Convolver::Filter> > Filters;
#endif

namespace Convolver {
	class Convolver {
	public:
		
		Convolver(boost::shared_ptr<BlockPattern> blockPattern, bool useBackgroundThreads);
		~Convolver();
		
		typedef std::vector<std::list<std::pair<uint32_t, float> > > InputMixMap;
		typedef boost::tuple<uint32_t, boost::shared_ptr<Filter>, uint32_t> ConvolutionOp;
		typedef std::vector<boost::shared_ptr<TimeBlock> > Inputs;
		typedef std::vector<boost::tuple<TimeSample *, bool> > Outputs;
		typedef std::vector<boost::shared_ptr<Filter> > Filters;
		
		Kernel &getKernel() { return convolver; }
		boost::shared_ptr<BlockPattern> getBlockPattern() { return blockPattern; }
		void setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern);
		
		// Thread-safe
		virtual void setupMonoIn(Filters &filters, bool &changeOutChannels, uint32_t &numOutChannels, InputMixMap *mixMap = NULL);
		virtual void setupStereoIn(Filters &filters, bool &changeOutChannels, uint32_t &numOutChannels, float stereoSeparation);
		virtual void queueNewSetup(InputMixMap &inputMixMap, uint32_t numStates, std::list<ConvolutionOp> &convolutionOps);
		
        // The Main Dealy
		virtual void convolve(std::vector<const TimeSample *> & in,
							  std::vector<TimeSample *> &		out,
							  uint32_t							blockSize,
							  float								dryGain,
							  float								wetGain);        
        
	protected:
		class Setup {
		public:
			Setup(InputMixMap &inputMixMap, uint32_t numStates, std::list<ConvolutionOp> &convolutionOps)
				: inputMixMap(inputMixMap), numStates(numStates), convolutionOps(convolutionOps) {};
			InputMixMap inputMixMap;
			uint32_t numStates;
			std::list<ConvolutionOp> convolutionOps;
			
			void print();
			static std::string toString(ConvolutionOp &convolutionOp, InputMixMap *inputMixMap=NULL);
			static std::string toString(uint32_t channel, InputMixMap &inputMixMap);
		};
		
		

			
		bool swapInNewSetup();
		
		static void convolve(Kernel &							convolver,
							 InputMixMap &						inputMixMap,
							 std::list<ConvolutionOp> &			convolutionOps,
							 std::vector<boost::shared_ptr<State> > &	channelStates,
							 std::vector<const TimeSample *> &	in,
							 std::vector<TimeSample *> &		out,
							 uint32_t							blockSize,
							 float								dryGain,
							 float								wetGain,
							 bool								useBackgroundThreads);
				
		static void setupOutputs(Outputs &outputs, std::vector<TimeSample *> & out);
		
	private:
		static std::list<std::pair<uint32_t, float> > stereoMixer(uint32_t channelNum1, uint32_t channelNum2, float stereoSeparation);		
		
		boost::shared_ptr<BlockPattern> blockPattern;
		Kernel convolver;
		std::vector<boost::shared_ptr<State> > channelStates;

		boost::shared_ptr<Setup> setup;
		boost::shared_ptr<Setup> newSetup;
		pthread_mutex_t newSetupMutex;
		
		bool useBackgroundThreads;
	};
}

#endif // CONVOLVER_H
