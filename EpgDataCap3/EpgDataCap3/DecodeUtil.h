#pragma once

#include "EpgDBUtil.h"
#include "../../Common/TSPacketUtil.h"
#include "../../Common/TSBuffUtil.h"
#include "../../Common/Util.h"
#include "../../Common/EpgDataCap3Def.h"

#include "./Table/TableUtil.h"

class CDecodeUtil
{
public:
	CDecodeUtil(void);
	~CDecodeUtil(void);

	void SetEpgDB(CEpgDBUtil* epgDBUtil);
	void AddTSData(BYTE* data);

	//��̓f�[�^�̌��݂̃X�g���[���h�c���擾����
	//�����F
	// originalNetworkID		[OUT]���݂�originalNetworkID
	// transportStreamID		[OUT]���݂�transportStreamID
	BOOL GetTSID(
		WORD* originalNetworkID,
		WORD* transportStreamID
		);

	//���X�g���[���̃T�[�r�X�ꗗ���擾����
	//�����F
	// serviceListSize			[OUT]serviceList�̌�
	// serviceList				[OUT]�T�[�r�X���̃��X�g�iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	BOOL GetServiceListActual(
		DWORD* serviceListSize,
		SERVICE_INFO** serviceList
		);

	//�X�g���[�����̌��݂̎��ԏ����擾����
	//�����F
	// time				[OUT]�X�g���[�����̌��݂̎���
	// tick				[OUT]time���擾�������_�̃`�b�N�J�E���g
	BOOL GetNowTime(
		FILETIME* time,
		DWORD* tick = NULL
		);

protected:
	typedef struct _NIT_SECTION_INFO{
		WORD network_id;
		BYTE version_number;
		BYTE last_section_number;
		map<BYTE, CNITTable*> nitSection;
		~_NIT_SECTION_INFO(void){
			map<BYTE, CNITTable*>::iterator itr;
			for( itr=nitSection.begin(); itr != nitSection.end(); itr++ ){
				SAFE_DELETE(itr->second);
			}
			nitSection.clear();
		};
	}NIT_SECTION_INFO;
	typedef struct _SDT_SECTION_INFO{
		WORD original_network_id;
		WORD transport_stream_id;
		BYTE version_number;
		BYTE last_section_number;
		map<BYTE, CSDTTable*> sdtSection;
		~_SDT_SECTION_INFO(void){
			map<BYTE, CSDTTable*>::iterator itr;
			for( itr=sdtSection.begin(); itr != sdtSection.end(); itr++ ){
				SAFE_DELETE(itr->second);
			}
			sdtSection.clear();
		};
	}SDT_SECTION_INFO;

	CEpgDBUtil* epgDBUtil;

	//PID���̃o�b�t�@�����O
	//�L�[ PID
	map<WORD, CTSBuffUtil*> buffUtilMap;

	CPATTable* patInfo;
	map<WORD, CPMTTable*> pmtMap;
	NIT_SECTION_INFO* nitActualInfo;
	SDT_SECTION_INFO* sdtActualInfo;
	CBITTable* bitInfo;
	CSITTable* sitInfo;
	FILETIME totTime;
	FILETIME tdtTime;
	FILETIME sitTime;
	DWORD totTimeTick;
	DWORD tdtTimeTick;
	DWORD sitTimeTick;


	DWORD serviceListSize;
	SERVICE_INFO* serviceList;

protected:
	void Clear();
	void ClearBuff(WORD noClearPid);
	void ChangeTSIDClear(WORD noClearPid);

	BOOL CheckPAT(WORD PID, CPATTable* pat);
	BOOL CheckPMT(WORD PID, CPMTTable* pmt);
	BOOL CheckNIT(WORD PID, CNITTable* nit);
	BOOL CheckSDT(WORD PID, CSDTTable* sdt);
	BOOL CheckTOT(WORD PID, CTOTTable* tot);
	BOOL CheckTDT(WORD PID, CTDTTable* tdt);
	BOOL CheckEIT(WORD PID, CEITTable* eit);
	BOOL CheckBIT(WORD PID, CBITTable* bit);
	BOOL CheckSIT(WORD PID, CSITTable* sit);
	BOOL CheckEIT_SD(WORD PID, CEITTable_SD* eit);
	BOOL CheckEIT_SD2(WORD PID, CEITTable_SD2* eit);

	//���X�g���[���̃T�[�r�X�ꗗ��SIT����擾����
	//�����F
	// serviceListSize			[OUT]serviceList�̌�
	// serviceList				[OUT]�T�[�r�X���̃��X�g�iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	BOOL GetServiceListSIT(
		DWORD* serviceListSize,
		SERVICE_INFO** serviceList
		);

};
