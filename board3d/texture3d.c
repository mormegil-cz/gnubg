/*
* texture3d.c
* by Jon Kinsey, 2003
*
* Functions to load texture files
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#pragma pack(1)

/* DIB file header structure */
typedef struct		
{
	unsigned short	bfType;		/* Magic number for file */
	unsigned int	bfSize;		/* Size of file */
	unsigned short	bfReserved1;	/* Reserved */
	unsigned short	bfReserved2;	/* ... */
	unsigned int	bfOffBits;	/* Offset to bitmap data */
} BITMAPFILEHEADER;

/* DIB file info structure */
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

#if __BIG_ENDIAN__
#define TEXTURE_SWAP(x) for (i = 0; i < sizeof (x) / 2; i ++) { \
                            char c = ((char *) &x)[i]; \
                            ((char *) &x)[i] = ((char *) &x)[sizeof (x) - i - 1];\
                            ((char *) &x)[sizeof (x) - i - 1] = c; \
                        }
#endif
                            

int MakeLittleEndian_BMPHeader (BITMAPFILEHEADER *ph)
{
#if __BIG_ENDIAN__

    int i;
    
    /*TEXTURE_SWAP (ph->bfType);*/ /* do not swap this one! */
    TEXTURE_SWAP (ph->bfSize);
    TEXTURE_SWAP (ph->bfReserved1);
    TEXTURE_SWAP (ph->bfReserved2);
    TEXTURE_SWAP (ph->bfOffBits);

#endif

    return 0;
}


int MakeLittleEndian_BMPInfoHeader (BITMAPINFOHEADER *ph)
{
#if __BIG_ENDIAN__

    int i;
    
    TEXTURE_SWAP (ph->biSize);
    TEXTURE_SWAP (ph->biWidth);
    TEXTURE_SWAP (ph->biHeight);
    TEXTURE_SWAP (ph->biPlanes);
    TEXTURE_SWAP (ph->biBitCount);
    TEXTURE_SWAP (ph->biCompression);
    TEXTURE_SWAP (ph->biSizeImage);
    TEXTURE_SWAP (ph->biXPelsPerMeter);
    TEXTURE_SWAP (ph->biYPelsPerMeter);
    TEXTURE_SWAP (ph->biClrUsed);
    TEXTURE_SWAP (ph->biClrImportant);

#endif

    return 0;
}

#include <assert.h>

unsigned char *LoadDIBTexture(FILE *fp, int *width, int *height)
{
	unsigned char *bits;	/* Bitmap pixel bits */
	unsigned int bitsize;	/* Size of bitmap */
	BITMAPFILEHEADER header;
	BITMAPINFOHEADER infoheader;
	unsigned int i;
	unsigned char *ptr;
        
	/* Read in header info */
	if ((fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) != 1) ||
                MakeLittleEndian_BMPHeader (&header) ||
		(memcmp(&header.bfType, "BM", 2) != 0) ||
		(fread(&infoheader, sizeof(BITMAPINFOHEADER), 1, fp) != 1) ||
                MakeLittleEndian_BMPInfoHeader (&infoheader) ||
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
	{	/* 8 bit bitmap, duplicate rgb values */
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

	return bits;
}

#include "config.h"

#ifdef HAVE_LIBPNG

#include <png.h>

#define PNG_CHECK_BYTES 8

unsigned char *LoadPNGTexture(FILE *fp, int *width, int *height)
{
	unsigned char header[PNG_CHECK_BYTES];
	png_structp png_ptr;
	png_infop info_ptr;
	int rowbytes, i, colour;
	png_bytep data, *row_p;

	fread(header, 1, PNG_CHECK_BYTES, fp);
	if (!png_check_sig(header, PNG_CHECK_BYTES))
		return 0;	/* Not a PNG file */

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);

	if (!png_ptr || !info_ptr || setjmp(png_jmpbuf(png_ptr)))
		return 0;	/* Problem with png library */

	/* Setup input */
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, PNG_CHECK_BYTES);

	/* Read file info */
	png_read_info(png_ptr, info_ptr);

	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);

	/* Standardize colour formats */
	colour = png_get_color_type(png_ptr, info_ptr);

	if (colour == PNG_COLOR_TYPE_GRAY || colour == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (colour & PNG_COLOR_MASK_ALPHA)
		png_set_strip_alpha(png_ptr);

	if (colour == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);

	/* Reflect updated flags */
	png_read_update_info(png_ptr, info_ptr);

	/* Alloc space for image */
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	data = (png_bytep)malloc((*height) * rowbytes);
	/* Row pointers for png read */
	row_p = (png_bytep *)malloc(sizeof(png_bytep) * (*height));
	if (!data || !row_p)
		return 0;	/* Allocation failed */
	/* Assign row points inside data
		NB. notice reveresed y co-ords as opengl is upside down */
	for (i = 0; i < *height; i++)
		row_p[*height - 1 - i] = &data[i * rowbytes];

	/* Read data */
	png_read_image(png_ptr, row_p);

	/* Tidy up */
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(row_p);

	return data;
}

#endif
