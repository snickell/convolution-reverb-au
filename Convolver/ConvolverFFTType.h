/*
 *  ConvolverFFTType.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/14/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifndef _ConvolverFFTType_h__
#define _ConvolverFFTType_h__

#define USE_APPLE_ACCELERATE 0

#if USE_APPLE_ACCELERATE

#include <Accelerate/Accelerate.h>

typedef struct DSPComplexUs {
	float r;
	float i;
} DSPComplexUs;

// Help GDB out
#if DEBUG
typedef DSPComplexUs FreqSample;
#endif
namespace Convolver {
	typedef DSPComplexUs FreqSample;
}

#else

#include "kiss_fftr.h"
// Help GDB out
#if DEBUG
typedef kiss_fft_cpx FreqSample;
#endif
namespace Convolver {
	typedef kiss_fft_cpx FreqSample;
}
#endif // USE_VDSP

#endif