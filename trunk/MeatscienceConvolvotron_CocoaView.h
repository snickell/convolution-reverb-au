/*
*	File:		CocoaView.h
*	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright © 2009 Meatscience, All Rights Reserved
*
*/

#import <Cocoa/Cocoa.h>
#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>

#import "ConvolvotronProperties.h"
#import "MeatScienceConvolvotron_GraphView.h"
#import "MeatScienceConvolvotron_SampleView.h"

typedef struct {
	NSSlider **slider;
	NSTextField **textField;
	NSButton **checkbox;
} UIParameter;
	
@interface MeatscienceConvolvotron_CocoaView : NSView
{
    // IB Members
	IBOutlet MeatScienceConvolvotron_GraphView * graphView;
	IBOutlet MeatScienceConvolvotron_SampleView * sampleView;
	
    IBOutlet NSSlider *				wetdrySlider;
    IBOutlet NSTextField *			wetdryTextField;
	IBOutlet NSSlider *				predelaySlider;
	IBOutlet NSTextField *			predelayTextField;
	IBOutlet NSSlider *				gainSlider;
	IBOutlet NSTextField *			gainTextField;
	
	IBOutlet NSButton *				loadIRButton;
	Boolean							loadIRButtonCancels;	
	IBOutlet NSProgressIndicator *	loadIRProgressIndicator;
	
	IBOutlet NSTextField *			fileInfoTextField;
	IBOutlet NSTextField *			isDemoTextField;

	IBOutlet NSButton *				volumeCompensationCheckbox;
	
	IBOutlet NSPopUpButton *		recentIRButton;

	
	
    // Other Members
    AudioUnit 				mAU;
	AUEventListenerRef		mAUEventListener;	
    AUParameterListenerRef	mParameterListener;
	
	FrequencyResponse 	   *mData;				// the data used to draw the filter curve
	float				   *mSamples;
	UInt32					mNumSamples;
	NSColor				   *mBackgroundColor;	// the background color (pattern) of the view
	
	UIParameter uiParameters[kNumberOfParameters];
	NSMutableArray *recentIRs;
}

#pragma mark ____ PUBLIC FUNCTIONS ____
- (void)setAU:(AudioUnit)inAU;

#pragma mark ____ INTERFACE ACTIONS ____
- (IBAction)iaParamChanged:(id)sender;
- (IBAction)iaCheckboxChanged:(id)sender;

- (IBAction)loadIRButtonPressed:(id)sender;
- (IBAction)recentIRButtonPressed:(id)sender;

#pragma mark ____ PRIVATE FUNCTIONS
- (void)_synchronizeUIWithParameterValues;
- (void)_addListeners;
- (void)_removeListeners;
- (void)_updatePeakTimerTick;

#pragma mark ____ LISTENER CALLBACK DISPATCHEE ____
- (void)_parameterListener:(void *)inObject parameter:(const AudioUnitParameter *)inParameter value:(Float32)inValue;
- (void)priv_eventListener:(void *) inObject event:(const AudioUnitEvent *)inEvent value:(Float32)inValue;

@end
