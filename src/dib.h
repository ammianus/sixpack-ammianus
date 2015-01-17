/*
	DIB (Device Independent Bitmap) interface
	
	/Mic, 2009
*/

#ifndef __DIB_H__
#define __DIB_H__

#include <stdlib.h>
#include <stdio.h>


class DIB
{
public:
	int width, height, pitch, bitCount, errorCode;
	bool allocatedMemory;
	unsigned char *bits;
	unsigned char *palette;

	DIB()
	{
		bits = palette = NULL;
		width = height = bitCount = 0;
		allocatedMemory = false;
	}
	
	DIB(unsigned char *diBits, int diWidth, int diHeight, int diBitCount, unsigned char *diPal)
	{
		//printf("new DIB %p %d %d %d\n",dibits,diwidth,diheight,dibitcount);
		bits = diBits;
		width = diWidth;
		height = diHeight;
		bitCount = diBitCount;
		palette = diPal;
		allocatedMemory = false;
	}
	
	
	DIB(int diWidth, int diHeight, int diBitCount)
	{
		bits =(unsigned char*)malloc(diWidth * diHeight * (diBitCount >> 3));
		palette = NULL;
		width = diWidth;
		height = diHeight;
		bitCount = diBitCount;
		allocatedMemory = true;
	}
	
	
	~DIB()
	{
		if (allocatedMemory)
			free(bits);
	}
	
	// Writes the contents of a DIB to a .BMP file
	// Works only for 256-color DIBs
	// The DIB width must be a multiple of 4
	void saveBMP(char *fileName, bool flip);
};


#endif
