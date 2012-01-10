/*
 *  CarbonView.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/23/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "CarbonView.h"

OSStatus	CarbonView::CreateUI(Float32 xoffset, Float32 yoffset)
{
#if _CARBON_UI_WRAPPER_DEBUG
	cout << "AUCarbonViewCocoaWrapper::CreateUI(" << xoffset << ", " << yoffset << ")" << endl;
#endif
	
#if 0
	
	OSStatus result;
	
	IBNibRef 		nibRef;
	
    // Create a Nib reference passing the name of the nib file (without the .nib extension)
    // CreateNibReference only searches into the application bundle.
    result = CreateNibReference( CFSTR( "MeatscienceConvolvotron_CarbonView" ), &nibRef );
	require_noerr( err, CantGetNibRef );

    err = CreateWindowFromNib( nibRef, CFSTR( "MainWindow" ), &window );
    require_noerr( err, CantCreateWindow );	
	
	err = GetControlByID( window, &kDialControl, &view );
	require_noerr( err, CantGetControlByID );
	
	/*
	NSView *cocoaView = this->loadCocoaView();
	assert(cocoaView);
	NSSize cocoaViewSize = [cocoaView frame].size;
	
	HIViewRef hiView;
	result = HICocoaViewCreate(cocoaView, 0, &hiView);
	assert(result == noErr);
	SizeControl(hiView, cocoaViewSize.width, cocoaViewSize.height);	
	HIViewSetVisible(hiView, true);
	*/
	
	/* Add it to the AudioUnitCarbonView */
	verify_noerr(EmbedControl(hiView));
#endif
	
	return noErr;
}