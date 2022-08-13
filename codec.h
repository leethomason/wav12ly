
#ifndef codec_h
#define codec_h

//#include "Types.h"
#include <stdint.h>
typedef int16_t s16;
typedef uint8_t u8;

struct CodecState
{
	int valprev;
	int index;

	void init() { valprev = 0; index = 0; }
};

void encodeADPCM(CodecState* state, const s16* input, int numSamples, u8* output);
void decodeADPCM(CodecState* state, const u8* input, int numSamples, s16* output);


#endif
