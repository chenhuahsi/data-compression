#if !defined(CONFIG_LZW) || CONFIG_LZW == 0

#include "ifp_data_compress.h"

BITMASK_INFO m_bitmask_Infos[] = {
    {0, 0x00},
    {1, 0x00},
    {2, 0x01},
    {3, 0x03},
    {4, 0x07},
    {5, 0x0F},
    {6, 0x1F},
    {7, 0x3F},
    {8, 0x7F},
    {9, 0xFF},
    {10, 0x01FF},
    {11, 0x03FF},
    {12, 0x07FF},
    {13, 0x0FFF},
    {14, 0x1FFF},
    {15, 0x3FFF},
};

static int16 mDiffArray[(MAX_TX + 2)*(MAX_RX + 1) + 1] = {0};
static int16 mFrameCounter = 0;

void memset16_local(uint16 *p, uint16 c, uint16 len)
{
	uint16 i;
	for (i = 0; i < len; i++)
	{
		*p++ = c;
	}
}

void memcpy16_local(uint16 *dst, uint16* src, uint16 len)
{
	uint16 i;
	for (i = 0; i < len; i++)
	{
		dst[i] = src[i];
	}
}

void get_diff_of_neighbour_in_one_frame(uint16 *input_this_frame, int16 *diff,
               uint16 rxCount, uint16 txCount, int16* max, int16* min, int inputDataType)
{
    uint16 rowSkip = MAX_RX - rxCount;
    uint16 i = txCount;
    int16 lastValue = 0;
    int16 diffDataIndex = (MAX_RX + 1) + 1;  // account for border
    int16 dataValue = 0;
    int16 inputDataIndex = ( inputDataType == DATA_TYPE_RAW ) ? 0 : ((MAX_RX + 1) + 1);

    *max = 0;
    *min = 0;

    lastValue = input_this_frame[inputDataIndex];

    do
    {
        uint16 j = rxCount;
        do
        {
            dataValue = input_this_frame[inputDataIndex] - lastValue;
            diff[diffDataIndex] = dataValue;

            if (dataValue > *max) {
                *max = dataValue;
            }
            if (dataValue < *min) {
                *min = dataValue;
            }

            lastValue = input_this_frame[inputDataIndex];
            inputDataIndex++;
            diffDataIndex++;
        } while (--j);

        inputDataIndex += ( inputDataType == DATA_TYPE_RAW ) ? rowSkip : (rowSkip + 1);
        diffDataIndex += rowSkip + 1;
    } while (--i);
}

void get_diff_of_two_continuous_frames(uint16 *input_last_frame, uint16 *input_this_frame, int16 *diff,
                  uint16 rxCount, uint16 txCount, int16* max, int16* min, int inputDataType)
{
    uint16 rowSkip = MAX_RX - rxCount;
    uint16 i = txCount;
    int16 dataIndex = (MAX_RX + 1) + 1;  // account for border
    int16 dataValue = 0;
    int16 inputDataIndex = ( inputDataType == DATA_TYPE_RAW ) ? 0 : ((MAX_RX + 1) + 1);

    *max = 0;
    *min = 0;
    do
    {
        uint16 j = rxCount;
        do
        {
            dataValue = input_this_frame[inputDataIndex] - input_last_frame[inputDataIndex];
            diff[dataIndex] = dataValue;

            if (dataValue > *max) {
                *max = dataValue;
            }
            if (dataValue < *min) {
                *min = dataValue;
            }

            inputDataIndex++;
            dataIndex++;
        } while (--j);

        inputDataIndex += ( inputDataType == DATA_TYPE_RAW ) ? rowSkip : (rowSkip + 1);
        dataIndex += rowSkip + 1;
    } while (--i);
}

static int16 mymod(int16 x, int16 y)
{
    while (x >= y)
    {
        x -= y; //x=x-y
    }
    return x;
}

static int16 myabs(int16 a) {
    return a >= 0 ? a : -a;
}

int determine_longbits(int16 max, int16 min) {
    int16 value = 0;
    int maxindex = sizeof(m_bitmask_Infos) / sizeof(BITMASK_INFO);
    int i = 0;

#if defined(SIGN_EXT) && (SIGN_EXT==1)
	max = (max > 0) ? max : (myabs(max) - 1);
	min = (min > 0) ? min : (myabs(min) - 1);
#else
    max = myabs(max);
    min = myabs(min);
#endif

    value = max > min ? max : min;

    for (i = maxindex - 1; i >= 0; i--) {
        if (value & (1 << (i - 1))) {
            break;
        }
    }

    return i + 1; // 1 is for the bit of sign
}

//
//  (info area) + (header area) + (variable length int16 area)
//
//
int compress_data(int16* data_to_be_compressed, int bitmask_index, void *compressedBinary,
                  int compressType, int dataType, int pixel0) {
    int headerLength = 0;
    uint8* header = NULL;
    uint16* compressedData = NULL;
    MetaData* metaData = NULL;
    int totalLength = 0;
    int headerStartIndex = sizeof(MetaData);
    int longbits = 0;
    int longmask = 0;
    int smallbits = m_bitmask_Infos[SMALL_BITS].bits;
    int smallmask = m_bitmask_Infos[SMALL_BITS].bitmask;
    int small_threshold = (1 << (smallbits - 1));
    uint16 rowSkip = MAX_RX - MAX_RX;
    uint16 i = MAX_TX;
    int deltaImageIndex = 0;
    int pixelIndex = 0;
    int compressDataIndex = 0;
    uint16 currentActiveInt16Data = 0;
    char remaingBits = 16;
    int bits = 0;
    int bitmask = 0;

    longbits = m_bitmask_Infos[bitmask_index].bits;
    longmask = m_bitmask_Infos[bitmask_index].bitmask;

    headerLength = (MAX_RX * MAX_TX + 7) / 8;

    header = (uint8*)&((uint8*)compressedBinary)[headerStartIndex];
    compressedData = (uint16*)&((uint8*)compressedBinary)[sizeof(MetaData) + headerLength];
    metaData = (MetaData*)compressedBinary;

    metaData->compressType = compressType;
    metaData->Counter = mFrameCounter;
    metaData->headerLength = headerLength;
    metaData->pixelCount = MAX_RX * MAX_TX;
    metaData->longbits = longbits;
    metaData->smallbits = smallbits;
    metaData->pixel0 = pixel0;
    metaData->dataType = dataType;

    rowSkip = MAX_RX - MAX_RX;
    i = MAX_TX;
    remaingBits = 16;
    deltaImageIndex = (MAX_RX + 1) + 1;  // account for border

    do
    {
        uint16 j = MAX_RX;
        do
        {
            int16 deltaValue = data_to_be_compressed[deltaImageIndex];
            uint16 data = 0;

            int headerByteIndex = 0;
            int bitIndexInheaderByte = 0;

            headerByteIndex = pixelIndex / 8;
            bitIndexInheaderByte = mymod(pixelIndex, 8);

#if defined(SIGN_EXT) && (SIGN_EXT == 1)
			int16 check_value = (deltaValue < 0) ? -deltaValue - 1 : deltaValue;
			if (check_value >= small_threshold) {
#else
            if (myabs(deltaValue) >= small_threshold) {
#endif
                header[headerByteIndex] |= 1 << bitIndexInheaderByte;

                bits = longbits;
                bitmask = longmask;
            }
            else {
                header[headerByteIndex] &= ~(1 << bitIndexInheaderByte);

                bits = smallbits;
                bitmask = smallmask;
            }

#if defined(SIGN_EXT) && (SIGN_EXT == 1)
			data = deltaValue & ((1 << bits) - 1);
#else
            data = myabs(deltaValue) & bitmask;

            if (deltaValue < 0) {
                data |= 1 << (bits - 1);
            }
#endif

            if (remaingBits >= bits) {
                data = data << (16 - remaingBits);
                currentActiveInt16Data += data;
                remaingBits -= bits;

                if (remaingBits == 0) {
                    compressedData[compressDataIndex] = currentActiveInt16Data;
                    currentActiveInt16Data = 0;
                    compressDataIndex++;
                    remaingBits = 16;
                }
                else {
                    // maybe this is the last one, so we should save currentActive data firstly
                    // it will be updated if necessary in next iteration.
                    // Without this statement here, the last pixel will be lost!!!
                    compressedData[compressDataIndex] = currentActiveInt16Data;
                }
            }
            else {
                uint16 data_for_this = (uint16)(data << (16 - remaingBits));
                uint16 data_for_next = data >> remaingBits;

                currentActiveInt16Data += data_for_this;
                compressedData[compressDataIndex] = currentActiveInt16Data;
                compressDataIndex++;

                currentActiveInt16Data = data_for_next;
                remaingBits = 16 - (bits - remaingBits);

                // maybe this is the last one, so we should save currentActive data firstly
                // it will be updated if necessary in next iteration
                // Without this statement here, the last pixel will be lost!!!
                compressedData[compressDataIndex] = currentActiveInt16Data;
            }

            deltaImageIndex++;
            pixelIndex++;
        } while (--j);

        deltaImageIndex += rowSkip + 1;
    } while (--i);

    if (compressDataIndex > 0 && remaingBits == 16) {
        compressDataIndex--; // remove the last whole un-used uint16
    }

    totalLength = sizeof(MetaData) + headerLength + compressDataIndex * sizeof(uint16);
    metaData->binaryStreamTotalLength = totalLength;

    mFrameCounter++;

    return totalLength;
}

int compress_data_one_frame(uint16 *input_this_frame, void *compressedBinary, int datatype) {

    int16 maxValue = 0;
    int16 minValue = 0;
    int bitmask_index = 0;
    int totalLength = 0;

	memset16_local(mDiffArray, 0, sizeof(mDiffArray) / sizeof(int16));

    get_diff_of_neighbour_in_one_frame(input_this_frame, mDiffArray, MAX_RX, MAX_TX, &maxValue, &minValue, datatype);

    bitmask_index = determine_longbits(maxValue, minValue);

    totalLength = compress_data(mDiffArray, bitmask_index, compressedBinary, COMPRESS_TYPE_LOSSLESS_SINGLE_FRAME_TWO_DATA_LENGTH, datatype, input_this_frame[0]);

    return totalLength;
}

int compress_data_two_continuous_frames(uint16 *input_last_frame, uint16 *input_this_frame, void *compressedBinary, int datatype) {

    int16 maxValue = 0;
    int16 minValue = 0;
    int bitmask_index = 0;
    int pixel0 = 0;
    int totalLength = 0;

	memset16_local(mDiffArray, 0, sizeof(mDiffArray) / sizeof(int16));

    get_diff_of_two_continuous_frames(input_last_frame, input_this_frame, mDiffArray, MAX_RX, MAX_TX, &maxValue, &minValue, datatype);

    bitmask_index = determine_longbits(maxValue, minValue);

    pixel0 = mDiffArray[(MAX_RX + 1) + 1];

    totalLength = compress_data(mDiffArray, bitmask_index, compressedBinary, COMPRESS_TYPE_LOSSLESS_TWO_FRAMES_TWO_DATA_LENGTH, datatype, pixel0);

    return totalLength;
}

int decompress_binary_data(void *compressedBinary, int length, int16* decompressedData) {

    int headerLength = 0;
    uint8* header = NULL;
    uint16* compressedData = NULL;
    MetaData* metaData = NULL;
    int totalLength = 0;
    int headerStartIndex = sizeof(MetaData);
    int longbits = 0;
    int longmask = 0;
    int smallbits = m_bitmask_Infos[SMALL_BITS].bits;
    int smallmask = m_bitmask_Infos[SMALL_BITS].bitmask;
    int pixel0 = 0;
    int pixelCount = 0;
    int pixelIndex = 0;
    int compressType = 0;
    int dataType;
    int counter;

    int compressDataIndex = 0;
    uint16 currentActiveInt16Data = 0;
    char remainingBits = 16;

    metaData = (MetaData*)compressedBinary;

    totalLength = metaData->binaryStreamTotalLength;
    headerLength = metaData->headerLength;
    longbits = metaData->longbits;
    longmask = m_bitmask_Infos[longbits].bitmask;
    smallbits = metaData->smallbits;
    smallmask = m_bitmask_Infos[smallbits].bitmask;
    pixel0 = metaData->pixel0;
    pixelCount = metaData->pixelCount;
    compressType = metaData->compressType;
    dataType = metaData->dataType;
    counter = metaData->Counter;

    if (length < totalLength) {
        // incorrect data
        return -1;
    }

    header = (uint8*)&((uint8*)compressedBinary)[headerStartIndex];
    compressedData = (uint16*)&((uint8*)compressedBinary)[sizeof(MetaData) + headerLength];

    for (pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
        // get the header byte and bitindex in head byte
        int headerByteIndex = 0;
        int bitIndexInheaderByte = 0;
        int isLong = 0;
        int16 lowPart = 0;
        int16 highPart = 0;
        int bits = 0;
        int16 sum = 0;
        int sign = 0;

        headerByteIndex = pixelIndex / 8;
        bitIndexInheaderByte = mymod(pixelIndex, 8);

        isLong = header[headerByteIndex] & (1 << bitIndexInheaderByte);
        if (isLong) {
            bits = longbits;
        }
        else {
            bits = smallbits;
        }

        if (remainingBits >= bits) {
            // the whole data can be got from compressedData[compressDataIndex]
            uint16 value = compressedData[compressDataIndex];
            uint16 mask = (1 << bits) - 1;
            value = value >> (16 - remainingBits);

            lowPart = value & mask;
            highPart = 0;
            remainingBits -= bits;
            if (remainingBits == 0) {
                compressDataIndex++;
                remainingBits = 16;
            }
        }
        else {
            // the whole data should be got from compressedData[compressDataIndex] and compressedData[compressDataIndex+1]
            uint16 value = compressedData[compressDataIndex];
            uint16 mask = 0;

            lowPart = (int16)(value >> (16 - remainingBits));
            compressDataIndex++;

            value = compressedData[compressDataIndex];
            mask = (1 << (bits - remainingBits)) - 1;
            highPart = value & mask;
            highPart = highPart << remainingBits;
            remainingBits = 16 - (bits - remainingBits);
        }

        sum = highPart + lowPart;

#if defined(SIGN_EXT) && (SIGN_EXT==1)
		decompressedData[pixelIndex] = sum;

		int16 sign_bit = (1 << (bits - 1));
		if (sum >= sign_bit)
		{
			decompressedData[pixelIndex] -= sign_bit * 2;
		}
#else
        decompressedData[pixelIndex] = sum & m_bitmask_Infos[bits].bitmask;

        sign = sum & (1 << (bits - 1));
        if (sign != 0) {
            decompressedData[pixelIndex] *= -1;
        }
#endif
    }

    return 0;
}

int decompress_one_frame_data_by_itself(int16 pixel0, int pixelCount, int16* decompressedData) {

    int index = 0;

    decompressedData[0] = pixel0;
    for (index = 1; index < pixelCount; index++) {
        decompressedData[index] = decompressedData[index - 1] + decompressedData[index];
    }

    return 0;
}

int decompress_one_frame_data_by_this_and_last_frames(int16* decompressedData, int16* lastFrame, int pixelCount) {

    int index = 0;

    for (index = 0; index < pixelCount; index++) {
        decompressedData[index] += lastFrame[index];
    }

    return 0;
}

int get_decompress_data(void *compressedBinary, int length, int16* decompressedData, int16* lastFrameData) {

    int result = decompress_binary_data(compressedBinary, length, decompressedData);
    MetaData* metaData = NULL;
    int totalLength = 0;
    int pixel0 = 0;
    int pixelCount = 0;
    int pixelIndex = 0;
    int compressType = 0;
    int dataType;

    if (result) {
        return result;
    }

    metaData = (MetaData*)compressedBinary;

    totalLength = metaData->binaryStreamTotalLength;
    pixel0 = metaData->pixel0;
    pixelCount = metaData->pixelCount;
    compressType = metaData->compressType;
    dataType = metaData->dataType;

    if (compressType == COMPRESS_TYPE_LOSSLESS_SINGLE_FRAME_TWO_DATA_LENGTH) {

        decompress_one_frame_data_by_itself(pixel0, pixelCount, decompressedData);

        return 0;
    }
    else if (compressType == COMPRESS_TYPE_LOSSLESS_TWO_FRAMES_TWO_DATA_LENGTH) {

        decompress_one_frame_data_by_this_and_last_frames(decompressedData, lastFrameData, pixelCount);

        return 0;
    }
    else if (compressType == COMPRESS_TYPE_LOSS_SINGLE_FRAME_ONE_DATA_LENGTH){
        return -1;
    }
    else {
        return -1;
    }
}

#endif
