/*
*	File:		ConvolvotronVersion.h
* 	
*	Version:	1.0
* 
*	Created:	5/26/09
*	
*	Copyright:  Copyright © 2009 Meatscience, All Rights Reserved
* 
*/

#ifndef __ConvolvotronVersion_h__
#define __ConvolvotronVersion_h__

#include "DebugSettings.h"

#ifdef DEBUG_VERSIONS
	#define kConvolutionVersion 0xFFFFFFFF
#else
	#define kConvolutionVersion 0x00010002
#endif

#if DEMO
#define Convolution_COMP_SUBTYPE		'cvRD'
#define Convolution_COMP_MANF			'MSci'
#else
#define Convolution_COMP_SUBTYPE		'cnvR'
#define Convolution_COMP_MANF			'MSci'
#endif

#endif

