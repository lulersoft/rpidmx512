/**
 * @file ltc.cpp
 *
 */
/* Copyright (C) 2019 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "ltc.h"

#ifndef ALIGNED
 #define ALIGNED __attribute__ ((aligned (4)))
#endif

static const char aTypes[5][TC_TYPE_MAX_LENGTH + 1] ALIGNED =
	{ "Film 24fps ", "EBU 25fps  ", "DF 29.97fps", "SMPTE 30fps", "----- -----" };


const char* Ltc::GetType(TTimecodeTypes tTimeCodeType) {
	if (tTimeCodeType > TC_TYPE_UNKNOWN) {
		return aTypes[TC_TYPE_UNKNOWN];
	}

	return aTypes[tTimeCodeType];
}

TTimecodeTypes Ltc::GetType(uint8_t nFps) {
	switch (nFps) {
		case 24:
			return TC_TYPE_FILM;
			break;
		case 25:
			return TC_TYPE_EBU;
			break;
		case 29:
			return TC_TYPE_DF;
			break;
		case 30:
			return TC_TYPE_SMPTE;
			break;
		default:
			break;
	}

	return TC_TYPE_UNKNOWN;
}

inline static void itoa_base10(uint32_t arg, char *buf) {
	char *n = buf;

	if (arg == 0) {
		*n++ = '0';
		*n = '0';
		return;
	}

	*n++ = (char) ('0' + (arg / 10));
	*n = (char) ('0' + (arg % 10));
}

void Ltc::ItoaBase10(const struct TLtcTimeCode *ptLtcTimeCode, char aTimeCode[TC_CODE_MAX_LENGTH]) {
	itoa_base10(ptLtcTimeCode->nHours, (char *) &aTimeCode[0]);
	itoa_base10(ptLtcTimeCode->nMinutes, (char *) &aTimeCode[3]);
	itoa_base10(ptLtcTimeCode->nSeconds, (char *) &aTimeCode[6]);
	itoa_base10(ptLtcTimeCode->nFrames, (char *) &aTimeCode[9]);
}

#define DIGIT(x)	((int32_t) (x) - '0')
#define VALUE(x,y)	(((x) * 10) + (y))

bool Ltc::ParseTimeCode(const char aTimeCode[TC_CODE_MAX_LENGTH], uint8_t nFps, struct TLtcTimeCode *ptLtcTimeCode) {
	assert(ptLtcTimeCode != 0);

	int32_t nTenths;
	int32_t nDigit;
	uint32_t nValue;

	if ((aTimeCode[2] != ':') || (aTimeCode[5] != ':') || (aTimeCode[8] != '.')) {
		return false;
	}

	// Frames first

	nTenths = DIGIT(aTimeCode[9]);
	if ((nTenths < 0) || (nTenths > 3)) {
		return false;
	}

	nDigit = DIGIT(aTimeCode[10]);
	if ((nDigit < 0) || (nDigit > 9)) {
		return false;
	}

	nValue = VALUE(nTenths, nDigit);

	if (nValue >= nFps) {
		return false;
	}

	ptLtcTimeCode->nFrames = nValue;

	// Seconds

	nTenths = DIGIT(aTimeCode[6]);
	if ((nTenths < 0) || (nTenths > 5)) {
		return false;
	}

	nDigit = DIGIT(aTimeCode[7]);
	if ((nDigit < 0) || (nDigit > 9)) {
		return false;
	}

	nValue = VALUE(nTenths, nDigit);

	if (nValue > 59) {
		return false;
	}

	ptLtcTimeCode->nSeconds = nValue;

	// Minutes

	nTenths = DIGIT(aTimeCode[3]);
	if ((nTenths < 0) || (nTenths > 5)) {
		return false;
	}

	nDigit = DIGIT(aTimeCode[4]);
	if ((nDigit < 0) || (nDigit > 9)) {
		return false;
	}

	nValue = VALUE(nTenths, nDigit);

	if (nValue > 59) {
		return false;
	}

	ptLtcTimeCode->nMinutes = nValue;

	// Hours

	nTenths = DIGIT(aTimeCode[0]);
	if ((nTenths < 0) || (nTenths > 2)) {
		return false;
	}

	nDigit = DIGIT(aTimeCode[1]);
	if ((nDigit < 0) || (nDigit > 9)) {
		return false;
	}

	nValue = VALUE(nTenths, nDigit);

	if (nValue > 23) {
		return false;
	}

	ptLtcTimeCode->nHours = nValue;

	return true;
}
