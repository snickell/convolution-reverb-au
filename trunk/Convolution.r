/*
*	File:		Convolvotron.r
*	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright © 2009 Meatscience, All Rights Reserved
* 
*/
#include <AudioUnit/AudioUnit.r>

#include "ConvolutionVersion.h"

// Note that resource IDs must be spaced 2 apart for the 'STR ' name and description
#define kAudioUnitResID_Convolution					1000
#define kAudioUnitResID_ConvolutionView				2000

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Convolvotron~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define RES_ID			kAudioUnitResID_Convolution
#define COMP_TYPE		kAudioUnitType_Effect
#define COMP_SUBTYPE	Convolution_COMP_SUBTYPE
#define COMP_MANUF		Convolution_COMP_MANF	

#define VERSION			kConvolutionVersion
#define NAME			"Meatscience: Convolution"
#define DESCRIPTION		"Convolvotron AU"
#define ENTRY_POINT		"ConvolvotronEntry"

#include "AUResources.r"