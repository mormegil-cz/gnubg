/*
 * neuralnet.h
 *
 * by Gary Wong, 1998
 */

#ifndef _NEURALNET_H_
#define _NEURALNET_H_

#include <stdio.h>

#define VECTOR 0
#define VECTOR_VE 0

//#if VECTOR
    typedef struct {
        int dirty;
        float *arHiddenWeight, *arOutputWeight;
        float *arHiddenThreshold, *arOutputThreshold;
    } _neuralnetvector;
//#endif

typedef struct _neuralnet {
    int cInput, cHidden, cOutput, nTrained, fDirect;
    float rBetaHidden, rBetaOutput, *arHiddenWeight, *arOutputWeight,
	*arHiddenThreshold, *arOutputThreshold;
    #if !PROCESSING_UNITS 
    float * savedBase;
    float * savedIBase;     
    /* since these can't be shared among different threads using the same
        neural networks, we'll be using a thread-specific mechanism for storing 
        them : see the savedBases struct below */
    #endif
//#if VECTOR
    _neuralnetvector vector;
//#endif
} neuralnet;


extern int NeuralNetCreate( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput, float rBetaHidden,
			    float rBetaOutput );

extern void *NeuralNetCreateDirect( neuralnet *pnn, void *p );

extern int NeuralNetDestroy( neuralnet *pnn );

typedef enum  {
  NNEVAL_NONE,
  NNEVAL_SAVE,
  NNEVAL_FROMBASE,
} NNEvalType;

extern int NeuralNetEvaluate( neuralnet *pnn, float arInput[],
			      float arOutput[], NNEvalType t);
extern int NeuralNetDifferentiate( neuralnet *pnn, float arInput[],
				   float arOutput[], float arDerivative[] );
extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha );
extern int NeuralNetResize( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput );

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf );
extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf );
extern int NeuralNetSave( neuralnet *pnn, FILE *pf );
extern int NeuralNetSaveBinary( neuralnet *pnn, FILE *pf );

#if PROCESSING_UNITS

typedef struct _savedBases {
    struct _savedBases *next;
    neuralnet	*pnn;
    float 	*savedBase;
    float 	*savedIBase;
} savedBases;

void FreeThreadSavedBases (void);

#endif

#endif
