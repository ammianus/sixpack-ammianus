/*
	LZSS codec implementation
*/
	
#include "lzss.h"


// For mingw
#ifndef _MSC_VER
#define fopen_s(a,b,c) *(a)=fopen(b,c)
#endif


void LZSS::configure(int dictLen, int threshold, char initVal)
{
	THRESHOLD = threshold;
	
    for (dictbits=0; dictbits<32; dictbits++)
    {
        if ((dictLen >> dictbits) == 1) break;
    }   
    //dictbits = (int)(log((float)dictlen) / log(2.0f));
	
    N = 1 << dictbits;
	F = (1 << (16 - dictbits)) + THRESHOLD;
	NIL = N;
	C = initVal;
	dictmask = (((N - 1) & 0xF00) >> 8) << (16 - dictbits);
	lenmask = (1 << (16 - dictbits)) - 1;
	
	//printf("LZSS::Configure: dictlen=%d,dictbits=%d N=%d F=%d dictmask=0x%x lenmask=0x%x\n",dictlen,dictbits,N,F,dictmask,lenmask);
}

	
// Encode a file into another file
bool LZSS::encode(char *srcFileName, char *destFileName)
{
	int c,len,r,s,last_match_length,mask,i;
	int *code_buf;
	int code_buf_ptr;
	FILE *infile,*outfile;
		
	text_buf = new unsigned char[N + F - 1];
	lson = new int[N + 1];
	rson = new int[N + 257];
	dad = new int[N + 1];

	textsize = codesize = 0;
    
	fopen_s(&infile, srcFileName, "rb");
	fopen_s(&outfile, destFileName, "wb");
		
	code_buf = new int[17];
	for (i=0; i<17; i++)
		code_buf[i] = 0;
			
	InitTree();
	code_buf_ptr = 1;
	mask = 1;
	s = 0;
	r = N - F;
		
	for (i=0; i<N+F-1; i++)
		text_buf[i] = C;
			
	len = 0;
	while (len < F)
	{
		c = fgetc(infile);
		if (c == EOF)
			break;
		else
		{
			text_buf[r + len] = c;
			len++;
		}
	}
		
	if ((textsize = len) == 0)
		return false;
		
	for (i=1; i<=F; i++)
		InsertNode(r - i);
	InsertNode(r);

	while (len > 0)
	{
		if (match_length > len)
			match_length = len;
		if (match_length <= THRESHOLD)
		{
			match_length = 1;
			code_buf[0] |= mask;
			code_buf[code_buf_ptr++] = text_buf[r];
		}
		else
		{
			code_buf[code_buf_ptr++] = match_position & 0xFF;
			code_buf[code_buf_ptr++] = (((match_position >> (dictbits - 8)) & dictmask) | (match_length - (THRESHOLD + 1))) & 0xFF;
		}

		mask += mask;
		if (mask > 0xFF)
		{
			for (i=0; i<code_buf_ptr; i++)
				fputc(code_buf[i], outfile);
			codesize += code_buf_ptr;
			code_buf[0] = 0;
			code_buf_ptr = 1;
			mask = 1;
		}
		last_match_length = match_length;
			
		for (i=0; i<last_match_length; i++)
		{
			c = fgetc(infile);
			if (c == EOF)
				break;
			DeleteNode(s);
				
			text_buf[s] = c;
			if ((s+1) < F)
				text_buf[s + N] = c;
			s = (s + 1) & (N - 1);
			r = (r + 1) & (N - 1);
			InsertNode(r);
		}
			
		textsize += i;
			
		while (i < last_match_length)
		{
			DeleteNode(s);
			s = (s + 1) & (N - 1);
			r = (r + 1) & (N - 1);
			len--;
			if (len)
				InsertNode(r);
			i++;
		}
	}
		
	if (code_buf_ptr > 1)
	{
		for (i=0; i<code_buf_ptr; i++)
			fputc(code_buf[i], outfile);
		codesize += code_buf_ptr;
	}
		
	fclose(infile);
	fclose(outfile);
				
	delete [] text_buf;
	delete [] lson;
	delete [] rson;
	delete [] dad;
	delete [] code_buf;

	return true;
}
    
    
// Encode a chunk of memory and return a pointer to the encoded data
char* LZSS::encode(char *src, int srcLength)
{
	int c,len,r,s,last_match_length,mask, i;
	int *code_buf;
	int code_buf_ptr, dest_offs, dest_capacity;
	char *dest;
		
	text_buf = new unsigned char[N + F - 1];
	lson = new int[N + 1];
	rson = new int[N + 257];
	dad = new int[N + 1];

	textsize = codesize = 0;
		
	code_buf = new int[17];
	for (i=0; i<17; i++)
		code_buf[i] = 0;
			
	InitTree();
	code_buf_ptr = 1;
	mask = 1;
	s = 0;
	r = N - F;
		
	for (i=0; i<N+F-1; i++)
		text_buf[i] = C;

	dest = (char*)malloc(50);
	dest_capacity = 50;
	dest_offs = 0;

	len = 0;
	//dest[dest_offs++] = F;
	while (len < F)
	{
		if (srcLength<=0)
		    break;
		c = *(src++);
		srcLength--;
		text_buf[r + len] = c;
		len++;

		//dest[dest_offs++] = c;
	}
    //codesize += F+1;

	if ((textsize = len) == 0)
		return NULL;
		
	for (i=1; i<=F; i++)
		InsertNode(r - i);
	InsertNode(r);

	while (len > 0)
	{
        //printf("len = %d, srclen=%d\n", len,src_length);
          
		if (match_length > len)
			match_length = len;
		if (match_length <= THRESHOLD)
		{
			match_length = 1;
			code_buf[0] |= mask;
			code_buf[code_buf_ptr++] = text_buf[r];
		}
		else
		{
			code_buf[code_buf_ptr++] = match_position & 0xFF;
			code_buf[code_buf_ptr++] = (((match_position >> (dictbits - 8)) & dictmask) | (match_length - (THRESHOLD + 1))) & 0xFF;
		}

		mask += mask;
		if (mask > 0xFF)
		{
			for (i=0; i<code_buf_ptr; i++)
			{
				dest[dest_offs++] = code_buf[i];
				if (dest_offs >= dest_capacity)
				{
					dest_capacity *= 2;
					dest = (char*)realloc(dest, dest_capacity);
                }
			}
			codesize += code_buf_ptr;
			code_buf[0] = 0;
			code_buf_ptr = 1;
			mask = 1;
		}
		last_match_length = match_length;
			
		for (i=0; i<last_match_length; i++)
		{
			if (srcLength<=0)
				break;
			c = *(src++);
			srcLength--;
			DeleteNode(s);
			
			text_buf[s] = c;
			if ((s+1) < F)
				text_buf[s + N] = c;
			s = (s + 1) & (N - 1);
			r = (r + 1) & (N - 1);
			InsertNode(r);
		}
			
		textsize += i;
			
		while (i < last_match_length)
		{
			DeleteNode(s);
			s = (s + 1) & (N - 1);
			r = (r + 1) & (N - 1);
			len--;
			if (len)
				InsertNode(r);
			i++;
		}
	}
		
	if (code_buf_ptr > 1)
	{
		for (i=0; i<code_buf_ptr; i++)
		{
			dest[dest_offs++] = code_buf[i];
			if (dest_offs >= dest_capacity)
            {
                dest_capacity *= 2;
                dest = (char*)realloc(dest, dest_capacity);
            }
		}
		codesize += code_buf_ptr;
	}
		
	delete [] text_buf;
	delete [] lson;
	delete [] rson;
	delete [] dad;
	delete [] code_buf;

	return dest;
}
	

// Decode a file into another file
bool LZSS::decode(char *srcFileName, char *destFileName)
{
	int i,ii,j,r,c,flags;
	int gotbytes;
	FILE *infile,*outfile;
		
	text_buf = new unsigned char[N + F - 1];
	lson = new int[N + 1];
	rson = new int[N + 257];
	dad = new int[N + 1];

	gotbytes = 0;

	fopen_s(&infile, srcFileName, "rb");
	fopen_s(&outfile, destFileName, "wb");

	r = N - F;
	flags = 0;

	for (i=0; i<N+F-1; i++)
		text_buf[i] = C;
			
	while (true)
	{
		flags >>= 1;
		if (!(flags & 0x100))
		{
			c = fgetc(infile);
			gotbytes++;
			if (c == EOF) 
				break; 
			flags = c | 0xFF00;
		}
		if (flags & 1)
		{
			c = fgetc(infile);
			gotbytes++;
			if (c == EOF)
				break;
			fputc(c, outfile);
			text_buf[r++] = c;
			r &= (N - 1);
		}
		else
		{
			ii = fgetc(infile);
			gotbytes++;
			if (ii == EOF) 
				break; 

			j = fgetc(infile);
			gotbytes++;
			if (j == EOF)
				break;
					
			ii = ii | ((j & dictmask) << (dictbits - 8));
			j = (j & lenmask) + THRESHOLD;

			while (j-- >= 0)
			{
				c = text_buf[ii++];
				ii &= (N - 1);
				fputc(c, outfile);
				text_buf[r++] = c;
				r &= (N - 1);
			}
		}
	}

	delete [] text_buf;
	delete [] lson;
	delete [] rson;
	delete [] dad;

	fclose(infile),
	fclose(outfile);
		
	return true;
}


    	
void LZSS::InsertNode(int r)
{
	int i,p,cmp, c1, c2;
	unsigned char *key = &text_buf[r];
		
	cmp = 1;
	c1 = r; 
	p = N+1+key[0];
	c2 = p;
	rson[r] = NIL;
	lson[r] = NIL;
	match_length = 0;

   //printf("InsertNode(%d)\n", r);
   
	while (true)
	{
		if (cmp>=0)
		{
			if (rson[p]!=NIL)
				p = rson[p]; 
			else
			{
				rson[p] = r;
				dad[r] = p;
				return;
			}
		}
		else
		{
			if (lson[p]!=NIL)
				p = lson[p];
			else
			{
				lson[p] = r;
				dad[r] = p;
				return;
			}
		}

		for (i=1; i<F; i++)
			if ((cmp = key[i] - text_buf[p + i]) != 0)
				break;

		if (i>match_length)
		{
			match_position = p;
			match_length = i;
			if (i >= F) 
				break;
		}
	}

	dad[r] = dad[p];
	lson[r] = lson[p];
	rson[r] = rson[p];
	dad[lson[p]] = r;
	dad[rson[p]] = r;
	c1 = dad[p];	
	if (rson[c1]==p)
		rson[c1] = r;
	else
		lson[c1] = r;
	dad[p] = NIL;
}


void LZSS::DeleteNode(int p)
{
	int q, d;

	if (dad[p]==NIL) return;

	if (rson[p]==NIL)
		q = lson[p];
	else
	{
		if (lson[p]==NIL) 
			q = rson[p];
		else
		{
			q = lson[p];
			if (rson[q]!=NIL) 
			{
				while (rson[q]!=NIL)
					q = rson[q];
				rson[dad[q]] = lson[q];
				dad[lson[q]] = dad[q];
				lson[q] = lson[p];
				dad[lson[p]] = q;
			} 
			rson[q] = rson[p];
			dad[rson[p]] = q;
		}
	}

	dad[q] = dad[p];
	d = dad[p];
	if (rson[d]==p)
		rson[d] = q;
	else
		lson[d] = q;
	dad[p] = NIL;
}
	
