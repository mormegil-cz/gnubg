#include "neuralnet.h"
#include "vector_ve.h"
#include <math.h>
#include <signal.h>


#define MATRIX_LAYOUT 1

// [0:3b][blocksize*16:5b][blockcount:8b][stride:16b]
const unsigned long kPrefetchControl = 0x08040080;  // blocksize=8*16=128b, blockcount=8, stride=+128b
const signed long kPrefetchDistance = 0x0040;


/* e[k] = exp(k/10) / 10 */
static float e[100] = {
0.10000000000000001, 
0.11051709180756478, 
0.12214027581601698, 
0.13498588075760032, 
0.14918246976412702, 
0.16487212707001281, 
0.18221188003905089, 
0.20137527074704767, 
0.22255409284924679, 
0.245960311115695, 
0.27182818284590454, 
0.30041660239464335, 
0.33201169227365473, 
0.36692966676192446, 
0.40551999668446748, 
0.44816890703380646, 
0.49530324243951152, 
0.54739473917271997, 
0.60496474644129461, 
0.66858944422792688, 
0.73890560989306509, 
0.81661699125676512, 
0.90250134994341225, 
0.99741824548147184, 
1.1023176380641602, 
1.2182493960703473, 
1.3463738035001691, 
1.4879731724872838, 
1.6444646771097049, 
1.817414536944306, 
2.0085536923187668, 
2.2197951281441637, 
2.4532530197109352, 
2.7112638920657881, 
2.9964100047397011, 
3.3115451958692312, 
3.6598234443677988, 
4.0447304360067395, 
4.4701184493300818, 
4.9402449105530168, 
5.4598150033144233, 
6.034028759736195, 
6.6686331040925158, 
7.3699793699595784, 
8.1450868664968148, 
9.0017131300521811, 
9.9484315641933776, 
10.994717245212353, 
12.151041751873485, 
13.428977968493552, 
14.841315910257659, 
16.402190729990171, 
18.127224187515122, 
20.033680997479166, 
22.140641620418716, 
24.469193226422039, 
27.042640742615255, 
29.886740096706028, 
33.029955990964865, 
36.503746786532886, 
40.34287934927351, 
44.585777008251675, 
49.274904109325632, 
54.457191012592901, 
60.184503787208222, 
66.514163304436181, 
73.509518924197266, 
81.24058251675433, 
89.784729165041753, 
99.227471560502622, 
109.66331584284585, 
121.19670744925763, 
133.9430764394418, 
148.02999275845451, 
163.59844299959269, 
180.80424144560632, 
199.81958951041173, 
220.83479918872089, 
244.06019776244983, 
269.72823282685101, 
298.09579870417281, 
329.44680752838406, 
364.09503073323521, 
402.38723938223131, 
444.7066747699858, 
491.47688402991344, 
543.16595913629783, 
600.29122172610175, 
663.42440062778894, 
733.19735391559948, 
810.3083927575384, 
895.52927034825075, 
989.71290587439091, 
1093.8019208165192, 
1208.8380730216988, 
1335.9726829661872, 
1476.4781565577266, 
1631.7607198015421, 
1803.3744927828525, 
1993.0370438230298, 
};

/* Calculate an approximation to the sigmoid function 1 / ( 1 + e^x ).
   This is executed very frequently during neural net evaluation, so
   careful optimisation here pays off.

   Statistics on sigmoid(x) calls:
   * >99% of the time, x is positive.
   *  82% of the time, 3 < abs(x) < 8.

   The Intel x87's `f2xm1' instruction makes calculating accurate
   exponentials comparatively fast, but still about 30% slower than
   the lookup table used here. */
static inline float sigmoid(float const xin) {
    
    if( !signbit( xin ) ) { /* signbit() can be faster than a compare to 0.0 */
	/* xin is almost always positive; we place this branch of the `if'
	   first, in the hope that the compiler/processor will predict the
	   conditional branch will not be taken. */
	if( xin < 10.0f ) {
	    /* again, predict the branch not to be taken */
	    const float x1 = 10.0f * xin;
	    const unsigned int i = (int) x1;
	    
	    return 1.0f / ( 1.0f + e[ i ] * ( 10.0f - (int) i + x1 ) );
	} else
	    return 1.0f / 19931.370438230298f;
    } else {
	if( xin > -10.0f ) {
	    const float x1 = -10.0f * xin;
	    const unsigned int i = (int) x1;
	    
	    return 1.0f - 1.0f / ( 1.0f + e[ i ] * ( 10.0f - (int) i + x1 ) );
	} else
	    return 19930.370438230298f / 19931.370438230298f;	
    }
}

    
#define SIG_Q 10.0f	/* reciprocal of precision step */
#define SIG_MAX 100	/* half number of entries in lookup table */
/* note: we must have SIG_Q * 10 == SIG_MAX, so other possible values are:
            SIG_Q == 20.0f and SIG_MAX == 200
         or SIG_Q == 100.0f and SIG_MAX = 1000 */

static struct { float sig; float dsig; } Sig[2*SIG_MAX+1];
static int sigTableReady = 0;

static void ComputeSigTable (void)
{
    int i;
    
    for (i = -SIG_MAX; i < SIG_MAX+1; i++) {
        float x = (float) i / SIG_Q;
        //Sig[SIG_MAX+i].sig = 1.0f / (1.0f + exp (x));
        Sig[SIG_MAX+i].sig = sigmoid(x);
        if (i > -SIG_MAX) Sig[SIG_MAX+i-1].dsig = Sig[SIG_MAX+i].sig - Sig[SIG_MAX+i-1].sig;
    }
    
    sigTableReady = 1;
}

static const vector float zero = (vector float) (0);
static const vector signed int izero = (vector signed int) (0);
static const vector float vSIG_Q = (vector float) (SIG_Q);
static const vector signed int vSIG_MAX = (vector signed int) (SIG_MAX);
static const vector signed int vmSIG_MAX = (vector signed int) (- SIG_MAX);
static const vector float vOverLimitValue = (vector float) (1.0f / 19931.370438230298f);
static const vector float vUnderLimitValue = (vector float) (19930.370438230298f / 19931.370438230298f);

typedef union {
    vector float vector;
    float scalars[4];
} svector_float;

typedef union {
    vector signed int vector;
    long int scalars[4];
} svector_int;


float sigmoid2 (float const x)
{
    float x2 = x * SIG_Q;
    int i = x2;
    
    if (!sigTableReady) ComputeSigTable ();
    
    if (i > - SIG_MAX) {
        if (i < SIG_MAX) {
            //if (x > 9.0f && x < 10.5f) return sigmoid (x);
            //else
            return Sig[i+SIG_MAX].sig + Sig[i+SIG_MAX].dsig * (x2 - i);
        }
        else 
            return 1.0f / 19931.370438230298f;
    }
    else 
        return 19930.370438230298f / 19931.370438230298f;	
}


vector float sigmoidv (vector float vx)
{
    register vector float vx2, vdx, v;
    register vector bool int vIsOverLimit, vIsUnderLimit;
    register long int i0, i1, i2, i3;

    svector_float vsig;
    svector_float vdsig;
    svector_int vi;    
    
    if (!sigTableReady) ComputeSigTable ();

    vx2 = vec_madd (vx, vSIG_Q, zero);
    vi.vector = vec_cts (vx2, 0);
    vdx = vec_sub (vx2, vec_ctf (vi.vector, 0));
    
    vIsOverLimit = vec_cmpgt (vi.vector, vSIG_MAX);
    vIsUnderLimit = vec_cmplt (vi.vector, vmSIG_MAX);

    vi.vector = vec_sel (vi.vector, izero, vIsOverLimit); // to prevent memory error access in Sig[] below
    vi.vector = vec_sel (vi.vector, izero, vIsUnderLimit);

    vi.vector = vec_add (vi.vector, vSIG_MAX);
    i0 = vi.scalars[0]; 
    i1 = vi.scalars[1]; 
    i2 = vi.scalars[2]; 
    i3 = vi.scalars[3];

    vsig.scalars[0] = Sig[i0].sig;
    vdsig.scalars[0] = Sig[i0].dsig;
    vsig.scalars[1] = Sig[i1].sig;
    vdsig.scalars[1] = Sig[i1].dsig;
    vsig.scalars[2] = Sig[i2].sig;
    vdsig.scalars[2] = Sig[i2].dsig;
    vsig.scalars[3] = Sig[i3].sig;
    vdsig.scalars[3] = Sig[i3].dsig;
    
    v = vec_madd (vdsig.vector, vdx, vsig.vector);
    
    v = vec_sel (v, vOverLimitValue, vIsOverLimit);
    v = vec_sel (v, vUnderLimitValue, vIsUnderLimit);
    
    return v;
}

static void Vector_MatrixLayout1 (vector float *m, int cols, int rows)
{
    int i, j;
    
    vector float *n = (vector float *) malloc (rows * cols * sizeof (vector float));
    vector float *p = n;
    
    for (i = 0; i < rows; i += 4)
        for (j = 0; j < cols; j ++) {
            *p++ = m[i*cols+j];
            *p++ = m[(i+1)*cols+j];
            *p++ = m[(i+2)*cols+j];
            *p++ = m[(i+3)*cols+j];
        }
        
    memcpy (m, n, rows * cols * sizeof (vector float));
    free (n);
}

void Vector_Realign (neuralnet *pnn)
{
    int i, j;
    int cInput = pnn->cInput;
    int cHidden = pnn->cHidden;
    int cOutput = pnn->cOutput;
    float *pSrc, *pDest;
    
    pSrc = pnn->arHiddenWeight;
    pDest = pnn->vector.arHiddenWeight;
    for (i = 0; i < VECTOR_COUNTALIGN(cHidden); i++) 
        for (j = 0; j < VECTOR_COUNTALIGN(cInput); j++) {	
            if (i < cHidden && j < cInput) *(pDest++) = pSrc[i+j*cHidden];
            else *(pDest++) = 0.0f;
        }
    if (MATRIX_LAYOUT == 1) Vector_MatrixLayout1 ((vector float *) pnn->vector.arHiddenWeight, 
                                VECTOR_COUNTALIGN(cInput)/VECTOR_COUNT, VECTOR_COUNTALIGN(cHidden));
            
    pSrc = pnn->arOutputWeight;
    pDest = pnn->vector.arOutputWeight;
    for (i = 0; i < VECTOR_COUNTALIGN(cOutput); i++) 
        for (j = 0; j < VECTOR_COUNTALIGN(cHidden); j++) 
            if (i < cOutput && j < cHidden) *pDest++ = *pSrc++;
            else *pDest++ = 0.0f;
            
    pSrc = pnn->arHiddenThreshold;
    pDest = pnn->vector.arHiddenThreshold;
    for (i = 0; i < VECTOR_COUNTALIGN(cHidden); i++) 
        if (i < cHidden) *(pDest++) = *(pSrc++);
        else *(pDest++) = 0.0f;

    pSrc = pnn->arOutputThreshold;
    pDest = pnn->vector.arOutputThreshold;
    for (i = 0; i < VECTOR_COUNTALIGN(cOutput); i++) 
        if (i < cOutput) *(pDest++) = *(pSrc++);
        else *(pDest++) = 0.0f;   
        
    pnn->vector.dirty = 0;
}


static inline vector float AddAcross4 (vector float a, vector float b, vector float c, vector float d)
{
    vector float tempA, tempB, tempC, tempD;                                                                 

    //First half of 4x4 a matrix transpose
    tempA = vec_mergeh (a, c);	// {a0, c0, a1, c1}
    tempC = vec_mergel (a, c);	// {a2, c2, a3, c3}
    tempB = vec_mergeh (b, d);	// {b0, d0, b1, d1}
    tempD = vec_mergel (b, d);	// {b2, d2, b3, d3}
    
    //Add intermediate values
    b = vec_add (tempA, tempC); // {a0 + a2, c0 + c2, a1 + a3, c1 + c3}
    d = vec_add (tempB, tempD); // {b0 + b2, d0 + d2, b1 + b3, d1 + d3}
    
    //Do half of the second half of the transpose
    a = vec_mergeh (b, d); // { a0 + a2, b0 + b2, c0 + c2, d0 + d2 }
    c = vec_mergel (b, d); // { a1 + a3, b1 + b3, c1 + c3, d1 + d3 }
    
    //Find the result
    return vec_add (a, c);  // { a0 + a1 + a2 + a3, b0 + b1 + b2 + b3 + b4,
                            //   c0 + c1 + c2 + c3, d0 + d1 + d2 + d3 + d4 }
}




static const vector unsigned int mask0 = (vector unsigned int) (0xFFFFFFFF, 0, 0, 0);
static const vector unsigned int mask1 = (vector unsigned int) (0, 0xFFFFFFFF, 0, 0);
static const vector unsigned int mask2 = (vector unsigned int) (0, 0, 0xFFFFFFFF, 0);
static const vector unsigned int mask3 = (vector unsigned int) (0, 0, 0, 0xFFFFFFFF);


void Vector_MultiplySig (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[], float beta)
{
    // count of float vectors across an arMatrix row (250 elements in a row -> 63 float vectors)
    int vMRowCount = VECTOR_COUNTALIGN(cIn) / 4;
    
    int	i, j;
    vector float *pvOut = (vector float *) arOut;
    svector_float vBeta;
    
    vBeta.scalars[0] = beta;
    vBeta.scalars[1] = beta;
    vBeta.scalars[2] = beta;
    vBeta.scalars[3] = beta;
    
    #if MATRIX_LAYOUT==1
    vector float *pvM = (vector float *) arMatrix;
    #endif

    if ( (((unsigned long) arIn) & 0xf) || (((unsigned long) arOut) & 0xf) || (((unsigned long) arMatrix) & 0xf)) {
        printf ("*** %20xs %20xs %20xs\n\n\n", arIn, arOut, arMatrix);
        raise(SIGBUS);
        exit(-1);
    }

   if (pnn && pnn->vector.dirty)
        Vector_Realign (pnn);

    
    for (i = 0; i < cOut; i += 4) {
        
        register vector float v, v0, v1, v2, v3;
        register vector float *pvIn = (vector float *) arIn;
        #if MATRIX_LAYOUT==0
            vector float *pvM0 = (vector float *) arMatrix + i * vMRowCount;
            vector float *pvM1 = pvM0 + vMRowCount;
            vector float *pvM2 = pvM1 + vMRowCount;
            vector float *pvM3 = pvM2 + vMRowCount;
        #endif
        vector float vThres = *pvOut;
                
        v0 = vec_sel (zero, vThres, mask0);	// v0 = ( arOut[i+0], 0, 0, 0 )
        v1 = vec_sel (zero, vThres, mask1);	// v1 = ( 0, arOut[i+1], 0, 0 )
        v2 = vec_sel (zero, vThres, mask2);
        v3 = vec_sel (zero, vThres, mask3);

        for (j = 0; j < cIn; j += 4) {
            register vector float vb = *pvIn++;
            #if MATRIX_LAYOUT==0
                /*
                v0 = vec_madd (*pvM0, vb, v0);	// v0 += ( arMatrix[i,j] * arIn[j+0], arMatrix[i,j+1] * arIn[j+1], ... )
                v1 = vec_madd (*(pvM0+dM1), vb, v1); 
                v2 = vec_madd (*(pvM0+dM2), vb, v2);
                v3 = vec_madd (*(pvM0+dM3), vb, v3);
                pvM0 ++;
                */
                v0 = vec_madd (*pvM0++, vb, v0);	// v0 += ( arMatrix[i,j] * arIn[j+0], arMatrix[i,j+1] * arIn[j+1], ... )
                v1 = vec_madd (*pvM1++, vb, v1); // v1 += ( arMatrix[i+1,j] * arIn[j+0], arMatrix[i+1,j+1] * arIn[j+1], ... )
                v2 = vec_madd (*pvM2++, vb, v2);
                v3 = vec_madd (*pvM3++, vb, v3);
            #elif MATRIX_LAYOUT==1
                vec_dst (pvM + kPrefetchDistance, kPrefetchControl, 0);
                v0 = vec_madd (*pvM, vb, v0);
                v1 = vec_madd (*pvM++, vb, v1); 
                v2 = vec_madd (*pvM++, vb, v2);
                v3 = vec_madd (*pvM++, vb, v3);
            #endif
        }
        
        v = AddAcross4 (v0, v1, v2, v3);
        
        /*
        v = vec_madd (v, vBeta.vector, zero);
        v = sigmoidv (v);
        */
        
        *pvOut++ = v;
    }
    
    
    pvOut = (vector float *) arOut;
    for (i = 0; i < cOut; i += 4) {
        vector float v = *pvOut;
        v = vec_madd (v, vBeta.vector, zero);
        v = sigmoidv (v);
        *pvOut++ = v;
    }
    
}


void Vector_Multiply (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[])
{
    // count of float vectors across an arMatrix row (250 elements in a row -> 63 float vectors)
    int vMRowCount = VECTOR_COUNTALIGN(cIn) / 4;
    
    int	i, j;
    vector float *pvOut = (vector float *) arOut;

    #if MATRIX_LAYOUT==1
    vector float *pvM = (vector float *) arMatrix;
    #endif
    
    if ( (((unsigned long) arIn) & 0xf) || (((unsigned long) arOut) & 0xf) || (((unsigned long) arMatrix) & 0xf)) {
        printf ("*** %20xs %20xs %20xs\n\n\n", arIn, arOut, arMatrix);
        raise(SIGBUS);
        exit(-1);
    }
    
    if (pnn && pnn->vector.dirty)
        Vector_Realign (pnn);
        
    for (i = VECTOR_COUNTALIGN(cIn) - 1; i >= cIn; i --)
        arIn[i] = 0.0f;

    for (i = 0; i < cOut; i += 4) {
        
        register vector float v, v0, v1, v2, v3;
        vector float *pvIn = (vector float *) arIn;
        #if MATRIX_LAYOUT==0
            vector float *pvM0 = (vector float *) arMatrix + i * vMRowCount;
            vector float *pvM1 = pvM0 + vMRowCount;
            vector float *pvM2 = pvM1 + vMRowCount;
            vector float *pvM3 = pvM2 + vMRowCount;
        #endif
        vector float vThres = *pvOut;
        
        v0 = vec_sel (zero, vThres, mask0);	// v0 = ( arOut[i+0], 0, 0, 0 )
        v1 = vec_sel (zero, vThres, mask1);	// v1 = ( 0, arOut[i+1], 0, 0 )
        v2 = vec_sel (zero, vThres, mask2);
        v3 = vec_sel (zero, vThres, mask3);

        for (j = 0; j < cIn; j += 4) {
            register vector float vb = *pvIn++;
            #if MATRIX_LAYOUT==0
                v0 = vec_madd (*pvM0++, vb, v0);	// v0 += ( arMatrix[i,j] * arIn[j+0], arMatrix[i,j+1] * arIn[j+1], ... )
                v1 = vec_madd (*pvM1++, vb, v1); // v1 += ( arMatrix[i+1,j] * arIn[j+0], arMatrix[i+1,j+1] * arIn[j+1], ... )
                v2 = vec_madd (*pvM2++, vb, v2);
                v3 = vec_madd (*pvM3++, vb, v3);
            #elif MATRIX_LAYOUT==1
                vec_dst (pvM + kPrefetchDistance, kPrefetchControl, 0);
                v0 = vec_madd (*pvM, vb, v0);
                v1 = vec_madd (*pvM++, vb, v1); 
                v2 = vec_madd (*pvM++, vb, v2);
                v3 = vec_madd (*pvM++, vb, v3);
            #endif
        }
        
        *pvOut = AddAcross4 (v0, v1, v2, v3);
        
        pvOut ++;
    }
    
    
}


void Vector_Sigmoid (int c, float ar[], float beta)
{
    int i;
    vector float *pv = (vector float *) ar;
    svector_float vBeta;
    
    vBeta.scalars[0] = beta;
    vBeta.scalars[1] = beta;
    vBeta.scalars[2] = beta;
    vBeta.scalars[3] = beta;
    
    for (i = 0; i < c; i += 4)
        *pv++ = sigmoidv (vec_madd(*pv, vBeta.vector, zero));

}


