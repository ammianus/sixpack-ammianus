#include "cximage.h"

HMODULE CxImage::hLib = NULL;
PFNCXILOAD CxImage::cxi_load;
PFNCXISAVE CxImage::cxi_save;
PFNCXIFREE CxImage::cxi_free;
PFNCXIGETXY CxImage::cxi_getw, CxImage::cxi_geth,
            CxImage::cxi_getbpp, CxImage::cxi_getpalsize;
PFNCXIGETBITS CxImage::cxi_getbits, CxImage::cxi_getpal;
PFNCXIRESIZE CxImage::cxi_resize;

	
bool CxImage::load(char *fileName, int imgType)
{
	img = cxi_load(fileName, CXI_FROM_FILE, 0, imgType);
		
	if (img)
		return true;
	return false;
}
	
	
void CxImage::free()
{
	if (img != NULL)
	{
		cxi_free(img);
		img = NULL;
	}
}
	
	
DIB *CxImage::getDIB()
{
	if (img == NULL)
		return new DIB;
		
    return new DIB(cxi_getbits(img), cxi_getw(img), cxi_geth(img), cxi_getbpp(img), NULL);
}
	

unsigned char *CxImage::getPalette()
{
	unsigned char *pal, *outPal;
    int palSize;
    
	if (img == NULL)
		return NULL;
	if (cxi_getbpp(img) != 8)
		return NULL;

	pal = cxi_getpal(img);
	palSize = cxi_getpalsize(img);
	
    if (pal == NULL)
		return NULL;
	outPal = (unsigned char*)malloc(768);
	for (int i=0; i<palSize/4; i++)
	{
		outPal[i*3+0] = pal[i*4+0];
		outPal[i*3+1] = pal[i*4+1];
		outPal[i*3+2] = pal[i*4+2];
	}

	return outPal;
}
