
#include <math.h>
#include <stdio.h>
#include <memory.h>

#define PI 3.14159265358979323846f

void copyMatrix(float dest[4][4], float src[4][4])
{
	memcpy(dest, src, sizeof(float[4][4]));
}

void setIdMatrix(float m[4][4])
{
	static float id[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
	copyMatrix(m, id);
}

void mult_matrix_vec(float mat[4][4], float src[4], float dst[4])
{
    dst[0] = (
		src[0] * mat[0][0] +
		src[1] * mat[0][1] +
		src[2] * mat[0][2] +
		src[3] * mat[0][3]);
	dst[1]  = (
		src[0] * mat[1][0] +
		src[1] * mat[1][1] +
		src[2] * mat[1][2] +
		src[3] * mat[1][3]);
	dst[2]  = (
		src[0] * mat[2][0] +
		src[1] * mat[2][1] + 
		src[2] * mat[2][2] +
		src[3] * mat[2][3]);
	dst[3] = (
		src[0] * mat[3][0] +
		src[1] * mat[3][1] + 
		src[2] * mat[3][2] +
		src[3] * mat[3][3]);
}

void matrixmult(float m[4][4], float b[4][4])
{
	int i, j, c;
	float a[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

	for(i = 0; i < 4; i++)
	{
		for(j = 0; j < 4; j++)
		{
			for(c = 0; c < 4; c++)
			{
				a[i][j] += m[i][c] * b[c][j];
			}
		}
	}

	copyMatrix(m, a);
}

void makeInverseTransposeMatrix(float m[4][4], float v[3])
{
	setIdMatrix(m);
	m[0][3] = -v[0];
	m[1][3] = -v[1];
	m[2][3] = -v[2];
}

/* Simple rotation matrices */
void makeInverseRotateMatrixX(float m[4][4], float degRot)
{
	float radRot = -(degRot * PI) / 180.0f; 
	float cosRot = (float)cos(radRot);
	float sinRot = (float)sin(radRot);

	setIdMatrix(m);
	m[1][1] = cosRot;
	m[2][2] = cosRot;
	m[1][2] = -sinRot;
	m[2][1] = sinRot;
}

void makeInverseRotateMatrixY(float m[4][4], float degRot)
{
	float radRot = -(degRot * PI) / 180.0f; 
	float cosRot = (float)cos(radRot);
	float sinRot = (float)sin(radRot);

	setIdMatrix(m);
	m[0][0] = cosRot;
	m[2][2] = cosRot;
	m[0][2] = sinRot;
	m[2][0] = -sinRot;
}

void makeInverseRotateMatrixZ(float m[4][4], float degRot)
{
	float radRot = -(degRot * PI) / 180.0f; 
	float cosRot = (float)cos(radRot);
	float sinRot = (float)sin(radRot);

	setIdMatrix(m);
	m[0][0] = cosRot;
	m[1][1] = cosRot;
	m[0][1] = -sinRot;
	m[1][0] = sinRot;
}

/* Generic rotation matrix - just used for testing */
void makeInverseRotateMatrix(float m[4][4], float degRot, float x, float y, float z)
{
	float radRot = -(degRot * PI) / 180.0f; 
	float sqnorm = x * x + y * y + z * z;
	float sin_theta;
	float q[4];

	radRot *= 0.5f;	// ?
		sin_theta = (float)sin(radRot);

	if (sqnorm != 1) 
		sin_theta /= (float)sqrt(sqnorm);

	q[0] = sin_theta * x;
	q[1] = sin_theta * y;
	q[2] = sin_theta * z;
	q[3] = (float)cos(radRot);

{
    float s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;
    float norm = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];

	if (norm == 0)
		s = 0;
	else
		s = 2 / norm;

    xs = q[0] * s;
    ys = q[1] * s;
    zs = q[2] * s;

    wx = q[3] * xs;
    wy = q[3] * ys;
    wz = q[3] * zs;

    xx = q[0] * xs;
    xy = q[0] * ys;
    xz = q[0] * zs;

    yy = q[1] * ys;
    yz = q[1] * zs;
    zz = q[2] * zs;

    m[0][0] = 1 - (yy + zz);
    m[1][0] = xy + wz;
    m[2][0] = xz - wy;

    m[0][1] = xy - wz;
    m[1][1] = 1 - (xx + zz);
    m[2][1] = yz + wx;

    m[0][2] = xz + wy;
    m[1][2] = yz - wx;
    m[2][2] = 1 - (xx + yy);

    m[3][0] = m[3][1] = m[3][2] = m[0][3] = m[1][3] = m[2][3] = 0;
    m[3][3] = 1;
}

}

/* Test function */
void dumpMatrix(float m[4][4])
{
	static int create = 1;
	int i, j;
	FILE* fp;
	
	if (create)
	{
		create = 0;
		fp = fopen("test.txt", "w");
	}
	else
	{
		fp = fopen("test.txt", "a");
	}

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
			fprintf(fp, "%f ", m[i][j]);
		fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
	fclose(fp);
}
