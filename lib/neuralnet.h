/*
 * neuralnet.h
 *
 * by Gary Wong, 1998
 */

#ifndef _NEURALNET_H_
#define _NEURALNET_H_

#include <stdio.h>

typedef struct _neuralnet {
    int cInput, cHidden, cOutput, nTrained;
    float rBetaHidden, rBetaOutput, *arHiddenWeight, *arOutputWeight,
	*arHiddenThreshold, *arOutputThreshold;
} neuralnet;

extern int NeuralNetCreate( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput, float rBetaHidden,
			    float rBetaOutput );
extern int NeuralNetDestroy( neuralnet *pnn );

extern int NeuralNetEvaluate( neuralnet *pnn, float arInput[],
			      float arOutput[] );
extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha );
extern int NeuralNetResize( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput );

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf );
extern int NeuralNetSave( neuralnet *pnn, FILE *pf );

#endif
