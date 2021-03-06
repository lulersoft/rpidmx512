/**
 * @file net.c
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

#include <stdint.h>
#include <stdbool.h>

#include "net/net.h"

#include "net_packets.h"
#include "net_debug.h"

extern int emac_eth_recv(uint8_t **);
extern void emac_free_pkt(void);

extern void net_timers_init(void);
extern void net_timers_run(void);

extern void arp_init(const uint8_t *, const struct ip_info  *);
extern void arp_handle(struct t_arp *);

extern void ip_init(const uint8_t *, const struct ip_info  *);
extern void ip_set_ip(const struct ip_info  *);
extern void ip_handle(struct t_ip4 *);

extern int dhcp_client(const uint8_t *, struct ip_info  *, const uint8_t *);

static struct ip_info s_ip_info;
static uint8_t s_mac_address[ETH_ADDR_LEN];

static uint8_t *s_p;

static void s_get_default_ip(const uint8_t *mac_address, struct ip_info *p_ip_info) {
	p_ip_info->ip.addr = 2
			+ (((uint32_t) mac_address[3]) << 8)
			+ (((uint32_t) mac_address[4]) << 16)
			+ (((uint32_t) mac_address[5]) << 24);
	p_ip_info->netmask.addr = 255;
	p_ip_info->gw.addr = p_ip_info->ip.addr;
}

void net_init(const uint8_t *mac_address, struct ip_info *p_ip_info, const uint8_t *hostname, bool *use_dhcp) {
	uint16_t i;

	net_timers_init();

	if (*use_dhcp) {
		s_get_default_ip(mac_address, p_ip_info);
	} else {
		arp_init(mac_address, p_ip_info);
	}

	ip_init(mac_address, p_ip_info);

	if (*use_dhcp) {
		if (dhcp_client(mac_address, p_ip_info, hostname) < 0) {
			*use_dhcp = false;
			DEBUG_PUTS("DHCP Client failed");
		}

		arp_init(mac_address, p_ip_info);
		ip_set_ip(p_ip_info);
	}

	for (i = 0; i < ETH_ADDR_LEN; i++) {
		s_mac_address[i] = mac_address[i];
	}

	const uint8_t *src = (const uint8_t*) p_ip_info;
	uint8_t *dst = (uint8_t*) &s_ip_info;

	for (i = 0; i < sizeof(struct ip_info); i++) {
		*dst++ = *src++;
	}
}

void net_handle(void) {
	const int length = emac_eth_recv(&s_p);

	if (__builtin_expect((length > 0), 0)) {
		const struct ether_packet *eth = (struct ether_packet*) s_p;

		if (eth->type == __builtin_bswap16(ETHER_TYPE_IPv4)) {
			ip_handle((struct t_ip4*) s_p);
		} else if (eth->type == __builtin_bswap16(ETHER_TYPE_ARP)) {
			arp_handle((struct t_arp*) s_p);
		} else {
			DEBUG_PRINTF("type %04x is not implemented", __builtin_bswap16(eth->type));
		}

		emac_free_pkt();
	}

	net_timers_run();
}

void net_set_ip(uint32_t ip) {
	s_ip_info.ip.addr = ip;

	arp_init(s_mac_address, &s_ip_info);
	ip_set_ip(&s_ip_info);
}

void net_set_default_ip(struct ip_info *p_ip_info) {
	uint32_t i;

	s_get_default_ip(s_mac_address, &s_ip_info);

	arp_init(s_mac_address, &s_ip_info);
	ip_set_ip(&s_ip_info);

	const uint8_t *src = (const uint8_t*) &s_ip_info;
	uint8_t *dst = (uint8_t*) p_ip_info;

	for (i = 0; i < sizeof(struct ip_info); i++) {
		*dst++ = *src++;
	}
}
