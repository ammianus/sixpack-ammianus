/*
  Sixpack : Image converter and packer

  /Mic, 2009-2010
*/

#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <string.h>
#include <time.h>
#include "cximage.h"
#include "dib.h"
#include "lzss.h"
#include "neuquant.h"
#include "aplib.h"
//added by ammianus - 2/19/2012
#include "palreference.h"

//yes
using namespace std;
/*
typedef struct tagRGBQUAD {
  unsigned char rgbBlue;
  unsigned char rgbGreen;
  unsigned char rgbRed;
  unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagPOINT {
	int x;
	int y;
} POINT;*/

// For mingw
#ifndef _MSC_VER
#define fopen_s(a,b,c) *(a)=fopen(b,c)
#endif

#ifdef _MSC_VER
#pragma comment (lib,"uuid.lib")
#endif

enum
{
    CODEC_NONE=0,
    CODEC_LZSS=1,
    CODEC_RLE=2,
	CODEC_APLIB=3,
};

enum
{
    FILETYPE_BINARY = 0,
    FILETYPE_IMAGE = 1
};

enum
{
    COMPMODE_NONE=0,
    COMPMODE_ENCODE=1,
    COMPMODE_DECODE=2
};

enum
{
	TARGET_UNKNOWN = 0,
 	TARGET_SMS = 1,		// SEGA Master System
 	TARGET_SGG = 2,		// SEGA Game Gear
	TARGET_SNES = 3,	// Super Nintendo Entertainment System
	TARGET_TGX = 4,		// TurboGrafx-16
	TARGET_SMD = 5,		// SEGA Megadrive
	TARGET_32X = 6,		// SEGA 32X
	TARGET_SAT = 7,		// SEGA Saturn
	TARGET_DMG = 8,		// Gameboy (monochrome)
	TARGET_LAST
};

enum
{
	FMT_P1  = 0x01,	 // 1-bit planar
    FMT_P2  = 0x02,  // 2-bit planar
    FMT_P4  = 0x04,  // 4-bit planar
	FMT_P8  = 0x08,  // 8-bit planar (e.g. SNES)
    FMT_L4  = 0x10,  // 4-bit linear
    FMT_L8  = 0x20,  // 8-bit linear
    FMT_D15 = 0x40,  // 15-bit direct
	FMT_RLE8= 0x80,  // 8-bit run length encoded (32X)
};

// The set of LZSS parameters to try when using the -autocomp option
const int LZSS_PARAMS[2][3] =
{
    {1024, 2048, 4096}, // Dictionary sizes
    {2, 3, 4} // Thresholds
};

const int NUM_TARGETS = TARGET_LAST-1;
const char TARGET_STRINGS[NUM_TARGETS][5] =
{
	"sms",
	"sgg",
	"snes",
	"tgx",
	"smd",
	"32x",
	"sat",
	"dmg"
};


const int NUM_CODECS=3;
const char CODEC_STRINGS[NUM_CODECS][8] =
{
	"lzss",
	"rle",
	"aplib",
};

const int NUM_FORMATS = 8;
const char FORMAT_STRINGS[NUM_FORMATS][4] =
{
	"p1",
	"p2",
	"p4",
	"p8",
	"l4",
	"l8",
	"d15",
	"rl8",
};


int MAX_FORMAT_QUANT[256];
int MAX_FORMAT_PLANES[256];

#define ARG_PALREF "-palref"
#define ARG_SAVEPAL "-savepal"

typedef struct
{
    int dictSize,threshold,compressedSize;
    char *data;
} CompressionResult;

typedef struct
{
	unsigned short red;
	unsigned short green;
	unsigned short blue;
	unsigned short alpha;
} MaskColor16Bpp;

typedef struct
{
	const int target;
    const int formatsAllowed;
    const int defaultFormat;
    int palBits;
    int maxPalettes;
    int maxTiles;
	int supportsFlip;
    char assembler[16];
    char romExt[8];
	char asmExt[8];
	int nMask16;
} TargetCaps;


const MaskColor16Bpp MASK_COLOR_16_BPP[] =
{
	{0x001F, 0x03E0, 0x7C00, 0x8000}
};

unsigned char dmgPalette[][3] =
{
	{225, 248, 200},
	{134, 192, 128},
	{12, 102, 93},
	{8, 24, 0}
};

const TargetCaps TARGET_CAPS[NUM_TARGETS + 1] =
{
    {TARGET_UNKNOWN, FMT_L8,               FMT_L8,    8, 1, 65536, 0, "",               "",     "", 0},
    {TARGET_SMS,     FMT_P1|FMT_P4,        FMT_P4,    2, 2,   512, 1, "wla-z80",     "sms", ".asm", 0},
    {TARGET_SGG,     FMT_P1|FMT_P4,        FMT_P4,    4, 2,   512, 1, "wla-z80",      "gg", ".asm", 0},
    {TARGET_SNES,    FMT_P2|FMT_P4|FMT_P8, FMT_P4,    5, 8,  1024, 1, "wla-65816",   "smc", ".asm", 0},
	{TARGET_TGX,     FMT_P4,               FMT_P4,    3, 4,  4096, 0, "wla-huc6280", "pce", ".asm", 0},
	{TARGET_SMD,     FMT_L4,               FMT_L4,    3, 4,  2048, 1, "m68k-elf-as", "bin",   ".s", 0},
	{TARGET_32X,     FMT_L8|FMT_RLE8|FMT_D15, FMT_L8, 8, 1,  4096, 0, "sh-elf-as",   "32x",   ".s", 0},
	{TARGET_SAT,     FMT_L4|FMT_L8|FMT_D15,FMT_L8,   16, 1, 32768, 0, "sh-elf-as",   "sat",   ".s", 0},
    {TARGET_DMG,     FMT_P2,               FMT_P2,    2, 4,   256, 0, "wla-gb",      "dmg", ".asm", 0},
};


vector<int> chksums;
vector<int> nameTable;
vector<int> flipTable;
vector<unsigned char> palTable;
vector<POINT> coords;
vector<CompressionResult> compRes;

int optimise = 0,
    compmode = COMPMODE_NONE,
    verbose = 0,
    codec = CODEC_LZSS,
    transparent = 0,
    dither = 0,
    in_filetype = FILETYPE_BINARY,
    scaleX = 0,
    scaleY = 0,
    resizeType = CXI_FAST_RESIZE,
	ncolors,
	quantise = 0,
	metaheight =8,
	metawidth = 8 ,
	bitmapMode = 0,
	planesWanted = -1,
	planesNeeded = 0,
	chainplanes = 0,
	basecol = 0,
	threshold = 2,
	dictSize = 4096,
	bestopt = 0,
	genbat = 0,
	format = 0,
	maptype = 0,
	generateCode = 0,
	target = TARGET_UNKNOWN,
	paletteRef = 0,
	savePaletteRef = 0;


unsigned char bgcol = 0;

POINT bgcol_pos;

string inFileName,
       inFileNameShort,
       outFileName,
       outFileNameShort,
       preview,
       inPaletteFileName,
       inPaletteFileNameShort,
       outPaletteFileName,
       outPaletteFileNameShort;



int findColor(vector<RGBQUAD> *colorList, RGBQUAD *color)
{
    int result = -1, j = -1;
    vector<RGBQUAD>::const_iterator i;

    for (i=colorList->begin(); i!=colorList->end(); i++)
    {
        j++;
        if ((i->rgbRed == color->rgbRed) && (i->rgbGreen == color->rgbGreen) &&
            (i->rgbBlue == color->rgbBlue) && (i->rgbReserved == color->rgbReserved))
        {
            result = j;
            break;
        }
    }

    return result;
}


/*
 *  Count the number of colors in a DIB, up to a limit, and return a vector of the colors found
 *  added reference to colorList from palette reference file - ammianus 2/19/2012
 */
vector<RGBQUAD> *countDIBColors(DIB *dib, int giveUp, std::vector<RGBQUAD> * colorList)
{
    RGBQUAD color;
    //vector<RGBQUAD> *colorList = new vector<RGBQUAD>;
    int c;

	for (int i=0; i<dib->height; i++)
	{
		for (int j=0; j<dib->width; j++)
		{
            if (dib->bitCount == 8)
            {
                color.rgbReserved = dib->bits[i * dib->width + j];
                color.rgbRed = color.rgbGreen = color.rgbBlue = 0;
            }
            else if (dib->bitCount == 24)
            {
                 color.rgbRed = dib->bits[(i * dib->width + j) * 3];
                 color.rgbGreen = dib->bits[(i * dib->width + j) * 3 + 1];
                 color.rgbBlue = dib->bits[(i * dib->width + j) * 3 + 2];
                 color.rgbReserved = 0;
            }
            else if (dib->bitCount == 32)
            {
                 color.rgbRed = dib->bits[(i * dib->width + j) * 4];
                 color.rgbGreen = dib->bits[(i * dib->width + j) * 4 + 1];
                 color.rgbBlue = dib->bits[(i * dib->width + j) * 4 + 2];
                 //TODO was this supposed to be rgbBlue or rgbReserved?
                 color.rgbBlue = dib->bits[(i * dib->width + j) * 4 + 3];
            }

            c = findColor(colorList, &color);
            if (c == -1)
            {
                colorList->push_back(color);
            }

            if ((giveUp != 0) && (colorList->size() > giveUp))
               return colorList;
        }
    }

    return colorList;
}


// Calculate the checksum of a tile. Used for tile matching.
int calcChksum(DIB *dib, int x, int y)
{
	int chksum;
	unsigned char *ptr;

	ptr = &dib->bits[y * dib->width * 8 + x * 8];
	chksum = 0;

	for (int v=0; v<8; v++)
	{
		for (int h=0; h<8; h++)
			chksum += ptr[h];
		ptr += dib->width;
    }

	return chksum;
}


// Check if two tiles are equal with the given h/v flip setting
bool equalTile(DIB *dib, int x1, int y1, int x2, int y2, int flip)
{
	unsigned char *ptr1,*ptr2;
	int xStart,xEnd,xStep,yStart,yEnd,yStep;

	if (!(flip & 1))
	{
		xStart = 0;
		xEnd = 8;
		xStep = 1;
    }
    else
    {
		xStart = 7;
		xEnd = -1;
		xStep = -1;
    }

	if (!(flip & 2))
	{
		yStart = 0;
		yEnd = 8;
		yStep = 1;
    }
    else
    {
		yStart = 7;
		yEnd = -1;
		yStep = -1;
    }

	ptr1 = &dib->bits[(y1 * 8 * dib->width) + x1 * 8];
	ptr2 = &dib->bits[(y2 * 8 * dib->width) + x2 * 8];

	for (int v=yStart; v!=yEnd; v+=yStep)
	    for (int h=xStart; h!=xEnd; h+=xStep)
			if (ptr1[v * dib->width + h] != ptr2[v * dib->width + h])
				return false;

	return true;
}


// Check if the tile matches any of the other unique tiles we've found so far
void matchTile(DIB *dib, int x, int y)
{
	int chksum,found,flip;
	POINT coord;

	chksum = calcChksum(dib, x, y);
	found = -1;
	flip = 0;

	if (optimise)
	{
		for (int i=0; ((found==-1)&&(i<chksums.size())); i++)
		{
			if (chksum==chksums[i])
			{
                for (flip=0; flip<4; flip++)
                {
					if (equalTile(dib, x, y, coords[i].x, coords[i].y, flip))
				    {
		  			    found = i;
					    break;
                    }
					if (TARGET_CAPS[target].supportsFlip == 0)
						break;
                }
            }
        }
    }

	if (found != -1)
	{
		nameTable.push_back(found);
		flipTable.push_back(flip);
    }
	else
	{
		chksums.push_back(chksum);
		coord.x = x; coord.y = y;
		coords.push_back(coord);
		nameTable.push_back((int)chksums.size() - 1);
		flipTable.push_back(0);
    }
}


DIB *flipAndPad(DIB *srcImage)
{
    DIB *flipped;
    int i;

    if (((srcImage->width & 7) | (srcImage->height & 7)) == 0)
	{
	    flipped = new DIB(srcImage->width, srcImage->height, srcImage->bitCount);
		if (flipped->bits == NULL)
			return NULL;

		// Flip the DIB
		for (i=0; i<srcImage->height; i++)
			for (int j=0; j<srcImage->width; j++)
			    for (int k=0; k<(srcImage->bitCount >> 3); k++)
				    flipped->bits[(i * flipped->width + j) * (flipped->bitCount >> 3) + k] =
					  srcImage->bits[((srcImage->height - (i + 1)) * srcImage->width + j) * (srcImage->bitCount >> 3) + k];
	}
	else
	{
        int newWidth = srcImage->width, newHeight = srcImage->height;
		if (newWidth & 7)
        {
		    newWidth = (newWidth & 0xFFF8) + 8;
			printf("Warning: Width is not a multiple of 8. Padding to %d pixels.\n", newWidth);
        }
		if (newHeight & 7)
		{
		    newHeight = (newHeight & 0xFFF8) + 8;
			printf("Warning: Height is not a multiple of 8. Padding to %d pixels.\n", newHeight);
        }

        flipped = new DIB(newWidth, newHeight, srcImage->bitCount);

		// Pad and flip the DIB

		if (srcImage->bitCount == 8)
		{
            unsigned char c = srcImage->bits[bgcol_pos.y * srcImage->width + bgcol_pos.x];
            for (i=0; i<newHeight; i++)
            {
                if (i >= srcImage->height)
                    for (int j=0; j<newWidth; j++)
                        flipped->bits[i * newWidth + j] = c;
                else
                {
                    for (int j=0; j<newWidth; j++)
                        if (j >= srcImage->width)
                            flipped->bits[i * newWidth + j] = c;
                        else
                            flipped->bits[i * newWidth + j] = srcImage->bits[(srcImage->height - (i + 1)) * srcImage->width + j];
                }
            }
        }
        else if ((srcImage->bitCount == 24) || (srcImage->bitCount == 32))
        {
            RGBQUAD c;
            int bpp = srcImage->bitCount >> 3;
            c.rgbRed   = srcImage->bits[bgcol_pos.y * srcImage->width * bpp + bgcol_pos.x * bpp + 0];
            c.rgbGreen = srcImage->bits[bgcol_pos.y * srcImage->width * bpp + bgcol_pos.x * bpp + 1];
            c.rgbBlue  = srcImage->bits[bgcol_pos.y * srcImage->width * bpp + bgcol_pos.x * bpp + 2];

            for (i=0; i<newHeight; i++)
            {
                if (i >= srcImage->height)
                    for (int j=0; j<newWidth; j++)
                    {
                        flipped->bits[(i * newWidth + j) * bpp + 0] = c.rgbRed;
                        flipped->bits[(i * newWidth + j) * bpp + 1] = c.rgbGreen;
                        flipped->bits[(i * newWidth + j) * bpp + 2] = c.rgbBlue;
                    }
                else
                {
                    for (int j=0; j<newWidth; j++)
                        if (j >= srcImage->width)
                        {
                            flipped->bits[(i * newWidth + j) * bpp + 0] = c.rgbRed;
                            flipped->bits[(i * newWidth + j) * bpp + 1] = c.rgbGreen;
                            flipped->bits[(i * newWidth + j) * bpp + 2] = c.rgbBlue;
                        }
                        else
                        {
                            flipped->bits[(i * newWidth + j) * bpp + 0] =
                              srcImage->bits[((srcImage->height - (i + 1)) * srcImage->width + j) * bpp + 0];
                            flipped->bits[(i * newWidth + j) * bpp + 1] =
                              srcImage->bits[((srcImage->height - (i + 1)) * srcImage->width + j) * bpp + 1];
                            flipped->bits[(i * newWidth + j) * bpp + 2] =
                              srcImage->bits[((srcImage->height - (i + 1)) * srcImage->width + j) * bpp + 2];
                        }
                }
            } //  for (i=0; i<newheight; i++)
	    }
    }

    return flipped;
}



DIB *quantiseDIB(DIB *srcimage, vector<RGBQUAD> *colors, NeuQuant *neuquant)
{
	DIB *quantised;
	int i;

	if (format == FMT_D15)
	{
		quantised = new DIB(srcimage->width, srcimage->height, 16);
		if (srcimage->bitCount == 8)
		{
			unsigned char c;
			for (i = 0; i < srcimage->height; i++)
			{
				for (int j = 0; j < srcimage->width; j++)
				{
					c = srcimage->bits[i * srcimage->width + j];
					unsigned short p15 = 0x8000 |
						                 (srcimage->palette[c * 3 + 2] >> 3) |
										 ((srcimage->palette[c * 3 + 1] >> 3) << 5) |
										 ((srcimage->palette[c * 3 + 0] >> 3) << 10);
					quantised->bits[(i * quantised->width + j) * 2 + 0] = (unsigned char)(p15 & 0xFF);
					quantised->bits[(i * quantised->width + j) * 2 + 1] = (unsigned char)(p15 >> 8);
				}
			}
		}
		else if (srcimage->bitCount == 24)
		{
			for (i = 0; i < srcimage->height; i++)
			{
				for (int j = 0; j < srcimage->width; j++)
				{
					unsigned short p15 = 0x8000 |
						                 (srcimage->bits[(i * srcimage->width + j) * 3 + 0] >> 3) |
										 ((srcimage->bits[(i * srcimage->width + j) * 3 + 1] >> 3) << 5) |
										 ((srcimage->bits[(i * srcimage->width + j) * 3 + 2] >> 3) << 10);
					quantised->bits[(i * quantised->width + j) * 2 + 0] = (unsigned char)(p15 & 0xFF);
					quantised->bits[(i * quantised->width + j) * 2 + 1] = (unsigned char)(p15 >> 8);
				}
			}
		}
		else if (srcimage->bitCount == 32)
		{
		}
		//printf("(0)inDIBQ = % p, inDIBQ->w,h = %d, %d. bpp = %d\n", inDIBQ, inDIBQ->width, inDIBQ->height, inDIB->bitCount);

	}
	else if (ncolors > quantise)
	{
		if (verbose)
			printf("More than %d colors found. Quantising image..\n", quantise);

		if (srcimage->bitCount == 8)
		{
			DIB *tempDIB = new DIB(srcimage->width, srcimage->height, 24);
			if (tempDIB->bits == NULL)
			{
				printf("Error: Failed to allocate memory.\n");
				return NULL;
			}
			unsigned char c;
			for (i = 0; i < tempDIB->height; i++)
				for (int j  =0; j < tempDIB->width; j++)
				{
					c = srcimage->bits[i * srcimage->width + j];
					tempDIB->bits[(i * tempDIB->width + j) * 3 + 0] = srcimage->palette[c * 3 + 0];
					tempDIB->bits[(i * tempDIB->width + j) * 3 + 1] = srcimage->palette[c * 3 + 1];
					tempDIB->bits[(i * tempDIB->width + j) * 3 + 2] = srcimage->palette[c * 3 + 2];
				}
			quantised = neuquant->quantise(tempDIB, quantise, 100, dither);
			delete tempDIB;
		}
		else if (srcimage->bitCount == 24)
		{
			quantised = neuquant->quantise(srcimage, quantise, 100, dither);
		}
		else if (srcimage->bitCount == 32)
		{
		}

		ncolors = quantise;
	}
	else
	{
        // No need to quantise the image. Just remap the colors

		quantised = new DIB(srcimage->bits, srcimage->width, srcimage->height, srcimage->bitCount, srcimage->palette);
		if (srcimage->bitCount == 8)
		{
			unsigned char c;
			for (i = 0; i < srcimage->width*srcimage->height; i++)
			{
				c = quantised->bits[i];
				for (int j = 0; j < colors->size(); j++)
					if ((*colors)[j].rgbReserved == c)
					{
						quantised->bits[i] = j;
						break;
					}
			}
			unsigned char *remapped_pal = (unsigned char*)malloc(768);
			for (int j=0; j<colors->size(); j++)
			{
				remapped_pal[j * 3 + 0] = srcimage->palette[(*colors)[j].rgbReserved * 3 + 2];
				remapped_pal[j * 3 + 1] = srcimage->palette[(*colors)[j].rgbReserved * 3 + 1];
				remapped_pal[j * 3 + 2] = srcimage->palette[(*colors)[j].rgbReserved * 3 + 0];
			}
			quantised->palette = remapped_pal;
		}
		else
		{
			unsigned char *remapped_pal = (unsigned char*)malloc(768);
			for (int j=0; j<colors->size(); j++)
			{
				remapped_pal[j * 3 + 0] = (*colors)[j].rgbRed;
				remapped_pal[j * 3 + 1] = (*colors)[j].rgbGreen;
				remapped_pal[j * 3 + 2] = (*colors)[j].rgbBlue;
			}
			quantised->palette = remapped_pal;
		}
	}

	return quantised;
}




void show_help()
{
	puts("sixpack r3\n/mic, 2009\n\nOptions:\n");
	puts("-autocomp\tAutomatically choose the best settings for the LZSS encoder.");
	puts("-base n\t\tSpecify the base color. For example if the image uses 4 colors\n\t\tand the base color is 2, the image colors will be mapped");
	puts("\t\tto palette entries 2..5.");
	//puts("-bat\t\tGenerate a batch file with commands to assemble the program\n\t\tthat is using the LZSS decoder with the correct flags.");
	puts("-bg x,y\t\tThe color referenced by the pixel at x,y will be treated as the\n\t\tbackground color.");
	puts("\t\tWhen this option isn't used x,y will be set to 0,0.");
	puts("-dither\t\tEnabled dithering (only used when quantising is done).");
	puts("-height n\tSpecify a new height for the image.");
	puts("-image\t\tTreat the input file as an image file. If not specified the\n\t\tinput file will be treated as a plain binary file.");
	puts("-o filename\tSpecify the output filename.");
	puts("-opt\t\tEnable tile optimisation (redundant tiles will be excluded).");
	puts("-pack\t\tEncode data before generating the output file.");
	puts("-planes n\tSpecify the desired number of bitplanes to use for storing each\n\t\tpixel. At least n planes will be used.");
	puts("\t\tA value of 0 means that the smallest possible number of\n\t\tbitplanes will be used. The default value is 4, which is also\n\t\tthe maximum.");
	puts("-preview fname\tCreate a .BMP file with a preview of what the processed image\n\t\twill look like.");
	puts("-q n\t\tQuantise the image to use n colors. The limits are 2 <= n <= 16");
	puts("-sizefilter n\tIf n==1 smooth filtering will be used when resizing the image\n\t\t(when -width and/or -height is used).");
	puts("\t\tOtherwise nearest neighbor sampling will be used.");
	puts("-t\t\tEnable transparency. When this option is used, all pixels using\n\t\tthe background color (see -bg) will be mapped to color 0.");
	puts("-threshold n\tSet the threshold parameter for the LZSS codec. This can\n\t\tusually be left at the default value (2).");
	puts("-unpack\t\tDecode the input file (not valid when -image is used).");
	puts("-v\t\tVerbose mode.");
	puts("-width \t\tSpecify a new width for the image.");
	//new options added by ammianus 2/19/2012
	puts("-palref fname\tUse a palette reference when processing image.");
	puts("-savepal fname\tSave resulting palette reference file after processing image.");
}



int main(int argc, char *argv[])
{
	printf("help!1\n");
	int i;
	CxImage *image = new CxImage;
	NeuQuant *neuquant = new NeuQuant;
	LZSS lzss;
	DIB *inDIB,*inDIBPad,*inDIBQ;
	vector<RGBQUAD> *colors;
    char *tileptr;
    palreference::PaletteReferenceUtil *paletteRefUtil = new palreference::PaletteReferenceUtil();
    bgcol_pos.x = bgcol_pos.y = 0;
    printf("#%d args",argc);

	if (argc < 3)
	{
		if (argc == 2)
		{
			if ((strcmp(argv[1], "-h")==0) ||
				(strcmp(argv[1], "-help")==0) ||
				(strcmp(argv[1], "-?")==0))
				show_help();
			else
				printf("Usage: sixpack [options] <input>\n");
		}
		else
			printf("Usage: sixpack [options] <input>\n help!");
        return 0;
    }

    MAX_FORMAT_QUANT[FMT_P1] = 2;
    MAX_FORMAT_QUANT[FMT_P2] = 4;
    MAX_FORMAT_QUANT[FMT_P4] = 16;
    MAX_FORMAT_QUANT[FMT_L4] = 16;
    MAX_FORMAT_QUANT[FMT_P8] = 256;
    MAX_FORMAT_QUANT[FMT_L8] = 256;
    MAX_FORMAT_QUANT[FMT_D15] = 0;
    MAX_FORMAT_QUANT[FMT_RLE8] = 256;

    MAX_FORMAT_PLANES[FMT_P1] = 1;
    MAX_FORMAT_PLANES[FMT_P2] = 2;
    MAX_FORMAT_PLANES[FMT_P4] = 4;
    MAX_FORMAT_PLANES[FMT_P8] = 8;
    MAX_FORMAT_PLANES[FMT_L4] = -4;
    MAX_FORMAT_PLANES[FMT_L8] = -8;
    MAX_FORMAT_PLANES[FMT_D15] = -16;
    MAX_FORMAT_PLANES[FMT_RLE8] = -8;

    for (i=1; i<argc;)
    {
        if (argv[i][0] == '-')
        {
			if ((strcmp(argv[i], "-h")==0) ||
				(strcmp(argv[i], "-help")==0) ||
				(strcmp(argv[i], "-?")==0))
			{
				show_help();
				return 0;
			}

            if (strcmp(argv[i], "-pack")==0)
            {
                compmode = 1;
                i++;
            }
            else if (strcmp(argv[i], "-unpack")==0)
            {
                compmode = 2;
                i++;
            }
            else if (strcmp(argv[i], "-v")==0)
            {
                verbose = 1;
                i++;
            }
            else if (strcmp(argv[i], "-o")==0)
            {
                if ((i+1) < argc)
                {
                    outFileName = argv[i + 1];
                    string::size_type dot = outFileName.find_first_of('.');
                    if (dot == string::npos)
                        outFileNameShort = outFileName;
                    else
                        outFileNameShort = outFileName.substr(0, dot);
                }
                else
                {
                    printf("Error: Missing value for -o\n");
                    return 0;
                }
                i += 2;
            }
            else if (strcmp(argv[i], "-preview")==0)
            {
                if ((i+1) < argc)
                {
                    preview = argv[i + 1];
                }
                else
                {
                    printf("Error: Missing value for -preview\n");
                    return 0;
                }
                i += 2;
            }
			else if (strcmp(argv[i], "-format")==0)
			{
				if ((i+1) < argc)
				{
					for (int j=0; j<NUM_FORMATS; j++)
						if (strcmp(argv[i + 1], FORMAT_STRINGS[j])==0)
						{
							format = j;
							format = 1 << format;
							break;
						}
				}
				else
				{
					printf("Error: Missing value for -format.\n");
					return 0;
				}
				i += 2;
			}
			else if (strcmp(argv[i], "-target")==0)
			{
				if ((i+1) < argc)
				{
					int t;
					for (t=0; t<NUM_TARGETS; t++)
						if (strcmp(argv[i + 1], TARGET_STRINGS[t])==0)
							break;
					if (t < NUM_TARGETS)
						target = t + 1;
					else
					{
						printf("Error: Bad value for -target.\n");
						return 0;
                    }
                }
				else
				{
					printf("Error: Missing value for -target.\n");
					return 0;
                }
				i += 2;
            }
            else if (strcmp(argv[i], "-opt")==0)
			{
				optimise = 1;
				i++;
            }
			else if (strcmp(argv[i], "-dither")==0)
			{
				dither = 1;
				i++;
            }
			else if (strcmp(argv[i], "-t")==0)
			{
				transparent = 1;
				i++;
            }
			else if (strcmp(argv[i], "-image")==0)
			{
				in_filetype = FILETYPE_IMAGE;
				i++;
            }
            else if (strcmp(argv[i], "-bat")==0)
            {
                i++;
            }
			else if (strcmp(argv[i], "-chain")==0)
			{
				chainplanes = 1;
				i++;
			}
			else if (strcmp(argv[i], "-code")==0)
			{
				generateCode = 1;
				i++;
			}
			/*else if (strcmp(argv[i], "-lzss")==0)
			{
				codec = CODEC_LZSS;
				i++;
            }*/
			else if (strcmp(argv[i], "-codec")==0)
			{
				if ((i+1) < argc)
				{
					int t;
					for (t=0; t<NUM_CODECS; t++)
						if (strcmp(argv[i + 1], CODEC_STRINGS[t])==0)
							break;
					if (t < NUM_CODECS)
						codec = t + 1;
					else
					{
						printf("Error: Bad value for -codec.\n");
						return 0;
                    }
                }
				else
				{
					printf("Error: Missing value for -codec.\n");
					return 0;
                }
				i += 2;
            }
            else if (strcmp(argv[i], "-autocomp")==0)
            {
                bestopt = 1;
                i++;
            }
			else if (strcmp(argv[i], "-bitmap")==0)
			{
				bitmapMode = 1;
				i++;
			}
			else if (strcmp(argv[i], "-bg")==0)
			{
				if ((i + 1) < argc)
				{
					char *argCpy = new char[strlen(argv[i + 1]) + 1];
					strcpy(argCpy, argv[i + 1]);
					char *commaPtr = strchr(argCpy, ',');
					if (commaPtr)
					{
						*commaPtr = 0;
						bgcol_pos.x = atoi(argCpy);
						bgcol_pos.y = atoi(&commaPtr[1]);
						//printf("bgcol_pos = {%d, %d}\n", bgcol_pos.x, bgcol_pos.y);
					}
					else
					{
						printf("Bad value for -bg.\n");
						return 0;
					}
					delete [] argCpy;
				}
				else
				{
					printf("Missing value for -bg.\n");
					return 0;
				}
				i += 2;
			}
            else if (strcmp(argv[i], "-planes")==0)
            {
                if ((i + 1) < argc)
                    planesWanted = atoi(argv[i + 1]);
                else
                {
                    printf("Missing value for -planes.\n");
                    return 0;
                }
                i += 2;
            }
            else if (strcmp(argv[i], "-threshold")==0)
            {
                if ((i + 1) < argc)
                    threshold = atoi(argv[i + 1]);
                else
                {
                    printf("Missing value for -threshold.\n");
                    return 0;
                }
                i += 2;
            }
            else if ((strcmp(argv[i], "-base")==0) ||
                     (strcmp(argv[i], "-q")==0) ||
                     (strcmp(argv[i], "-width")==0) ||
                     (strcmp(argv[i], "-sizefilter")==0) ||
					 (strcmp(argv[i], "-maptype")==0) ||
                     (strcmp(argv[i], "-height")==0))
            {
                if ((i + 1) < argc){
                    if (strcmp(argv[i], "-base")==0) basecol = atoi(argv[i + 1]);
                    else if (strcmp(argv[i], "-q")==0) quantise = atoi(argv[i + 1]);
                    else if (strcmp(argv[i], "-width")==0) scaleX = atoi(argv[i + 1]);
                    else if (strcmp(argv[i], "-sizefilter")==0) resizeType = (atoi(argv[i + 1])&1)^1;
					else if (strcmp(argv[i], "-maptype")==0) maptype = atoi(argv[i + 1]);
                    else if (strcmp(argv[i], "-height")==0) scaleY = atoi(argv[i + 1]);
                }else
                {
                    printf("Missing value for %s.\n", argv[i]);
                    return 0;
                }
                i += 2;
            }

            else if (strcmp(argv[i], "-dict")==0)
            {
                if ((i + 1) < argc)
                {
                    dictSize = atoi(argv[i + 1]) >> 9;
                    if ((dictSize==1)||(dictSize==2)||(dictSize==4)||(dictSize==8))
                        dictSize <<= 9;
                    else
                    {
                        printf("Warning: Bad value for dict. Using default (4096).\n");
                        dictSize = 4096;
                    }
                }
                else
                {
                    printf("Missing value for -dict.\n");
                    return 0;
                }
                i += 2;
            }
            //new options added by ammianus 2/19/2012
            else if ((strcmp(argv[i], ARG_PALREF)==0))
			{
				paletteRef = 1;
				if ((i + 1) < argc)
				{
					inPaletteFileName = argv[i + 1];
				}else
				{
					printf("Error: Missing input file name for -palref\n");
					return 0;
				}

				i += 2;
			}
			else if ((strcmp(argv[i], ARG_SAVEPAL)==0))
			{
				savePaletteRef = 1;
				if ((i + 1) < argc)
				{
					outPaletteFileName = argv[i + 1];
				}else
				{
					printf("Error: Missing output file name for -savepal\n");
					return 0;
				}
				i += 2;
			}
        }
        else
        {
            if (inFileName.length()==0)
            {
                inFileName = argv[i];
                string::size_type dot = inFileName.find_last_of('.');
                if (dot == string::npos)
                    inFileNameShort = inFileName;
                else
				{
					if (dot > inFileName.find_first_of('.'))
						inFileNameShort = inFileName.substr(0, dot);
					else
						inFileNameShort = inFileName;
				}
                if (outFileNameShort.length()==0)
                    outFileNameShort = inFileNameShort;
                i++;
            }

            else
            {
                printf("Warning: Ignoring argument: %s\n", argv[i]);
                i++;
            }
        }
    }

    printf("\n");

    if ((in_filetype == FILETYPE_IMAGE) && (compmode != COMPMODE_DECODE))
    {
        if (verbose)
		    printf("Loading image file: %s\n", inFileName.data());
	    if (image->load((char*)inFileName.data(), CXI_FORMAT_UNKNOWN) == false)
	    {
		    printf("Error: Failed to load image file: %s\n", inFileName.data());
		    return 0;
        }

        if (format == 0)
            format = TARGET_CAPS[target].defaultFormat;
        if ((TARGET_CAPS[target].formatsAllowed & format)==0)
        {
            printf("Warning: Unsupported format (%d) for this target. Using default.\n", format);
            format = TARGET_CAPS[target].defaultFormat;
        }

		if (target == TARGET_32X)
		{
			bitmapMode = 1;
		}

	    if (((scaleX != 0) && (scaleX != image->getWidth())) ||
            ((scaleY != 0) && (scaleY != image->getHeight())))
        {
		    if (verbose)
				printf("Resizing image to %d*%d pixels.\n", (scaleX?scaleX:image->getWidth()), (scaleY?scaleY:image->getHeight()));
		    image->resize((scaleX?scaleX:image->getWidth()), (scaleY?scaleY:image->getHeight()), resizeType);
        }

	    inDIB = image->getDIB();

		if (verbose && (inDIB->bitCount == 32))
			printf("Warning: Alpha channel is ignored for 32-bit images.\n");

		switch (inDIB->bitCount)
		{
		case 8: case 24: case 32:
			break;
		default:
			printf("Error: Color format not supported: %d bpp.\n", inDIB->bitCount);
			delete image;
			return 0;
		}

		inDIB->palette = image->getPalette();

        if ((quantise < 2) || (quantise > MAX_FORMAT_QUANT[format]))
        {
            printf("Warning: Illegal quantise parameter (%d). Using default (%d).\n", quantise, MAX_FORMAT_QUANT[format]);
            quantise = MAX_FORMAT_QUANT[format];
        }
		if (format != FMT_D15)
		{
			//if option used, read the provided palette reference file into colors vector
			//added by ammianus 2/19/2012
			if(paletteRef == 1){
				colors = paletteRefUtil->readColorReference(inPaletteFileName);
			}else{
				colors = new vector<RGBQUAD>;
			}
			if (verbose)
				printf("Counting colors..\n");
			colors = countDIBColors(inDIB, quantise, colors);
			ncolors = colors->size();
		}

		//TODO export the colors to plain text file after getting them from the image
		//added by ammianus 2/19/2012
		if(savePaletteRef == 1){
			paletteRefUtil->saveColorReference(outPaletteFileName,colors);
		}

        if ((inDIBPad = flipAndPad(inDIB)) == NULL)
        {
			delete image;
			delete inDIB;
			return 0;
        }

  		inDIBPad->palette = inDIB->palette;

		if (planesWanted == -1)
		{
			planesWanted = 0;
		}
		else if (MAX_FORMAT_PLANES[format] < 0)
		{
			printf("Warning: -planes ignored for linear/direct formats.\n");
		}

		if (target == TARGET_DMG)
		{
			inDIBQ = new DIB(inDIBPad->width, inDIBPad->height, 8);
			inDIBQ->palette = (unsigned char*)malloc(768);
			memcpy(inDIBQ->palette, dmgPalette, sizeof(dmgPalette));
			int luma;
			if (inDIBPad->bitCount == 8)
			{
				for (i = 0; i < inDIBQ->height; i++)
				{
					for (int j = 0; j < inDIBQ->width; j++)
					{
						unsigned char c = inDIBPad->bits[i * inDIBPad->width + j];
						luma = (inDIBPad->palette[c * 3 + 2] * 299 +
							    inDIBPad->palette[c * 3 + 1] * 587 +
								inDIBPad->palette[c * 3 + 0] * 114);
						if (luma < 40250)
							inDIBQ->bits[i * inDIBQ->width + j] = 3;
						else if (luma < 126500)
							inDIBQ->bits[i * inDIBQ->width + j] = 2;
						else if (luma < 211000)
							inDIBQ->bits[i * inDIBQ->width + j] = 1;
						else
							inDIBQ->bits[i * inDIBQ->width + j] = 0;
					}
				}
			}
			else if (inDIBPad->bitCount == 24)
			{
				for (i = 0; i < inDIBQ->height; i++)
				{
					for (int j = 0; j < inDIBQ->width; j++)
					{
						luma = (inDIBPad->bits[(i * inDIBQ->width + j) * 3 + 0] * 299 +
							    inDIBPad->bits[(i * inDIBQ->width + j) * 3 + 1] * 587 +
								inDIBPad->bits[(i * inDIBQ->width + j) * 3 + 2] * 114);
						if (luma < 38250)
							inDIBQ->bits[i * inDIBQ->width + j] = 3;
						else if (luma < 127500)
							inDIBQ->bits[i * inDIBQ->width + j] = 2;
						else if (luma < 216000)
							inDIBQ->bits[i * inDIBQ->width + j] = 1;
						else
							inDIBQ->bits[i * inDIBQ->width + j] = 0;
					}
				}
			}

			ncolors = 4;
		}
		else
		{
			if ((inDIBQ = quantiseDIB(inDIBPad, colors, neuquant)) == NULL)
			{
				delete image;
				delete inDIB;
				delete inDIBPad;
				delete neuquant;
				return 0;
			}
		}

		planesNeeded = (int)((log((float)ncolors) / log(2.0f)) + 0.9f);


		if (format != FMT_D15)
		{
			// Lower the palette resolution to the level of the target
			for (i=0; i<256; i++)
			{
				inDIBQ->palette[i * 3 + 0] = (inDIBQ->palette[i * 3 + 0] >> (8-TARGET_CAPS[target].palBits)) << (8-TARGET_CAPS[target].palBits);
				inDIBQ->palette[i * 3 + 1] = (inDIBQ->palette[i * 3 + 1] >> (8-TARGET_CAPS[target].palBits)) << (8-TARGET_CAPS[target].palBits);
				inDIBQ->palette[i * 3 + 2] = (inDIBQ->palette[i * 3 + 2] >> (8-TARGET_CAPS[target].palBits)) << (8-TARGET_CAPS[target].palBits);
			}
		}

		if (MAX_FORMAT_PLANES[format] < 0)
		{
			planesNeeded = -MAX_FORMAT_PLANES[format];
			planesWanted = planesNeeded;
		}
		else
		{
			if ((planesWanted < 0) || (planesWanted > MAX_FORMAT_PLANES[format]))
			{
				planesWanted = MAX_FORMAT_PLANES[format];
				printf("Warning: Bad value for -planes. Using default (%d).\n", planesWanted);
			}

			if (planesWanted && (planesWanted > planesNeeded))
				planesNeeded = planesWanted;

			if (chainplanes)
				planesWanted = planesNeeded = MAX_FORMAT_PLANES[format];
		}

		if (!bitmapMode)
		{
			if (verbose)
				printf("Identifying unique tiles.. ");

			for (int y=0; y<inDIBQ->height/metaheight; y++)
			{
				for (int x=0; x<inDIBQ->width/metawidth; x++)
				{
					for (int v=0; v<metaheight/8; v++)
					{
						for (int u=0; u<metawidth/8; u++)
						{
							int tx = x * (metawidth / 8) + u;
							int ty = y * (metaheight / 8) + v;
							matchTile(inDIBQ, tx, ty);
						}
					}
				}
			}

			if (verbose)
				printf("Found %d unique tiles (of %d).\n", chksums.size(), (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight));

		}


		if ((target == TARGET_SMS) || (target == TARGET_SGG) || (target == TARGET_SNES) || (target == TARGET_TGX) ||
			(target == TARGET_SMD) || (target == TARGET_32X) || (target == TARGET_SAT) || (target == TARGET_DMG))
		{
			if (format == FMT_P1)
			{
				if ((basecol + ncolors) > 2)
				{
					printf("Warning: base + # colors > 2. Using base=0.\n");
					basecol = 0;
				}
			}
			else if (format == FMT_P2)
			{
				if ((basecol + ncolors) > 4)
				{
					printf("Warning: base + # colors > 4. Using base=0.\n");
					basecol = 0;
				}
			}
			else if ((format == FMT_P4) || (format == FMT_L4))
			{
				if ((basecol + ncolors) > 16)
				{
					printf("Warning: base + # colors > 16. Using base=0.\n");
					basecol = 0;
				}
			}
			else if (format == FMT_L8)
			{
				if ((basecol + ncolors) > 256)
				{
					printf("Warning: base + # colors > 256. Using base=0.\n");
					basecol = 0;
				}
			}

			if ((format == FMT_P1) || (format == FMT_P2) || (format == FMT_P4) || (format == FMT_P8))
				printf("Using %d bitplanes (%d bytes) for each tile.\n", planesNeeded, planesNeeded * 8);

			//printf("inDIBQ = % p, inDIBQ->w,h = %d, %d. bpp = %d\n", inDIBQ, inDIBQ->width, inDIBQ->height, inDIB->bitCount);
			//printf("Allocating %d bytes, planesNeeded = %d\n", inDIBQ->width * inDIBQ->height * planesNeeded / 8, planesNeeded);

			if (bitmapMode)
			{
				tileptr = new char[inDIBQ->width * inDIBQ->height * planesNeeded / 8];
			}
			else
			{
				tileptr = new char[coords.size() * planesNeeded * 8 + 4096];
			}
			if (tileptr == NULL)
			{
				printf("Error: Failed to allocate memory.\n");
				delete image;
				delete inDIB;
				delete inDIBQ;
				delete inDIBPad;
				return 0;
			}

		    if (transparent)
			    bgcol = inDIBQ->bits[bgcol_pos.y * inDIBQ->width + bgcol_pos.x];

            // Convert the tiles to the planar format used by the SMS/SGG
            unsigned char *row;
            unsigned char rowCopy[8];

			if (chainplanes)
			{
	            for (i = 0; i < coords.size(); i++)
		        {
			        for (int v = 0; v < 8; v++)
				    {
					    row = &inDIBQ->bits[(coords[i].y * 8 + v) * inDIBQ->width + coords[i].x * 8];
						for (int j = 0; j < 8; j++)
	                    {
		                    int k;
			                for (k = 0; k < palTable.size(); k++)
				                if (palTable[k] == row[j])
					                break;
						    if (k >= palTable.size())
							    palTable.push_back(row[j]);
					        if (transparent)
				            {
			                    if (row[j] == bgcol)
				                    rowCopy[j] = 0;
					            else if (row[j] == 0)
						            rowCopy[j] = bgcol + basecol;
							    else
								    rowCopy[j] = row[j] + basecol;
							}
							else
								rowCopy[j] = row[j] + basecol;
						}

						if (format == FMT_P4)
						{
							for (int b = 0; b < 4; b++)
							{
								tileptr[i * planesNeeded * 8 + v * planesNeeded + b] =
									(rowCopy[b*2]&0x0F) | ((rowCopy[b*2+1]&0x0F)<<4);
							}
						}
						else if (format == FMT_P8)
						{
							for (int b = 0; b < 8; b++)
							{
								tileptr[i * planesNeeded * 8 + v * planesNeeded + b] =
									rowCopy[b];
							}
						}
					}
				}
			}
			else if ((target == TARGET_SMS) || (target == TARGET_SGG))
			{
	            for (i=0; i<coords.size(); i++)
		        {
			        for (int v=0; v<8; v++)
				    {
					    row = &inDIBQ->bits[(coords[i].y * 8 + v) * inDIBQ->width + coords[i].x * 8];
						for (int j=0; j<8; j++)
	                    {
		                    int k;
			                for (k = 0; k < palTable.size(); k++)
				                if (palTable[k] == row[j])
					                break;
						    if (k >= palTable.size())
							    palTable.push_back(row[j]);
					        if (transparent)
				            {
			                    if (row[j] == bgcol)
				                    rowCopy[j] = 0;
					            else if (row[j] == 0)
						            rowCopy[j] = bgcol + basecol;
							    else
								    rowCopy[j] = row[j] + basecol;
							}
							else
								rowCopy[j] = row[j] + basecol;
						}
						char c;
						for (int b = 0; b < planesNeeded; b++)
						{
							c = 0;
							for (int u = 0; u < 8; u++)
								if (rowCopy[u] & (1 << b))
									c += 1 << (7 - u);
							tileptr[i * planesNeeded * 8 + v * planesNeeded + b] = c;
						}
					}
				}
			}
			else if ((target == TARGET_SNES) || (target == TARGET_TGX))
			{
					for (i = 0; i < coords.size(); i++)
					{
						for (int b4 = 0; b4 < planesNeeded/2; b4++)
						{
							for (int v=0; v<8; v++)
							{
								row = &inDIBQ->bits[(coords[i].y * 8 + v) * inDIBQ->width + coords[i].x * 8];
								for (int j = 0; j < 8; j++)
								{
									int k;
									for (k = 0; k < palTable.size(); k++)
										if (palTable[k] == row[j])
											break;
									if (k >= palTable.size())
										palTable.push_back(row[j]);
									if (transparent)
									{
										if (row[j] == bgcol)
											rowCopy[j] = 0;
										else if (row[j] == 0)
											rowCopy[j] = bgcol + basecol;
										else
											rowCopy[j] = row[j] + basecol;
									}
									else
										rowCopy[j] = row[j] + basecol;
								}
								unsigned char c;
								for (int b = 0; (b<2)&&((b4*2+b)<planesNeeded); b++)
								{
									c = 0;
									for (int u = 0; u < 8; u++)
										if (rowCopy[u] & (1 << (b4*2+b)))
											c += 1 << (7 - u);
									tileptr[i * planesNeeded * 8 + b4 * 16 + v * ((planesNeeded<2)?planesNeeded:2) + b] = (char)c;
								}
							}
						}
					}
			}
			else if (target == TARGET_SMD)
			{
	            for (i = 0; i < coords.size(); i++)
		        {
					for (int v = 0; v < 8; v++)
					{
						row = &inDIBQ->bits[(coords[i].y * 8 + v) * inDIBQ->width + coords[i].x * 8];
						unsigned char c;
						for (int j=0; j<8; j++)
						{
							int k;
							for (k = 0; k < palTable.size(); k++)
								if (palTable[k] == row[j])
									break;
							if (k >= palTable.size())
								palTable.push_back(row[j]);
							if (transparent)
							{
								if (row[j] == bgcol)
									rowCopy[j] = 0;
					            else if (row[j] == 0)
						            rowCopy[j] = bgcol + basecol;
							    else
								    rowCopy[j] = row[j] + basecol;
							}
							else
								rowCopy[j] = row[j] + basecol;
						}
						c = 0;
						for (int u = 0; u < 8; u++)
						{
							if (u & 1)
								tileptr[i * 32 + v * 4 + (u >> 1)] = c | rowCopy[u];
							else
								c = rowCopy[u] << 4;
						}
					}
				}
			}
			else if (target == TARGET_32X)
			{
				if (format == FMT_L8)
				{
					for (i = 0; i < inDIBQ->width * inDIBQ->height * planesNeeded / 8; i++)
					{
						tileptr[i] = inDIBQ->bits[i];
					}
				}
				else if (format == FMT_D15)
				{
					for (i = 0; i < inDIBQ->width * inDIBQ->height * planesNeeded / 8; i += 2)
					{
						tileptr[i] = inDIBQ->bits[i + 1];
						tileptr[i + 1] = inDIBQ->bits[i];
					}
				}
			}
			else if (target == TARGET_SAT)
			{
				if (bitmapMode)
				{
					if (format == FMT_L8)
					{
						for (i = 0; i < inDIBQ->width * inDIBQ->height * planesNeeded / 8; i++)
						{
							tileptr[i] = inDIBQ->bits[i];
						}
					}
					else if (format == FMT_D15)
					{
						for (i = 0; i < inDIBQ->width * inDIBQ->height * planesNeeded / 8; i += 2)
						{
							tileptr[i] = inDIBQ->bits[i + 1];
							tileptr[i + 1] = inDIBQ->bits[i];
						}
					}
				}
			}

		    if ((format != FMT_D15) && (palTable.size() > quantise))
		    {
			    printf("Error: Something went wrong.. found %d colors in the quantised image", palTable.size());
			    delete [] tileptr;
			    delete image;
				delete inDIB;
				delete inDIBQ;
				delete inDIBPad;
				return 0;
            }

			string s = outFileNameShort + ".pal";

			if (format != FMT_D15)
			{
			    if (verbose)
			        printf("Writing palette data (%d colors) to %s.pal\n", ncolors, outFileNameShort.data());

				FILE *palfile;
				fopen_s(&palfile, s.data(), "wb");
				unsigned char c;

				for (i = 0; i < ncolors; i++)
				{
					c = i;
					if (transparent)
					{
						if (c == 0)
							c = bgcol;
						else if (c == bgcol)
							c = 0;
					}
					unsigned char *col = &inDIBQ->palette[c * 3];
					//debug print color
					unsigned short int thisColor;
					thisColor = 0x8000 | (col[0] >> 3) | ((col[1] >> 3) << 5) | ((col[2] >> 3) << 10);
					printf("Color %d: 0x%0004x\n",c,thisColor);

					if (target == TARGET_SMS)
					{
					   fputc((col[0] >> 6) | ((col[1] >> 6) << 2) | ((col[2] >> 6) << 4), palfile);
					}
					else if (target == TARGET_SGG)
					{
						fputc((col[0] >> 4) | ((col[1] >> 4) << 4), palfile);
						fputc(col[2] >> 4, palfile);
					}
					else if (target == TARGET_SNES)
					{
						unsigned short int c16;
						c16 = (col[0] >> 3) | ((col[1] >> 3) << 5) | ((col[2] >> 3) << 10);
						fputc(c16 & 0xff, palfile);
						fputc(c16 >> 8, palfile);
					}
					else if (target == TARGET_SMD)
					{
						fputc(col[2] >> 4, palfile);
						fputc((col[0] >> 4) | ((col[1] >> 4) << 4), palfile);
					}
					else if (target == TARGET_TGX)
					{
						unsigned short int c16;
						c16 = (col[0] >> 5) | ((col[2] >> 5) << 3) | ((col[1] >> 5) << 6);
						fputc(c16 & 0xff, palfile);
						fputc(c16 >> 8, palfile);
					}
					else if ((target == TARGET_32X) && (format != FMT_D15))
					{
						unsigned short int c16;
						c16 = 0x8000 | (col[0] >> 3) | ((col[1] >> 3) << 5) | ((col[2] >> 3) << 10);
						fputc(c16 >> 8, palfile);
						fputc(c16 & 0xff, palfile);
					}
				}
				fclose(palfile);
			}

		    if ((chksums.size() <= TARGET_CAPS[target].maxTiles) && (!bitmapMode))
		    {
			    if ((chksums.size() > 448) && ((target == TARGET_SMS) || (target == TARGET_SGG)))
				    printf("Warning: More than 448 unique tiles found. Pattern table and name table will overlap.\n");

				s = outFileNameShort + ".nam";

                if (verbose)
				    printf("Writing name table data to %s\n", s.data());

				FILE *namfile;
			    fopen_s(&namfile, s.data(), "wb");

				if ((target == TARGET_SMS) || (target == TARGET_SGG))
	               for (i=0; i<nameTable.size(); i++)
		            {
			            fputc(nameTable[i] & 0xff, namfile);
				        fputc((nameTable[i] >> 8) | (flipTable[i] * 2), namfile);
					}
				else if (target == TARGET_SNES)
	               for (i=0; i<nameTable.size(); i++)
		            {
			            fputc(nameTable[i] & 0xff, namfile);
				        fputc((nameTable[i] >> 8) | (flipTable[i] * 64), namfile);
					}
				else if (target == TARGET_SMD)
	               for (i=0; i<nameTable.size(); i++)
		            {
				        fputc((nameTable[i] >> 8) | (flipTable[i] * 8), namfile);
			            fputc(nameTable[i] & 0xff, namfile);
					}
				else if (target == TARGET_TGX)
	               for (i=0; i<nameTable.size(); i++)
		            {
			            fputc(nameTable[i] & 0xff, namfile);
				        fputc((nameTable[i] >> 8), namfile);
					}

                fclose(namfile);
            }
            else if (!bitmapMode)
			    printf("Warning: More than %d unique tiles. No name table will be written.\n", TARGET_CAPS[target].maxTiles);


			if (format == FMT_RLE8)
			{
				if (verbose)
				    printf("Warning: Ignoring option -pack for this format.\n");
				compmode = COMPMODE_NONE;
			}

			if ((codec == CODEC_APLIB) && (compmode == COMPMODE_ENCODE))
			{
			    if (outFileName.length() == 0)
				    outFileName = outFileNameShort + ".apx";
			    printf("Encoding pattern data and writing to %s..\n", outFileName.data());

				char *compressedData, *workMem;
				unsigned int compressedSize;

    			int unoptsize, optsize;
				if (bitmapMode)
				{
					optsize = unoptsize = inDIBQ->width * inDIBQ->height * planesNeeded / 8;
				}
				else
				{
					unoptsize = (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * ((MAX_FORMAT_PLANES[format]>0)?MAX_FORMAT_PLANES[format]:-MAX_FORMAT_PLANES[format])*metaheight;
					optsize = chksums.size() * ((MAX_FORMAT_PLANES[format]>0)?MAX_FORMAT_PLANES[format]:-MAX_FORMAT_PLANES[format])*metaheight;
				}

				compressedData = new char[aP_max_packed_size(optsize)];
				workMem = new char[aP_workmem_size(optsize)];
				if ((compressedData == NULL) || (workMem == NULL))
				{
					printf("Error: Failed to allocate memory for apLib");
					delete [] tileptr;
					delete image;
					delete inDIB;
					delete inDIBQ;
					delete inDIBPad;
					return 0;
				}

				compressedSize = aP_pack(tileptr, compressedData, optsize, workMem, NULL, NULL);

				if ((!compressedSize) || (compressedSize > aP_max_packed_size(optsize)))
				{
					printf("Error: Compression failed. Bad return value from aP_pack()");
					delete [] tileptr;
					delete image;
					delete inDIB;
					delete inDIBQ;
					delete inDIBPad;
					return 0;
				}

                // Write the compressed data to the output file
                FILE *outfile;
                fopen_s(&outfile, outFileName.data(), "wb");
                for (i=0; i<compressedSize; i++)
                    fputc(compressedData[i], outfile);
                fclose(outfile);

                delete [] compressedData;
				delete [] workMem;

                printf("Unoptimized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\nCompressed size: %d bytes (%1.0f%%).\n",
                       unoptsize,
                       optsize,
                       ((float)optsize / (float)unoptsize) * 100.0f,
                       compressedSize,
                       ((float)compressedSize / (float)unoptsize) * 100.0f);
			}
		    else if ((codec == CODEC_LZSS) && (compmode == COMPMODE_ENCODE))
		    {
			    if (outFileName.length() == 0)
				    outFileName = outFileNameShort + ".lzs";
			    printf("Encoding pattern data and writing to %s..\n", outFileName.data());
			    CompressionResult result;
			    if (bestopt &&
					((target != TARGET_32X) &&
					 (target != TARGET_SAT) &&
					 (target != TARGET_SNES)))
			    {
                    // Try all codec settings in lzss_params
                    if (verbose)
                        printf("Trying codec settings");
                    for (i=0; i<3; i++)
                    {
                        for (int j=0; j<3; j++)
                        {
                            lzss.configure(LZSS_PARAMS[0][i], LZSS_PARAMS[1][j], 0);
                            result.data = lzss.encode(tileptr, (int)coords.size() * planesNeeded * 8);
                            result.dictSize = LZSS_PARAMS[0][i];
                            result.threshold = LZSS_PARAMS[1][j];
                            lzss.stats(NULL, &result.compressedSize);
                            compRes.push_back(result);
                            if (verbose)
                                printf(".");
                        }
                    }
                    if (verbose)
                        printf("\n");

                    // Find the best settings
                    CompressionResult *bestResult;
                    bestResult = &compRes[0];
                    for (int j=1; j<compRes.size(); j++)
                        if (compRes[j].compressedSize < bestResult->compressedSize)
                            bestResult = &compRes[j];

                    if (verbose)
                        printf("Using LZSS settings -dict %d -threshold %d.\n", bestResult->dictSize, bestResult->threshold);
                    threshold = bestResult->threshold;
                    dictSize = bestResult->dictSize;

                    // Write the most heavily compressed data to the output file
                    FILE *outfile;
                    fopen_s(&outfile, outFileName.data(), "wb");
                    for (i=0; i<bestResult->compressedSize; i++)
                        fputc(bestResult->data[i], outfile);
                    fclose(outfile);

                    // Free all compressed data
                    for (i=0; i<compRes.size(); i++)
                        free(compRes[i].data);

                    int unoptsize = (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * ((MAX_FORMAT_PLANES[format]>0)?MAX_FORMAT_PLANES[format]:-MAX_FORMAT_PLANES[format])*metaheight;
                    lzss.stats(&i, NULL);
                    printf("Unoptimized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\nCompressed size: %d bytes (%1.0f%%).\n",
                           unoptsize,
                           i,
                           ((float)i / (float)unoptsize) * 100.0f,
                           bestResult->compressedSize,
                           ((float)bestResult->compressedSize / (float)unoptsize) * 100.0f);
                }
                else
                {
                    // Use the codec settings specified by the user (or the defaults)

					if (verbose && bestopt)
						printf("Warning: Ignoring option -autocomp for this target");

                    // Compress the tiles
                    char *compressedData;
                    int compressedSize;

					if (target == TARGET_32X)
					{
						dictSize = 4096;
						threshold = 2;
					}

					lzss.configure(dictSize, threshold, 0);
					if (bitmapMode)
					{
	                    compressedData = lzss.encode(tileptr, inDIBQ->width * inDIBQ->height * planesNeeded / 8);
					}
					else
					{
	                    compressedData = lzss.encode(tileptr, coords.size() * planesNeeded * 8);
					}
					lzss.stats(NULL, &compressedSize);

                    // Write the compressed data to the output file
                    FILE *outfile;
                    fopen_s(&outfile, outFileName.data(), "wb");
                    for (i=0; i<compressedSize; i++)
                        fputc(compressedData[i], outfile);
                    fclose(outfile);

                    free(compressedData);

                    int unoptsize;
					if (bitmapMode)
					{
						unoptsize = inDIBQ->width * inDIBQ->height * planesNeeded / 8;
					}
					else
					{
						unoptsize = (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * ((MAX_FORMAT_PLANES[format]>0)?MAX_FORMAT_PLANES[format]:-MAX_FORMAT_PLANES[format])*metaheight;
					}
					lzss.stats(&i, NULL);
                    printf("Unoptimized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\nCompressed size: %d bytes (%1.0f%%).\n",
                           unoptsize,
                           i,
                           ((float)i / (float)unoptsize) * 100.0f,
                           compressedSize,
                           ((float)compressedSize / (float)unoptsize) * 100.0f);
                }
				if (generateCode)
				{
					int dictBits;
					for (dictBits=0; dictBits<32; dictBits++)
					{
						if ((dictSize >> dictBits) == 1) break;
					}
					FILE *asmFile;
					string s = outFileNameShort + TARGET_CAPS[target].asmExt;
					printf("Writing code to %s\n", s.data());
					fopen_s(&asmFile, s.data(), "w");

					char dateStr[64];
					char timeStr[64];

 					time_t rawtime;
  					struct tm * timeinfo;
  					time ( &rawtime );
  					timeinfo = localtime ( &rawtime );

					//_strdate( dateStr);
					//_strtime( timeStr );
					strftime(dateStr, 60, "%Y:%m:%d %H:%M:%s\n\n",timeinfo);
					s = outFileNameShort + ".lzs";
					if (target == TARGET_SMD)
					{
						//fprintf(asmFile, "# Created by Sixpack on %s %s\n\n", dateStr, timeStr);
						fprintf(asmFile, "# Created by Sixpack on ");
						fprintf(asmFile, dateStr);
						fprintf(asmFile, ".EQU LZSS_DICTIONARY_SIZE, %d\n", dictSize);
						fprintf(asmFile, ".EQU LZSS_THRESHOLD, %d\n", threshold);
						fprintf(asmFile, ".EQU LZSS_LEN_BITS, %d\n", 16 - dictBits);
						fprintf(asmFile, ".EQU LZSS_LEN_MASK, 0x%02x\n", (1 << (16 - dictBits)) - 1);
						fprintf(asmFile, ".EQU LZSS_MAX_LEN, %d\n", (1 << (16 - dictBits)) + threshold);
						fprintf(asmFile, "\n.text\n.globl %s_pattern_decode\n\n%s_pattern:\n.incbin \"%s\"\n%s_pattern_end:",
							outFileNameShort.data(), outFileNameShort.data(), s.data(), outFileNameShort.data());
						fprintf(asmFile, "\n\n%s_pattern_decode:\nmove.l #%s_pattern,a2\nmove.l #(%s_pattern_end-%s_pattern),d0\njsr lzss_decode_vram\nrts\n\n.align 1\n.include \"..\\\\..\\\\decoder\\\\smd\\\\lzss_decode.s\"\n",
							outFileNameShort.data(), outFileNameShort.data(), outFileNameShort.data(), outFileNameShort.data());
					}
					else if ((target == TARGET_SMS) || (target == TARGET_SGG))
					{
						fprintf(asmFile, "; Created by Sixpack on ");
						fprintf(asmFile, dateStr);
						//fprintf(asmFile, "; Created by Sixpack on %s %s\n\n", dateStr, timeStr);
						fprintf(asmFile, ".DEFINE LZSS_DICTIONARY_SIZE %d\n", dictSize);
						fprintf(asmFile, ".DEFINE LZSS_THRESHOLD %d\n", threshold);
						fprintf(asmFile, ".DEFINE LZSS_LEN_BITS %d\n", 16 - dictBits);
						fprintf(asmFile, ".DEFINE LZSS_LEN_MASK $%02x\n", (1 << (16 - dictBits)) - 1);
						fprintf(asmFile, ".DEFINE LZSS_MAX_LEN %d\n", (1 << (16 - dictBits)) + threshold);
						fprintf(asmFile, ".DEFINE LZSS_PLANES_USED %d\n", planesNeeded);
						fprintf(asmFile, ".DEFINE LZSS_FORMAT_PLANES %d\n", MAX_FORMAT_PLANES[format]);
						fprintf(asmFile, "\n%s_pattern:\n.incbin \"%s\"\n%s_pattern_end:", outFileNameShort.data(), s.data(),
							outFileNameShort.data());
						fprintf(asmFile, "\n\n%s_pattern_decode:\nld ix,%s_pattern\nld iy,%s_pattern_end-%s_pattern\ncall lzss_decode_vram\nret\n",
							outFileNameShort.data(), outFileNameShort.data(), outFileNameShort.data(), outFileNameShort.data());
					}
					fclose(asmFile);

				}
            }
            else if (compmode == COMPMODE_NONE)
            {
                // Write uncompressed pattern data

                if (outFileName.length() == 0)
                    outFileName = outFileNameShort + ".pat";
                printf("Writing pattern data to %s..\n", outFileName.data());
                FILE *patfile;
                fopen_s(&patfile, outFileName.data(), "wb");
				if (bitmapMode)
				{
					if (format == FMT_RLE8)
					{
						if (target == TARGET_32X)
						{
							unsigned short *lineTable = new unsigned short[inDIBQ->height];
							unsigned short pos = 0;
							unsigned short run, lastColor;
							for (i = 0; i < inDIBQ->height; i++)
							{
								lineTable[i] = pos;
								run = 0;
								lastColor = 0x100;
								for (int x = 0; x < inDIBQ->width; x++)
								{
									if ((unsigned short)inDIBQ->bits[i * inDIBQ->width + x] == lastColor)
									{
										if (256 == run++)
										{
											fputc(255, patfile);
											fputc((unsigned char)lastColor, patfile);
											pos++;
											run = 0;
											lastColor = 0x100;
										}
									}
									else
									{
										if (lastColor != 0x100)
										{
											fputc((unsigned char)run-1, patfile);
											fputc((unsigned char)lastColor, patfile);
											pos++;
										}
										run = 1;
										lastColor = (unsigned short)inDIBQ->bits[i * inDIBQ->width + x];
									}
								}
								if (run)
								{
									fputc((unsigned char)run-1, patfile);
									fputc((unsigned char)lastColor, patfile);
									pos++;
									run = 0;
								}
							}

							s = outFileNameShort + ".ltb";
			                if (verbose)
							    printf("Writing line table data to %s\n", s.data());
							FILE *ltbfile;
							fopen_s(&ltbfile, s.data(), "wb");
							for (i = 0; i < inDIBQ->height; i++)
							{
								fputc(lineTable[i] >> 8, ltbfile);
								fputc(lineTable[i] & 0xff, ltbfile);
							}
							fclose(ltbfile);

							printf("Unoptmized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\n",
								inDIBQ->width * inDIBQ->height,
						        pos * 2,
						       ((float)(pos * 2) / (float)(inDIBQ->width * inDIBQ->height)) * 100.0f);
						}
					}
					else {
						for (i = 0; i < inDIBQ->width * inDIBQ->height * planesNeeded / 8; i++)
							fputc(tileptr[i], patfile);
						fclose(patfile);
					}
					/*printf("Unoptmized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\n",
						   (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * 32,
						   coords.size() * planesNeeded * 8,
						   ((float)(coords.size() * planesNeeded * 8) / (float)((inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * 32)) * 100.0f);
					*/
				}
				else
				{
					for (i = 0; i < coords.size()*planesNeeded*8; i++)
						fputc(tileptr[i], patfile);
					fclose(patfile);
					printf("Unoptmized size: %d bytes.\nOptimized size: %d bytes (%1.0f%%).\n",
						   (inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * 32,
						   coords.size() * planesNeeded * 8,
						   ((float)(coords.size() * planesNeeded * 8) / (float)((inDIBQ->width / metawidth) * (inDIBQ->height / metaheight) * 32)) * 100.0f);
				}
			}

            // Adjust the palette before writing a preview BMP
 		    for (i = 0; i < palTable.size(); i++)
 		        for (int j = 0; j < 3; j++)
 		           if (target == TARGET_SMS)
 		              inDIBQ->palette[i * 3 + j] += (((inDIBQ->palette[i * 3 + j] >> 6) + 1) << 4) - 1;
                   else if (target == TARGET_SGG)
                      inDIBQ->palette[i * 3 + j] += inDIBQ->palette[i * 3 + j] >> 4;
				   else if (target == TARGET_SNES)
					  inDIBQ->palette[i * 3 + j] += inDIBQ->palette[i * 3 + j] >> 5;

            if ((preview.length() > 0) && (format != FMT_D15))
            {
			    if (verbose)
				    printf("Writing preview to %s\n", preview.data());
                inDIBQ->saveBMP((char*)preview.data(), true);
            }

			if (tileptr)
				delete [] tileptr;
		}


        delete image;
		delete inDIB;
		delete inDIBPad;
		delete inDIBQ;
		delete neuquant;

		printf("Done.\n");
    }
    else if (in_filetype == FILETYPE_BINARY)
    {
        if ((codec == CODEC_LZSS) && (compmode == COMPMODE_ENCODE))
        {
		    if (outFileName.length()==0)
		    {
			    outFileName = outFileNameShort + ".lzs";
            }
		    if (verbose)
			    printf("Encoding file %s into %s\n", inFileName.data(), outFileName.data());
		    if (codec = CODEC_LZSS)
		    {
                int bytesIn,bytesOut;
                lzss.configure(dictSize, threshold, 0);
                lzss.encode((char*)inFileName.data(), (char*)outFileName.data());
                lzss.stats(&bytesIn, &bytesOut);
				printf("Input size: %d bytes.\nCompressed size: %d bytes (%1.0f%%).\n",
			           bytesIn,
			           bytesOut,
			           ((float)bytesOut / (float)bytesIn) * 100.0f);
            }
		}

        else if ((codec == CODEC_LZSS) && (compmode == COMPMODE_DECODE))
        {
		    if (outFileName.length()==0)
			    outFileName = outFileNameShort + ".bin";
		    if (verbose)
			    printf( "Decoding file %s into %s\n", inFileName.data(), outFileName.data());
		    if (codec == CODEC_LZSS)
			{
                lzss.configure(dictSize, threshold, 0);
                lzss.decode((char*)inFileName.data(), (char*)outFileName.data());
            }
        }
        else
        {
        }

	    printf("Done.\n");
    }
    else
    {
	    printf("Bad combination of file type and compression mode. Nothing will be done.\n");
    }

    return EXIT_SUCCESS;
}
