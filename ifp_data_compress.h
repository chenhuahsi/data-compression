
#ifndef __ifp_data_compress_h
#define __ifp_data_compress_h

#define CONFIG_LZW 1
#define SIGN_EXT 1


#if defined(CONFIG_LZW) || CONFIG_LZW == 0

#include <stdio.h>

typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned char uint8;
typedef char int8;

#define MAX_TX 34
#define MAX_RX 15
#define SMALL_BITS (4)

enum {
	COMPRESS_TYPE_LOSSLESS_SINGLE_FRAME_TWO_DATA_LENGTH = 1,
	COMPRESS_TYPE_LOSSLESS_TWO_FRAMES_TWO_DATA_LENGTH = 2,
	COMPRESS_TYPE_LOSS_SINGLE_FRAME_ONE_DATA_LENGTH = 3,
};

enum
{
	DATA_TYPE_RAW = 1,
	DATA_TYPE_DLETA_BEFORE_ANTI_BENDING,
	DATA_TYPE_DELTA_AFTER_ANTI_BENDING,
};

typedef struct __MetaData {
	uint16 binaryStreamTotalLength : 12;
	uint16 longbits : 4;
	uint16 pixelCount : 12;
	uint16 smallbits : 4;
	uint8 compressType : 4;
	uint8 Counter : 4;
	uint8 headerLength;
	int16 pixel0;
	int8 dataType;
	uint16 crc16;
} MetaData;

typedef struct __BITMASK_INFO {
	int bits;
	int bitmask;
}BITMASK_INFO;


void memcpy16_local(uint16 *dst, uint16* src, uint16 len);
void memset16_local(uint16 *p, uint16 c, uint16 len);

int compress_data_one_frame(uint16 *rawImage_this_frame, void *compressedBinary, int datatype);
int compress_data_two_continuous_frames(uint16 *rawImage_last_frame, uint16 *rawImage_this_frame, void *compressedBinary, int datatype);
int get_decompress_data(void *compressedBinary, int length, int16* decompressedData, int16* lastFrameData);

unsigned short CRC16_CCITT(unsigned char *puchMsg, unsigned int usDataLen);

#endif

#endif
