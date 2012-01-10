CC = g++
OBJS = main.o kiss_fftr.o kiss_fft.o Convolver.o ConvolverFFT.o ConvolverFilter.o ConvolverKernel.o ConvolverSignal.o ConvolverState.o ConvolverTypes.o FilterLab.o SSEConvolution.o
CFLAGS = -c -g -Wall -msse3 -I/usr/local/include -I../boost_1_39_0
CPPFLAGS = ${CFLAGS}

LFLAGS = -g -Wall -L/usr/local/lib -lsndfile

all: ${OBJS}
	${CC} -o Convolver ${LFLAGS} ${OBJS}

.c.o:
	${CC} ${CFLAGS} $<