// VECTOR_COUNTALIGN(x) returns the element count of a float vector that
// can hold x float elements; eg, on a 128-bit vector architecture, a float
// vector holds VECTOR_COUNT = 4 floats (4 * 32-bit float = 128-bit float vector),
// so to store 3 floats, you need a VECTOR_COUNTALIGN(3) = 4 element float vector,
// and to store 250 floats, you need a VECTOR_COUNTALIGN(250) = 252 element float 
// vector;
#define VECTOR_COUNT 4
#define VECTOR_COUNTALIGN(x) (((x)+(VECTOR_COUNT-1))&0xFFFFFFFCUL)


void Vector_Multiply (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[]);
void Vector_Sigmoid (int c, float ar[], float beta);
void Vector_MultiplySig (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[], float beta);

#define VECTOR_FLOATARRAY(NAME,COUNT) union { vector float v[VECTOR_COUNTALIGN(COUNT)/VECTOR_COUNT]; } v_ ## NAME; \
                                        float * NAME = (float *) &v_ ## NAME .v
#define VECTOR_FLOATARRAY2(NAME,C1,C2) union { vector float v[C1*VECTOR_COUNTALIGN(C2)/VECTOR_COUNT]; } v_ ## NAME; \
                                        float (* NAME)[VECTOR_COUNTALIGN(C2)] = (void *) &v_ ## NAME .v
#define VECTOR_FLOATARRAY3(NAME,C1,C2,C3) union { vector float v[C1*C2*VECTOR_COUNTALIGN(C3)/VECTOR_COUNT]; }  v_ ## NAME ; \
                                            float (* NAME)[C2][VECTOR_COUNTALIGN(C3)] = (void *) &v_ ## NAME .v
//#define VECTOR_FLOATARRAY(NAME,COUNT) float NAME [ COUNT ];
