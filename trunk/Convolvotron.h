/*
*	File:		Convolvotron.h
*	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright © 2009 Meatscience, All Rights Reserved
* 
*/


#define DEBUG 1

#if DEBUG
#define CoreAudio_Debug 1
#endif

#include "ConvolutionVersion.h"
#include "ConvolvotronProperties.h"

#include "AUEffectBase.h"

#include <AudioUnit/AudioUnitCarbonView.h>

#include <AudioToolbox/AudioToolbox.h>

#include <Convolver.h>
#include <ConvolverFilter.h>

#include "AUConvolver.h"

#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>


#if AU_DEBUG_DISPATCHER
	#include "AUDebugDispatcher.h"
#endif

#include <pthread.h>


#ifndef __Convolvotron_h__
#define __Convolvotron_h__


#pragma mark ____Convolvotron Parameters

typedef struct {
	int parameter;
	CFStringRef name;
	float minValue;
	float maxValue;
	float defaultValue;
	AudioUnitParameterUnit unit;
	UInt32 flags;
} ParameterDescription;


ParameterDescription parameters[] = {
	{kParam_WetDry, CFSTR("Dry/Wet Mix"), 0.0, 100.0, 70.0, kAudioUnitParameterUnit_EqualPowerCrossfade, 0},
	{kParam_OutputGain, CFSTR("Output Gain"), -40.0, 12.0, 0.0, kAudioUnitParameterUnit_Decibels, 0},
	/*{kParam_StereoSeparation, CFSTR("Stereo Separation"), 0.0, 100.0, 100.0, kAudioUnitParameterUnit_EqualPowerCrossfade, 0}
	{kParam_PreDelay, CFSTR("Pre-delay"), 0.0, 250.0, 0.0, kAudioUnitParameterUnit_Milliseconds, 0},
	{kParam_VolumeCompensation, CFSTR("Volume Compensation"), 0.0, 1.0, 1.0, kAudioUnitParameterUnit_Boolean, 0}*/
};

//static const UInt32 irBufferSize = 32768;

#pragma mark ____Convolvotron
class Convolvotron : public AUEffectBase
{
public:
	Convolvotron(AudioUnit component);
	virtual ~Convolvotron ();

	virtual ComponentResult		Initialize();
	
	
	/*! @method ProcessBufferLists */
	virtual OSStatus			ProcessBufferLists(AudioUnitRenderActionFlags &	ioActionFlags,
												   const AudioBufferList &			inBuffer,
												   AudioBufferList &				outBuffer,
												   UInt32							inFramesToProcess );	
	
	virtual	OSStatus			GetParameterValueStrings(AudioUnitScope			inScope,
														 AudioUnitParameterID		inParameterID,
														 CFArrayRef *			outStrings);
    
	virtual	OSStatus			GetParameterInfo(AudioUnitScope			inScope,
												 AudioUnitParameterID	inParameterID,
												 AudioUnitParameterInfo	&outParameterInfo);
    
	virtual OSStatus			GetPropertyInfo(AudioUnitPropertyID		inID,
												AudioUnitScope			inScope,
												AudioUnitElement		inElement,
												UInt32 &			outDataSize,
												Boolean	&			outWritable );
	
	virtual OSStatus			GetProperty(AudioUnitPropertyID inID,
											AudioUnitScope 		inScope,
											AudioUnitElement 		inElement,
											void *			outData);
	
	virtual OSStatus		SetProperty(	AudioUnitPropertyID 			inID,
											AudioUnitScope 					inScope,
											AudioUnitElement 				inElement,
											const void *					inData,
											UInt32 							inDataSize);	
	
	virtual ComponentResult		SaveState(CFPropertyListRef * outData);
	virtual ComponentResult		RestoreState(CFPropertyListRef plist);	

	virtual  bool        SupportsTail () { return true; }
    virtual Float64        GetTailTime();
    virtual Float64        GetLatency();
	
	/*! @method Version */
	virtual OSStatus		Version() { return kConvolutionVersion; }
	
	void LoadIR(std::string &filename, bool inBackground=false);
	void LoadIRInBackground(std::string &filename);	

	int		GetNumCustomUIComponents () { return 1; }
	
	void	GetUIComponentDescs (ComponentDescription* inDescArray) {
        inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
        inDescArray[0].componentSubType = Convolution_COMP_SUBTYPE;
        inDescArray[0].componentManufacturer = Convolution_COMP_MANF;
        inDescArray[0].componentFlags = 0;
        inDescArray[0].componentFlagsMask = 0;
	}	
	
protected:
	boost::shared_ptr<Convolver::AUConvolver> auConvolver;
	
	// We need a mutex because the background thread can scribble us
	boost::shared_ptr<Convolver::IR> ir;
	std::map<std::string, boost::shared_ptr<Convolver::IR> > filenameToIRCache;
	
	pthread_mutex_t filtersMutex;
	
	std::string irFilename;
	pthread_t irLoadingThread;
	IRLoadingStatus irLoadingStatus;

	Float64 tailTime;
	
	static Float32 peak;
	
private:
	uint32_t frameSize;
	void setFrameSize(uint32_t frameSize);
	void LoadUnitIR();
	boost::shared_ptr<Convolver::Filter> getMonoIR();
	void cancelIRLoadingThread();
	void SetIR(std::string &filename, boost::shared_ptr<Convolver::IR > ir);
	
#if DEMO
	time_t endTime;
	time_t lastBlipTime;
	bool demoExpired;
#endif
	
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#endif