/*
 *  CarbonView.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/23/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _CarbonView_h__
#define _CarbonView_h__

#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"

#include <Carbon/Carbon.h>

class CarbonView : public AUCarbonViewBase {
	CarbonView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv) {}
	
	virtual OSStatus CreateUI (Float32	inXOffset, Float32 	inYOffset);
};

#endif