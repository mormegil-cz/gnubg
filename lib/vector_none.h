#define VECTOR_COUNT 1
#define VECTOR_COUNTALIGN(x) (x)

#define VECTOR_MULTIPLY 0
#define VECTOR_SIGMOID 0
#define VECTOR_MULTIPLYSIG 0

void Vector_Multiply (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[]);
void Vector_Sigmoid (neuralnet *pnn, int cIn, float arIn[], float beta);
void Vector_MultiplySig (neuralnet *pnn, int cIn, int cOut, float arIn[], float arOut[], float arMatrix[], float beta);
