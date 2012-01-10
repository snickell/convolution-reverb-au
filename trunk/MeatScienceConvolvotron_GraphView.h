//
//  MeatScienceConvolvotron_GraphView.h
//  Convolvotron
//
//  Created by Seth Nickell on 5/26/09.
//  Copyright 2009 Meatscience. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "ConvolvotronProperties.h"

@interface MeatScienceConvolvotron_GraphView : NSView {
	NSRect	mGraphFrame;		// This is the frame of the drawing area of the view
	float	mActiveWidth;		// The usable portion of the graph
	NSPoint mEditPoint;			// This is the current location in the drawing area that is active
	BOOL	mMouseDown;			// True if the mouse is currently down
	
	NSColor *curveColor;		// the current color of the graph curve
	
	NSImage *mBackgroundCache;	// An image cache of the background so that we don't have to re-draw the grid lines and labels all the time
	
	float mRes;					// internal copy of the resonance value
	float mFreq;				// internal copy of the frequency value
	
	NSBezierPath *mCurvePath;	// The bezier path that is used to draw the curve.
	
	NSDictionary *mDBAxisStringAttributes;		// Text attributes used to draw the strings on the db axis
	NSDictionary *mFreqAxisStringAttributes;	// Text attributes used to draw the strings on the frequency axis
	
}

-(void) setRes: (float) res;	// sets the graph's internal resonance value (to match the au)
-(void) setFreq: (float) freq;	// sets the graphs' internal frequency value (to match the au

-(float)getRes;					// gets the graph's internal resonance value (so the au can match the graph)
-(float)getFreq;				// gets the graph's internal frequency value (so the au can match the graph)

-(double) locationForFrequencyValue: (double) value;	// converts a frequency value to the pixel coordinate in the graph
-(double) locationForDBValue: (double) value;			// converts a db value to the pixel coordinate in the graph

-(FrequencyResponse *) prepareDataForDrawing: (FrequencyResponse *) data;	// prepares the data for drawing by initializing frequency fields based on values on pixel boundaries
-(void) plotData: (FrequencyResponse *) data;								// draws the curve data
-(void) disableGraphCurve;													// update the view, but don't draw the curve (used when the AU is not initialized and the curve can not be retrieved)

-(void) handleBeginGesture;		// called when parameter automation started
-(void) handleEndGesture;		// called when parameter automation finished

@end
