#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "dictionary.c"
//#include "file.c" // binary file write & read
//#include "list.c"
//#include "array.c" // a faster array for decompression

#include "ifp_data_compress.h"
#include "LZW.h"

#if defined(CONFIG_LZW) && CONFIG_LZW == 1
extern writeBinaryLength;
extern int decompressed_data_frame_this_idx;
extern int readBinaryLength;

// compression
int compress_LZW(uint16 *inputData, FILE *outputFile, int length) {
    
	char* in_idx = (char*) inputData;

	unsigned int prefix = (unsigned int) (*in_idx++) & 0xFF;

	unsigned int character;

    int nextCode;
    int index;
    
    // LZW starts out with a dictionary of 256 characters (in the case of 8 codeLength) and uses those as the "standard"
    //  character set.
    nextCode = 256; // next code is the next available string code
    dictionaryInit();
	writeBinaryLength = 0;
    
	int loop = 0;
    // while (there is still data to be read)
    while (loop++ < length) { 
        
		unsigned int character = (unsigned int)(*in_idx++) & 0xFF; // ch = read a character;

        // if (dictionary contains prefix+character)
        if ((index = dictionaryLookup(prefix, character)) != -1) prefix = index; // prefix = prefix+character
        else { // ...no, try to add it
            // encode s to output file
            writeBinary(outputFile, prefix);
            
            // add prefix+character to dictionary
            if (nextCode < dictionarySize) dictionaryAdd(prefix, character, nextCode++);
            
            // prefix = character
            prefix = character; //... output the last string after adding the new one
        }
    }
    // encode s to output file
    writeBinary(outputFile, prefix); // output the last code
	writeLeftOver(outputFile);

    //if (leftover > 0) fputc(leftoverBits << 4, outputFile);
    
    // free the dictionary here
    dictionaryDestroy();

	return writeBinaryLength;
}

// decompression
// to reconstruct a string from an index we need to traverse the dictionary strings backwards, following each
//   successive prefix index until this prefix index is the empty index

void decompress_LZW(FILE * inputFile, FILE * outputFile, char * decompressPtr, int length) {
    // int prevcode, currcode
    int previousCode; int currentCode;
    int nextCode = 256; // start with the same dictionary of 256 characters
	readBinaryLength = 0;

    int firstChar;
    
	decompressed_data_frame_this_idx = 0;

    // prevcode = read in a code
    previousCode = readBinary(inputFile);
	if (previousCode == 0) {
		return;
	}

    fputc_lzw(previousCode, outputFile, decompressPtr);

	//printf("%c", previousCode);
    
    // while (there is still data to read)
    //while ((currentCode = readBinary(inputFile)) > 0) { // currcode = read in a code
	while (readBinaryLength <= length ) { // currcode = read in a code
		currentCode = readBinary(inputFile);
        if (currentCode >= nextCode) {
			firstChar = decode(previousCode, outputFile, decompressPtr);
			fputc_lzw(firstChar, outputFile, decompressPtr); // S+C+S+C+S exception [2.]
            //printf("%c", firstChar);
            //appendCharacter(firstChar = decode(previousCode, outputFile));
		}
		else
		{
			firstChar = decode(currentCode, outputFile, decompressPtr); // first character returned! [1.]
		}
        
        // add a new code to the string table
		if (nextCode < dictionarySize)
		{
			dictionaryArrayAdd(previousCode, firstChar, nextCode++);
			//printf("code=%d, pre=%d, char=%d\n", nextCode, previousCode, firstChar);
		}
        
        // prevcode = currcode
        previousCode = currentCode;
    }
    //printf("\n");
}

int decode(int code, FILE * outputFile, char* decompressPtr) {
    int character; int temp;

    if (code > 255) { // decode
        character = dictionaryArrayCharacter(code);
        temp = decode(dictionaryArrayPrefix(code), outputFile, decompressPtr); // recursion
    } else {
        character = code; // ASCII
        temp = code;
    }

	fputc_lzw(character, outputFile, decompressPtr);
    //printf("%c", character);
    //appendCharacter(character);
    return temp;
}

#endif
