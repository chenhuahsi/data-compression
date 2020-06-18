/*
 * The way this works is that 12 bits on input are split into an 8 bit byte that is written
 *  to the output and the leftover 4 bits are saved for next time with a leftover switch
 *  being flipped.
 * The next time we write another 12 bits we check for leftover, write it with 4 bits from
 *  the current input and write the rest, a byte, as well.
 * 
 */

#include <stdio.h>
#include "LZW.h"

#define LZW_ADJ_CODE_LENGTH 1

int leftover;
int leftoverBits;
int writeBinaryLength;
int readBinaryLength;

void writeLeftOver(FILE * output) {
	if (leftover > 0) {
		int buffer = (leftoverBits << (8- leftover));
		fputc(buffer, output);
		writeBinaryLength++;

		leftover = 0;
	}
}

void writeBinary(FILE * output, int code) {
    if (leftover > 0) {
#if defined(LZW_ADJ_CODE_LENGTH) && LZW_ADJ_CODE_LENGTH == 1
		int buffer = (leftoverBits << codeLength) + code;
		leftover = codeLength + leftover - 8;

		int previousCode = buffer >> leftover;
		fputc(previousCode, output);
		writeBinaryLength++;

		if (leftover == 8)
		{
			fputc(buffer & 0xFF, output);
			writeBinaryLength++;

			leftover = 0;
			leftoverBits = 0;
		}
		else
		{
			leftoverBits = buffer & ((1 << leftover) - 1);
		}
#else
        int previousCode = (leftoverBits << 4) + (code >> 8);
        
        fputc(previousCode, output);
        fputc(code, output);
		writeBinaryLength += 2;
      
        leftover = 0; // no leftover now
#endif
    } else {
#if defined(LZW_ADJ_CODE_LENGTH) && LZW_ADJ_CODE_LENGTH == 1
		leftover = codeLength - 8;
		leftoverBits = code & ((1 << leftover) - 1);
		fputc(code >> leftover, output);
#else
        leftoverBits = code & 0xF; // save leftover, the last 00001111
        leftover = 1;
        
        fputc(code >> 4, output);
#endif
		writeBinaryLength++;
    }
}

int readBinary(FILE * input) {
    int code = fgetc(input);    
	readBinaryLength++;

	if (code == EOF)
	{
		code = 0;
		//return 0;
	}

    if (leftover > 0) {
#if defined(LZW_ADJ_CODE_LENGTH) && LZW_ADJ_CODE_LENGTH == 1
		int buffer = (leftoverBits << 8) + code;
		leftover = leftover + 8 - codeLength;

		code = (buffer >> leftover );
		leftoverBits = buffer & ((1 << leftover) - 1);
#else
        code = (leftoverBits << 8) + code;
        
        leftover = 0;
#endif
    } else {
#if defined(LZW_ADJ_CODE_LENGTH) && LZW_ADJ_CODE_LENGTH == 1
		int nextCode = fgetc(input);
		readBinaryLength++;

		int buffer = (code << 8) + nextCode;
		leftover = 8 + 8 - codeLength;

		leftoverBits = buffer & ((1 << leftover) - 1);

		code = (buffer >> leftover);
#else
        int nextCode = fgetc(input);
		readBinaryLength++;
        
        leftoverBits = nextCode & 0xF; // save leftover, the last 00001111
        leftover = 1;
        
        code = (code << 4) + (nextCode >> 4);
#endif
    }
    return code;
}

int decompressed_data_frame_this_idx;

void fputc_lzw(int data, FILE *outputFile, char * decompressPtr) {
	fputc(data, outputFile);
	fflush(outputFile);
	//printf("%x", data & 0xFF);

	decompressPtr[decompressed_data_frame_this_idx++] = data & 0xFF;
}
