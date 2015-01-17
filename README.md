# sixpack-ammianus
Image processing utility for 32X homebrew development. Modified version of sixpack utility developed by mic_

Based on source code of sixpack
http://jiggawatt.org/badc0de/sixpack/

Modified sixpack source to export a file containing all of the colors it finds in the image. I also added a flag so that it can read the file in that format and then sixpack won't duplicate colors but refer to the same set.

From this I was able to pass each of my images through sixpack, and build a single common palette containing all the colors used by all the images that I wanted to display. 
New arguments added:
-palref <filename>
-savepal <filename>

palref reads the colors from the provided file, in that order(can be produced using -savepal argument).

savepal writes an output file with all the colors found in the image by sixpack, plus any from the palette reference file if provided.

The idea is that you could use the same palette file for multiple images and the end .pal file would be the one you actually use in your program. This would solve the duplication issue, as well as ensure the colors referred to in your images would always be in the same place in your palette. 
