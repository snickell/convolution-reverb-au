//
//  MeatScienceConvolvotron_SampleView.m
//  Convolvotron
//
//  Created by Seth Nickell on 6/15/09.
//  Copyright 2009 Meatscience. All rights reserved.
//

#import "MeatScienceConvolvotron_SampleView.h"
#import "ConvolvotronProperties.h"

@implementation MeatScienceConvolvotron_SampleView

#define kRightMargin		30
#define kLeftMargin			30
#define kBottomMargin		5
#define kTopMargin			5

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
		mGraphFrame = NSMakeRect(kLeftMargin, kTopMargin, 0, 0);
		[self setGraphFrameSize: frame.size];
		NSBundle * bundle = [NSBundle bundleWithIdentifier: @"net.meatscience.Convolvotron"];
		NSString * path = [bundle pathForResource:@"logo" ofType:@"png"];
		
		meatscienceLogo = [[NSImage alloc] initWithContentsOfFile: path];
		if (!meatscienceLogo) {
			printf("LOADING TITLE IMAGE FAILED\n");
		}
		
		lastPeak = 0.0f;
		
		meatscienceLogoRect = NSMakeRect(0.0f, 0.0f, 0.0f, 0.0f);
		meatscienceLogoRect.size = [meatscienceLogo size];
    }
    return self;
}

-(void) dealloc {
	[mCurvePath release];
	[mBackgroundCache release];
	[mSampleCurveCache release];
	
	[super dealloc];
}

- (void)drawMeatscienceLogo:(NSRect)rect {
	NSAffineTransform* xform = [NSAffineTransform transform];
	
	Float32 scaleBump = 0.0;
		
	// Add the transformations
	[xform translateXBy: 0.0 yBy:0.0];
	//[xform rotateByDegrees:90.0]; // counterclockwise rotation
	[xform scaleXBy:0.75+scaleBump yBy:0.75+scaleBump];
		
	// Apply the changes
	[xform concat];
		
	NSRect imageRect = [meatscienceLogo alignmentRect];
	Float32 logoAlpha = peak;
	if (logoAlpha < 0.3) logoAlpha = 0.3;
	[meatscienceLogo drawInRect: imageRect fromRect: imageRect operation: NSCompositeSourceOver fraction: logoAlpha];
		
	[xform invert];
	[xform concat];		
}

- (void)drawBackground:(NSRect)rect {
	// fill the graph area
	[[NSColor colorWithDeviceWhite: 1.0 alpha: 1.0] set];
	NSRectFill(rect);
	
	// FIXME: lets skip caching for now
	return;
	
	if (!mBackgroundCache) {
		mBackgroundCache = [[NSImage alloc] initWithSize: [self frame].size];
		
		[mBackgroundCache lockFocus];
		// fill the graph area
		[[NSColor colorWithDeviceWhite: 1.0 alpha: 1.0] set];
		NSRectFill(rect);
		
		[mBackgroundCache unlockFocus];
	}

	
	[mBackgroundCache drawInRect: rect fromRect: rect operation: NSCompositeSourceOver fraction: 1.0];
}

- (void)drawSampleCurve:(NSRect)rect {
	if (!mSampleCurveCache) {

		
		mSampleCurveCache = [[NSImage alloc] initWithSize: [self frame].size];
		
		[mSampleCurveCache lockFocus];

		// Draw grey base line

		[NSBezierPath setDefaultLineWidth: 1.0f];
		 [[NSColor colorWithDeviceWhite: .0 alpha: 0.5] set];		
		 [NSBezierPath strokeLineFromPoint: NSMakePoint(rect.origin.x, rect.origin.y + (rect.size.height * 0.5f))
		 toPoint: NSMakePoint(rect.origin.x + rect.size.width, rect.origin.y + (rect.size.height * 0.5f))];
		 
		
		if (mCurvePath) {
#if DEBUG_COCOA
			printf("[SampleView drawRect]: re-drawing curve cache\n");
#endif
			
			[[NSColor colorWithDeviceRed: 0.5 green: 0.0 blue: 0.0 alpha: .8] set];		
			[NSBezierPath setDefaultLineWidth: 2.0f];
			[mCurvePath stroke];
			[[NSColor colorWithDeviceRed: 0.8 green: 0.6 blue: 0.6 alpha: .8] set];		
			[mCurvePath fill];
		} else {
#if DEBUG_COCOA
			printf("[SampleView drawRect]: re-drawing curve cache, NO CURVE TO DRAW\n");
#endif		
		}
		
		[mSampleCurveCache unlockFocus];
	}

	[mSampleCurveCache drawInRect: rect fromRect: rect operation: NSCompositeSourceOver fraction: 1.0];
}


- (void)drawRect:(NSRect)rect {
	#if DEBUG_COCOA_DRAWING
	printf("[SampleView drawRect]\n");
	#endif
	
	[self drawBackground: rect];
	[self drawMeatscienceLogo: rect];
	[self drawSampleCurve: rect];
}

-(void) setPeak: (Float32) inPeak {
	Float32 newPeak = sqrt(inPeak * 2);
	if (newPeak > 1.0) newPeak = 1.0;
	
	if (newPeak > peak || peak - newPeak < 0.15) {
		peak = newPeak;
	} else {
		peak -= 0.15;
	}

	if (peak > lastPeak + 0.05 || peak < lastPeak - 0.05) {
		lastPeak = peak;
		[self setNeedsDisplayInRect: meatscienceLogoRect];
	}
}

-(void) plotData: (float *) data size: (UInt32) size {
	#if DEBUG_COCOA
	printf("\n[SampleView plotData]: plotting %u samples\n", size);
	#endif

	
	CGFloat height = mGraphFrame.size.height * 0.5f;
	CGFloat width = mGraphFrame.size.width;
	
	NSPoint origin = {mGraphFrame.origin.x, mGraphFrame.origin.y + height};
	CGFloat sampleWidth = mGraphFrame.size.width / size;
		
	[mCurvePath release];												// release previous bezier path
	mCurvePath = [[NSBezierPath bezierPath] retain];					// create a new default empty path
	[mCurvePath moveToPoint: origin];						// start the bezier path at the bottom left corner of the graph
	
	int step = 3;
	int increment = (int)(((float)size / width) * (float)step);
	if (increment < 1) increment = 1;
	
	#if DEBUG_COCOA
	printf("[SampleView plotData]: Increment is %d, width is %3.2f\n", increment, width);
	#endif

	int i, j, lastI;
	float sample;
	for(i=0; i < size; i+=increment) {		
		lastI = i;
		sample = data[i];
		for (j=0; j < increment; j++) {
			if (i+j >= size) break;
			if (data[i+j] > sample)
				sample = data[i+j];
		}
		
		if (sample >= 0.0f) {
			NSPoint point = NSMakePoint(origin.x + sampleWidth * i, origin.y + height * sample);
			#if DEBUG_COCOA_DRAWING
			printf("\t[SampleView plotData]: plotting point (%3.2f, %3.f).... data[%d]=%3.3f\n", point.x, point.y, i, data[i]);			
			#endif
			[mCurvePath lineToPoint: point];
		}
	}
	
	for(i=lastI; i >= 0; i-=increment) {		
		lastI = i;
		sample = data[i];
		for (j=0; j < increment; j++) {
			if (i+j >= size) break;			
			if (data[i+j] < sample)
				sample = data[i+j];
		}
		
		if (sample <= 0.0f) {
			NSPoint point = NSMakePoint(origin.x + sampleWidth * i, origin.y + height * sample);
			//printf("\tSampleView: point = (%3.2f, %3.f).... data[%d]=%3.3f\n", point.x, point.y, i, data[i]);			
			//printf("\tdata[%d] = %3.6f\n", i, data[i]);
			[mCurvePath lineToPoint: point];
		}
	}
	
	[mCurvePath lineToPoint: origin];	
	//[mCurvePath lineToPoint: NSMakePoint(origin.x + mGraphFrame.size.width, origin.y)];	// set the final point to the lower right hand corner of the graph

	// Flush the sample curve cache
	[mSampleCurveCache release];
	mSampleCurveCache = nil;
	
	[self setNeedsDisplay: YES];										// mark the graph as needing to be updated
	
	#if DEBUG_COCOA
	printf("[SampleView plotData]: done plotting...\n\n");
	#endif	
}

/* update the graph frame and edit point when the view frame size changes */
-(void) setFrameSize: (NSSize) newSize {
	[self setGraphFrameSize: newSize];
	
	[mSampleCurveCache release];
	mSampleCurveCache = nil;	
	
	[mBackgroundCache release];
	mBackgroundCache = nil;
	
	[super setFrameSize: newSize];
}


/* update the graph frame and edit point when the view frame changes */
-(void) setFrame: (NSRect) frameRect {
	[self setGraphFrameSize: frameRect.size];
	
	[mSampleCurveCache release];
	mSampleCurveCache = nil;	
	
	[mBackgroundCache release];
	mBackgroundCache = nil;
	
	[super setFrame: frameRect];	
}

#pragma mark ____ PRIVATE FUNCTIONS ____

-(void) setGraphFrameSize: (NSSize) size {
	mGraphFrame.size.width = size.width - kLeftMargin - kRightMargin;
	mGraphFrame.size.height = size.height - kBottomMargin - kTopMargin;
}

@end
