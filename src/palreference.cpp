/*
 * palreference.cpp
 *
 *  Created on: Feb 19, 2012
 *      Author: ammianus
 */
#include "palreference.h"

/*
 * definition for PaletteReferenceUtil class
 */

namespace palreference {

	/*
	 * Reads the file, containing structured data in format of:
	 * R<tab>G<tab>tB<tab>reserved<newline>
	 */
	std::vector<RGBQUAD>* PaletteReferenceUtil::readColorReference(std::string filename)
	{
		RGBQUAD color;
		uint8_t r, g, b, res;

		//const char * charName = filename.c_str();
		std::vector<RGBQUAD> *colorList = new std::vector<RGBQUAD>;
		FILE *inFile;
		//read input file
		inFile = fopen(filename.c_str(), "r");
		if(inFile != NULL)
		{
			//color = new RGBQUAD;
			r = 0;
			g = 0;
			b = 0;
			res = 0;
			while(!feof(inFile)){
				//read one-line of tab delimited r, g, b values, last tab for 'reserved'
				fscanf(inFile,"%u\t%u\t%u\t%u\n",&r,&g,&b,&res);
				color.rgbRed = (BYTE) r;
				color.rgbGreen = (BYTE) g;
				color.rgbBlue = (BYTE) b;
				color.rgbReserved = (BYTE) res;
				colorList->push_back(color);
			}

			fclose(inFile);
		}else{
			printf("Could not read from palette reference file %s\n",filename.c_str());
		}

		return colorList;
	}

	/*
	 * Writes the vector of RGB colors to the provided filename
	 */
	void PaletteReferenceUtil::saveColorReference(std::string filename, std::vector<RGBQUAD> *colorVector)
	{
		//RGBQUAD color;
		FILE *outFile;
		std::vector<RGBQUAD>::const_iterator cIter;
		//write colors to output file
		outFile = fopen(filename.c_str(), "w");
		if(outFile != NULL)
		{
			//iterate over color vector
			for(cIter = colorVector->begin(); cIter != colorVector->end(); cIter++)
			{
				//print the RGB info to the outpuf file
				fprintf(outFile,"%u\t%u\t%u\t%u\n",
						(uint8_t)cIter->rgbRed,
						(uint8_t)cIter->rgbGreen,
						(uint8_t)cIter->rgbBlue,
						(uint8_t)cIter->rgbReserved);
			}

			fclose(outFile);
		}else{
			printf("Could not write to new palette file %s\n",filename.c_str());
		}
		return;
	}

}
