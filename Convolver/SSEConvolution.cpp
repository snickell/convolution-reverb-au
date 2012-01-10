/*
 *  SSEConvolution.c
 *  Convolvotron
 *
 *  Created by Seth Nickell on 5/31/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */


#if defined __i386__ || defined __x86_64__
#include <emmintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif


#include "SSEConvolution.h"


// Multiplying complex numbers using SSE3 intrinsics
int multiply_complex_SSE3(float *input, float *filter, float *accumulator, int numComplexNumbers)
{
    // Prime the registers
	static float minus1=-1.0f;	
    __m128 mm_data,mm_exp1,mm_exp,mm_exp_c,mm_exp_s,mm_acl =_mm_load_ps1(&minus1);
		
    register int i = 0;
	register int nFloats = numComplexNumbers*2;
    do {
        mm_data = _mm_load_ps(&input[i]);
        mm_exp = _mm_load_ps(&filter[i]);
		mm_acl = _mm_load_ps(&accumulator[i]);
		
        // next two commands SSE3
        mm_exp_c = _mm_moveldup_ps(mm_exp);
        mm_exp_s = _mm_movehdup_ps(mm_exp);

        mm_exp = _mm_mul_ps(mm_exp_c,mm_data);
        mm_exp1 = _mm_mul_ps(mm_exp_s,mm_data);

		//FIXME: what is this?
		// 0b10110001
		// 2  3  0  1
		// number[1]
		// number[0]
		// number[3]
		// number[2]
		// I think this swaps one with two and three with fours
		mm_exp1 = _mm_shuffle_ps(mm_exp1,mm_exp1,0xB1);
		
        // next command is SSE3
        mm_exp = _mm_addsub_ps(mm_exp,mm_exp1);
		
		mm_acl = _mm_add_ps(mm_exp, mm_acl);
		
        _mm_store_ps(&accumulator[i],mm_acl); // aligned FIXME: on OS/X we are gauranteed alignment.
        i += 4;
	} while (i <= nFloats-4);
	return (i - 4) / 2;
};

/*

// Multiplying complex numbers using SSE3 intrinsics
void multiply_SSE3(complex_num x, complex_num y, complex_num *z)
{
	__m128d num1, num2, num3;
	
	// Duplicates lower vector element into upper vector element.
	//   num1: [x.real, x.real]
	
	num1 = _mm_loaddup_pd(&x.real);
	
	// Move y elements into a vector
	//   num2: [y.img, y.real]
	
	num2 = _mm_set_pd(y.img, y.real);
	
	// Multiplies vector elements
	//   num3: [(x.real*y.img), (x.real*y.real)]
	
	num3 = _mm_mul_pd(num2, num1);
	
	//   num1: [x.img, x.img]
	
	num1 = _mm_loaddup_pd(&x.img);
	
	// Swaps the vector elements
	//   num2: [y.real, y.img]
	
	num2 = _mm_shuffle_pd(num2, num2, 1);
	
	//   num2: [(x.img*y.real), (x.img*y.img)]
	
	num2 = _mm_mul_pd(num2, num1);
	
	// Adds upper vector element while subtracting lower vector element
	//   num3: [((x.real *y.img)+(x.img*y.real)),
	//          ((x.real*y.real)-(x.img*y.img))]
	
	num3 = _mm_addsub_pd(num3, num2);
	
	// Stores the elements of num3 into z
	
	_mm_storeu_pd((double *)z, num3);
	
}


// Multiplying complex numbers using SSE2 intrinsics

void multiply_SSE2(complex_num x, complex_num y, complex_num *z)

{
	__m128d num1, num2, num3, num4;
	
	// Copies a single element into the vector
	//   num1:  [x.real, x.real]
	
	num1 = _mm_load1_pd(&x.real);
	
	// Move y elements into a vector
	//   num2: [y.img, y.real]
	
	num2 = _mm_set_pd(y.img, y.real);
	
	// Multiplies vector elements
	//   num3: [(x.real*y.img), (x.real*y.real)]
	
	num3 = _mm_mul_pd(num2, num1);
	
	//   num1: [x.img, x.img]
	
	num1 = _mm_load1_pd(&x.img);
	
	// Swaps the vector elements.
	//   num2: [y.real, y.img]
	
	num2 = _mm_shuffle_pd(num2, num2, 1);
	
	//   num2: [(x.img*y.real), (x.img*y.img)]
	
	num2 = _mm_mul_pd(num2, num1);
	num4 = _mm_add_pd(num3, num2);
	num3 = _mm_sub_pd(num3, num2);
	num4 = _mm_shuffle_pd(num3, num4, 2);
	
	// Stores the elements of num4 into z
	
	_mm_storeu_pd((double *)z, num4);
}

*/
