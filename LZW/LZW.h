#ifndef __LZW_h
#define __LZW_h

#define CONFIG_LZW 1

#include "ifp_data_compress.h"

enum {
	dictionarySize = 1023, //4095, // maximum number of entries defined for the dictionary (2^12 = 4096)
	codeLength = 10, //12, // the codes which are taking place of the substrings
	maxValue = dictionarySize - 1
};


void dictionaryInit();
void appendNode(struct DictNode *node);
void dictionaryDestroy();
int dictionaryLookup(int prefix, int character);
int dictionaryPrefix(int value);
int dictionaryCharacter(int value);
void dictionaryAdd(int prefix, int character, int value);

int compress_LZW(uint16 *inputData, FILE *outputFile, int length);
void decompress_LZW(FILE * inputFile, FILE * outputFile, char* decompressPtr, int length);

inline char readByte(char * inPtr) {
	return(*inPtr);
};

inline void writeByte(int data, char * outPtr) {
	*outPtr = data & 0xFF;
};

void fputc_lzw(int data, FILE *outputFile, char* decompressPtr);

#endif