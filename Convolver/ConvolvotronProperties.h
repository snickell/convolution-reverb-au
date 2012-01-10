/*
 *  ConvolvotronProperties.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 6/2/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef CONVOLVOTRON_PROPERTIES_H
#define CONVOLVOTRON_PROPERTIES_H

#include "DebugSettings.h"
#include <stdint.h>

#define CONVOLVOTRON_BUNDLE_ID "net.meatscience.ConvolutionReverb"

enum {
    kMyNumberOfResponseFrequencies = 512
};

enum {
	kAudioUnitCustomProperty_IRFile = 66800,
	kAudioUnitCustomProperty_FilterFrequencyResponse,
	kAudioUnitCustomProperty_Samples,
	kAudioUnitCustomProperty_Peak,
	kAudioUnitCustomProperty_IRLoadingStatus,
	kAudioUnitCustomProperty_IRInfo,
	kAudioUnitCustomProperty_IsDemo
};


typedef enum {
	LOAD_STATUS_LOADING,
	LOAD_STATUS_FAILED,
	LOAD_STATUS_LOADED,
	LOAD_STATUS_CANCELED,
	LOAD_ACTION_CANCEL
} LoadStatus;


#define IRLoadingStatus_filename_size 1024

typedef struct IRLoadingStatus {
	char filename[IRLoadingStatus_filename_size];
	LoadStatus status;
} IRLoadingStatus;

typedef struct IRInfo {
	uint32_t numChannels;
	float seconds;
} IRInfo;

enum {
	kParam_WetDry =0,
	kParam_OutputGain,
	/*kParam_StereoSeparation,
	kParam_PreDelay,	
	kParam_VolumeCompensation,*/
	kNumberOfParameters=2
};

typedef struct FrequencyResponse {
		float		mFrequency;
		float		mMagnitude;
} FrequencyResponse;

extern int loadCounter;

#endif