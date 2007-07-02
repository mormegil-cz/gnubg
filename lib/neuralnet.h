/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * neuralnet.h
 *
 * by Gary Wong, 1998
 * $Id$
 */

#ifndef _NEURALNET_H_
#define _NEURALNET_H_

#include <stdio.h>
#include "common.h"

#define SIGMOID_BAUR 0
#define SIGMOID_JTH 0

#if SIGMOID_BAUR
#define sigmoid sigmoid_baur
#elif SIGMOID_JTH
#define sigmoid sigmoid_jth
#else
extern float sigmoid_original(float const xin);
#define sigmoid sigmoid_original
#endif

typedef struct _neuralnet {
    unsigned int cInput, cHidden, cOutput, fDirect;
	int nTrained;
    float rBetaHidden, rBetaOutput, *arHiddenWeight, *arOutputWeight,
	*arHiddenThreshold, *arOutputThreshold;
} neuralnet;

extern int NeuralNetCreate( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput, float rBetaHidden,
			    float rBetaOutput );

extern void *NeuralNetCreateDirect( neuralnet *pnn, void *p );

extern void NeuralNetDestroy( neuralnet *pnn );

typedef enum  {
  NNEVAL_NONE,
  NNEVAL_SAVE,
  NNEVAL_FROMBASE
} NNEvalType;

typedef enum  {
  NNSTATE_NONE = -1,
  NNSTATE_INCREMENTAL,
  NNSTATE_DONE
} NNStateType;

typedef struct _NNState
{
	NNStateType state;
    float *savedBase, *savedIBase;
} NNState;

extern int (*NeuralNetEvaluateFn)( const neuralnet *pnn, float arInput[],
			      float arOutput[], NNState *pnState);

extern int NeuralNetEvaluate( const neuralnet *pnn, float arInput[],
			      float arOutput[], NNState *pnState);
extern int NeuralNetEvaluate128( const neuralnet *pnn, float arInput[],
			      float arOutput[], NNState *pnState);
extern int NeuralNetDifferentiate( const neuralnet *pnn, const float arInput[],
				   float arOutput[], float arDerivative[] );
extern int NeuralNetTrain( neuralnet *pnn, const float arInput[], float arOutput[],
			   const float arDesired[], float rAlpha );
extern int NeuralNetResize( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput );

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf );
extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf );
extern int NeuralNetSave( const neuralnet *pnn, FILE *pf );
extern int NeuralNetSaveBinary( const neuralnet *pnn, FILE *pf );

extern void 
ComputeSigTable (void);

extern int SSE_Supported(void);

#endif
