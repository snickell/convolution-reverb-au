/*
 *  AUCarbonViewCocoaWrapper.cpp
 *
 *  Carbon wrapper for an AU Cocoa UI, under BSD license.
 *
 *  Created by Seth Nickell <seth@meatscience.net> on 7/25/09.
 *  Bugfixes & improvements appreciated.
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

#import "AUCarbonViewCocoaWrapper.h"
#import <AudioUnit/AUCocoaUIView.h>

#if _CARBON_UI_WRAPPER_DEBUG
#import <iostream>
using namespace std;
#endif

#if _CARBON_UI_WRAPPER_ADD_COMPONENT_ENTRY
COMPONENT_ENTRY(AUCarbonViewCocoaWrapper)
#endif

AUCarbonViewCocoaWrapper::AUCarbonViewCocoaWrapper(AudioUnitCarbonView auv) 
	: AUCarbonViewBase(auv), defaultWidth(_CARBON_UI_WRAPPER_DEFAULT_WIDTH), 
	  defaultHeight(_CARBON_UI_WRAPPER_DEFAULT_HEIGHT)
{
	#if _CARBON_UI_WRAPPER_DEBUG
	cout << "AUCarbonViewCocoaWrapper::AUCarbonViewCocoaWrapper()" << endl;
	#endif
	
	// Initialize required Cocoa bits
	NSApplicationLoad();
}

OSStatus	AUCarbonViewCocoaWrapper::CreateUI(Float32 xoffset, Float32 yoffset)
{
	#if _CARBON_UI_WRAPPER_DEBUG
	cout << "AUCarbonViewCocoaWrapper::CreateUI(" << xoffset << ", " << yoffset << ")" << endl;
	#endif
	
	OSStatus result;
	
	/* Load the Cocoa UI from the AU */
	NSView *cocoaView = this->loadCocoaView();
	assert(cocoaView);
	NSSize cocoaViewSize = [cocoaView frame].size;
	
	/* Wrap it in an HICocoaView */
	HIViewRef hiView;
	result = HICocoaViewCreate(cocoaView, 0, &hiView);
	assert(result == noErr);
	SizeControl(hiView, cocoaViewSize.width, cocoaViewSize.height);	
	HIViewSetVisible(hiView, true);
	
	/* Add it to the AudioUnitCarbonView */
	verify_noerr(EmbedControl(hiView));
		
	return noErr;
}

/* Queries the AU for its CocoaUI and returns it */
NSView *AUCarbonViewCocoaWrapper::loadCocoaView() 
{
#if _CARBON_UI_WRAPPER_DEBUG
	cout << "AUCarbonViewCocoaWrapper::loadCocoaView()" << endl;
#endif
	
	Class auViewClass = this->loadAUViewFactoryClass();
	
	if (auViewClass)  {
        NSObject <AUCocoaUIBase> *auViewFactory = [[auViewClass alloc] init];
		assert(auViewFactory);
		
		NSSize size = {defaultWidth, defaultHeight};
		NSView *auView = [auViewFactory uiViewForAudioUnit: mEditAudioUnit withSize: size];
		
		[auViewFactory release];
		
		return auView;
    } else {
		return NULL;
	}
}

/* Queries the AU, loads the bundle, extracts the AUViewFactory class and returns it */
Class AUCarbonViewCocoaWrapper::loadAUViewFactoryClass()
{
	#if _CARBON_UI_WRAPPER_DEBUG
	cout << "AUCarbonViewCocoaWrapper::loadAUViewFactoryClass()" << endl;
	#endif
	
	OSStatus result;
	AudioUnitCocoaViewInfo cocoaViewInfo;
	UInt32 dataSize = sizeof(AudioUnitCocoaViewInfo);
	
	result = AudioUnitGetProperty(mEditAudioUnit,
								  kAudioUnitProperty_CocoaUI,
								  kAudioUnitScope_Global,
								  0,
								  &cocoaViewInfo,
								  &dataSize);
	
	assert(result == noErr);
	assert(cocoaViewInfo.mCocoaAUViewBundleLocation);
	assert(cocoaViewInfo.mCocoaAUViewClass[0]);
	
	CFStringRef bundlePath = CFURLCopyPath(cocoaViewInfo.mCocoaAUViewBundleLocation);
	NSString* nsBundlePath = (NSString*)bundlePath;
	NSBundle *bundle = [NSBundle bundleWithPath: nsBundlePath];
	assert(bundle);
	
	NSString* auViewClassName = (NSString*)cocoaViewInfo.mCocoaAUViewClass[0];
	Class auViewFactoryClass = [bundle classNamed: auViewClassName];

	CFRelease(bundlePath);
	CFRelease(cocoaViewInfo.mCocoaAUViewBundleLocation);
	CFRelease(cocoaViewInfo.mCocoaAUViewClass[0]);
	[bundle release];
	
	return auViewFactoryClass;
}
