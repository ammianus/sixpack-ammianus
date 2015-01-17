/*
 * palreference.h
 *
 *  Created on: Feb 19, 2012
 *      Author: ammianus
 */

#ifndef PALREFERENCE_H_
#define PALREFERENCE_H_

#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <iterator>
#include <windows.h>

/*
 * Created by ammianus
 *
 * Class containing methods for reading a list of colors from a file and writing
 * a list of RGB colors to file in the same format.
 *
 * Allows you to specify a single consistent palette when working with multiple images
 *
 */

namespace palreference {

class PaletteReferenceUtil
{

public:

	/*
	 *
	 */
	std::vector<RGBQUAD>* readColorReference(std::string filename);

	/*
	 *
	 */
	void saveColorReference(std::string filename, std::vector<RGBQUAD> *colorVector);


private:

};

}

#endif /* PALREFERENCE_H_ */
