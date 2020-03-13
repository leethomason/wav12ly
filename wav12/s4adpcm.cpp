#include "s4adpcm.h"

#ifdef _MSC_VER
#include <assert.h>
#define ASSERT assert
#endif

#ifndef S4ADPCM_OPT
const
#endif
int S4ADPCM::DELTA_TABLE_4[TABLE_SIZE] = {
    // The last entry is used for the special delta=8 case
    -1, 0, 0, 0, 1, 1, 1, 2, 3
};

#ifndef S4ADPCM_OPT
const
#endif
int S4ADPCM::DELTA_TABLE_8[TABLE_SIZE] = {
    // 8-bit version doesn't use the last entry
    -1, 0, 1, 2, 3, 3, 4, 4, 4  
};

void S4ADPCM::encode8(const int16_t* data, int32_t nSamples, uint8_t* target, State* state, int64_t* e2)
{
    for (int i = 0; i < nSamples; ++i) {
        int value = data[i];
        int guess = state->guess();
        int error = value - guess;
        int positiveGuess = guess;

        uint8_t sign = 0;
        if (error < 0) {
            sign = 0x80;
            error = -error;
            positiveGuess = -positiveGuess;
        }
#ifdef _MSC_VER
        ASSERT(positiveGuess >= SHRT_MIN && positiveGuess <= SHRT_MAX);
#endif

        // Bias up so we round up and down equally.
        // Error always positive, delta always positive.
        int bias = ((1 << state->shift) - 1) / 2;
        int delta = (error + bias) >> state->shift;
        if (delta > 127) delta = 127;

        if ((positiveGuess + (delta << state->shift)) > SHRT_MAX) {
            delta = (SHRT_MAX - positiveGuess) >> state->shift;
            if (delta > 127) delta = 127;
        }

        *target++ = delta | sign;

        int p = guess + (delta << state->shift) * (sign ? -1 : 1);
        state->push(p);

        *e2 += calcError(value, p);
        assert(*e2 >= 0);    // check for overflow

        state->shift += DELTA_TABLE_8[delta >> 4];
        if (state->shift < 0) state->shift = 0;
        if (state->shift > SHIFT_LIMIT_8) state->shift = SHIFT_LIMIT_8;
    }
}


int S4ADPCM::encode4(const int16_t* data, int32_t nSamples, uint8_t* target, State* state, int64_t* e2)
{
    const uint8_t* start = target;
    for (int i = 0; i < nSamples; ++i) {
        int value = data[i];
        int guess = state->guess();
        int error = value - guess;

        // Bias up so we round up and down equally.
        // Error always positive, delta always positive.
        //int errorSign = error >= 0 ? 1 : -1;
        int bias = (((1 << state->shift) - 1) / 2);
        int delta = (error + bias) >> state->shift;

        if (delta > 7) delta = 7;
        if (delta < -8) delta = -8;

        while (guess + (delta << state->shift) > SHRT_MAX)
            --delta;
        while (guess + (delta << state->shift) < SHRT_MIN)
            ++delta;

        assert(delta >= -8 && delta <= 7);

        uint8_t d = (delta >= 0) ? uint8_t(delta) : uint8_t(delta & 0x0f);
        assert((d & 0xf0) == 0);

        if (state->high)
            *target++ |= d << 4;
        else
            *target = d;

        state->high = !state->high;

        int p = guess + (delta << state->shift);
#ifdef _MSC_VER
        ASSERT(p >= SHRT_MIN && p <= SHRT_MAX);
#endif

        state->push(p);

        *e2 += calcError(value, p);
        assert(*e2 >= 0);    // check for overflow

        state->shift += DELTA_TABLE_4[delta >= 0 ? delta : -delta];
        if (state->shift < 0) state->shift = 0;
        if (state->shift > SHIFT_LIMIT_4) state->shift = SHIFT_LIMIT_4;
    }
    if (state->high) target++;
    return int(target - start);
}

void S4ADPCM::decode4(const uint8_t* p, int32_t nSamples,
    int volume,
    bool add,
    int32_t* out, State* state)
{
    state->volumeTarget = volume << 8;

    for (int32_t i = 0; i < nSamples; ++i) {
        uint8_t d = 0;
        if (state->high) {
            d = *p >> 4;
            p++;
        }
        else {
            d = *p & 0xf;
        }
        int8_t delta = int8_t(d << 4) >> 4;

        int guess = state->guess();
        int value = guess + (delta << state->shift);

#ifdef _MSC_VER
        if (value < SHRT_MIN || value > SHRT_MAX) {
            ASSERT(false);
        }
#endif
        state->push(value);

        if (state->volumeShifted < state->volumeTarget)
            state->volumeShifted += VOLUME_EASING;
        else if (state->volumeShifted > state->volumeTarget)
            state->volumeShifted -= VOLUME_EASING;

        // max: SHRT_MAX * 256 * 256
        //      32767 * 256 * 256 = 2147418112
        //              INT32_MAX = 2147483647
        int32_t s = value * state->volumeShifted;
        if (add)
            out[0] = out[1] = sat_add(s, out[0]);
        else
            out[0] = out[1] = s;
        out += 2;

        state->shift += DELTA_TABLE_4[delta >= 0 ? delta : -delta];
        if (state->shift < 0) state->shift = 0;
        if (state->shift > SHIFT_LIMIT_4) state->shift = SHIFT_LIMIT_4;
        state->high = !state->high;
    }
}

void S4ADPCM::decode8(const uint8_t* p, int32_t nSamples,
    int volume, bool add, int32_t* out, State* state)
{
    state->volumeTarget = volume << 8;

    for (int32_t i = 0; i < nSamples; ++i) {
        int delta = *p & 0x7f;
        uint8_t sign = *p & 0x80;
        p++;

        int guess = state->guess();
        int value = 0;
        if (sign) {
            value = guess - (delta << state->shift);
        }
        else {
            value = guess + (delta << state->shift);
        }
        state->push(value);

        if (state->volumeShifted < state->volumeTarget)
            state->volumeShifted += VOLUME_EASING;
        else if (state->volumeShifted > state->volumeTarget)
            state->volumeShifted -= VOLUME_EASING;

        int32_t s = value * state->volumeShifted;
        if (add) {
            out[0] = out[1] = sat_add(s, out[0]);
        }
        else {
            out[0] = out[1] = s;
        }
        out += 2;

        state->shift += DELTA_TABLE_8[delta >> 4];
        if (state->shift < 0) state->shift = 0;
        if (state->shift > SHIFT_LIMIT_8) state->shift = SHIFT_LIMIT_8;
    }
}
