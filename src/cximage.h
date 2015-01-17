/*
  CxImage wrapper interface
  
  /Mic 2009
*/

#ifndef __CXIMAGE_H__
#define __CXIMAGE_H__

#include <windows.h>
#include <stdio.h>
#include "dib.h"


// Image types. Used with Load() and Save().
enum
{
	CXI_FORMAT_UNKNOWN = 0,
	CXI_FORMAT_BMP = 1,
	CXI_FORMAT_GIF = 2,
	CXI_FORMAT_JPG = 3,
	CXI_FORMAT_PNG = 4,
	CXI_FORMAT_ICO = 5,
	CXI_FORMAT_TGA = 6,
	CXI_FORMAT_PCX = 7
};

enum
{
	CXI_GOOD_RESIZE = 0,
	CXI_FAST_RESIZE = 1
};


enum
{
	CXI_FROM_FILE = 1,
	CXI_FROM_MEMORY = 0
};

typedef int* (_stdcall *PFNCXILOAD)(char*,int,int,int);
typedef int (_stdcall *PFNCXISAVE)(int*,char*,int,int,int);
typedef void (_stdcall *PFNCXIFREE)(int*);
typedef int (_stdcall *PFNCXIGETXY)(int*);
typedef unsigned char* (_stdcall *PFNCXIGETBITS)(int*);
typedef void (_stdcall *PFNCXIRESIZE)(int*,int,int,int);


class CxImage
{
public:
    CxImage()
    {   
        if (hLib == NULL)
        {
            hLib = (HMODULE)LoadLibraryA("cximage.dll");
            if (hLib == NULL)
            {
                printf("error loading cximage.dll\n");
                return;
       	    }

            cxi_load    = (PFNCXILOAD)GetProcAddress(hLib, "CXI_LoadImage");
            cxi_save    = (PFNCXISAVE)GetProcAddress(hLib, "CXI_SaveImage");
            cxi_free    = (PFNCXIFREE)GetProcAddress(hLib, "CXI_FreeImage");
            cxi_getbits = (PFNCXIGETBITS)GetProcAddress(hLib, "CXI_GetBits");
            cxi_getw    = (PFNCXIGETXY)GetProcAddress(hLib, "CXI_GetWidth");
            cxi_geth    = (PFNCXIGETXY)GetProcAddress(hLib, "CXI_GetHeight");
            cxi_getbpp  = (PFNCXIGETXY)GetProcAddress(hLib, "CXI_GetBpp");
            cxi_getpalsize  = (PFNCXIGETXY)GetProcAddress(hLib, "CXI_GetPaletteSize");
			cxi_getpal  = (PFNCXIGETBITS)GetProcAddress(hLib, "CXI_GetPalette");
			cxi_resize  = (PFNCXIRESIZE)GetProcAddress(hLib, "CXI_Resize");
        }
                	
        img = NULL;
    }
    
	~CxImage()
	{
		free();
	}

    bool load(char *fileName, int imgType);
    void free();
    
    int getWidth()
    {
        return cxi_getw(img);
    }
    
    int getHeight()
    {
        return cxi_geth(img);
    }
    
	void resize(int width, int height, int mode)
	{
		cxi_resize(img, width, height, mode);
	}

	unsigned char *getPalette();

    DIB *getDIB();
            
private:
	static HMODULE hLib;
	static PFNCXILOAD cxi_load;
	static PFNCXISAVE cxi_save;
	static PFNCXIFREE cxi_free;
	static PFNCXIGETXY cxi_getw,cxi_geth,cxi_getbpp,cxi_getpalsize;
	static PFNCXIGETBITS cxi_getbits,cxi_getpal;
	static PFNCXIRESIZE cxi_resize;

	int *img;
          
};


#endif
