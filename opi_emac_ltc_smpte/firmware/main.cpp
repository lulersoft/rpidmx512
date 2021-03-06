/**
 * @file main.cpp
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hardware.h"
#include "networkh3emac.h"
#include "ledblink.h"

#include "console.h"

#include "ltcparams.h"
#include "ltcleds.h"

#include "artnetnode.h"
#include "artnetparams.h"

#include "artnetconst.h"
#include "networkconst.h"

#include "ipprog.h"
#include "timesync.h"

#include "midi.h"
#include "rtpmidi.h"
#include "midiparams.h"

#include "mdnsservices.h"

#include "tcnet.h"
#include "tcnetparams.h"
#include "tcnettimecode.h"

#include "ntpserver.h"

#include "oscserver.h"

#include "display.h"
#include "displaymax7219.h"

#include "sourceselect.h"
#include "sourceselectconst.h"

#include "ntpserver.h"

#include "h3/artnetreader.h"
#include "h3/ltcreader.h"
#include "h3/ltcsender.h"
#include "h3/midireader.h"
#include "h3/tcnetreader.h"
#include "h3/ltcgenerator.h"
#include "h3/rtpmidireader.h"

#include "spiflashinstall.h"

#include "spiflashstore.h"
#include "storeltc.h"
#include "storetcnet.h"
#include "storeremoteconfig.h"

#include "remoteconfig.h"
#include "remoteconfigparams.h"

#include "firmwareversion.h"

#include "software_version.h"

extern "C" {

__attribute__((noinline)) static void print_disabled(bool b, const char *p) {
	if (b) {
		printf(" %s output is disabled\n", p);
	}
}

__attribute__((noinline)) static void console_display_puts(const char *p) {
	console_puts(p);
	Display::Get()->PutString(p);
}

void notmain(void) {
	Hardware hw;
	NetworkH3emac nw;
	LedBlink lb;
	Display display(0,4);
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);

	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;

	StoreLtc storeLtc;
	LtcParams ltcParams((LtcParamsStore *)&storeLtc);

	struct TLtcDisabledOutputs tLtcDisabledOutputs;
	struct TLtcTimeCode tStartTimeCode;
	struct TLtcTimeCode tStopTimeCode;

	if (ltcParams.Load()) {
		ltcParams.CopyDisabledOutputs(&tLtcDisabledOutputs);
		ltcParams.StartTimeCodeCopyTo(&tStartTimeCode);
		ltcParams.StopTimeCodeCopyTo(&tStopTimeCode);
		ltcParams.Dump();
	}

	const bool bEnableNtp = ltcParams.IsNtpEnabled();
	tLtcDisabledOutputs.bNtp = !bEnableNtp;

	LtcLeds leds;

	fw.Print();

	hw.SetLed(HARDWARE_LED_ON);

	display.ClearLine(1);
	display.ClearLine(2);

	console_status(CONSOLE_YELLOW, NetworkConst::MSG_NETWORK_INIT);
	display.TextStatus(NetworkConst::MSG_NETWORK_INIT, DISPLAY_7SEGMENT_MSG_INFO_NETWORK_INIT);

	nw.Init((NetworkParamsStore *)spiFlashStore.GetStoreNetwork());
	nw.SetNetworkStore((NetworkStore *)spiFlashStore.GetStoreNetwork());
	nw.Print();

	Midi midi;
	ArtNetNode node;
	TCNet tcnet;
	LtcSender ltcSender;
	RtpMidi rtpMidi;
	OSCServer oscServer;

	LtcReader ltcReader(&node, &tLtcDisabledOutputs);
	MidiReader midiReader(&node, &tLtcDisabledOutputs);
	ArtNetReader artnetReader(&tLtcDisabledOutputs);
	TCNetReader tcnetReader(&node, &tLtcDisabledOutputs);
	RtpMidiReader rtpMidiReader(&node, &tLtcDisabledOutputs);

	LtcGenerator ltcGenerator(&node, &tStartTimeCode, &tStopTimeCode, &tLtcDisabledOutputs);
	NtpServer ntpServer(ltcParams.GetYear(), ltcParams.GetMonth(), ltcParams.GetDay());

	console_status(CONSOLE_YELLOW, ArtNetConst::MSG_NODE_PARAMS);
	display.TextStatus(ArtNetConst::MSG_NODE_PARAMS, DISPLAY_7SEGMENT_MSG_INFO_NODE_PARMAMS);

	node.SetShortName("LTC Node");

	ArtNetParams artnetparams((ArtNetParamsStore *)spiFlashStore.GetStoreArtNet());

	if (artnetparams.Load()) {
		artnetparams.Set(&node);
		artnetparams.Dump();
	}

	IpProg ipprog;
	node.SetIpProgHandler(&ipprog);

	TimeSync timeSync;
	if (!ltcParams.IsTimeSyncDisabled()) {
		node.SetTimeSyncHandler(&timeSync);
	}

	console_status(CONSOLE_YELLOW, ArtNetConst::MSG_NODE_START);
	display.TextStatus(ArtNetConst::MSG_NODE_START, DISPLAY_7SEGMENT_MSG_INFO_NODE_START);

	node.Print();
	node.Start();

	console_status(CONSOLE_GREEN, ArtNetConst::MSG_NODE_STARTED);
	display.TextStatus(ArtNetConst::MSG_NODE_STARTED, DISPLAY_7SEGMENT_MSG_INFO_NODE_STARTED);

	StoreTCNet storetcnet;
	TCNetParams tcnetparams((TCNetParamsStore *) &storetcnet);

	if (tcnetparams.Load()) {
		tcnetparams.Set(&tcnet);
		tcnetparams.Dump();
	}

	DisplayMax7219 max7219(ltcParams.GetMax7219Type(), ltcParams.IsShowSysTime());
	max7219.Init(ltcParams.GetMax7219Intensity());

	display.ClearLine(3);
	display.Printf(3, IPSTR " /%d %c", IP2STR(nw.GetIp()), (int) nw.GetNetmaskCIDR(), nw.IsDhcpKnown() ? (nw.IsDhcpUsed() ? 'D' : 'S') : ' ');

	// Select the source

	TLtcReaderSource source = ltcParams.GetSource();

	SourceSelect sourceSelect(source);

	if (sourceSelect.Check()) {
		while (sourceSelect.Wait(source)) {
			nw.Run();
			lb.Run();
		}
	}

	// From here work with source selection

	if (source != LTC_READER_SOURCE_MIDI) {
		midi.Init(MIDI_DIRECTION_OUTPUT);
	}

	if ((source != LTC_READER_SOURCE_LTC) && (!tLtcDisabledOutputs.bLtc)) {
		ltcSender.Start();
	}

	switch (source) {
	case LTC_READER_SOURCE_ARTNET:
		node.SetTimeCodeHandler(&artnetReader);
		artnetReader.Start();
		break;
	case LTC_READER_SOURCE_MIDI:
		midiReader.Start();
		break;
	case LTC_READER_SOURCE_TCNET:
		tcnet.SetTimeCodeHandler(&tcnetReader);
		tcnet.Start();
		tcnetReader.Start();
		break;
	case LTC_READER_SOURCE_INTERNAL:
		ltcGenerator.Start();
		break;
	case LTC_READER_SOURCE_APPLEMIDI:
		rtpMidi.SetHandler(&rtpMidiReader);
		rtpMidiReader.Start();
		break;
	default:
		ltcReader.Start();
		break;
	}

	if ((source == LTC_READER_SOURCE_APPLEMIDI) || !tLtcDisabledOutputs.bRtpMidi) {
		rtpMidi.Start();
		rtpMidi.AddServiceRecord(0, MDNS_SERVICE_CONFIG, 0x2905);
	}

	const bool bRunOSCServer = ((source == LTC_READER_SOURCE_INTERNAL) && ltcParams.IsOscEnabled());

	if (bRunOSCServer) {
		oscServer.Start();
		oscServer.Print();
		rtpMidi.AddServiceRecord(0, MDNS_SERVICE_OSC, oscServer.GetPortIncoming(), "type=server");
	}

	if (bEnableNtp) {
		ntpServer.SetTimeCode(&tStartTimeCode);
		ntpServer.Print();
		ntpServer.Start();
	}

	midi.Print();
	rtpMidi.Print();
	tcnet.Print();
	ltcGenerator.Print();

	RemoteConfig remoteConfig(REMOTE_CONFIG_LTC, REMOTE_CONFIG_MODE_TIMECODE, 1 + source);

	StoreRemoteConfig storeRemoteConfig;
	RemoteConfigParams remoteConfigParams(&storeRemoteConfig);

	if(remoteConfigParams.Load()) {
		remoteConfigParams.Set(&remoteConfig);
		remoteConfigParams.Dump();
	}

	display.ClearLine(4);

	console_puts("Source : ");
	display.SetCursorPos(0,3);

	puts(SourceSelectConst::SOURCE[source]);
	display.PutString(SourceSelectConst::SOURCE[source]);

	if (source == LTC_READER_SOURCE_TCNET) {
		console_display_puts(" ");

		if (tcnet.GetLayer() != TCNET_LAYER_UNDEFINED) {
			console_putc('L');
			display.PutChar('L');
			console_putc(TCNet::GetLayerName(tcnet.GetLayer()));
			display.PutChar(TCNet::GetLayerName(tcnet.GetLayer()));
			console_display_puts(" Time");

			if (tcnet.IsSetTimeCodeType()) {
				switch (tcnet.GetTimeCodeType()) {
				case TCNET_TIMECODE_TYPE_FILM:
					console_display_puts(" F24");
					break;
				case TCNET_TIMECODE_TYPE_EBU_25FPS:
					console_display_puts(" F25");
					break;
				case TCNET_TIMECODE_TYPE_DF:
					console_display_puts(" F29");
					break;
				case TCNET_TIMECODE_TYPE_SMPTE_30FPS:
					console_display_puts(" F30");
					break;
				default:
					break;
				}
				puts("FPS");
			}
		} else {
			console_display_puts("SMPTE");
		}
	}

	print_disabled(tLtcDisabledOutputs.bDisplay, "Display");
	print_disabled(tLtcDisabledOutputs.bMax7219, "Max7219");
	print_disabled((source != LTC_READER_SOURCE_LTC) && (tLtcDisabledOutputs.bLtc), "LTC");
	print_disabled((source != LTC_READER_SOURCE_TCNET) && (tLtcDisabledOutputs.bTCNet), "TCNet");
	print_disabled((source != LTC_READER_SOURCE_APPLEMIDI) && (tLtcDisabledOutputs.bRtpMidi), "AppleMIDI");
	print_disabled((source != LTC_READER_SOURCE_MIDI) && (tLtcDisabledOutputs.bMidi), "MIDI");
	print_disabled((source != LTC_READER_SOURCE_ARTNET) && (tLtcDisabledOutputs.bArtNet), "Art-Net");
	print_disabled(tLtcDisabledOutputs.bNtp, "NTP");

	printf("Display : %d (%d,%d)\n", display.GetDetectedType(), display.getCols(), display.getRows());

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();

		if (source == LTC_READER_SOURCE_TCNET) {		//FIXME Remove when MASTER is implemented
			tcnet.Run();
		}

		if ((source == LTC_READER_SOURCE_APPLEMIDI) || !tLtcDisabledOutputs.bRtpMidi) {
			rtpMidi.Run();
		}

		switch (source) {
		case LTC_READER_SOURCE_LTC:
			ltcReader.Run();
			break;
		case LTC_READER_SOURCE_ARTNET:
			artnetReader.Run();		// Handles MIDI Quarter Frame output messages
			break;
		case LTC_READER_SOURCE_MIDI:
			midiReader.Run();
			break;
		case LTC_READER_SOURCE_TCNET:
			tcnetReader.Run();		// Handles MIDI Quarter Frame output messages
			break;
		case LTC_READER_SOURCE_INTERNAL:
			ltcGenerator.Run();		// Handles MIDI Quarter Frame output messages
			break;
		case LTC_READER_SOURCE_APPLEMIDI:
			rtpMidiReader.Run();	// Handles status led
			break;
		default:
			break;
		}

		node.Run();

		if (bRunOSCServer) {
			oscServer.Run();
		}

		if (bEnableNtp) {
			ntpServer.Run();
		}

		remoteConfig.Run();
		spiFlashStore.Flash();

		if (sourceSelect.IsConnected()) {
			sourceSelect.Run();
		}

		if (tLtcDisabledOutputs.bDisplay) {
			display.Run();
		}

		lb.Run();
	}
}

}
