/*
* texture3d.c
* by Jon Kinsey, 2003
*
* Function to load DIB texture file
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

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#pragma pack(1)

/* BMP file header structure */
typedef struct		
{
	unsigned short	bfType;		/* Magic number for file */
	unsigned int	bfSize;		/* Size of file */
	unsigned short	bfReserved1;	/* Reserved */
	unsigned short	bfReserved2;	/* ... */
	unsigned int	bfOffBits;	/* Offset to bitmap data */
} BITMAPFILEHEADER;

/* BMP file info structure */
typedef struct
{
	unsigned int	biSize;			/* Size of info header */
	int				biWidth;			/* Width of image */
	int				biHeight;		/* Height of image */
	unsigned short	biPlanes;		/* Number of color planes */
	unsigned short	biBitCount;		/* Number of bits per pixel */
	unsigned int	biCompression;	/* Type of compression to use */
	unsigned int	biSizeImage;		/* Size of image data */
	int				biXPelsPerMeter;	/* X pixels per meter */
	int				biYPelsPerMeter;	/* Y pixels per meter */
	unsigned int	biClrUsed;		/* Number of colors used */
	unsigned int	biClrImportant;	/* Number of important colors */
} BITMAPINFOHEADER;

#pragma pack()

unsigned char *LoadDIBitmap(const char *filename, int *width, int *height)
{
	FILE *fp;
	unsigned char *bits;	/* Bitmap pixel bits */
	unsigned int bitsize;	/* Size of bitmap */
	BITMAPFILEHEADER header;
	BITMAPINFOHEADER infoheader;
	unsigned int i;
	unsigned char *ptr;

	fp = fopen(filename, "rb");
	if (!fp)
		return 0;

	/* Read in header info */
	if ((fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) != 1) ||
		(memcmp(&header.bfType, "BM", 2) != 0) ||
		(fread(&infoheader, sizeof(BITMAPINFOHEADER), 1, fp) != 1) ||
		(fseek(fp, header.bfOffBits, SEEK_SET)))	/* Skip colour map */
	{
		fclose(fp);
		return 0;
	}
	*width = infoheader.biWidth;
	*height = infoheader.biHeight;
	bitsize = infoheader.biSizeImage;
	if (bitsize == 0)
		bitsize = infoheader.biWidth * infoheader.biHeight * infoheader.biBitCount / 8;

	if (infoheader.biBitCount == 24)
	{
		unsigned char temp;
		unsigned int triplets = infoheader.biWidth * infoheader.biHeight;

		if (((bits = malloc(bitsize)) == 0) ||
			(fread(bits, 1, bitsize, fp) != bitsize))
		{
			free(bits);
			fclose(fp);
			return 0;
		}
		/*
		 * Convert the bitmap data from BGR to RGB. Since texture images
		 * must be a power of two, and since we can figure that we won't
		 * have a texture image as small as 2x2 pixels, we ignore any
		 * alignment concerns...
		 */
		ptr = bits;
		for (i = 0; i < triplets; i++)
		{	/* Swap red and blue */
			temp = ptr[0];
			ptr[0] = ptr[2];
			ptr[2] = temp;
			ptr += 3;
		}
	}
	else
	{	// 8 bit bitmap, duplicate rgb values
		if ((bits = malloc(bitsize * 3)) == NULL)
		{
			fclose(fp);
			return 0;
		}

		ptr = bits;
		for (i = 0; i < bitsize; i++)
		{
			int bit;

			if (fread(&bit, 1, 1, fp) != 1)
			{
				free(bits);
				fclose(fp);
				return 0;
			}
			ptr[0] = ptr[1] = ptr[2] = bit;
			ptr += 3;
		}
	}

	fclose(fp);
	return (bits);
}
