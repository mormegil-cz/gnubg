/*
 * neuralnet.h
 *
 * by Gary Wong, 1998
 */

#ifndef _NEURALNET_H_
#define _NEURALNET_H_

#include <stdio.h>

typedef struct _neuralnet {
    int cInput, cHidden, cOutput, nTrained, fDirect;
    float rBetaHidden, rBetaOutput, *arHiddenWeight, *arOutputWeight,
	*arHiddenThreshold, *arOutputThreshold;
    float *savedBase, *savedIBase;
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

extern void 
ComputeSigTable (void);

#endif
