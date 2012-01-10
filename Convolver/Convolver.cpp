/*
 *  Convolver.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 7/21/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "Convolver.h"
#include "ConvolverInternal.h"

namespace Convolver {

	Convolver::Convolver(shared_ptr<BlockPattern> blockPattern, bool useBackgroundThreads)
		: convolver(blockPattern), blockPattern(blockPattern), channelStates(), setup(), newSetup(), useBackgroundThreads(useBackgroundThreads)
	{
		pthread_mutex_init(&newSetupMutex, NULL);
		#if DEBUG
		cout << "Convolver::Convolver(): created with " << blockPattern->toString() << endl;
		#endif
	}
	
	Convolver::~Convolver() {
		pthread_mutex_destroy(&newSetupMutex);
		
	}
	


	void Convolver::setupOutputs(Outputs &outputs, std::vector<TimeSample *> & out) {
		assert(outputs.size() == out.size());
		assert(sizeof(AudioSampleType) == sizeof(float));
		
		uint32_t size = out.size();
		
		for(uint32_t i=0; i < size; i++) {
			outputs[i] = boost::make_tuple(out[i], false);
		}
	}
	
	bool Convolver::swapInNewSetup() {
		if (this->newSetup != NULL) {		
			
			pthread_mutex_lock(&this->newSetupMutex); {
				this->setup = newSetup;
				this->newSetup = shared_ptr<Setup>();
			} pthread_mutex_unlock(&this->newSetupMutex);
			
			uint32_t numChannelStates = channelStates.size();
			if (setup->numStates > numChannelStates) {
				uint32_t numToAdd = setup->numStates - numChannelStates;
				#if DEBUG
				cout << "Convolver::swapInNewSetup(): new setup has " << setup->numStates << " channels";
				cout << ", we currently have " << numChannelStates << " channels" << endl;
				cout << "Convolver::swapInNewSetup(): FIXME: stupid appending " << numToAdd << " channel states" << endl;				
				#endif
				channelStates.reserve(setup->numStates);
				for (uint32_t i=0; i < numToAdd; i++) {
					shared_ptr<State> newState(new State(convolver, blockPattern));
					channelStates.push_back(newState);
				}
			} else if (setup->numStates < numChannelStates) {
				uint32_t numToDrop = numChannelStates - setup->numStates;
				#if DEBUG
				cout << "Convolver::swapInNewSetup(): new setup has " << setup->numStates << " channels";
				cout << ", we currently have " << numChannelStates << " channels" << endl;
				cout << "Convolver::swapInNewSetup(): FIXME: cold dropping " << numToDrop << " channel states" << endl;
				#endif
				vector<shared_ptr<State> >::iterator end = channelStates.end();
				channelStates.erase(end - numToDrop, end);
			}
			
			return true;
		} else {
			return false;
		}		
	}
	
	void Convolver::convolve(std::vector<const TimeSample *> & in,
							 std::vector<TimeSample *> &		out,
							 uint32_t							blockSize,
							 float								dryGain,
							 float								wetGain)
	{
		bool newSetup = swapInNewSetup();
		#if DEBUG
		if (newSetup) {
			cout << "Convolver::convolve(): swapped in new setup" << endl;
		}
		#endif
		
		assert(setup != NULL);
		assert(shared_ptr<Setup>() == NULL);
		
		InputMixMap &inputMixMap = setup->inputMixMap;
		list<ConvolutionOp> &convolutionOps = setup->convolutionOps;
		
		Convolver::convolve(convolver, inputMixMap, convolutionOps, channelStates,
							in, out, blockSize, dryGain, wetGain, useBackgroundThreads);		
	}
	
	
	void Convolver::queueNewSetup(InputMixMap &inputMixMap,
								  uint32_t numOutputs,
								  std::list<ConvolutionOp> &convolutionOps)
	{
		shared_ptr<Setup> newSetupPtr(new Setup(inputMixMap, numOutputs, convolutionOps));
		
		#if DEBUG
		cout << "Convolver::queueNewSetup()" << endl;
		newSetupPtr->print();
		#endif
		
		pthread_mutex_lock(&this->newSetupMutex); {
			this->newSetup = newSetupPtr;
		} pthread_mutex_unlock(&this->newSetupMutex);		
	}
	
	void Convolver::setupMonoIn(Filters &filters, bool &changeOutChannels, uint32_t &numOutChannels, InputMixMap *mixMap) {
		bool mayChangeOutChannels = changeOutChannels;
		InputMixMap inputMixMap(1);
		
		// Stereo may call us in full mono mode
		if (mixMap) {
			inputMixMap = *mixMap;
		} else {
			inputMixMap[0] = stereoMixer(0, -1, 1.0f);
		}			
		
		list<ConvolutionOp> newConvolutionOps;
		
		assert(filters.size() > 0);
		uint32_t numFilters = filters.size();
		
		if (numFilters == numOutChannels) {
			// We already have the right number of channels
			changeOutChannels = false;
		} else if (mayChangeOutChannels) {
			// We don't have the right number of channels, but we're allowed to mod them
			numOutChannels = numFilters;
			changeOutChannels = true;
		} else {
			// We'll use as many channels as we can
			assert(numOutChannels > 0);
		}

		for (uint32_t i=0; i < numOutChannels; i++) {
			newConvolutionOps.push_back(boost::make_tuple(i, filters[i], i));
		}
		
		return queueNewSetup(inputMixMap, numOutChannels, newConvolutionOps);
	}
	
	list<pair<uint32_t, float> > Convolver::stereoMixer(uint32_t channelNum1, uint32_t channelNum2, float stereoSeparation) {
		uint32_t numChannels = 1;
		if (stereoSeparation < 1.0f) numChannels = 2;
		bool fullStereo = stereoSeparation >= 0.999;
		
		// Do a cross fade that only goes down to 50%
		float crossFade = stereoSeparation / 2.0f + 0.5;		
		float amount1 = sqrt(crossFade);			
		float amount2 = sqrt(1.0 - crossFade);
		
		list<pair<uint32_t, float> > channels;
		pair<uint32_t, float> channel(channelNum1, fullStereo ? 0.0f : amount1);
		channels.push_back(channel);
		
		if (numChannels > 1) {
			pair<uint32_t, float> channel2(channelNum2, amount2);
			channels.push_back(channel2);
		}
		
		return channels;
	}
	
	// InChannelNum * ConvolutionNum -> OutChannelNum:
	//
	// mono   mono:   1*1->1
	// mono   stereo  1*1->1, 1*2->2
	// mono   quad    1*1->1, 1*2->2
	
	// stereo mono    1*1->1, 2*1->2
	// stereo stereo  [1+s2]*1->1, [1+s1]*2->2
	// stereo quad    1*1->1, 1*2->2, 2*3->1, 2*4->2	
	
	void Convolver::setupStereoIn(Filters &filters, bool &changeOutChannels, uint32_t &numOutChannels, float stereoSeparation) {
		bool mayChangeOutChannels = changeOutChannels;		
		
		//bool fullStereo = stereoSeparation >= 0.999;
		bool fullMono = stereoSeparation <= 0.001;
		
		uint32_t numInputChannels = fullMono ? 1 : 2;
		InputMixMap inputMixMap(numInputChannels);
		inputMixMap[0] = stereoMixer(0, 1, stereoSeparation);
		if (numInputChannels == 2) inputMixMap[1] = stereoMixer(1, 0, stereoSeparation);
		
		assert(filters.size() > 0);
		uint32_t numFilters = filters.size();
		
		list<ConvolutionOp> convolutionOps;		

		
		if (fullMono) {
			// We're mixed all the way to mono, treat the same
			#if DEBUG
			cout << "Convolver::setupStereoIn(): stereo input mixed to mono, calling Convolver::setupMonoIn()" << endl;
			#endif					
			setupMonoIn(filters, mayChangeOutChannels, numOutChannels, &inputMixMap);	
			return;
		} else {
			if (numOutChannels != 2 && !mayChangeOutChannels) {
				printf("WARNING: we should output 2 channels, but you won't let us change channels, doing mono\n");
				stereoSeparation = 0.0;
				setupStereoIn(filters, mayChangeOutChannels, numOutChannels, stereoSeparation);
				return;
			}
			
			changeOutChannels = numOutChannels == 2;				
			numOutChannels = 2;
			
			if (numFilters == 1) {
				// stereo mono    1*1->1, 2*1->2
				#if DEBUG
				cout << "Convolver::setupStereoIn(): stereo input, mono IR" << endl;
				#endif
				for (uint32_t i=0; i < numOutChannels; i++) {
					convolutionOps.push_back(boost::make_tuple(i, filters[0], i));
				}
			} else if (numFilters == 2) {
				// stereo stereo  [1+s2]*1->1, [1+s1]*2->2
				#if DEBUG
				cout << "Convolver::setupStereoIn(): stereo input, stereo IR" << endl;
				#endif
				
				for (uint32_t i=0; i < numOutChannels; i++) {
					convolutionOps.push_back(boost::make_tuple(i, filters[i], i));
				}
			} else if (numFilters == 4) {
				// stereo quad    1*1->1, 1*2->2, 2*3->1, 2*4->2
				#if DEBUG
				cout << "Convolver::setupStereoIn(): stereo input, quad IR" << endl;
				#endif				
				convolutionOps.push_back(boost::make_tuple(0, filters[0], 0));
				convolutionOps.push_back(boost::make_tuple(0, filters[1], 1));
				convolutionOps.push_back(boost::make_tuple(1, filters[2], 0));
				convolutionOps.push_back(boost::make_tuple(1, filters[3], 1));
			} else {
				// wtf
				printf("WARNING: only IRs with 1, 2 and 4 channels are supported with stereo input, mixing to mono\n");
				stereoSeparation = 0.0;
				return setupStereoIn(filters, changeOutChannels, numOutChannels, stereoSeparation);
			}
		}
		
		queueNewSetup(inputMixMap, numOutChannels, convolutionOps);		
		return;
	}
	
	void Convolver::setBlockPattern(boost::shared_ptr<BlockPattern> &blockPattern) {
		this->blockPattern = blockPattern;
		
		getKernel().setBlockPattern(blockPattern);
		foreach(shared_ptr<State> &state, channelStates) {
			state->setBlockPattern(blockPattern);
		}
	}

	void Convolver::convolve(Kernel &							convolver,
							 InputMixMap &						inputMixMap,
							 std::list<ConvolutionOp> &			convolutionOps,
							 std::vector<shared_ptr<State> > &	channelStates,
							 std::vector<const TimeSample *> &	in,
							 std::vector<TimeSample *> &		out,
							 uint32_t							blockSize,
							 float								dryGain,
							 float								wetGain,
							 bool								useBackgroundThreads)
	{
		#if DEBUG_CONVOLVE
		cout << endl << "Convolver::convolve() {" << endl;
		#endif
		
		// Push the new input blocks to each channel state
		assert(channelStates.size() == in.size());
		uint32_t numChannels = channelStates.size();
		for(uint32_t i=0; i < numChannels; i++) {
			const TimeSample *data = in[i];
			State &channel = *channelStates[i];
			shared_ptr<TimeBlock> timeBlock(new TimeBlock(data, data+blockSize));
			channel.push(timeBlock);
		}		
		
		#if DEBUG_CONVOLVE
		cout << "\tconvolving:" << endl;
		#endif		
				
		foreach(ConvolutionOp &convolutionOp, convolutionOps) {
			#if DEBUG_CONVOLVE
			cout << "\t\t" << Setup::toString(convolutionOp) << endl;
			#endif
			
			uint32_t inputNum = convolutionOp.get<0>();
			shared_ptr<Filter> filterPtr = convolutionOp.get<1>();
			uint32_t stateNum = convolutionOp.get<2>();
			
			assert(inputNum < channelStates.size());
			assert(stateNum < channelStates.size());
			
			State &state = *channelStates[stateNum];
			//boost::shared_ptr<TimeBlock> signal = inputs[inputNum];
			Filter &filter = *filterPtr;
			
			convolver.schedule_convolution(filter, state);
		}
		
		
		
		Outputs outputs(out.size());
		setupOutputs(outputs, out);
		assert(outputs.size() == channelStates.size());
		uint32_t size = outputs.size();
		
		#if DEBUG_CONVOLVE
		cout << "\toutputing:" << endl;
		#endif
		for(uint32_t i=0; i < size; i++) {
			TimeSample *output = outputs[i].get<0>();
			bool &outputInitialized = outputs[i].get<1>();
			
			// FIXME: we don't currently use the "outputInitialized" bool, should we?
			assert(outputInitialized == false);

			#if DEBUG_CONVOLVE
			cout << "\t\tcopying to channel: " << i << " pointer(" << (uint32_t)output << ")" << endl;
			#endif			
			
			State &state = *channelStates[i];
			convolver.process_convolutions(state, useBackgroundThreads);
			shared_ptr<TimeBlock> blockPtr = state.pop(blockSize);
			TimeBlock &block = *blockPtr;

			if (dryGain == 0.0f) {
				if (wetGain > 0.999 && wetGain < 1.001) {
					std::copy(block.begin(), block.end(), output);
				} else {
					for (uint32_t j=0; j < blockSize; j++){
						output[j] = block[j] * wetGain;
					}					
				}
			} else if (in.size() == out.size()) {
				const TimeSample *dry = in[i];
				for (uint32_t j=0; j < blockSize; j++){
					output[j] = (block[j] * wetGain) + (dry[j] * dryGain);
				}
			} else if (in.size() == 1) {
				const TimeSample *dry = in[0];
				for (uint32_t j=0; j < blockSize; j++){
					output[j] = (block[j] * wetGain) + (dry[j] * dryGain);
				}				
			} else {
				// FIXME: what are we supposed to do here? inputs != outputs, and we're
				// supposed to apply dry gain.... ugh. for now, we go all wet
				#if DEBUG
				cerr << "Convolver::convolve(): error applying dry gain where numOut != numIn, and numIn != 1" << endl;
				#endif
				for (uint32_t j=0; i < blockSize; i++){
					output[j] = block[j] * wetGain;
				}					
			}
		}
		#if DEBUG_CONVOLVE
		cout << "} Convolver::convolve()" << endl << endl;
		#endif
		
	}

	
	std::string Convolver::Setup::toString(uint32_t channel, InputMixMap &inputMixMap) {
		std::stringstream stream;

		list<pair<uint32_t, float> > &inputChannel = inputMixMap[channel];
		list<pair<uint32_t, float> >::iterator i;
		for(i = inputChannel.begin(); i != inputChannel.end(); ++i) {
			pair<uint32_t, float> &mix = *i;
			uint32_t inChannelNum = mix.first;
			float scale = mix.second;
			
			stream << inChannelNum;
			if (scale != 0.0) stream << "*" << scale;
			i++;
			if (i != inputChannel.end()) stream << " + ";
			i--;
		}
		return stream.str();
	}
	
	std::string Convolver::Setup::toString(ConvolutionOp &convolutionOp, InputMixMap *inputMixMap) {
		std::stringstream stream;

		uint32_t inputChannel = convolutionOp.get<0>();
		shared_ptr<Filter> filter = convolutionOp.get<1>();
		uint32_t outputChannel = convolutionOp.get<2>();
		
		if (inputMixMap) {
			std::string inputChannelDescription = toString(inputChannel, *inputMixMap);
			stream << "INPUTS[" << inputChannelDescription << "]";
		} else {
			stream << "SIGNAL[" << inputChannel << "]";
		}
		
		stream << " X ";
		stream << filter->description << " -> " << outputChannel;
		
		return stream.str();
	}


	void Convolver::Setup::print() {
		cout << "Convolver::Setup::print(): numOps=" << convolutionOps.size() << " {" << endl;
		foreach (ConvolutionOp &convolutionOp, convolutionOps) {
			cout << "\t" << toString(convolutionOp, &inputMixMap) << endl;
		}
		cout << "}" << endl;

	}
}
