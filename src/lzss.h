/*
	LZSS codec interface

	Based on the program LZSS.C by Haruhiko Okumura, 1989
	C++ version and modifications by Mic, 2009
*/

#ifndef __LZSS_H__
#define __LZSS_H__

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


class LZSS
{
public:	
    LZSS()
    {
        N = 4096;
		Np1 = N + 1;
		F = 18;
		NM1 = N - 1;
		FM1 = F - 1;
		NIL = N;
		THRESHOLD = 2;
		dictbits = 12;
		C = ' ';
		match_length = 0;
    }
    
    // Set the dictionary size (should be a power of 2) and the threshold value for string matching
    void configure(int dictLen, int threshold, char initVal);
    
    // Return statistics from the last call to Encode/Decode
	void stats(int *sizeBefore, int *sizeAfter)
	{
        if (sizeBefore) 
		    *sizeBefore = textsize;
		if (sizeAfter)
            *sizeAfter = codesize;
	}

	// Encode a file into another file
	bool encode(char *srcFileName, char *destFileName);
	
	// Encode a chunk of memory and return a pointer to the encoded data
    char *encode(char *src, int srcLength);

    // Decode a file into another file
	bool decode(char *srcFileName, char *destFileName);    
            
private:
	void InsertNode(int r);
	void DeleteNode(int p);

	void InitTree()
	{
		for (int i=0; i<=N; i++)
			dad[i] = NIL;
		for (int i=0; i<=N+256; i++)
			rson[i] = NIL;
		for (int i=0; i<=N; i++)
			lson[i] = NIL;
	}
	
	int N, Np1, F, NM1, FM1, NIL, THRESHOLD;
	char C;
	int match_length, match_position;
	int dictbits, dictmask, lenmask;
	int textsize, codesize;
	int bytes_in, bytes_out;
	unsigned char *text_buf;
	int *lson,*rson,*dad;
};

#endif
