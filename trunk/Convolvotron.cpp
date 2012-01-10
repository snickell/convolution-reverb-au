/*
*	File:		Convolvotron.cpp
*	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright (C) 2009 Meatscience, All Rights Reserved
* 
*/

#include "Convolvotron.h"
#include "CAXException.h"

#include <boost/bind.hpp>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <signal.h>

// Used only for converting channel num to string
#include <sstream>


#include "Convolver.h"

using boost::shared_ptr;

using std::vector;
using std::cerr;
using std::cout;
using std::endl;
using std::pair;

Float32 Convolvotron::peak = 0.0f;

#if DEMO
uint32_t blipInterval = 30; // 10 seconds
#endif


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMPONENT_ENTRY(Convolvotron)


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::Convolvotron
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Convolvotron::Convolvotron(AudioUnit component)
	: AUEffectBase(component), auConvolver(), 
	  irFilename(), irLoadingThread(NULL), irLoadingStatus(), tailTime(0.0f)
{
	#if DEBUG
	cout << "Convolvotron::Convolvotron()" << endl;
	#endif
	
	#if DEMO
	demoExpired = false;
	#endif
	
	irLoadingStatus.status = LOAD_STATUS_LOADED;
	strcpy(irLoadingStatus.filename, "");
	
	CreateElements();
	Globals()->UseIndexedParameters(kNumberOfParameters);
	for (int i=0; i < kNumberOfParameters; i++) {
		SetParameter(i, parameters[i].defaultValue);
	}
	
#if AU_DEBUG_DISPATCHER
	mDebugDispatcher = new AUDebugDispatcher (this);
#endif
	
	#if DEBUG
	printf("Convolvotron::Convolvotron(): initializing with %u channels\n", (uint32_t)this->GetNumberOfChannels());
	#endif
	
	pthread_mutex_init(&filtersMutex, NULL);
	
}

void Convolvotron::setFrameSize(uint32_t frameSize) {
	if (this->frameSize != frameSize) {
		#if DEBUG
		cout << endl << "Convolvotron::setFrameSize() changing frameSize from " << this->frameSize << " to " << frameSize << endl << endl;
		#endif
		
		this->frameSize = frameSize;

		shared_ptr<Convolver::BlockPattern> blockPattern;
		if (frameSize > 1024 && frameSize < 2048) {
			blockPattern.reset(new Convolver::TwoSizeBlockPattern(frameSize, frameSize*2, 2));		
		} else if (frameSize > 512 && frameSize <= 1024) {
			blockPattern.reset(new Convolver::TwoSizeBlockPattern(frameSize, frameSize*4, 4));
		} else if (frameSize <= 512) {
			blockPattern.reset(new Convolver::TwoSizeBlockPattern(frameSize, frameSize*8, 8));		
		} else {
			// We're >= 2048
			blockPattern.reset(new Convolver::FixedSizeBlockPattern(frameSize));
		}
		
		assert(blockPattern != NULL);
		
		auConvolver->setBlockPattern(blockPattern);
		ir->setBlockPattern(auConvolver->getKernel(), blockPattern);
		filenameToIRCache.clear();	
	}
}

ComponentResult	Convolvotron::Initialize()
{	
	ComponentResult result = AUEffectBase::Initialize();	
	
	// Do this pre-emptively, because ableton likes to send lots of crazy calls
	cancelIRLoadingThread();
	
	UInt32 maxFrameSize = GetMaxFramesPerSlice();
	this->frameSize = maxFrameSize;
	
	Float32 sampleRate = this->GetSampleRate();

#if DEMO
	time(&endTime);
	endTime += 600; // 10 min = 1sec * 60sec/min
	time(&lastBlipTime);
#endif
	
	// Delete our caches
	ir = boost::shared_ptr<Convolver::IR>();
	filenameToIRCache.clear();	
	
	
	#if DEBUG
	cout << "Convolvolvotron::Initialize(): maxFramesPerSlice=" << maxFrameSize << ", sampleRate=" << sampleRate << endl;
	#endif
	
	shared_ptr<Convolver::BlockPattern> blockPattern;
	if (maxFrameSize > 1024 && maxFrameSize < 2048) {
		blockPattern.reset(new Convolver::TwoSizeBlockPattern(maxFrameSize, maxFrameSize*2, 2));		
	} else if (maxFrameSize > 512 && maxFrameSize <= 1024) {
		blockPattern.reset(new Convolver::TwoSizeBlockPattern(maxFrameSize, maxFrameSize*4, 4));
	} else if (maxFrameSize <= 512) {
		blockPattern.reset(new Convolver::TwoSizeBlockPattern(maxFrameSize, maxFrameSize*8, 8));		
	} else {
		// We're >= 2048
		blockPattern.reset(new Convolver::FixedSizeBlockPattern(maxFrameSize));
	}
	
	assert(blockPattern != NULL);
	
	//shared_ptr<Convolver::BlockPattern> blockPattern(new Convolver::FixedSizeBlockPattern(small));
	//shared_ptr<Convolver::BlockPattern> blockPattern(new Convolver::TwoSizeBlockPattern(small, big, numSmall));
	//shared_ptr<Convolver::BlockPattern> blockPattern(new Convolver::ThreeSizeBlockPattern(small, med, big, numSmall, numMed));	
	//shared_ptr<Convolver::BlockPattern> blockPattern(new Convolver::DoublingBlockPattern(small, 4096));	
	
	shared_ptr<Convolver::AUConvolver> auConvolver(new Convolver::AUConvolver(*this, blockPattern));
	this->auConvolver = auConvolver;
		
	
	if(result == noErr ) {	
		if (this->irFilename.size() > 0) {
						
			#if DEBUG
			cout << "Convolvotron::Initialize(): loading IR Filename " << irFilename << endl;
			#endif

			// Gotta cache this here, because LoadUnitIR() overwrites it
			std::string filename = this->irFilename;
			
			LoadUnitIR();

			// We load a unit ir synchronously, and load in background
			// because some hosts (Logic, lookin' at you) block their startup
			// on file load waiting for this, and the latency is bad
			bool inBackground = true;
			LoadIR(filename, inBackground);
			
			result = noErr;
		} else {
			LoadUnitIR();
			result = noErr;
		}
		
		// in case the AU was un-initialized and parameters were changed, the view can now
		// be made aware it needs to update
		PropertyChanged(kAudioUnitCustomProperty_Samples, kAudioUnitScope_Global, 0 );
	}
	
	#if DEBUG
	cout << "Convolvolvotron::Initialize(): done" << endl;
	#endif
		
	return result;
}

Convolvotron::~Convolvotron () {
	#if DEBUG_MEMORY
	cout << endl << "Convolvotron::~Convolvotron()" << endl;
	#endif
	
	pthread_mutex_destroy(&filtersMutex);
	
	#if AU_DEBUG_DISPATCHER
	delete mDebugDispatcher;
	#endif
	
	#if DEBUG_MEMORY
	cout << "Convolvotron::~Convolvotron()" << endl << endl;
	#endif	
}

OSStatus	Convolvotron::ProcessBufferLists(AudioUnitRenderActionFlags &	ioActionFlags,
											 const AudioBufferList &			inBuffer,
											 AudioBufferList &				outBuffer,
											 UInt32							inFramesToProcess )
{
	bool ioSilence;
	
	bool silentInput = IsInputSilent (ioActionFlags, inFramesToProcess);
	ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	
#if DEMO
	static uint32_t beepCounter = 0;
	time_t currentTime;
	time(&currentTime);
	if (currentTime >= endTime) {
		demoExpired = true;
		return noErr;
	}
	if (currentTime - lastBlipTime > blipInterval) {
		lastBlipTime = currentTime;
		beepCounter = 5;
	}
#endif
	
	// FIXME: we should pop remaining state out so we "ring out"
	if (silentInput) return noErr;
	
	// This should only change frame size if its different
	this->setFrameSize(inFramesToProcess);
	
	Float32 wetDryMix = GetParameter( kParam_WetDry ) / 100.0;
	Float32 outGain = pow(10.0, GetParameter(kParam_OutputGain) / 20.0);
	//Float32 stereoSeparation = GetParameter( kParam_StereoSeparation ) / 100.0;
	
	Float32 dryGain = outGain * sqrt(1.0 - wetDryMix);
	Float32 wetGain = outGain * sqrt(wetDryMix);
	
	ioSilence = silentInput;
	auConvolver->convolve(inBuffer, outBuffer, inFramesToProcess, dryGain, wetGain);
	if (!ioSilence)
		ioActionFlags &= ~kAudioUnitRenderAction_OutputIsSilence;
	
	// FIXME: for performance, do this inside convolve as a side effect, and pass back peaks...
	// also, this only listens to ONE channel
	float biggest = *std::max_element((AudioSampleType *)outBuffer.mBuffers[0].mData, (AudioSampleType *)outBuffer.mBuffers[0].mData + inFramesToProcess);	
	if (biggest > Convolvotron::peak) {
		Convolvotron::peak = biggest;
	}
	
#if DEMO
	if (beepCounter > 0) {
		beepCounter--;
		uint32_t size = outBuffer.mNumberBuffers;
		for (uint32_t i=0; i < size; i++) {	
			float * output = (float *)outBuffer.mBuffers[i].mData;
			bool on = true;
			uint32_t onCount = 0;
			for (uint32_t j=0; j < inFramesToProcess; j++) {
				if (onCount > 100) {
					onCount = 0;
					on = !on;
				}
				if (on) {
					output[j] += 0.2;
				} else {
					output[j] += -0.2;
				}
				onCount++;
			}
			
		}
	}
#endif
	
	return noErr;
	
/*
	
	// call the kernels to handle either interleaved or deinterleaved
	if (inBuffer.mNumberBuffers == 1) {
		// interleaved (or mono)
		int channel = 0;
		
		for (KernelList::iterator it = mKernelList.begin(); it != mKernelList.end(); ++it, ++channel) {
			AUKernelBase *kernel = *it;
			
			if (kernel != NULL) {
				ioSilence = silentInput;
				
				// process each interleaved channel individually
				kernel->Process(
								(const AudioSampleType *)inBuffer.mBuffers[0].mData + channel, 
								(AudioSampleType *)outBuffer.mBuffers[0].mData + channel,
								inFramesToProcess,
								inBuffer.mBuffers[0].mNumberChannels,
								ioSilence);
				
				if (!ioSilence)
					ioActionFlags &= ~kAudioUnitRenderAction_OutputIsSilence;
			}
		}
	} else {
		// deinterleaved	
		
		const AudioBuffer *srcBuffer = inBuffer.mBuffers;
		AudioBuffer *destBuffer = outBuffer.mBuffers;
		
		for (KernelList::iterator it = mKernelList.begin(); it != mKernelList.end(); 
			 ++it, ++srcBuffer, ++destBuffer) {
			AUKernelBase *kernel = *it;
			
			if (kernel != NULL) {
				ioSilence = silentInput;
				
				kernel->Process(
								(const AudioSampleType *)srcBuffer->mData, 
								(AudioSampleType *)destBuffer->mData, 
								inFramesToProcess,
								1,
								ioSilence);
				
				if (!ioSilence)
					ioActionFlags &= ~kAudioUnitRenderAction_OutputIsSilence;
			}
		}
	}
	*/
	
	return noErr;
}

void Convolvotron::LoadUnitIR() {
	#if DEBUG
	cout << "Convolvotron::LoadUnitIR()" << endl;
	#endif
	
	irFilename = "";
	
	// FIXME: right now, we gotta normalize, or else no frequency response = CRASH
	shared_ptr<Convolver::IR> unitIR(new Convolver::UnitIR(auConvolver->getKernel(), auConvolver->getBlockPattern()));
	
	std::string filename = "";
	SetIR(filename, unitIR);
		
	#if DEBUG
	cout << "Convolvotron::LoadUnitIR(): done" << endl;
	#endif
}

typedef struct {
	std::string *filename;
	Convolvotron *convolvotron;
} LoadIRArguments;

void Convolvotron::LoadIRInBackground(std::string &filename) {
	try {
		// LoadIR will call PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus) if it succeeds		
		LoadIR(filename, false);
	} catch (CAXException &cax) {
		#if DEBUG
		cerr << "Convolvotron::Initialize(): WARNING, couldn't load stuff. An error occurred in " << cax.mOperation << endl;
		#endif
		irLoadingStatus.status = LOAD_STATUS_FAILED;
		PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus, kAudioUnitScope_Global, 0);
		
	} catch (...) {
		#if DEBUG
		printf("Convolvotron::Initialize(): WARNING, couldn't load stuff. An unknown exception occurred.\n");
		#endif
		irLoadingStatus.status = LOAD_STATUS_FAILED;
		PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus, kAudioUnitScope_Global, 0);
	}
}

static void *loadIR(void *argumentsVoid) {
	LoadIRArguments *arguments = (LoadIRArguments *)argumentsVoid;
	
	int status = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);	
	status = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
	
	#if DEBUG
	cout << "THREAD: loadImpulseResponse()" << "\t filename is: " << arguments->filename << endl;
	#endif
	
	arguments->convolvotron->LoadIRInBackground(*arguments->filename);
	
	delete arguments->filename;
	delete arguments;
	
	
	#if DEBUG
	printf("THREAD: exiting\n");	
	#endif
	
	return NULL;
}

void Convolvotron::SetIR(std::string &filename, boost::shared_ptr<Convolver::IR> ir) {
	pthread_mutex_lock(&this->filtersMutex); {
		// Save the name of the filename, we're gonna need it to SaveState()
		
		this->irFilename = filename;
		
		this->ir = ir;
		
		// FIXME: we don't set stereo separation yet
		auConvolver->setFilters(ir->getFilters(), 1.0f);					
		
	} pthread_mutex_unlock(&this->filtersMutex);
	
	// Change the tail time
	tailTime = ir->getFilters()[0]->getNumSamples() / GetSampleRate();

	// Notify everyone that our IR changed
	PropertyChanged(kAudioUnitCustomProperty_Samples, kAudioUnitScope_Global, 0 );

	irLoadingStatus.status = LOAD_STATUS_LOADED;
	PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus, kAudioUnitScope_Global, 0);
	
#if DEBUG
	cout << "Convolvotron::LoadIR(): changing tail time to " << tailTime << " seconds." << endl;
#endif
}

void Convolvotron::LoadIR(std::string &filename, bool inBackground) {
	assert(filename != "");
	
	// Check if we already have this filename loaded into memory
	std::map<std::string, boost::shared_ptr<Convolver::IR> >::iterator cacheIter = filenameToIRCache.find(filename);
	if (cacheIter != filenameToIRCache.end()) {
		// We do! no need to use background loading
		#if DEBUG
		cout << "Convolvotron::LoadIR(): " << filename << "found in cache, loading" << endl;
		#endif			
		SetIR(filename, cacheIter->second);
		return;
	}
	
	if (inBackground) {
		#if DEBUG
		cout << "Convolvotron::LoadIR(inbackground=true): starting thread" << endl;
		#endif	
		
		cancelIRLoadingThread();
		
		// Notify views that we're loading a file in the background
		irLoadingStatus.status = LOAD_STATUS_LOADING;
		strncpy(irLoadingStatus.filename, filename.c_str(), IRLoadingStatus_filename_size);
		PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus, kAudioUnitScope_Global, 0);
		
		// Call ourselves on a new thread
		LoadIRArguments *arguments = new LoadIRArguments;
		arguments->filename = new std::string(filename);
		arguments->convolvotron = this;
		
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
		
		#if DEBUG
		cout << "LoadIR() default priority is " << schedParam.sched_priority << endl;
		#endif
		
		schedParam.sched_priority = sched_get_priority_min(schedPolicy);
		#if DEBUG
		cout << "LoadIR() MIN PRIORITY IS " << schedParam.sched_priority << endl;
		#endif
		result = pthread_attr_setschedparam(&threadAttr, &schedParam);
		assert(result ==0);		
		
		pthread_create(&this->irLoadingThread, NULL, &loadIR, arguments);
		
		return;
	}
	
	#if DEBUG
	cout << endl << "Convolvotron::LoadIR(inbackground=false) {" << endl;
	#endif	
	
	const char * filename_c = filename.c_str();
	
	#if DEBUG
	cout << "\t loading IR: " << filename_c << endl;
	#endif		
	
	Float64 kGraphSampleRate = this->GetSampleRate();
	ExtAudioFileRef xafref = NULL;
	float *data = NULL;
	uint32_t dataLength = 0;
	
	{
		OSStatus err = noErr;
		FSRef theRef;
		
		XThrowIfError(FSPathMakeRef((const UInt8 *)filename.c_str(), &theRef, NULL), "FSPathMakeRef");
		
		err = ExtAudioFileOpen(&theRef, &xafref);
		XThrowIfError (err, "ExtAudioFileOpen");
		
		UInt32 propSize;
		
		CAStreamBasicDescription clientFormat;
		uint32_t numChannels;
		
		propSize = sizeof(clientFormat);
		err = ExtAudioFileGetProperty(xafref, kExtAudioFileProperty_FileDataFormat, &propSize, &clientFormat);
		XThrowIfError(err, "kExtAudioFileProperty_FileDataFormat");
		numChannels = clientFormat.NumberChannels();
		
		SInt64 filelength;
		propSize = sizeof(filelength);
		err = ExtAudioFileGetProperty(xafref, kExtAudioFileProperty_FileLengthFrames, &propSize, &filelength);
		XThrowIfError(err, "kExtAudioFileProperty_FileLengthFrames");


		clientFormat.mSampleRate = kGraphSampleRate;
		bool interleaved = false;
		clientFormat.SetCanonical(numChannels, interleaved);
		
		propSize = sizeof(clientFormat);
		err = ExtAudioFileSetProperty(xafref, kExtAudioFileProperty_ClientDataFormat, propSize, &clientFormat);
		XThrowIfError(err, "kExtAudioFileProperty_ClientDataFormat");
		
		
		
		err = ExtAudioFileGetProperty(xafref, kExtAudioFileProperty_FileLengthFrames, &propSize, &filelength);
		XThrowIfError(err, "kExtAudioFileProperty_FileLengthFrames");
		
		#if DEBUG
		cout << "Convolvotron::LoadIR(): file has " << filelength << "samples" << endl;
		#endif
		
		// If you need to alloc a buffer, you'll need to alloc filelength*channels*rateRatio bytes
		double rateRatio = kGraphSampleRate / clientFormat.mSampleRate;
		uint32_t numFrames = (UInt32)ceil(filelength * rateRatio);
		uint32_t channelLength = numFrames * 2;
		
		dataLength = channelLength * numChannels;
		data = new float[dataLength]();
		
		AudioBufferList *bufList = (AudioBufferList*)calloc(1, sizeof(AudioBufferList) + numChannels * sizeof(AudioBuffer));
		bufList->mNumberBuffers = numChannels;
		for(uint32_t i=0; i < numChannels; i++) {
			bufList->mBuffers[i].mNumberChannels = 1;
			bufList->mBuffers[i].mData = &data[channelLength*i];
			bufList->mBuffers[i].mDataByteSize = channelLength * sizeof(Float32);
		}
		
		// FIXME: shouldn't we know precisely how many packets we're loading? this blows...
		UInt32 loadedFrames = INT_MAX;
		err = ExtAudioFileRead(xafref, &loadedFrames, bufList);
		XThrowIfError(err, "ExtAudioFileRead");
	
		//assert(loadedPackets == numFrames);
		
		bool normalize = true;
		
		// Convert from AudioBufferList to vector<const float *>
		std::vector<const float *> channels;
		channels.reserve(numChannels);
		for(uint32_t i=0; i < numChannels; i++) {
			channels.push_back((const float *)bufList->mBuffers[i].mData);
		}
		
		uint32_t slashLocation = filename.find_last_of("/");
		std::string shortName = slashLocation+1 < filename.size() ? filename.substr(slashLocation+1) : filename;
				
		shared_ptr<Convolver::IR> irPtr(new Convolver::IR(auConvolver->getKernel(), auConvolver->getBlockPattern(), 
														  channels, loadedFrames, normalize, shortName));
		
		delete[] data;
		delete bufList;

		SetIR(filename, irPtr);
		
		// We've loaded a new file, add it to the cache
		filenameToIRCache[filename] = ir;
		
		ExtAudioFileDispose(xafref);
	}
	
	if (false) { // FIXME: do this if an exception was thrown
		if(data) free(data);
		data = nil;
		
		if(xafref) ExtAudioFileDispose(xafref);
		
		//NSLog(@"loadSegment: Caught %@: %@", [exception name], [exception reason]);
	}

	#if DEBUG
	cout << "Convolvotron::LoadIR(): exiting" << endl << endl;
	#endif		
}	

Float64 Convolvotron::GetTailTime() {	
	return tailTime;
}

Float64 Convolvotron::GetLatency() {
	return 0.0;
	// FIXME: is this right? now we create kernel with same latency as maxSamples, so I guess so???
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::GetParameterValueStrings
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			Convolvotron::GetParameterValueStrings(AudioUnitScope		inScope,
                                                                AudioUnitParameterID	inParameterID,
                                                                CFArrayRef *		outStrings)
{
        
    return kAudioUnitErr_InvalidProperty;
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::GetParameterInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			Convolvotron::GetParameterInfo(AudioUnitScope		inScope,
                                                        AudioUnitParameterID	inParameterID,
                                                        AudioUnitParameterInfo	&outParameterInfo )
{
	OSStatus result = noErr;

	outParameterInfo.flags = 	kAudioUnitParameterFlag_IsWritable
						|		kAudioUnitParameterFlag_IsReadable
						|		parameters[inParameterID].flags;
    
    if (inScope == kAudioUnitScope_Global) {
		if (inParameterID < kNumberOfParameters) {
			AUBase::FillInParameterName (outParameterInfo, parameters[inParameterID].name, false);
			outParameterInfo.unit = parameters[inParameterID].unit;
			outParameterInfo.minValue = parameters[inParameterID].minValue;
			outParameterInfo.maxValue = parameters[inParameterID].maxValue;
			outParameterInfo.defaultValue = parameters[inParameterID].defaultValue;	
		} else {
			result = kAudioUnitErr_InvalidParameter;
		}			
	} else {
        result = kAudioUnitErr_InvalidParameter;
    }
    


	return result;
}

boost::shared_ptr<Convolver::Filter> Convolvotron::getMonoIR() {
	shared_ptr<Convolver::Filter> monoFilter;
	
	pthread_mutex_lock(&this->filtersMutex); {
		vector<shared_ptr<Convolver::Filter> > &filters = ir->getFilters();
		if (filters.size() > 0) {
			monoFilter = filters[0];
		}
	} pthread_mutex_unlock(&this->filtersMutex);
	
	return monoFilter;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::GetPropertyInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			Convolvotron::GetPropertyInfo (AudioUnitPropertyID	inID,
                                                        AudioUnitScope		inScope,
                                                        AudioUnitElement	inElement,
                                                        UInt32 &		outDataSize,
                                                        Boolean &		outWritable)
{
 if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case kAudioUnitProperty_CocoaUI:
				outWritable = false;
				outDataSize = sizeof (AudioUnitCocoaViewInfo);
				return noErr;
			case kAudioUnitCustomProperty_IRFile:	// our custom property
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				
				outDataSize = (irFilename.size() + 1) * sizeof(char);
				outWritable = true;
				return noErr;
			case kAudioUnitCustomProperty_FilterFrequencyResponse:	// our custom property
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				outDataSize = kMyNumberOfResponseFrequencies * sizeof(FrequencyResponse);
				outWritable = false;
				return noErr;
			case kAudioUnitCustomProperty_Samples:
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;

				{
					shared_ptr<Convolver::Filter> ir = getMonoIR();
					assert(ir != NULL);
					
					outDataSize = ir->getNumSamples() * sizeof(float);
				}

				outWritable = false;
				return noErr;
			case kAudioUnitCustomProperty_Peak:
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				outDataSize = sizeof(Float32);
				outWritable = false;
				return noErr;
			case kAudioUnitCustomProperty_IRLoadingStatus:
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				outDataSize = sizeof(IRLoadingStatus);
				outWritable = false;
				return noErr;	
			case kAudioUnitCustomProperty_IRInfo:
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				outDataSize = sizeof(IRInfo);
				outWritable = false;
				return noErr;						
			case kAudioUnitCustomProperty_IsDemo:
				if(inScope != kAudioUnitScope_Global ) return kAudioUnitErr_InvalidScope;
				outDataSize = sizeof(int);
				outWritable = false;
				return noErr;	
		}
	}

	return AUEffectBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::GetProperty
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus			Convolvotron::GetProperty(	AudioUnitPropertyID inID,
                                                        AudioUnitScope 		inScope,
                                                        AudioUnitElement 	inElement,
                                                        void *			outData )
{
if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case kAudioUnitProperty_CocoaUI:
			{
				// Look for a resource in the main bundle by name and type.
				CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR(CONVOLVOTRON_BUNDLE_ID) );
				
				if (bundle == NULL) {
					printf("WARNING: failed to load our bundle, where'd it go?\n");
					return fnfErr;
				}
				
				CFURLRef bundleURL = CFBundleCopyResourceURL( bundle, 
                    CFSTR("MeatscienceConvolvotron_CocoaViewFactory"), 
                    CFSTR("bundle"), 
                    NULL);
                
                if (bundleURL == NULL) return fnfErr;
				
				CFStringRef className = CFSTR("MeatscienceConvolvotron_CocoaViewFactory");
				AudioUnitCocoaViewInfo cocoaInfo = { bundleURL, {className} };
				*((AudioUnitCocoaViewInfo *)outData) = cocoaInfo;
				
				return noErr;
			}
				// This is our custom property which reports the current frequency response curve
				//
			case kAudioUnitCustomProperty_IRFile:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
				
				// the kernels are only created if we are initialized
				// since we're using the kernels to get the curve info, let
				// the caller know we can't do it if we're un-initialized
				// the UI should check for the error and not draw the curve in this case
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				char *fileName = (char*)outData;				
				
				if (irFilename.size() > 0) {
					UInt32 dataSize;
					Boolean	writable;
					ComponentResult result;
					result = this->GetPropertyInfo(   kAudioUnitCustomProperty_IRFile,
													  kAudioUnitScope_Global,
													  0,
													  dataSize,
													  writable);
					
					#if DEBUG
					cout << "Convolvotron::GetProperty(): returning fileName " << irFilename;
					#endif
					memcpy(fileName, irFilename.c_str(), dataSize * sizeof(char));
				} else {
					// We have the Unit Impulse loaded
					#if DEBUG
					printf("Convolvotron::GetProperty(): no fileName, returning null for unit impulse\n");
					#endif
					
					fileName[0] = '\0';
				}
				
				return noErr;
			}
			case kAudioUnitCustomProperty_FilterFrequencyResponse:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
				
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				FrequencyResponse *freqResponseTable = ((FrequencyResponse*)outData);
				{
					shared_ptr<Convolver::Filter> ir = getMonoIR();
					assert(ir != NULL);
					
					const FrequencyResponse *copyTable = ir->getFrequencyResponse();
					memcpy(freqResponseTable, copyTable, sizeof(FrequencyResponse) * kMyNumberOfResponseFrequencies);
				}
				
				return noErr;
			}
			case kAudioUnitCustomProperty_Samples:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
								
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				float *samples = (float *)outData;
				
				uint32_t numSamples;
				
				// FIXME: what if the GetPropertyInfo call returned a number of
				// samples appropriate for a smaller data size? i.e. a new IR
				// was swapped in on another thread between these calls: that would suck
				{
					shared_ptr<Convolver::Filter> ir = getMonoIR();
					assert(ir != NULL);
				
					const float *sampleBuffer = ir->getSamples();
					numSamples = ir->getNumSamples();
					
					memcpy(samples, sampleBuffer, sizeof(float) * numSamples);				
				}
				
				return noErr;
			}
			case kAudioUnitCustomProperty_Peak:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
				
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				Float32 *peak = (Float32 *)outData;
				*peak = Convolvotron::peak;
				Convolvotron::peak = 0.0f;
				
				return noErr;
			}				
			case kAudioUnitCustomProperty_IRLoadingStatus:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
				
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				IRLoadingStatus *status = (IRLoadingStatus *)outData;
				*status = irLoadingStatus;
				
				return noErr;
			}
			case kAudioUnitCustomProperty_IRInfo:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;
				
				if(!IsInitialized() ) return kAudioUnitErr_Uninitialized;
				
				IRInfo *outInfo = (IRInfo *)outData;
				
				IRInfo info;
				info.seconds = (float)getMonoIR()->getNumSamples() / GetSampleRate();
				info.numChannels = ir->getFilters().size();
				
				*outInfo = info;
				
				return noErr;
			}
			case kAudioUnitCustomProperty_IsDemo:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;

				int *outIsDemo = (int *)outData;
				#if DEMO
				*outIsDemo = -1;
				#else
				*outIsDemo = 0;
				#endif
				
				return noErr;
			}
		}
	}

	return AUEffectBase::GetProperty (inID, inScope, inElement, outData);
}

void handleInterrupt(int signal) {
	cout << "Got interrupt" << endl;
	throw;
}

void Convolvotron::cancelIRLoadingThread() {
	// Stop any existing worker thread, FIXME: this leaks memory, C++ doesn't call destructors
	pthread_cancel(this->irLoadingThread);
	pthread_join(this->irLoadingThread, NULL);

	
	
	irLoadingStatus.status = LOAD_STATUS_CANCELED;
	PropertyChanged(kAudioUnitCustomProperty_IRLoadingStatus, kAudioUnitScope_Global, 0);	
}


OSStatus Convolvotron::SetProperty(AudioUnitPropertyID 			inID,
									AudioUnitScope 					inScope,
									AudioUnitElement 				inElement,
									const void *					inData,
									UInt32 							inDataSize)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case kAudioUnitCustomProperty_IRFile:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;

				std::string irFilename = (char *)inData;
				assert(irFilename != "");
				bool inBackground = true;
				LoadIR(irFilename, inBackground);
				return noErr;
			}
			case kAudioUnitCustomProperty_IRLoadingStatus:
			{
				if(inScope != kAudioUnitScope_Global) 	return kAudioUnitErr_InvalidScope;

				IRLoadingStatus *loadingStatus = (IRLoadingStatus *) inData;
				
				if (loadingStatus->status == LOAD_ACTION_CANCEL) {
					#if DEBUG
					cout << "Convolvotron::SetProperty(IRLoadingStatus): cancelling background thread" << endl;
					#endif
					cancelIRLoadingThread();
					return noErr;
				} else {
					return kAudioUnitErr_InvalidPropertyValue;
				}
			}
		}
	}
	
	return AUEffectBase::SetProperty (inID, inScope, inElement, inData, inDataSize);
}

ComponentResult	Convolvotron::SaveState(CFPropertyListRef * outData) {
	#if DEBUG
	cout << "Convolvotron::SateState()" << endl;
	#endif
	
	ComponentResult result = AUEffectBase::SaveState(outData);
	if (result == noErr) {
		CFMutableDictionaryRef dict = (CFMutableDictionaryRef)*outData;
		
		
		if (this->irFilename.size() > 0) {
			#if DEBUG
			cout << "Convolvotron::SaveState(): saving ir filename " << this->irFilename << endl;
			#endif				
			CFStringRef value = CFStringCreateWithCString (NULL, this->irFilename.c_str(), kCFStringEncodingUTF8);
			CFDictionarySetValue (dict, CFSTR("ConvolvotronIRFilename"), value);
			CFRelease(value);		
		} else {
			#if DEBUG
			cout << "Convolvotron::SaveState(): no filename to save, skipping" << endl;
			#endif				
		}
	}
	return result;
}

ComponentResult	Convolvotron::RestoreState(CFPropertyListRef plist) {
	#if DEBUG
	cout << "Convolvotron::RestoreState()" << endl;
	#endif
	
	ComponentResult result = AUEffectBase::RestoreState(plist);
	if (result == noErr) {
		CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);		
		CFStringRef value;
		
		if (CFDictionaryGetValueIfPresent (dict, CFSTR("ConvolvotronIRFilename"), reinterpret_cast<const void**>(&value))) {			
			CFIndex length = CFStringGetLength(value);
			uint32_t bufferSize = length * sizeof(char) * 2;
			char *c_string = new char[bufferSize];
			CFStringGetCString(value, c_string, bufferSize, kCFStringEncodingUTF8);
			
			#if DEBUG
			cout << "Convolvotron::RestoreState(): found IR filename " << c_string << endl;
			#endif			

			// Some clients (AU Lab) call Initialize() first, some clients (Logic, Ableton)
			// call RestoreState() first, handle both
			if (!IsInitialized()) {
				#if DEBUG
				cout << "Convolvotron::RestoreState(): not initialized yet, caching filename" << endl;
				#endif
				
				// For now, we save this in the classes member variables, we'll
				// read it in when we initialize and do the actual SetProperty() call
				this->irFilename = c_string;
			} else {
				#if DEBUG
				cout << "Convolvotron::RestoreState(): initialized already, loading file" << endl;
				#endif
				
				std::string irFilenameString(c_string);
				bool inBackground = true;
				LoadIR(irFilenameString, inBackground);
			}
		} else {
			#if DEBUG
			cout << "Convolvotron::RestoreState(): didn't find an IR filename to restore" << endl;
			#endif					
		}
	}
	return result;
}

/*
#pragma mark ____ConvolvotronEffectKernel

Convolvotron::ConvolvotronKernel::~ConvolvotronKernel()
{
}

Convolvotron::ConvolvotronKernel::ConvolvotronKernel(AUEffectBase *inAudioUnit) 
	: AUKernelBase(inAudioUnit), convolvotron((Convolvotron *)inAudioUnit), convolver(convolvotron->convolver),
	blockSize(convolver->timeBlockSize), convolverState(*convolver), inBuffer(blockSize*3), outBuffer(blockSize*3)
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::ConvolvotronKernel::Reset()
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		Convolvotron::ConvolvotronKernel::Reset()
{
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Convolvotron::ConvolvotronKernel::Process
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Convolvotron::ConvolvotronKernel::Process(	const Float32 	*inSourceP,
                                                    Float32		 	*inDestP,
                                                    UInt32 			inFramesToProcess,
                                                    UInt32			inNumChannels, // for version 2 AudioUnits inNumChannels is always 1
                                                    bool			&ioSilence )
{	
	Float32 wetDryMix = GetParameter( kParam_WetDry ) / 100.0;
	Float32 outGain = pow(10.0, GetParameter(kParam_OutputGain) / 20.0);
	UInt32 channelNum = GetChannelNum();
		
	boost::shared_ptr<Convolver::TimeBlock> outputPtr;
	
	if (wetDryMix == 0.0f) {
		boost::shared_ptr<Convolver::TimeBlock>	result(new Convolver::TimeBlock(inFramesToProcess));
		memset(result->cArray(), 0, sizeof(float) * inFramesToProcess);
		outputPtr = result;
	} else if (this->convolvotron->zeroLatencyMode) {
		assert(inFramesToProcess == blockSize);
		
		UInt32 irNum = channelNum;
		if (irNum > convolvotron->irs.size()) irNum = 0;
		convolvotron->convolver->convolve((const float *)inSourceP, inFramesToProcess, 
												   *convolvotron->irs[irNum], convolverState);
		outputPtr = convolverState.pop();
	} 
	
	Convolver::TimeBlock &output = *outputPtr;
	
	// Apply Gain, Wet/Dry mix
	Float32 dryGain = outGain * sqrt(1.0 - wetDryMix);
	Float32 wetGain = outGain * sqrt(wetDryMix);	
	for (uint32_t i=0; i < inFramesToProcess; i++){
		inDestP[i] = (output[i] * wetGain) + (inSourceP[i] * dryGain);
	}
	
	float biggest = *std::max_element(inDestP, inDestP+inFramesToProcess);	
	if (biggest > Convolvotron::peak) {
		Convolvotron::peak = biggest;
	}
}
 */

