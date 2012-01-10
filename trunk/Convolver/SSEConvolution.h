/*
 *  SSEConvolution.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 5/31/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */


int multiply_complex_SSE3(float *input, float *filter, float *accumulator, int numComplexNumbers);
//inline void multiply_SSE2(complex_num x, complex_num y, complex_num *z);