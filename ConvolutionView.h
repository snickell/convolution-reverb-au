/*
*	File:		ConvolutionView.h
*	
*	Version:	1.0
* 
*	Created:	8/3/09
*	
*	Copyright:  Copyright 2009 Meatscience, All Rights Reserved
*
*/

#ifndef __ConvolutionView__H_
#define __ConvolutionView__H_

#include "ConvolutionVersion.h"
#include "CarbonUIWrapper.h"

class ConvolutionView : public CarbonUIWrapper {
public:
	ConvolutionView(AudioUnitCarbonView auv) : CarbonUIWrapper(auv, 500, 350) {};
};

#endif