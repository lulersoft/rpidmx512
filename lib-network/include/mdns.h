/**
 * @file mdns.h
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

#ifndef MDNS_H_
#define MDNS_H_

#include <stdint.h>

struct TmDNSFlags {
	uint32_t qr;
	uint32_t opcode;
	uint32_t aa;
	uint32_t tc;
	uint32_t rd;
	uint32_t ra;
	uint32_t zero;
	uint32_t ad;
	uint32_t cd;
	uint32_t rcode;
} ;

struct TMDNSServiceRecord {
	uint16_t nPort;
	uint8_t *pName;
	uint8_t *pServName;
	uint8_t *pTextContent;
};

struct TMDNSRecordData {
	uint32_t nSize;
	uint8_t aBuffer[512];
};

#define SERVICE_RECORDS_MAX		4

class MDNS {
public:
	MDNS(void);
	~MDNS(void);

	void Start(void);
	void Stop(void);

	void Run(void);

	void Print(void);

	void SetName(const char *pName);

	bool AddServiceRecord(const char* pName, const char *pServName, uint16_t nPort, const char* pTextContent = 0);

private:
	void Parse(void);
	void HandleRequest(uint16_t nQuestions);

	uint32_t DecodeDNSNameNotation(const char *pDNSNameNotation, char *pString);

	uint32_t WriteDnsName(const char *pSource, char *pDestination, bool bNullTerminated = true);
	uint8_t* FindFirstDotFromRight(const uint8_t* pString);

	void CreateAnswerLocalIpAddress(void);

	uint32_t CreateAnswerServiceSrv(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServiceTxt(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServicePtr(uint32_t nIndex, uint8_t *pDestination);
	uint32_t CreateAnswerServiceDnsSd(uint32_t nIndex, uint8_t *pDestination);

	void CreateMDNSMessage(uint32_t nIndex);

private:
	uint32_t m_nMulticastIp;
	int32_t m_nHandle;
	uint8_t *m_pBuffer;
	uint8_t *m_pOutBuffer;
	uint32_t m_nRemoteIp;
	uint16_t m_nRemotePort;
	uint16_t m_nBytesReceived;
	uint8_t *m_pName;
	uint32_t m_nLastAnnounceMillis;
	TMDNSServiceRecord m_aServiceRecords[SERVICE_RECORDS_MAX];
	TMDNSRecordData m_aServiceRecordsData[SERVICE_RECORDS_MAX];
	uint32_t m_nDNSServiceRecords;
	TMDNSRecordData m_tAnswerLocalIp;
};

#endif /* MDNS_H_ */
