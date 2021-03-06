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
#include <assert.h>

#include "hardware.h"
#include "networkh3emac.h"
#include "ledblink.h"

#include "console.h"

#include "displayudf.h"
#include "displayudfparams.h"
#include "storedisplayudf.h"

#include "networkconst.h"
#include "artnetconst.h"

#include "artnet4node.h"
#include "artnet4params.h"

#include "ipprog.h"
#include "displayudfhandler.h"

#include "ws28xxdmxparams.h"
#include "ws28xxdmxmulti.h"
#include "storews28xxdmx.h"

#include "spiflashinstall.h"
#include "spiflashstore.h"
#include "remoteconfig.h"
#include "remoteconfigparams.h"
#include "storeremoteconfig.h"

#include "firmwareversion.h"

#include "software_version.h"

extern "C" {

void notmain(void) {
	Hardware hw;
	NetworkH3emac nw;
	LedBlink lb;
	DisplayUdf display;
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);
	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;
	StoreWS28xxDmx storeWS28xxDmx;

	fw.Print();

	console_puts("Ethernet Art-Net 4 Node ");
	console_set_fg_color(CONSOLE_GREEN);
	console_puts("Pixel controller {4x 4 Universes}");
	console_set_fg_color(CONSOLE_WHITE);
	console_putc('\n');

	hw.SetLed(HARDWARE_LED_ON);

	console_status(CONSOLE_YELLOW, NetworkConst::MSG_NETWORK_INIT);
	display.TextStatus(NetworkConst::MSG_NETWORK_INIT, DISPLAY_7SEGMENT_MSG_INFO_NETWORK_INIT);

	nw.Init((NetworkParamsStore *)spiFlashStore.GetStoreNetwork());
	nw.SetNetworkStore((NetworkStore *)spiFlashStore.GetStoreNetwork());
	nw.Print();

	console_status(CONSOLE_YELLOW, ArtNetConst::MSG_NODE_PARAMS);
	display.TextStatus(ArtNetConst::MSG_NODE_PARAMS, DISPLAY_7SEGMENT_MSG_INFO_NODE_PARMAMS);

	WS28xxDmxMulti ws28xxDmxMulti(WS28XXDMXMULTI_SRC_ARTNET);
	WS28xxDmxParams ws28xxparms((WS28xxDmxParamsStore *) &storeWS28xxDmx);

	if (ws28xxparms.Load()) {
		ws28xxparms.Set(&ws28xxDmxMulti);
		ws28xxparms.Dump();
	}

	ws28xxDmxMulti.Start(0);

	const uint8_t nActivePorts = ws28xxDmxMulti.GetActivePorts();

	ArtNet4Node node(nActivePorts);
	ArtNet4Params artnetparams((ArtNet4ParamsStore *)spiFlashStore.GetStoreArtNet4());

	if (artnetparams.Load()) {
		artnetparams.Set(&node);
		artnetparams.Dump();
	}

	IpProg ipprog;
	node.SetIpProgHandler(&ipprog);

	DisplayUdfHandler displayUdfHandler(&node);
	node.SetArtNetDisplay((ArtNetDisplay *)&displayUdfHandler);
	nw.SetNetworkDisplay((NetworkDisplay *)&displayUdfHandler);

	node.SetArtNetDisplay(&displayUdfHandler);

	node.SetArtNetStore((ArtNetStore *)spiFlashStore.GetStoreArtNet());

	const uint16_t nLedCount = ws28xxDmxMulti.GetLEDCount();
	const uint8_t nUniverseStart = artnetparams.GetUniverse();

	node.SetDirectUpdate(true);
	node.SetOutput(&ws28xxDmxMulti);

	uint8_t nPortIndex = 0;
	uint8_t nPage = 1;

	for (uint32_t i = 0; i < nActivePorts; i++) {
		node.SetUniverseSwitch(nPortIndex, ARTNET_OUTPUT_PORT,  nUniverseStart);

		if (ws28xxDmxMulti.GetLEDType() == WS28XXMULTI_SK6812W) {
			if (nLedCount > 128) {
				node.SetUniverseSwitch(nPortIndex + 1, ARTNET_OUTPUT_PORT, nUniverseStart + 1);
			}

			if (nLedCount > 256) {
				node.SetUniverseSwitch(nPortIndex + 2, ARTNET_OUTPUT_PORT, nUniverseStart + 2);
			}

			if (nLedCount > 384) {
				node.SetUniverseSwitch(nPortIndex + 3, ARTNET_OUTPUT_PORT, nUniverseStart + 3);
			}
		} else {
			if (nLedCount > 170) {
				node.SetUniverseSwitch(nPortIndex + 1, ARTNET_OUTPUT_PORT, nUniverseStart + 1);
			}

			if (nLedCount > 340) {
				node.SetUniverseSwitch(nPortIndex + 2, ARTNET_OUTPUT_PORT, nUniverseStart + 2);
			}

			if (nLedCount > 510) {
				node.SetUniverseSwitch(nPortIndex + 3, ARTNET_OUTPUT_PORT, nUniverseStart + 3);
			}
		}

		if (nPage < ARTNET_MAX_PAGES) {
			uint8_t nSubnetSwitch = node.GetSubnetSwitch(nPage - 1);
			nSubnetSwitch = (nSubnetSwitch + 1) & 0x0F;
			node.SetSubnetSwitch(nSubnetSwitch, nPage);

			if (nSubnetSwitch == 0) {
				uint8_t nNetSwitch = node.GetNetSwitch(nPage);
				nNetSwitch = (nNetSwitch + 1) & 0x0F;
				node.SetNetSwitch(nNetSwitch, nPage);
			}
			nPage++;
		}
		nPortIndex += ARTNET_MAX_PORTS;
	}

	node.Print();
	ws28xxDmxMulti.Print();

	display.SetTitle("Eth Art-Net 4 Pixel");
	display.Set(2, DISPLAY_UDF_LABEL_NODE_NAME);
	display.Set(3, DISPLAY_UDF_LABEL_HOSTNAME);
	display.Set(4, DISPLAY_UDF_LABEL_IP);
	display.Set(5, DISPLAY_UDF_LABEL_NETMASK);
	display.Set(6, DISPLAY_UDF_LABEL_UNIVERSE);
	display.Printf(7, "%d-%s:%d", ws28xxDmxMulti.GetActivePorts(), ws28xxparms.GetLedTypeString(ws28xxparms.GetLedType()), ws28xxparms.GetLedCount());

	StoreDisplayUdf storeDisplayUdf;
	DisplayUdfParams displayUdfParams(&storeDisplayUdf);

	if(displayUdfParams.Load()) {
		displayUdfParams.Set(&display);
		displayUdfParams.Dump();
	}

	display.Show(&node);

	RemoteConfig remoteConfig(REMOTE_CONFIG_ARTNET,  REMOTE_CONFIG_MODE_PIXEL, node.GetActiveOutputPorts());

	StoreRemoteConfig storeRemoteConfig;
	RemoteConfigParams remoteConfigParams(&storeRemoteConfig);

	if(remoteConfigParams.Load()) {
		remoteConfigParams.Set(&remoteConfig);
		remoteConfigParams.Dump();
	}

	while (spiFlashStore.Flash())
		;

	console_status(CONSOLE_YELLOW, ArtNetConst::MSG_NODE_START);
	display.TextStatus(ArtNetConst::MSG_NODE_START, DISPLAY_7SEGMENT_MSG_INFO_NODE_START);

	node.Start();

	console_status(CONSOLE_GREEN, ArtNetConst::MSG_NODE_STARTED);
	display.TextStatus(ArtNetConst::MSG_NODE_STARTED, DISPLAY_7SEGMENT_MSG_INFO_NODE_STARTED);

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();
		node.Run();;
		remoteConfig.Run();
		spiFlashStore.Flash();
		lb.Run();
		display.Run();
	}
}

}
