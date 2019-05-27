/**
 * @file tcnetreader.cpp
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

#ifdef NDEBUG
 #undef NDEBUG
#endif

#include <stdint.h>
#include <assert.h>

#include "hardwarebaremetal.h"

#include "arm/synchronize.h"
#include "h3_hs_timer.h"
#include "h3_timer.h"
#include "irq_timer.h"

#ifndef NDEBUG
 #include <stdio.h>
 #include "console.h"
#endif

#include "tcnetreader.h"

// Output
#include "ltcleds.h"
#include "display.h"
#include "displaymax7219.h"
#include "artnetnode.h"
#include "midi.h"

static volatile uint32_t nUpdatesPerSecond = 0;
static volatile uint32_t nUpdatesPrevious = 0;
static volatile uint32_t nUpdates = 0;

static volatile uint32_t nLedToggle = 0;

static volatile uint32_t nMidiQuarterFrameUs = 0;
static volatile uint32_t nMidiQuarterFramePiece = 0;
static volatile bool IsMidiQuarterFrameMessage = false;

static volatile struct _midi_send_tc tMidiTimeCode = { 0, 0, 0, 0, MIDI_TC_TYPE_EBU };

static volatile uint32_t nLimitUs;

static void irq_timer0_update_handler(uint32_t clo) {
	dmb();
	nUpdatesPerSecond = nUpdates - nUpdatesPrevious;
	nUpdatesPrevious = nUpdates;

	if ((nUpdatesPerSecond >= 25) && (nUpdatesPerSecond <= 1000)) {
		if (nLedToggle++ & 0x01) {
			Hardware::Get()->SetLed(HARDWARE_LED_ON);
		} else {
			Hardware::Get()->SetLed(HARDWARE_LED_OFF);
		}
	} else {
		Hardware::Get()->SetLed(HARDWARE_LED_ON);
	}
}

static void irq_timer1_midi_handler(uint32_t clo) {
	H3_TIMER->TMR1_INTV = nMidiQuarterFrameUs * 12;
	H3_TIMER->TMR1_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD);

	dmb();
	IsMidiQuarterFrameMessage = true;
}

static void itoa_base10(int arg, char *buf) {
	char *n = buf;

	if (arg == 0) {
		*n++ = '0';
		*n = '0';
		return;
	}

	*n++ = (char) '0' + (char) (arg / 10);
	*n = (char) '0' + (char) (arg % 10);
}

TCNetTimeCode::~TCNetTimeCode(void) {

}

TCNetReader::TCNetReader(ArtNetNode* pNode) :
	m_pNode(pNode),
	m_nTimeCodePrevious(0xFF),
	m_tTimeCodeTypePrevious(TC_TYPE_INVALID)
{
	for (uint32_t i = 0; i < sizeof(m_aTimeCode) / sizeof(m_aTimeCode[0]) ; i++) {
		m_aTimeCode[i] = ' ';
	}

	m_aTimeCode[2] = ':';
	m_aTimeCode[5] = ':';
	m_aTimeCode[8] = '.';
}

TCNetReader::~TCNetReader(void) {
	Stop();
}

void TCNetReader::Start(void) {
	irq_timer_init();

	irq_timer_set(IRQ_TIMER_0, irq_timer0_update_handler);
	H3_TIMER->TMR0_INTV = 0xB71B00; // 1 second
	H3_TIMER->TMR0_CTRL &= ~(TIMER_CTRL_SINGLE_MODE);
	H3_TIMER->TMR0_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD);

	irq_timer_set(IRQ_TIMER_1, irq_timer1_midi_handler);
	H3_TIMER->TMR1_CTRL |= TIMER_CTRL_SINGLE_MODE;
}

void TCNetReader::Stop(void) {
	irq_timer_set(IRQ_TIMER_0, 0);
	irq_timer_set(IRQ_TIMER_1, 0);
}

void TCNetReader::Handler(const struct TTCNetTimeCode* pTimeCode) {
	assert(((uint32_t )pTimeCode & 0x3) == 0); // Check if we can do 4-byte compare

	char *pTimeCodeType;
#ifndef NDEBUG
	char aLimitWarning[16];
	const uint32_t nNowUs = h3_hs_timer_lo_us();
#endif

	nUpdates++;

	const uint32_t *p = (uint32_t *)pTimeCode;

	if (m_nTimeCodePrevious != *p) {
		m_pNode->SendTimeCode((const struct TArtNetTimeCode *) pTimeCode);
	}

	tMidiTimeCode.hour = pTimeCode->nHours;
	tMidiTimeCode.minute = pTimeCode->nMinutes;
	tMidiTimeCode.second = pTimeCode->nSeconds;
	tMidiTimeCode.frame = pTimeCode->nFrames;
	tMidiTimeCode.rate = (_midi_timecode_type) pTimeCode->nType;

	if ((m_tTimeCodeTypePrevious != (TTimecodeTypes) pTimeCode->nType)) {
		m_tTimeCodeTypePrevious = (TTimecodeTypes) pTimeCode->nType;

		switch ((_midi_timecode_type) pTimeCode->nType) {
		case TC_TYPE_FILM:
			nLimitUs = (uint32_t) ((double) 1000000 / (double) 24);
			break;
		case TC_TYPE_EBU:
			nLimitUs = (uint32_t) ((double) 1000000 / (double) 25);
			break;
		case TC_TYPE_DF:
		case TC_TYPE_SMPTE:
			nLimitUs = (uint32_t) ((double) 1000000 / (double) 30);
			break;
		default:
			assert(0);
			break;
		}

		Midi::Get()->SendTimeCode((struct _midi_send_tc *) &tMidiTimeCode);

		nMidiQuarterFramePiece = 0;
		nMidiQuarterFrameUs = nLimitUs / 4;

		H3_TIMER->TMR1_INTV = nMidiQuarterFrameUs * 12;
		H3_TIMER->TMR1_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD);

		pTimeCodeType = (char *) Ltc::GetType((TTimecodeTypes) pTimeCode->nType);

		Display::Get()->TextLine(2, pTimeCodeType, TC_TYPE_MAX_LENGTH);
		LtcLeds::Get()->Show((TTimecodeTypes) pTimeCode->nType);
	}

	if (m_nTimeCodePrevious != *p) {
		m_nTimeCodePrevious = *p;

		itoa_base10(pTimeCode->nHours, (char *) &m_aTimeCode[0]);
		itoa_base10(pTimeCode->nMinutes, (char *) &m_aTimeCode[3]);
		itoa_base10(pTimeCode->nSeconds, (char *) &m_aTimeCode[6]);
		itoa_base10(pTimeCode->nFrames, (char *) &m_aTimeCode[9]);

		Display::Get()->TextLine(1, (const char *) m_aTimeCode, TC_CODE_MAX_LENGTH);
		DisplayMax7219::Get()->Show((const char *) m_aTimeCode);
	}

#ifndef NDEBUG
	const uint32_t nDeltaUs = h3_hs_timer_lo_us() - nNowUs;

	if (nLimitUs == 0) {
		sprintf(aLimitWarning, "%.2d:-----:%.5d", (int) nUpdatesPerSecond, (int) nDeltaUs);
		console_status(CONSOLE_CYAN, aLimitWarning);
	} else {
		sprintf(aLimitWarning, "%.2d:%.5d:%.5d", (int) nUpdatesPerSecond, (int) nLimitUs, (int) nDeltaUs);
		console_status(nDeltaUs < nLimitUs ? CONSOLE_YELLOW : CONSOLE_RED, aLimitWarning);
	}
#endif
}

void TCNetReader::Run(void) {
	dmb();
	if ((nUpdatesPerSecond >= 25) && (nUpdatesPerSecond <= 1000)) {
		dmb();
		if (__builtin_expect((IsMidiQuarterFrameMessage), 0)) {
			dmb();
			IsMidiQuarterFrameMessage = false;

			uint8_t bytes[2] = { 0xF1, 0x00 };
			const uint8_t data = nMidiQuarterFramePiece << 4;

			switch (nMidiQuarterFramePiece) {
			case 0:
				bytes[1] = data | (tMidiTimeCode.frame & 0x0F);
				break;
			case 1:
				bytes[1] = data | ((tMidiTimeCode.frame & 0x10) >> 4);
				break;
			case 2:
				bytes[1] = data | (tMidiTimeCode.second & 0x0F);
				break;
			case 3:
				bytes[1] = data | ((tMidiTimeCode.second & 0x30) >> 4);
				break;
			case 4:
				bytes[1] = data | (tMidiTimeCode.minute & 0x0F);
				break;
			case 5:
				bytes[1] = data | ((tMidiTimeCode.minute & 0x30) >> 4);
				break;
			case 6:
				bytes[1] = data | (tMidiTimeCode.hour & 0x0F);
				break;
			case 7:
				bytes[1] = data | (tMidiTimeCode.rate << 1) | ((tMidiTimeCode.hour & 0x10) >> 4);
				break;
			default:
				break;
			}

			Midi::Get()->SendRaw(bytes, 2);
			nMidiQuarterFramePiece = (nMidiQuarterFramePiece + 1) & 0x07;
		}
	}
}
