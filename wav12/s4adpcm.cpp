/*
  Copyright (c) Lee Thomason, Grinning Lizard Software

  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is furnished to do
  so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "s4adpcm.h"

const int S4ADPCM::DELTA_TABLE_4[N_TABLES][TABLE_SIZE] = {
    {-1, 0, 0, 0, 0, 1, 1, 2, 2},
    {-1, 0, 0, 1, 1, 1, 2, 3, 3},
    {-1, 0, 0, 1, 1, 2, 3, 4, 4},
    {-1, -1, 0, 1, 2, 3, 4, 5, 5}};

// -8 + 7
const int S4ADPCM::STEP[16] = {
    // -8
    //-24, -16, -14, -10, -8, -4, -2, -1,
    -8, -7, -6, -5, -4, -3, -2, -1,
    // 0..7
    //0, 1, 2, 4, 8, 12, 16, 24
    0, 1, 2, 3, 4, 5, 6, 7
};


int S4ADPCM::encode4(const int16_t* data, int32_t nSamples, uint8_t* target, State* state, const int* table, int32_t* err)
{
    W12ASSERT(STEP[ZERO_INDEX] == 0);
        
    int64_t err12Squared = 0;
    const uint8_t* start = target;
    for (int i = 0; i < nSamples; ++i) {
        int32_t value = data[i];
        int32_t guess = state->guess();

        // Bias up so we round up and down equally.
        int32_t mult = 1 << state->shift;

        // Search for minimum error.
        int bestE = 100000;
        uint8_t index = 8;  // zero index
        for (int j = 0; j < 16; ++j) {
            int32_t s = guess + mult * STEP[j];
            int32_t e = abs(s - value);

            if (e < bestE) {
                bestE = e;
                index = j;
                if (e == 0) break;
            }
        }
        if (state->high)
            *target++ |= index << 4;
        else
            *target = index;

        int32_t p = guess + STEP[index] * mult;
        state->push(p);

        int64_t err = int64_t(value) - int64_t(p);
        err12Squared += err * err;

        state->doShift(table, index);
        state->high = !state->high;
    }
    if (err) {
        *err = int32_t(err12Squared / nSamples);
    }
    if (state->high) target++;
    return int(target - start);
}

void S4ADPCM::decode4(const uint8_t* p, int32_t nSamples,
    int32_t volume,
    bool add,
    int32_t* out, State* state, const int* table)
{
    W12ASSERT(STEP[ZERO_INDEX] == 0);
    state->volumeTarget = volume << 8;

    for (int32_t i = 0; i < nSamples; ++i) {
        uint8_t index = 0;
        if (state->high) {
            index = *p >> 4;
            p++;
        }
        else {
            index = *p & 0xf;
        }
        int32_t mult = 1 << state->shift;
        int32_t guess = state->guess();
        int32_t value = guess + STEP[index] * mult;

        state->push(value);

        if (state->volumeShifted < state->volumeTarget)
            state->volumeShifted += VOLUME_EASING;
        else if (state->volumeShifted > state->volumeTarget)
            state->volumeShifted -= VOLUME_EASING;

        int32_t valueClipped = value;
        if (valueClipped > SHRT_MAX) valueClipped = SHRT_MAX;
        if (valueClipped < SHRT_MIN) valueClipped = SHRT_MIN;
        int32_t s = scaleVol(valueClipped, state->volumeShifted);
        out[0] = out[1] = add ? sat_add(s, out[0]) : s;
        out += 2;

        state->doShift(table, index);
        state->high = !state->high;
    }
}
