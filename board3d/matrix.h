/*
* matrix.c
* by Jon Kinsey, 2003
*
* Simple 3d matrix functions
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of version 2 of the GNU General Public License as
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
* $Id$
*/

void setIdMatrix(float m[4][4]);
#define copyMatrix(to, from) memcpy(to, from, sizeof(float[4][4]))

void makeInverseTransposeMatrix(float m[4][4], float v[3]);
void makeInverseRotateMatrix(float m[4][4], float degRot, float x, float y, float z);
void makeInverseRotateMatrixX(float m[4][4], float degRot);
void makeInverseRotateMatrixY(float m[4][4], float degRot);
void makeInverseRotateMatrixZ(float m[4][4], float degRot);

void mult_matrix_vec(float mat[4][4], float src[4], float dst[4]);
void matrixmult(float m[4][4], float b[4][4]);

void dumpMatrix(float m[4][4]);
