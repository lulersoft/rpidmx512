/**
 * @file ipprog.cpp
 *
 */
/* Copyright (C) 2018-2019 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
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

#include <stddef.h>
#ifndef NDEBUG
 #include <stdio.h>
#endif
#include <string.h>
#include <assert.h>

#include "artnetipprog.h"
#include "artnet.h"

#include "ipprog.h"

#include "network.h"

union uip {
	uint32_t u32;
	uint8_t u8[4];
} static ip_union;

ArtNetIpProg::~ArtNetIpProg(void) {
}

IpProg::IpProg(void) {

}

IpProg::~IpProg(void) {

}

void IpProg::Handler(const struct TArtNetIpProg *pArtNetIpProg, struct TArtNetIpProgReply *pArtNetIpProgReply) {
	// Ip
	ip_union.u32 = Network::Get()->GetIp();
	memcpy((void*) &pArtNetIpProgReply->ProgIpHi, (void*) ip_union.u8, ARTNET_IP_SIZE);
	// Netmask
	ip_union.u32 = Network::Get()->GetNetmask();
	memcpy((void*) &pArtNetIpProgReply->ProgSmHi, (void*) ip_union.u8, ARTNET_IP_SIZE);
	// Port
	pArtNetIpProgReply->ProgPortHi = (uint8_t) (ARTNET_UDP_PORT >> 8);
	pArtNetIpProgReply->ProgPortLo = ARTNET_UDP_PORT & 0xFF;
	//
	pArtNetIpProgReply->Filler = 0;
	// Gateway
	ip_union.u32 = Network::Get()->GetGatewayIp();
	memcpy((void*) &pArtNetIpProgReply->ProgGwHi, (void*) ip_union.u8, ARTNET_IP_SIZE);

#ifndef NDEBUG
	printf("IpProg::Handler, Command = %d\n", pArtNetIpProg->Command);
	printf("\tIP : " IPSTR "\n", IP2STR(Network::Get()->GetIp()));
	printf("\tNetmask : " IPSTR "\n", IP2STR(Network::Get()->GetNetmask()));
	printf("\tGateway : " IPSTR "\n", IP2STR(Network::Get()->GetGatewayIp()));
#endif

	if (pArtNetIpProg->Command == 0) {
		// Nothing to do
	}

	if ((pArtNetIpProg->Command & IPPROG_COMMAND_ENABLE_DHCP) == IPPROG_COMMAND_ENABLE_DHCP) {
		// FIXME Remove when DHCP supported
		pArtNetIpProgReply->Status = 0; // DHCP Disabled;
	}

	if ((pArtNetIpProg->Command & IPPROG_COMMAND_SET_TO_DEFAULT) == IPPROG_COMMAND_SET_TO_DEFAULT) {
		Network::Get()->SetIp(0);

		ip_union.u32 = Network::Get()->GetIp();
		memcpy((void*) &pArtNetIpProgReply->ProgIpHi, (void*) ip_union.u8, ARTNET_IP_SIZE);
		ip_union.u32 = Network::Get()->GetNetmask();
		memcpy((void*) &pArtNetIpProgReply->ProgSmHi, (void*) ip_union.u8, ARTNET_IP_SIZE);

		pArtNetIpProgReply->Status = 0; // DHCP Disabled;
	}

	if ((pArtNetIpProg->Command & IPPROG_COMMAND_PROGRAM_IPADDRESS) == IPPROG_COMMAND_PROGRAM_IPADDRESS) {
		// Get IPAddress from IpProg
		memcpy((void*) ip_union.u8, (void*) &pArtNetIpProg->ProgIpHi, ARTNET_IP_SIZE);

		Network::Get()->SetIp(ip_union.u32);

		// Set IPAddress in IpProgReply
		memcpy(&pArtNetIpProgReply->ProgIpHi, &pArtNetIpProg->ProgIpHi, ARTNET_IP_SIZE);
		pArtNetIpProgReply->Status = 0; // DHCP Disabled;
	}

	if ((pArtNetIpProg->Command & IPPROG_COMMAND_PROGRAM_SUBNETMASK) == IPPROG_COMMAND_PROGRAM_SUBNETMASK) {
		// Get SubnetMask from IpProg
		memcpy((void*) ip_union.u8, (void*) &pArtNetIpProg->ProgSmHi, ARTNET_IP_SIZE);

		Network::Get()->SetNetmask(ip_union.u32);

		// Set SubnetMask in IpProgReply
		memcpy(&pArtNetIpProgReply->ProgSmHi, &pArtNetIpProg->ProgSmHi, ARTNET_IP_SIZE);
	}

	if ((pArtNetIpProg->Command & IPPROG_COMMAND_PROGRAM_GATEWAY) == IPPROG_COMMAND_PROGRAM_GATEWAY) {
		// FIXME Remove when Gateway supported
	}

#ifndef NDEBUG
	printf("\tIP : " IPSTR "\n", IP2STR(Network::Get()->GetIp()));
	printf("\tNetmask : " IPSTR "\n", IP2STR(Network::Get()->GetNetmask()));
	printf("\tGateway : " IPSTR "\n", IP2STR(Network::Get()->GetGatewayIp()));
#endif
}
