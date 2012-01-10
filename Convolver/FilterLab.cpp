/*
 *  FilterTester.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 6/5/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#include "FilterLab.h"
#include "ConvolverInternal.h"
#include "ConvolverTypes.h"

#ifdef USE_CORE_AUDIO
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

#if USE_SNDFILE
#include <stdio.h>
#include <sndfile.h>
#endif

#include <math.h>
#include <time.h>
#include <limits.h>
#include <algorithm>
#include <pthread.h>

using namespace std;

#include <iostream> 
#include <stdlib.h>

#include "kiss_fftr.h"


#define PINK_MEASURE
#ifdef PINK_MEASURE
	float pinkMax = -999.0;
	float pinkMin =  999.0;
#endif

/************************************************************/
/* Calculate pseudo-random 32 bit number based on linear congruential method. */
static unsigned long GenerateRandomNumber( void )
{
	static unsigned long randSeed = 22222;  /* Change this for different random sequences. */
	randSeed = (randSeed * 196314165) + 907633515;
	return randSeed;
}

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24)
#define PINK_RANDOM_SHIFT      ((sizeof(long)*8)-PINK_RANDOM_BITS)

class PinkNoise {

	long      pink_Rows[PINK_MAX_RANDOM_ROWS];
	long      pink_RunningSum;   /* Used to optimize summing of generators. */
	int       pink_Index;        /* Incremented each sample. */
	int       pink_IndexMask;    /* Index wrapped by ANDing with this mask. */
	float     pink_Scalar;       /* Used to scale within range of -1.0 to +1.0 */
public:
  PinkNoise(int numRows) {
    int i;
    long pmax;
    pink_Index = 0;
    pink_IndexMask = (1<<numRows) - 1;
    /* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
    pmax = (numRows + 1) * (1<<(PINK_RANDOM_BITS-1));
    pink_Scalar = 1.0f / pmax;
    /* Initialize rows. */
    for( i=0; i<numRows; i++ ) pink_Rows[i] = 0;
    pink_RunningSum = 0;
  }
  void generate(float *signal, uint32_t numValues) {
    for (uint32_t i=0; i < numValues; i++) {
      signal[i] = GeneratePinkNoise();
    }
  }
  inline float GeneratePinkNoise()
  {
  	long newRandom;
  	long sum;
  	float output;

  /* Increment and mask index. */
  	pink_Index = (pink_Index + 1) & pink_IndexMask;

  /* If index is zero, don't update any random values. */
  	if( pink_Index != 0 )
  	{
  	/* Determine how many trailing zeros in PinkIndex. */
  	/* This algorithm will hang if n==0 so test first. */
  		int numZeros = 0;
  		int n = pink_Index;
  		while( (n & 1) == 0 )
  		{
  			n = n >> 1;
  			numZeros++;
  		}

  	/* Replace the indexed ROWS random value.
  	 * Subtract and add back to RunningSum instead of adding all the random
  	 * values together. Only one changes each time.
  	 */
  		pink_RunningSum -= pink_Rows[numZeros];
  		newRandom = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
  		pink_RunningSum += newRandom;
  		pink_Rows[numZeros] = newRandom;
  	}

  /* Add extra white noise value. */
  	newRandom = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
  	sum = pink_RunningSum + newRandom;

  /* Scale to range of -1.0 to 0.9999. */
  	output = pink_Scalar * sum;

  #ifdef PINK_MEASURE
  /* Check Min/Max */
  	if( output > pinkMax ) pinkMax = output;
  	else if( output < pinkMin ) pinkMin = output;
  #endif

  	return output;
  }
};

namespace Convolver {
  
	FilterLab::FilterLab(Filter *filter, Kernel &kernel)
		: testSignal(NULL), testSignalConvolved(NULL), filterSize(filter->getTimeSize())
	{
		shared_ptr<BlockPattern> blockPatternPtr = filter->getBlockPattern();
		uint32_t blockSize = blockPatternPtr->minimumBlockSize();

		#if DEBUG_FILTERLAB
		 cout << "FilterLab::FilterLab(): blockSize=" << blockSize << endl;
		#endif
		State filterMeasurementState(kernel, blockPatternPtr);

		measureAreaOffset = filterSize;
		
		// We want the measure area size to contain at least 4096 samples for detailed FFT
		measureAreaSize = filterSize;
		while (measureAreaSize < /*4096*/ 48000) measureAreaSize += measureAreaSize;
		
		// Buffer composition: [ringin]|[measureHere]|[ringout]	
		bufferSize = measureAreaOffset /* ringin */ + measureAreaSize /* measure */ + filterSize /* ringout */; 
		if (bufferSize % blockSize != 0) bufferSize += blockSize - (bufferSize % blockSize);
		
		unsigned minuto = time(0);
		
		// Seed random numbers
		srand((unsigned)(minuto)); 
		
		// Initialize the testSignal with white noise
		testSignal = new float[bufferSize]();
		fillWithPinkNoise(testSignal, measureAreaOffset + measureAreaSize);
		testSignalConvolved = new float[bufferSize]();
		
		// Save the test signal for diagnostics
		//save("testsignal-in.wav", testSignal, bufferSize);
		
		#if DEBUG_FILTERLAB	
		cout << "FilterLab::FilterLab(): convolving, bufferSize: " << bufferSize << ", blockSize: " << blockSize << ", filterSize: " << filterSize << endl;	
		#endif	
		
		// Perform the convolution
		assert(bufferSize % blockSize == 0);
		for(uint32_t i=0; i < bufferSize; i += blockSize) {
			// Make sure we don't read off the end
			assert(i + blockSize <= bufferSize);

			// FIXME: leak huge stinking chunks of C++ stack here
			pthread_testcancel();
			
			boost::shared_ptr<TimeBlock> testSignalBlock(new TimeBlock(&testSignal[i], &testSignal[i+blockSize]));
			shared_ptr<TimeBlock> block = kernel.convolve(testSignalBlock, *filter, filterMeasurementState);

			
			std::copy(block->begin(), block->end(), &testSignalConvolved[i]);
		}
			
		#if DEBUG_FILTERLAB
		cout << "FilterLab::FilterLab(): done" << endl;
		#endif	
	}

	FilterLab::~FilterLab() {
		delete testSignal;
		delete testSignalConvolved;
	}

	#define magnitude(r,i) sqrt(pow(r, 2) + pow(i, 2))

	FrequencyResponse *FilterLab::measureFrequencyResponse(int numFrequencies, float scaleAfter) {
		uint32_t size = (numFrequencies - 1) * 2;
		
		#if DEBUG_FILTERLAB	
		printf("\tFilterLab: Requested freq response for %d freqs\n", numFrequencies);
		printf("\tFilterLab: Going to use %d of buffer stuff\n", size);
		#endif
		
		assert(measureAreaSize > size);
		
		kiss_fftr_cfg cfg = kiss_fftr_alloc(size, 0, NULL, NULL);
		
		Buffer freqBefore = new kiss_fft_cpx[numFrequencies];
		Buffer freqAfter = new kiss_fft_cpx[numFrequencies];
		
		kiss_fftr(cfg, testSignal + measureAreaOffset, freqBefore);
		kiss_fftr(cfg, testSignalConvolved + measureAreaOffset, freqAfter);
		
		FrequencyResponse *response = new FrequencyResponse[numFrequencies];
		
		for (int i=0; i < numFrequencies; i++) {
			response[i].mFrequency = i * 10.0f;
			float magAfter  = magnitude(freqAfter[i].r * scaleAfter, freqAfter[i].i * scaleAfter);
			float magBefore = magnitude(freqBefore[i].r, freqBefore[i].i);
			float dbs = 10.0f * log10(magAfter / magBefore);
			assert(!isnan(dbs));
			// FIXME: do we want to compute dBs in the display or here?
			response[i].mMagnitude = magAfter / magBefore;
		}
		
		kiss_fftr_free(cfg);
		delete[] freqBefore;
		delete[] freqAfter;
		
		return response;
	}

	void FilterLab::saveTestSignal(const char *filename, float scalar) {
		save(filename, testSignal, bufferSize, scalar);
	}

	void FilterLab::saveResultSignal(const char *filename, float scalar) {
		save(filename, testSignalConvolved, bufferSize, scalar);	
	}

	void FilterLab::save(const char *filename, const float *constBuffer, uint32_t size, float scalar) {
	#ifdef USE_CORE_AUDIO
		FSRef ref;
		FSRef outFile;
		AudioFileID fileID;

		// Delete the file
		FSRef deleteFile;
		FSPathMakeRef((UInt8 *)filename, &deleteFile, NULL);	
		FSDeleteObject(&deleteFile);
		
		float *buffer = new float[size];
		for (uint32_t i=0; i < size; i++) {
			buffer[i] = constBuffer[i] * scalar;
		}
		
		AudioStreamBasicDescription	description;
		AudioStreamPacketDescription packetDescription;
		
		memset(&description, 0, sizeof(AudioStreamBasicDescription));
		memset(&packetDescription, 0, sizeof(AudioStreamPacketDescription));
		
		packetDescription.mStartOffset = 0;
		packetDescription.mVariableFramesInPacket = 0;
		packetDescription.mDataByteSize = 4;
		
		description.mChannelsPerFrame = 1; 
		description.mSampleRate = 48000; 
		description.mFormatID = kAudioFormatLinearPCM;
		description.mBytesPerPacket = description.mChannelsPerFrame * 4;
		description.mFramesPerPacket = 1;
		description.mBytesPerFrame = description.mBytesPerPacket;
		description.mBitsPerChannel = 32; 
		description.mFormatFlags =  kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked;
		
		CFStringRef  filenameSad = NULL;
		
		FSPathMakeRef((UInt8 *)"./", &ref, NULL);
		filenameSad = CFStringCreateWithCString(NULL, filename, kCFStringEncodingASCII);
		
		OSStatus result;
		result = AudioFileCreate(&ref, filenameSad, kAudioFileWAVEType, &description, 0, &outFile, &fileID);
		if (result != noErr) {
			char *chars = (char *)&result;
			printf("FilterLab: ERROR ON CREATE: %s (%c%c%c%c)\n", GetMacOSStatusErrorString(result), chars[0], chars[1], chars[2], chars[3]);
		}
		result = AudioFileWritePackets(fileID, false, sizeof(float) * size, &packetDescription, 0, (UInt32*)&size, buffer);
		if (result != noErr) {
			printf("FilterLab: ERROR ON WRITE: %s\n", GetMacOSStatusErrorString(result));
		}
		result = AudioFileClose(fileID);
		if (result != noErr) {
			printf("FilterLab: ERROR ON CLOSE\n");
		}
		if (result != noErr) printf("FilterLab: Error on CLOSE: %s\n", (char *)result);
		
		delete buffer;
	#else
		printf("WARNING: tried to save filter result to %s, but not supported on this platform (only OS/X)", filename);
	#endif
	}

	float FilterLab::measureGain() {
		assert(testSignal);
		assert(testSignalConvolved);
		
		double before = measureEnergy(testSignal + measureAreaOffset, measureAreaSize);
		double after  = measureEnergy(testSignalConvolved + measureAreaOffset, measureAreaSize);
		double gain = after / before;
		
		#if DEBUG_FILTERLAB
		printf("\tFilterLab, avg gain: %3.2f\n", gain);
		#endif	

		return gain;
	}

	float FilterLab::measurePeakGain() {
		assert(testSignal);
		assert(testSignalConvolved);
		
		float before = *std::max_element(testSignal + measureAreaOffset, testSignal + measureAreaOffset + measureAreaSize);
		float after = *std::max_element(testSignalConvolved + measureAreaOffset, testSignalConvolved + measureAreaOffset + measureAreaSize);
		
		double gain = after / before;
		
		#if DEBUG_FILTERLAB
		printf("\tFilterLab, peak gain: %3.2f\n", gain);
		#endif
		
		return gain;
	}

	float *loadSignalFromBundle(const char *name, uint32_t size) {
		bool keepReading = true;
		float *realBuffer = new float[size]();
		float *buffer = realBuffer;
		
		
		#if DEBUG_FILTERLAB
		cout << "FilterLab::loadSignalFromBundle('" << name << "', size=" << size << ")" << endl;
		#endif
		
		while (keepReading && size > 0) {
			uint32_t num_read;

	#ifdef USE_CORE_AUDIO
			
		#if DEBUG_FILTERLAB
		cout << "FilterLab::loadSignalFromBundle(): using coreaudio" << endl;
		#endif		
			
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR(CONVOLVOTRON_BUNDLE_ID) );
		CFStringRef nameCF = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
		CFURLRef irURL = CFBundleCopyResourceURL(bundle, nameCF, CFSTR("wav"), NULL);
		
		AudioFileID fileID;
				
		OSStatus result;
		result = AudioFileOpenURL(irURL, fsRdPerm, 0, &fileID);
		if (result != noErr) {
			char *chars = (char *)&result;
			printf("FilterLab: ERROR ON OPEN: %s (%c%c%c%c)\n", GetMacOSStatusErrorString(result), chars[0], chars[1], chars[2], chars[3]);
		}
			
		UInt32 ioNumBytes = size * sizeof(float);
			
		result = AudioFileReadBytes(fileID, false, 0, &ioNumBytes, buffer);
		num_read = ioNumBytes / sizeof(float);
			
		if (result != noErr) {
			printf("FilterLab: ERROR ON READ: %s\n", GetMacOSStatusErrorString(result));
		}
		
		result = AudioFileClose(fileID);
		if (result != noErr) {
			printf("FilterLab: ERROR ON CLOSE\n");
		}
			
	#endif
	
	#if USE_SNDFILE			
  		#if DEBUG_FILTERLAB
  		cout << "FilterLab::loadSignalFromBundle(): using libsndfile" << endl;
  		#endif		
			
  		int name_len = strlen(name);
  		char *filename = new char[name_len + 4];
  		strcpy(filename, name);
  		strcpy(filename + name_len, ".wav");
		
  		SF_INFO sfinfo;
  		sfinfo.format = 0;
  		SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);
  		if (sfinfo.frames <= 0) {
  			cerr << "ERROR: couldn't find " << filename << ": make sure its in the same directory as Convolver binary" << endl;
  			return NULL;
  		}
		
		assert(sfinfo.frames >= size);
		num_read = (uint32_t)sf_read_float(file, buffer, size);

			
		assert(num_read == size);
		sf_close(file);
		delete filename;
	#endif
			
			
			#if DEBUG_FILTERLAB
			cout << "FilterLab::loadSignalFromBundle(): num_read=" << num_read << ", size=" << size << endl;
			#endif			
			
			if (num_read <= 0) {
				keepReading = false;
			} else {
				size -= num_read;
				buffer += num_read;
			}
			
		}
		
		#if DEBUG_FILTERLAB
		cout << "FilterLab::loadSignalFromBundle(): done" << endl;
		#endif		
		
		return realBuffer;
			
	}

	void FilterLab::fillWithWhiteNoise(float *buffer, uint32_t size) {
		float *signal = loadSignalFromBundle("whitenoise", size);
		memcpy(buffer, signal, sizeof(float) * size);
		/*for (uint32_t i=0; i < size; i++) {
			buffer[i] = box_muller(0.0f, 0.5f);
		}*/
	}

	void FilterLab::fillWithPinkNoise(float *buffer, uint32_t size) {
	  #ifdef FILTER_LAB_GENERATE_PINKNOISE
    cout << "FilterLab::fillWithPinkNoise(): generating pinknoise" << endl;
    PinkNoise noise(16);
    noise.generate(buffer, size);
	  #else
    cout << "FilterLab::fillWithPinkNoise(): loading pinknoise.wav" << endl;
		float *signal = loadSignalFromBundle("pinknoise", size);
		memcpy(buffer, signal, sizeof(float) * size);
		#endif
	}

	double FilterLab::measureEnergy(const float *samples, uint32_t size) {
		double total = 0.0f;
		for (uint32_t i=0; i < size; i++) {
			total += fabs(samples[i]);
		}

		return total;
	}


}


class PinkNumber { 
private: 
	int max_key; 
	int key; 
	unsigned int white_values[5]; 
	unsigned int range; 
public: 
	PinkNumber(unsigned int range = 128) 
	{ 
		max_key = 0x1f; // Five bits set 
		this->range = range; 
		key = 0; 
		for (int i = 0; i < 5; i++) 
			white_values[i] = rand() % (range/5); 
	} 
	int GetNextValue() 
	{ 
		int last_key = key; 
		unsigned int sum;
		
		key++; 
		if (key > max_key) 
			key = 0; 
		// Exclusive-Or previous value with current value. This gives 
		// a list of bits that have changed. 
		int diff = last_key ^ key; 
		sum = 0; 
		for (int i = 0; i < 5; i++) 
		{ 
			// If bit changed get new random number for corresponding 
			// white_value 
			if (diff & (1 << i)) 
				white_values[i] = rand() % (range/5); 
			sum += white_values[i]; 
		} 
		return sum; 
	} 
}; 



