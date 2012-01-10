/*
*	File:		MeatscienceConvolvotron_CocoaView.m
*	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright © 2009 Meatscience, All Rights Reserved
*
*/

#import "MeatscienceConvolvotron_CocoaView.h"
#import "DebugSettings.h"

#pragma mark ____ LISTENER CALLBACK DISPATCHER ____

// This listener responds to parameter changes, gestures, and property notifications
void EventListenerDispatcher (void *inRefCon, void *inObject, const AudioUnitEvent *inEvent, UInt64 inHostTime, Float32 inValue)
{
	MeatscienceConvolvotron_CocoaView *SELF = (MeatscienceConvolvotron_CocoaView *)inRefCon;
	
	[SELF priv_eventListener:inObject event: inEvent value: inValue];
}

//AudioUnitParameter parameter[] = {	{ 0, kParam_One, kAudioUnitScope_Global, 0 }	};
AudioUnitParameter parameter[kNumberOfParameters];

void ParameterListenerDispatcher (void *inRefCon, void *inObject, const AudioUnitParameter *inParameter, Float32 inValue) {
	MeatscienceConvolvotron_CocoaView *SELF = (MeatscienceConvolvotron_CocoaView *)inRefCon;
    
    [SELF _parameterListener:inObject parameter:inParameter value:inValue];
}

@implementation MeatscienceConvolvotron_CocoaView
#pragma mark ____ (INIT /) DEALLOC ____
- (void)dealloc {
	#if DEBUG_COCOA
	printf("[CocoaView dealloc]\n");
	#endif
    [self _removeListeners];
	
	free (mData);	
	
    [super dealloc];
}

- (id)initWithFrame:(NSRect)frameRect
{
	if ((self = [super initWithFrame:frameRect]) != nil) {
		UInt32 i;

		// Initialize all UIParameters to NULL
		for(i=0; i < kNumberOfParameters; i++) {
			uiParameters[i].slider = NULL;
			uiParameters[i].textField = NULL;
			uiParameters[i].checkbox = NULL;
			
			parameter[i].mAudioUnit = 0;
			parameter[i].mParameterID = i;
			parameter[i].mScope = kAudioUnitScope_Global;
			parameter[i].mElement = 0;			
		}
		
		uiParameters[kParam_WetDry].slider = &wetdrySlider;
		uiParameters[kParam_WetDry].textField = &wetdryTextField;
		//uiParameters[kParam_PreDelay].slider = &predelaySlider;
		//uiParameters[kParam_PreDelay].textField = &predelayTextField;
		uiParameters[kParam_OutputGain].slider = &gainSlider;
		uiParameters[kParam_OutputGain].textField = &gainTextField;
		//uiParameters[kParam_VolumeCompensation].checkbox = &volumeCompensationCheckbox;
		
		
		NSTimer *timer;
		timer = [NSTimer scheduledTimerWithTimeInterval:0.1
												 target:self
											   selector: @selector(_updatePeakTimerTick)
											   userInfo:nil
												repeats: YES];
		loadIRButtonCancels = NO;
		
		[recentIRButton removeAllItems];
		recentIRs = [[NSMutableArray arrayWithCapacity: 10] retain];
	}
	
    return self;	
}

#pragma mark ____ PUBLIC FUNCTIONS ____
- (void)setAU:(AudioUnit)inAU {
	// remove previous listeners
	if (mAU) [self _removeListeners];
	
	if (!mData)											// only allocate the data once
		mData = malloc(kMyNumberOfResponseFrequencies * sizeof(FrequencyResponse));
	
	mData = [graphView prepareDataForDrawing: mData];	// fill out the initial frequency values for the data displayed by the graph	
	
	mAU = inAU;
    
	// add new listeners
	[self _addListeners];
	
	// initial setup
	[self _synchronizeUIWithParameterValues];
}

#pragma mark ____ INTERFACE ACTIONS ____
- (IBAction)iaParamChanged:(id)sender {	
	int paramNum = -1;
	bool fromSlider = false;
	int i;
	for(i=0; i < kNumberOfParameters; i++) {
		if (uiParameters[i].slider && *(uiParameters[i].slider) == sender) {
			paramNum = i;
			fromSlider = true;
		} else if (uiParameters[i].textField && *(uiParameters[i].textField) == sender) {
			paramNum = i;
			fromSlider = false;
		}
	}
	
	NSAssert( paramNum != -1, 
			 @"[MeatscienceConvolvotron_CocoaView iaParamChanged:] received values from an unknown control, add to uiParameters.\n");
	
    float floatValue = [sender floatValue];
	
	NSAssert(	AUParameterSet(mParameterListener, sender, &parameter[paramNum], (Float32)floatValue, 0) == noErr,
                @"[MeatscienceConvolvotron_CocoaView iaParam1Changed:] AUParameterSet()");
	
    if (fromSlider) {
        [*uiParameters[paramNum].textField setFloatValue:floatValue];
    } else {
        [*uiParameters[paramNum].slider setFloatValue:floatValue];
    }
	
	[self setNeedsDisplay: YES];
}

- (IBAction)iaCheckboxChanged:(id)sender {	
	int paramNum = -1;
	UInt32 i;
		
	for(i=0; i < kNumberOfParameters; i++) {
		if (uiParameters[i].checkbox && *(uiParameters[i].checkbox) == sender) {
			paramNum = i;
		}
	}
	
	NSAssert( paramNum != -1, 
			 @"[MeatscienceConvolvotron_CocoaView iaCheckboxChanged:] received values from an unknown control, add to uiParameters.\n");
		
	Float32 floatValue = [sender floatValue];
		
	NSAssert(AUParameterSet(mParameterListener, sender, &parameter[paramNum], floatValue, 0) == noErr,
			 @"[MeatscienceConvolvotron_CocoaView iaCheckboxChanged:] AUParameterSet()");
}

- (IBAction)recentIRButtonPressed:(id)sender {
	
	NSInteger index = [sender indexOfSelectedItem];
	
	NSString *filename = [recentIRs objectAtIndex: index];
	const char *fileNameUTF8 = [filename UTF8String];
	
	UInt32 dataSize = strlen(fileNameUTF8) * sizeof(char);
	
	#if DEBUG_COCOA
	printf("[CocoaView recentIRButtonPressed]: changing filename to %s (size=%d)\n", fileNameUTF8, dataSize);
	#endif
		
	ComponentResult result = AudioUnitSetProperty(	mAU,
												  kAudioUnitCustomProperty_IRFile,
												  kAudioUnitScope_Global,
												  0,
												  fileNameUTF8,
												  dataSize);		
}

- (void)cancelIRLoading {
	UInt32 dataSize = sizeof(IRLoadingStatus);
	ComponentResult result;
	
	IRLoadingStatus loadingStatus;
	loadingStatus.status = LOAD_ACTION_CANCEL;
	
	result = AudioUnitSetProperty(mAU,
								  kAudioUnitCustomProperty_IRLoadingStatus,
								  kAudioUnitScope_Global,
								  0,
								  &loadingStatus,
								  &dataSize);
	
	NSAssert ( result == noErr,
			  @"[MeatscienceConvolvotron_CocoaView cancelIRLoading] AudioUnitSetProperty()");
}

- (IBAction)loadIRButtonPressed:(id)sender {
	if (loadIRButtonCancels) {
		[self cancelIRLoading];
		return;
	}
	
	// Create the File Open Dialog class.
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];

	// Enable the selection of files in the dialog.
	[openDlg setCanChooseFiles:YES];
	
	// Enable the selection of directories in the dialog.
	[openDlg setCanChooseDirectories:YES];
	
	// Disallow multiple selection
	[openDlg setAllowsMultipleSelection:NO];
	
	// Display the dialog.  If the OK button was pressed,
	// process the files.
	if ( [openDlg runModalForDirectory:nil file:nil] == NSOKButton )
	{
		// Get an array containing the full filenames of all
		// files and directories selected.
		NSArray* files = [openDlg filenames];
		
		NSString* filename = [files objectAtIndex:0];
		char *fileNameUTF8 = [filename UTF8String];
		
		UInt32 dataSize = strlen(fileNameUTF8) * sizeof(char);
		
		#if DEBUG_COCOA
		printf("[CocoaView loadIRButtonPressed]: changing filename to %s (size=%d)\n", fileNameUTF8, dataSize);
		#endif
				
		ComponentResult result = AudioUnitSetProperty(	mAU,
													  kAudioUnitCustomProperty_IRFile,
													  kAudioUnitScope_Global,
													  0,
													  fileNameUTF8,
													  dataSize);		
		//NSAssert(	AUParameterSet(mParameterListener, sender, &parameter[0], (Float32)floatValue, 0) == noErr,
		//		 @"[MeatscienceConvolvotron_CocoaView iaParam1Changed:] AUParameterSet()");		
	}
	
}




#pragma mark ____ PRIVATE FUNCTIONS ____

- (void) updateCurve {
	// FIXME: Reenable the curve
	return;
	UInt32 dataSize = kMyNumberOfResponseFrequencies * sizeof(FrequencyResponse);
	ComponentResult result = AudioUnitGetProperty(	mAU,
												  kAudioUnitCustomProperty_FilterFrequencyResponse,
												  kAudioUnitScope_Global,
												  0,
												  mData,
												  &dataSize);
	if (result == noErr)
		[graphView plotData: mData];	// plot the new curve data and redraw the graph
	else if (result == kAudioUnitErr_Uninitialized)
		[graphView disableGraphCurve];
}


- (void) updateSamples {
	UInt32 dataSize;
	Boolean writable;
	ComponentResult result;
	
	result = AudioUnitGetPropertyInfo(mAU, kAudioUnitCustomProperty_Samples, kAudioUnitScope_Global, 0, &dataSize, &writable);
	mNumSamples = dataSize / sizeof(float);	

	if (result != noErr) {
		char *chars = (char *)&result;
		printf("[CocoaView updateSamples]: ERROR AUGetPropertyInfo(): %s (%c%c%c%c)\n", GetMacOSStatusErrorString(result), chars[0], chars[1], chars[2], chars[3]);
	}	
	
	if(mSamples) free(mSamples);
	
	mSamples = malloc(dataSize);
	
	result = AudioUnitGetProperty(	mAU,
												  kAudioUnitCustomProperty_Samples,
												  kAudioUnitScope_Global,
												  0,
												  mSamples,
												  &dataSize);
	if (result == noErr) {
		#if DEBUG_COCOA
		printf("[CocoaView updateSamples]: plotting %d samples\n", mNumSamples);
		#endif
		[sampleView plotData: mSamples size: mNumSamples];	// plot the new curve data and redraw the graph
	} else if (result == kAudioUnitErr_Uninitialized) {
		printf("[CocoaView updateSamples]: ERROR, audio unit uninitialized\n");
		
		[sampleView disableGraphCurve];
	} else {
		char *chars = (char *)&result;
		printf("[CocoaView updateSamples]: ERROR AUGetProperty(): %s (%c%c%c%c)\n", GetMacOSStatusErrorString(result), chars[0], chars[1], chars[2], chars[3]);
		
	}
}

- (void)_addListeners {
	NSAssert (	AUListenerCreate(	ParameterListenerDispatcher, self, 
                                    CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.100, // 100 ms
                                    &mParameterListener	) == noErr,
                @"[MeatscienceConvolvotron_CocoaView _addListeners] AUListenerCreate()");
	
    int i;
    for (i = 0; i < kNumberOfParameters; ++i) {
        parameter[i].mAudioUnit = mAU;
        NSAssert (	AUListenerAddParameter (mParameterListener, NULL, &parameter[i]) == noErr,
                    @"[MeatscienceConvolvotron_CocoaView _addListeners] AUListenerAddParameter()");
    }
	
	if (mAU) {
		verify_noerr( AUEventListenerCreate(EventListenerDispatcher, self,
											CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.05, 0.05, 
											&mAUEventListener));
		AudioUnitEvent auEvent;
		/* Add a listener for the changes in our custom property */
		/* The Audio unit will send a property change when the unit is intialized */		
		auEvent.mEventType = kAudioUnitEvent_PropertyChange;
		auEvent.mArgument.mProperty.mAudioUnit = mAU;
		auEvent.mArgument.mProperty.mPropertyID = kAudioUnitCustomProperty_Samples;
		auEvent.mArgument.mProperty.mScope = kAudioUnitScope_Global;
		auEvent.mArgument.mProperty.mElement = 0;
		verify_noerr (AUEventListenerAddEventType (mAUEventListener, self, &auEvent));
		
		auEvent.mArgument.mProperty.mPropertyID = kAudioUnitCustomProperty_IRLoadingStatus;
		verify_noerr (AUEventListenerAddEventType (mAUEventListener, self, &auEvent));
	}
}

- (void)drawRect:(NSRect)rect {
	[super drawRect: rect];
}

- (void)_updatePeakTimerTick {
	UInt32 dataSize =  sizeof(Float32);
	Float32 peak;
	ComponentResult result = AudioUnitGetProperty(mAU,
												  kAudioUnitCustomProperty_Peak,
												  kAudioUnitScope_Global,
												  0,
												  &peak,
												  &dataSize);
	if (result == noErr) {
		[sampleView setPeak: peak];
	}
}

- (void)_removeListeners {
    int i;
    for (i = 0; i < kNumberOfParameters; ++i) {
        NSAssert (	AUListenerRemoveParameter(mParameterListener, NULL, &parameter[i]) == noErr,
                    @"[MeatscienceConvolvotron_CocoaView _removeListeners] AUListenerRemoveParameter()");
    }
    
	NSAssert (	AUListenerDispose(mParameterListener) == noErr,
                @"[MeatscienceConvolvotron_CocoaView _removeListeners] AUListenerDispose()");

	NSAssert (	AUListenerDispose(mAUEventListener) == noErr,
			  @"[MeatscienceConvolvotron_CocoaView _removeListeners] AUListenerDispose()");	
}

- (void)updateRecentIRButton {
	#if DEBUG_COCOA
	printf("[CocoaView updateRecentIRButton]\n");
	#endif
	
	UInt32 dataSize;
	Boolean	writable;
	ComponentResult result;
	
	result = AudioUnitGetPropertyInfo(	mAU,
								  kAudioUnitCustomProperty_IRFile,
								  kAudioUnitScope_Global,
								  0,
								  &dataSize,
								  &writable);
	
	NSAssert (	result == noErr,
			  @"[MeatscienceConvolvotron_CocoaView updateRecentIRButton] AUGetPropertInfo()");
	
	char *irFileName = malloc(dataSize * sizeof(char));
	
	result = AudioUnitGetProperty(	mAU,
												  kAudioUnitCustomProperty_IRFile,
												  kAudioUnitScope_Global,
												  0,
												  irFileName,
												  &dataSize);
	
	NSAssert ( result == noErr,
			  @"[MeatscienceConvolvotron_CocoaView updateRecentIRButton] AudioUnitGetProperty()");
	
	#if DEBUG_COCOA
	printf("[CocoaView updateRecentIRButton]: file name is %s\n", irFileName);
	#endif
	
	if (irFileName[0] == '\0') {
		// Its the unit impulse
		// FIXME: how do we want to handle the unit impulse?
		return;
	}
	
	NSString *irFilenameNS = [[NSString alloc] initWithUTF8String: irFileName];
	NSString *irFilenamePrettyPrint = [irFilenameNS substringFromIndex: [irFilenameNS rangeOfString: @"/" options: NSBackwardsSearch].location + 1];
	[recentIRButton insertItemWithTitle: irFilenamePrettyPrint atIndex: 0];
	
	if ([recentIRs containsObject: irFilenameNS]) { 
		[recentIRs removeObject: irFilenameNS];
	}
	[recentIRs insertObject: irFilenameNS atIndex: 0];
	[recentIRButton selectItemAtIndex: 0];
	if ([recentIRs count] > 3) {
		[recentIRs removeObjectAtIndex: 3];
		[recentIRButton removeItemAtIndex: 3];
	}
	
	//[recentIRButton	setEnabled: [recentIRs count] > 0];
	
	[irFilenameNS release];
}

- (void)updateFileInfoTextField {
	UInt32 dataSize = sizeof(IRInfo);
	IRInfo info;
	ComponentResult err;

	err = AudioUnitGetProperty (mAU,kAudioUnitCustomProperty_IRInfo, kAudioUnitScope_Global,0,&info,&dataSize);	
		
	NSAssert ( err == noErr,
			  @"[MeatscienceConvolvotron_CocoaView updateFileInfoTextField] AudioUnitGetProperty()");
	
	NSString *string;
	if (info.numChannels == 1) {
		string = [NSString stringWithFormat: @"Mono, %3.1f seconds", info.seconds];
	} else if (info.numChannels == 2) {
		string = [NSString stringWithFormat: @"Split Stereo, %3.1f seconds", info.seconds];
	} else if (info.numChannels == 4) {
		string = [NSString stringWithFormat: @"True Stereo, %3.1f seconds", info.seconds];
	} else {
		string = [NSString stringWithFormat: @"%d-channel, %3.1f seconds", info.numChannels, info.seconds];
	}
	[fileInfoTextField setStringValue: string];
}

- (void)updateDemoTextField {
	UInt32 dataSize = sizeof(int);
	int isDemo;
	ComponentResult err;
	
	err = AudioUnitGetProperty (mAU,kAudioUnitCustomProperty_IsDemo, kAudioUnitScope_Global,0,&isDemo,&dataSize);	
	
	NSAssert ( err == noErr,
			  @"[MeatscienceConvolvotron_CocoaView updateDemoTextField] AudioUnitGetProperty()");
	
	printf("Result of isDemo is: %d\n", isDemo);
	
	[isDemoTextField setHidden: !isDemo];
}


- (void)updateIRLoadingStatus {
	UInt32 dataSize = sizeof(IRLoadingStatus);
	ComponentResult result;
	
	IRLoadingStatus loadingStatus;
	
	result = AudioUnitGetProperty(	mAU,
								  kAudioUnitCustomProperty_IRLoadingStatus,
								  kAudioUnitScope_Global,
								  0,
								  &loadingStatus,
								  &dataSize);
	
	NSAssert ( result == noErr,
			  @"[MeatscienceConvolvotron_CocoaView priv_eventListener] AudioUnitGetProperty()");
	
	if (loadingStatus.status == LOAD_STATUS_LOADING) {
		#if DEBUG_COCOA
		printf("\n[CocoaView priv_eventListener]: loading file %s\n", loadingStatus.filename);
		#endif
		[loadIRProgressIndicator startAnimation: self];
		loadIRButtonCancels = YES;
		[loadIRButton setTitle: @"Cancel"];
	} else {
		[loadIRProgressIndicator stopAnimation: self];
		loadIRButtonCancels = NO;		
		[loadIRButton setTitle: @"Load IR..."];
		
		if (loadingStatus.status == LOAD_STATUS_FAILED) {
			#if DEBUG_COCOA
			printf("\n[CocoaView priv_eventListener]: failed to load file %s\n", loadingStatus.filename);
			#endif							
			NSAlert *alert = [NSAlert alertWithMessageText: @"Failed to load IR" defaultButton: nil alternateButton: nil otherButton: nil informativeTextWithFormat: @"Failed to load an impulse response from file %s. Is it a valid WAV/AIFF/SDIR file?", loadingStatus.filename];
			[alert runModal];
		} else if (loadingStatus.status == LOAD_STATUS_CANCELED) {
			#if DEBUG_COCOA
			printf("\n[CocoaView priv_eventListener]: cancelled loading file %s\n", loadingStatus.filename);
			#endif
		} else {
			#if DEBUG_COCOA
			printf("\n[CocoaView priv_eventListener]: done loading file %s\n", loadingStatus.filename);
			#endif
		}						
	}
}



- (void)_synchronizeUIWithParameterValues {
	Float32 value;
    int i;
    
    for (i = 0; i < kNumberOfParameters; ++i) {
        // only has global parameters
        NSAssert (	AudioUnitGetParameter(mAU, parameter[i].mParameterID, kAudioUnitScope_Global, 0, &value) == noErr,
                    @"[MeatscienceConvolvotron_CocoaView synchronizeUIWithParameterValues] (x.1)");
        NSAssert (	AUParameterSet (mParameterListener, self, &parameter[i], value, 0) == noErr,
                    @"[MeatscienceConvolvotron_CocoaView synchronizeUIWithParameterValues] (x.2)");
        NSAssert (	AUParameterListenerNotify (mParameterListener, self, &parameter[i]) == noErr,
                    @"[MeatscienceConvolvotron_CocoaView synchronizeUIWithParameterValues] (x.3)");
    }
	
	[self updateRecentIRButton];	
	[self updateCurve];
	[self updateSamples];
	[self updateIRLoadingStatus];
	[self updateFileInfoTextField];
	[self updateDemoTextField];
}

#pragma mark ____ LISTENER CALLBACK DISPATCHEE ____

// Handle kAudioUnitProperty_PresentPreset event
- (void)priv_eventListener:(void *) inObject event:(const AudioUnitEvent *)inEvent value:(Float32)inValue {
	switch (inEvent->mEventType) {
		case kAudioUnitEvent_ParameterValueChange:					// Parameter Changes
			// FIXME: I don't think we should get param changes
			break;
		case kAudioUnitEvent_BeginParameterChangeGesture:			// Begin gesture
			[graphView handleBeginGesture];							// notify graph view to update visual state
			// FIXME: should we also handle the sampleView here?
			break;
		case kAudioUnitEvent_EndParameterChangeGesture:				// End gesture
			[graphView handleEndGesture];							// notify graph view to update visual state
			// FIXME: should we also handle the sampleView here?
			break;
		case kAudioUnitEvent_PropertyChange:						// custom property changed
			switch(inEvent->mArgument.mProperty.mPropertyID) {
				case kAudioUnitCustomProperty_IRFile:
					
					break;
				case kAudioUnitCustomProperty_Samples:
					#if DEBUG_COCOA
					printf("\n[CocoaView priv_eventListener]: kAudioUnitCustomProperty_Samples changed\n");
					#endif	
					
					[self updateRecentIRButton];
					[self updateCurve];
					[self updateSamples];
					[self updateFileInfoTextField];
					
					#if DEBUG_COCOA
					printf("[CocoaView priv_eventListener]: done\n\n");
					#endif						
					break;
				case kAudioUnitCustomProperty_IRLoadingStatus:
					#if DEBUG_COCOA
					printf("\n[CocoaView priv_eventListener]: kAudioUnitCustomProperty_IRLoadingStatus changed\n");
					#endif
					[self updateIRLoadingStatus];
					break;
			}
			break;
	}
}

- (void)_parameterListener:(void *)inObject parameter:(const AudioUnitParameter *)inParameter value:(Float32)inValue {
    //inObject ignored in this case.
	if (inParameter->mParameterID < kNumberOfParameters) {
		UIParameter parameter = uiParameters[inParameter->mParameterID];
		if (parameter.slider) [*parameter.slider    setFloatValue:inValue];
		if (parameter.textField) [*parameter.textField setStringValue:[[NSNumber numberWithFloat:inValue] stringValue]];
		if (parameter.checkbox) [*parameter.checkbox setFloatValue:inValue];
		
		if (!parameter.slider && !parameter.textField && !parameter.checkbox) printf("WARNING: unknown parameter %d\n", inParameter->mParameterID);
	} else {
		printf("WARNING: trying to set unknown parameter %d\n", inParameter->mParameterID);
	}
}


@end
