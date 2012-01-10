/*
 *  AUCarbonViewCocoaWrapper.h
 *
 *  Carbon wrapper for an AU Cocoa UI, under BSD license.
 *
 *  Created by Seth Nickell <seth@meatscience.net> on 7/25/09.
 *  Bugfixes & improvements appreciated.
 *
 *
 *  Make an Audio Unit with a Cocoa UI and discover it doesn't work in
 *  Ableton Live and other Carbon-AU-only hosts? This provides a Carbon UI
 *  that wraps your Cocoa.
 *
 *  Queries the AU for its CocoaUI property, instantiates the
 *  returned NSView, wraps it in an HICocoaView, and creates
 *  an associated AUCarbonViewBase.
 *
 *  NOTE: because of HICocoaView, this will only work on MacOS X 10.5 and greater
 *
 *  TO USE you'll need to edit MyAU.r and MyAU.exp (in "AU Source" group if you used Xcode templates):
 *
 *  1) include AUCarbonViewCocoaWrapper.[h|mm] in the main target of your AU Component.
 *  2) Edit your resource file "MyAU.r": Add a new resource ID for the Carbon view:
 *		below:
 *			#define kAudioUnitResID_MyAU					1000
 *		add:
 *			#define kAudioUnitResID_MyAUView				2000
 *  3) Edit your resource file "MyAU.r": Add a new resource section for the Carbon view:
 *		below:
 *			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ MyAU~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *				#define RES_ID		kAudioUnitResID_MyAU
 *				....
 *			#include "AUResources.r"
 *		add: 
 *			 //~~~~~~~~~~~~~~~~~~ MyAU View ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *			 #define RES_ID			kAudioUnitResID_MyAUView
 *			 #define COMP_TYPE		kAudioUnitCarbonViewComponentType
 *			 #define COMP_SUBTYPE	Convolution_COMP_SUBTYPE
 *			 #define COMP_MANUF		Convolution_COMP_MANF	
 *			 
 *			 #define VERSION		kMyAUVersion
 *			 #define NAME			"MyAU (carbon)"
 *			 #define DESCRIPTION	"MyAU Custom Carbon AU View"
 *			 #define ENTRY_POINT	"AUCarbonViewCocoaWrapperEntry"
 *			 
 *			 #include "AUResources.r"
 *
 *		NOTE: you should have two #include "AUResources.r" lines when done, one below each section
 *	4) Edit the "Exported Symbols File" for your AU component, MyAU.exp:
 *		below:
 *			_MyAUEntry
 *		add:
 *			_AUCarbonViewCocoaWrapperEntry
 *
 *
 *  BSD License:
 *
 *  Copyright (c) 2009, Meatscience, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without modification, are
 *  permitted provided that the following conditions are met:
 *
 *  Redistributions of source code must retain the above copyright notice, this list of
 *  conditions and the following disclaimer. Redistributions in binary form must reproduce the
 *  above copyright notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution. Neither the name of
 *  Meatscience nor the names of its contributors may be used to endorse or promote products
 *  derived from this software without specific prior written permission. THIS SOFTWARE IS
 *  PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __AUCarbonViewCocoaWrapper__H_
#define __AUCarbonViewCocoaWrapper__H_

/* These sizes are overridden by the Cocoa UI, if it specifies a size */
#define _CARBON_UI_WRAPPER_DEFAULT_WIDTH		500
#define _CARBON_UI_WRAPPER_DEFAULT_HEIGHT		350

#define _CARBON_UI_WRAPPER_ADD_COMPONENT_ENTRY	1
#define _CARBON_UI_WRAPPER_DEBUG				0


#import "AUCarbonViewBase.h"
#import "AUControlGroup.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>


class AUCarbonViewCocoaWrapper : public AUCarbonViewBase {
	
public:
	AUCarbonViewCocoaWrapper(AudioUnitCarbonView auv);
	
	virtual OSStatus CreateUI (Float32	inXOffset, Float32 	inYOffset);
	
protected:
	virtual NSView *loadCocoaView();
	virtual Class loadAUViewFactoryClass();
	
	// Width & Height to use if the Cocoa UI doesn't override
	UInt32 defaultWidth;
	UInt32 defaultHeight;
};


#endif