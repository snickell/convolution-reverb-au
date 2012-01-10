/*
 *  TestFrameBuffer.cpp
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/13/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */


#include <ConvolverState.h>
#include <iostream>

using std::cout;
using std::cerr;
using boost::shared_ptr;

using namespace Convolver;

/*
FrameBuffer() : NEGATIVE_BUFFER_INDEX(std::numeric_limits<uint32_t>::max()), bufferOffset(0), frameNum(0) {}

typedef deque<shared_ptr<TimeBlock> > Buffer;

FrameNum addFrame(shared_ptr<TimeBlock> frame);
void flushFramesBefore(FrameNum oldestFrameToKeep);
shared_ptr<TimeBlock> fulfill(FrameRequest &frameRequest, uint32_t frameSize, bool pad);
FrameNum getFrameNum() { return frameNum; }

 */
#define FRAME_SIZE 1024


static shared_ptr<TimeBlock> fulfillL(FrameBuffer *buffer, FrameNum start, FrameNum end) {
	FrameRequest request(FrameRequestID(start, end));
	return buffer->fulfill(request, FRAME_SIZE, true);
}


static shared_ptr<TimeBlock> getSingleFrame(FrameBuffer *buffer, FrameNum frame) {
	shared_ptr<TimeBlock> toReturn = fulfillL(buffer, frame, frame+1);
	assert(toReturn->size() == FRAME_SIZE * 2);
	return toReturn;
}

static shared_ptr<TimeBlock> makeBlock(uint32_t size, float value) {
	TimeBlock *block = new TimeBlock(size);
	block->at(0) = value;
	assert(block->size() == size);
	return shared_ptr<TimeBlock>(block);
}

static FrameNum addFrameL(FrameBuffer *buffer, float value) {
	return buffer->addFrame(makeBlock(FRAME_SIZE, value));
}

static bool isFrame(FrameBuffer *buffer, FrameNum num, float value) {
	shared_ptr<TimeBlock> frame(getSingleFrame(buffer, num));
	return ((*frame)[0] == value);
}

void FrameBuffer::test() {
	FrameNum one   = addFrameL(this, 1.0);
	assert(isFrame(this, one, 1.0));
	
	FrameNum two   = addFrameL(this, 2.0);
	assert(isFrame(this, one, 1.0));
	assert(isFrame(this, two, 2.0));
	
	FrameNum three = addFrameL(this, 3.0);
	assert(isFrame(this, one, 1.0));
	assert(!isFrame(this, two, 1.0));
	assert(isFrame(this, two, 2.0));
	assert(isFrame(this, three, 3.0));
	
	FrameNum four  = addFrameL(this, 4.0);
	assert(isFrame(this, four, 4.0));	
	assert(buffer.size() == 4);
	
	flushFramesBefore(two);
	assert(buffer.size() == 3);
	assert(isFrame(this, two, 2.0));
	
	flushFramesBefore(two);
	assert(buffer.size() == 3);
	assert(isFrame(this, two, 2.0));	
	assert(isFrame(this, four, 4.0));		

	flushFramesBefore(one);
	assert(isFrame(this, two, 2.0));	
	assert(buffer.size() == 3);
	
	FrameNum five = addFrameL(this, 5.0);
	FrameNum six = addFrameL(this, 6.0);
	assert(isFrame(this, five, 5.0));
	assert(isFrame(this, six, 6.0));	
	
	assert(buffer.size() == 5);
	
	flushFramesBefore(three);
	assert(buffer.size() == 4);	
	assert(isFrame(this, five, 5.0));
	assert(isFrame(this, three, 3.0));
	
	FrameNum seven = addFrameL(this, 7.0);
	FrameNum eight = addFrameL(this, 8.0);
	FrameNum nine  = addFrameL(this, 9.0);
	FrameNum ten   = addFrameL(this, 10.0);
	
	shared_ptr<TimeBlock> block(fulfillL(this, three, eight));
	assert(block->size() == 5 * FRAME_SIZE * 2);

	block = fulfillL(this, five, eight);
	assert(block->size() == 3 * FRAME_SIZE * 2);
	
	assert(block->at(0) == 5.0);
	assert(block->at(FRAME_SIZE) == 6.0);
	assert(block->at(FRAME_SIZE*2) == 7.0);
	
	cout << "All tests seem to have passed" << endl;
}
static void printComplex(kiss_fft_cpx *buffer, int size, float scale) {
	for (int j=0; j < size; j++) {
		char sign = buffer[j].i >= 0.0f ? '+' : '-';
		printf("%7.4f%c%7.4f \t", buffer[j].r*scale, sign, fabs(buffer[j].i*scale));
		if ((j+1) % 7 == 0) printf("\n\t%4d: ", j+1);	
	}
}

static void printFloats(float *buffer, int size, float scale) {
	for (int j=0; j < size; j++) {
		printf("%8.4f \t", buffer[j]*scale);
		if ((j+1) % 9 == 0) printf("\n\t%4d: ", j+1);	
	}
}

#include "kiss_fftr.h"
#define FFT_SIZE 1024
int main(int argc, char *argv[]) {
	cout << "Hi world" << endl;
	//FrameBuffer frameBuffer;
	//frameBuffer.test();
	kiss_fftr_cfg cfg = kiss_fftr_alloc(FFT_SIZE, 0, NULL, NULL);
	kiss_fftr_cfg cfgi = kiss_fftr_alloc(FFT_SIZE, 1, NULL, NULL);
	
	float james[FFT_SIZE];
	for (uint32_t i=0; i < FFT_SIZE; i++) {
		james[i] = (float)(i % 20);
	}
	
	kiss_fft_cpx jim[FFT_SIZE/2 + 1];	
	
	kiss_fftr(cfg, james, jim);
	
	float jimmy[FFT_SIZE];
	
	kiss_fftri(cfgi, jim, jimmy);
	
	cout << "Original:" << endl;
	printFloats(james, FFT_SIZE, 1.0);
	cout << endl << endl << endl << endl;
	cout << "Processed:" << endl;
	printFloats(jimmy, FFT_SIZE, 1.0 / FFT_SIZE);
}





