//
//  MeatScienceConvolvotron_SampleView.h
//  Convolvotron
//
//  Created by Seth Nickell on 6/15/09.
//  Copyright 2009 Meatscience. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface MeatScienceConvolvotron_SampleView : NSView {
	NSRect	mGraphFrame;		// This is the frame of the drawing area of the view
	NSImage *mBackgroundCache;	// An image cache of the background so that we don't have to re-draw the grid lines and labels all the time
	NSImage *mSampleCurveCache;
	
	NSImage *meatscienceLogo;
	NSRect meatscienceLogoRect;
	
	Float32 lastPeak;	
	NSBezierPath *mCurvePath;	// The bezier path that is used to draw the curve.
	Float32 peak;
}

-(void) plotData: (float *) data size: (UInt32) size;
-(void) setPeak: (Float32) peak;

#pragma mark ____ PRIVATE FUNCTIONS ____
-(void) setGraphFrameSize: (NSSize) size;

@end
