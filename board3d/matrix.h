
void setIdMatrix(float m[4][4]);
void copyMatrix(float dest[4][4], float src[4][4]);

void makeInverseTransposeMatrix(float m[4][4], float v[3]);
void makeInverseRotateMatrix(float m[4][4], float degRot, float x, float y, float z);
void makeInverseRotateMatrixX(float m[4][4], float degRot);
void makeInverseRotateMatrixY(float m[4][4], float degRot);
void makeInverseRotateMatrixZ(float m[4][4], float degRot);

void mult_matrix_vec(float mat[4][4], float src[4], float dst[4]);
void matrixmult(float m[4][4], float b[4][4]);

void dumpMatrix(float m[4][4]);
