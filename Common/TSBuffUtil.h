#pragma once

#include "TSPacketUtil.h"

#define ERR_ADD_NEXT		100
#define ERR_NOT_SUPPORT		101

class CTSBuffUtil
{
public:
	CTSBuffUtil(BOOL supportPES = FALSE);
	~CTSBuffUtil(void);

	//Add188TS()��TRUE��Ԃ���GetSectionBuff()��1��ȏ㐬������B���̂Ƃ��󂯎��Ȃ������o�b�t�@�͎���Add188TS()�ŏ�����
	DWORD Add188TS(CTSPacketUtil* tsPacket);
	BOOL GetSectionBuff(BYTE** sectionData, DWORD* dataSize);
	BOOL IsPES();

protected:
	DWORD sectionSize;
	vector<BYTE> sectionBuff;
	vector<BYTE> carryPacket;

	WORD lastPID;
	BYTE lastCounter;
	BOOL duplicateFlag;

	BOOL supportPES;
	BOOL PESMode;
protected:
	void Clear();
	BOOL CheckCounter(CTSPacketUtil* tsPacket);
	DWORD AddSectionBuff(CTSPacketUtil* tsPacket);
	DWORD AddPESBuff(CTSPacketUtil* tsPacket);
};
