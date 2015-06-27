#include "stdafx.h"
#include "BonCtrl.h"
#include <process.h>

#include "../Common/ErrDef.h"
#include "../Common/TimeUtil.h"
#include "../Common/SendCtrlCmd.h"
#include "../Common/BlockLock.h"

CBonCtrl::CBonCtrl(void)
{
	InitializeCriticalSection(&this->buffLock);
	this->TSBuffOffset = 0;

    this->recvThread = NULL;
    this->recvStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    this->analyzeThread = NULL;
    this->analyzeStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    this->chScanThread = NULL;
    this->chScanStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	this->chSt_space = 0;
	this->chSt_ch = 0;
	this->chSt_chName = L"";
	this->chSt_chkNum = 0;
	this->chSt_totalNum = 0;
	this->chSt_err = ST_STOP;

	this->epgCapThread = NULL;
	this->epgCapStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	this->epgSt_err = ST_STOP;

	this->epgCapBackThread = NULL;
	this->epgCapBackStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	this->enableLiveEpgCap = FALSE;
	this->enableRecEpgCap = FALSE;

	this->epgCapBackBSBasic = TRUE;
	this->epgCapBackCS1Basic = TRUE;
	this->epgCapBackCS2Basic = TRUE;
	this->epgCapBackStartWaitSec = 30;
	this->tsBuffMaxCount = 5000;
	this->writeBuffMaxCount = -1;
}


CBonCtrl::~CBonCtrl(void)
{
	_CloseBonDriver();

	if( this->chScanThread != NULL ){
		::SetEvent(this->chScanStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->chScanThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->chScanThread, 0xffffffff);
		}
		CloseHandle(this->chScanThread);
		this->chScanThread = NULL;
	}
	if( this->chScanStopEvent != NULL ){
		CloseHandle(this->chScanStopEvent);
		this->chScanStopEvent = NULL;
	}

	if( this->epgCapThread != NULL ){
		::SetEvent(this->epgCapStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapThread, 0xffffffff);
		}
		CloseHandle(this->epgCapThread);
		this->epgCapThread = NULL;
	}
	if( this->epgCapStopEvent != NULL ){
		CloseHandle(this->epgCapStopEvent);
		this->epgCapStopEvent = NULL;
	}

	if( this->epgCapBackThread != NULL ){
		::SetEvent(this->epgCapBackStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapBackThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapBackThread, 0xffffffff);
		}
		CloseHandle(this->epgCapBackThread);
		this->epgCapBackThread = NULL;
	}
	if( this->epgCapBackStopEvent != NULL ){
		CloseHandle(this->epgCapBackStopEvent);
		this->epgCapBackStopEvent = NULL;
	}

	if( this->analyzeThread != NULL ){
		::SetEvent(this->analyzeStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->analyzeThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->analyzeThread, 0xffffffff);
		}
		CloseHandle(this->analyzeThread);
		this->analyzeThread = NULL;
	}
	if( this->analyzeStopEvent != NULL ){
		CloseHandle(this->analyzeStopEvent);
		this->analyzeStopEvent = NULL;
	}

	if( this->recvThread != NULL ){
		::SetEvent(this->recvStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->recvThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->recvThread, 0xffffffff);
		}
		CloseHandle(this->recvThread);
		this->recvThread = NULL;
	}
	if( this->recvStopEvent != NULL ){
		CloseHandle(this->recvStopEvent);
		this->recvStopEvent = NULL;
	}
	DeleteCriticalSection(&this->buffLock);
}

//BonDriver�t�H���_���w��
//�����F
// bonDriverFolderPath		[IN]BonDriver�t�H���_�p�X
void CBonCtrl::SetBonDriverFolder(
	LPCWSTR bonDriverFolderPath
)
{
	this->bonUtil.SetBonDriverFolder(bonDriverFolderPath);
}

void CBonCtrl::SetEMMMode(BOOL enable)
{
	this->tsOut.SetEmm(enable);
}

void CBonCtrl::SetTsBuffMaxCount(DWORD tsBuffMaxCount, int writeBuffMaxCount)
{
	this->tsBuffMaxCount = tsBuffMaxCount;
	this->writeBuffMaxCount = writeBuffMaxCount;
}

//BonDriver�t�H���_��BonDriver_*.dll���
//�߂�l�F
// �G���[�R�[�h
//�����F
// bonList			[OUT]�����ł���BonDriver�ꗗ
DWORD CBonCtrl::EnumBonDriver(
	vector<wstring>* bonList
)
{
	return this->bonUtil.EnumBonDriver(bonList);
}

//BonDriver�����[�h���ă`�����l�����Ȃǂ��擾�i�t�@�C�����Ŏw��j
//�߂�l�F
// �G���[�R�[�h
//�����F
// bonDriverFile	[IN]EnumBonDriver�Ŏ擾���ꂽBonDriver�̃t�@�C����
DWORD CBonCtrl::OpenBonDriver(
	LPCWSTR bonDriverFile,
	int openWait
)
{
	_CloseBonDriver();
	DWORD ret = this->bonUtil.OpenBonDriver(bonDriverFile, openWait);
	wstring bonFile = this->bonUtil.GetOpenBonDriverFileName();
	if( ret == NO_ERR ){
		ret = _OpenBonDriver();
		this->tsOut.ResetChChange();

		this->tsOut.SetBonDriver(bonFile);
	}
	if( bonFile.empty() == false ){
		wstring settingPath;
		GetSettingPath(settingPath);
		wstring bonFileTitle;
		GetFileTitle(bonFile, bonFileTitle);
		wstring tunerName = this->bonUtil.GetTunerName();
		CheckFileName(tunerName);
		this->chUtil.LoadChSet( settingPath + L"\\" + bonFileTitle + L"(" + tunerName + L").ChSet4.txt", settingPath + L"\\ChSet5.txt" );
	}

	return ret;
}

//���[�h���Ă���BonDriver�̊J��
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::CloseBonDriver()
{
	StopBackgroundEpgCap();
	return _CloseBonDriver();
}

//���[�h����BonDriver�̃t�@�C�������擾����i���[�h�������Ă��邩�̔���j
//�߂�l�F
// TRUE�i�����j�FFALSE�iOpen�Ɏ��s���Ă���j
//�����F
// bonDriverFile		[OUT]BonDriver�̃t�@�C����(NULL��)
BOOL CBonCtrl::GetOpenBonDriver(
	wstring* bonDriverFile
	)
{
	BOOL ret = FALSE;

	wstring strBonDriverFile = this->bonUtil.GetOpenBonDriverFileName();
	if( strBonDriverFile.empty() == false ){
		ret = TRUE;
		if( bonDriverFile != NULL ){
			*bonDriverFile = strBonDriverFile;
		}
	}

	return ret;
}
//���[�h����BonDriver�̏��擾
//Space��Ch�̈ꗗ���擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// spaceMap			[OUT] Space��Ch�̈ꗗ�imap�̃L�[ Space�j
DWORD CBonCtrl::GetOriginalChList(
	map<DWORD, BON_SPACE_INFO>* spaceMap
)
{
	return this->bonUtil.GetOriginalChList(spaceMap);
}

//BonDriver�̃`���[�i�[�����擾
//�߂�l�F
// �`���[�i�[��
wstring CBonCtrl::GetTunerName()
{
	return this->bonUtil.GetTunerName();
}

//�`�����l���ύX
//�߂�l�F
// �G���[�R�[�h
//�����F
// space			[IN]�ύX�`�����l����Space
// ch				[IN]�ύX�`�����l���̕���Ch
DWORD CBonCtrl::SetCh(
	DWORD space,
	DWORD ch
)
{
	if( this->tsOut.IsRec() == TRUE ){
		return ERR_FALSE;
	}

	return _SetCh(space, ch);
}

//�`�����l���ύX
//�߂�l�F
// �G���[�R�[�h
//�����F
// space			[IN]�ύX�`�����l����Space
// ch				[IN]�ύX�`�����l���̕���Ch
// SID			[IN]�ύX�`�����l���̕���service_id
DWORD CBonCtrl::SetCh(
	DWORD space,
	DWORD ch,
	WORD SID
)
{
	if( this->tsOut.IsRec() == TRUE ){
		return ERR_FALSE;
	}

	DWORD ret = ERR_FALSE;
	ret = _SetCh(space, ch);
	this->lastSID = SID;

	return ret;
}

//�`�����l���ύX
//�߂�l�F
// �G���[�R�[�h
//�����F
// ONID			[IN]�ύX�`�����l����orignal_network_id
// TSID			[IN]�ύX�`�����l���̕���transport_stream_id
// SID			[IN]�ύX�`�����l���̕���service_id
DWORD CBonCtrl::SetCh(
	WORD ONID,
	WORD TSID,
	WORD SID
)
{
	if( this->tsOut.IsRec() == TRUE ){
		return ERR_FALSE;
	}

	DWORD space=0;
	DWORD ch=0;

	DWORD ret = ERR_FALSE;
	if( this->chUtil.GetCh( ONID, TSID, space, ch ) == TRUE ){
		ret = _SetCh(space, ch);
		this->lastSID = SID;
	}

	return ret;
}

DWORD CBonCtrl::_SetCh(
	DWORD space,
	DWORD ch,
	BOOL chScan
	)
{
	DWORD spaceNow=0;
	DWORD chNow=0;

	DWORD ret = ERR_FALSE;
	if( this->bonUtil.GetOpenBonDriverFileName().empty() == false ){
		ret = NO_ERR;
		if( this->bonUtil.GetNowCh(&spaceNow, &chNow) == FALSE || space != spaceNow || ch != chNow ){
			this->tsOut.SetChChangeEvent(chScan);
			_OutputDebugString(L"SetCh space %d, ch %d", space, ch);
			ret = this->bonUtil.SetCh(space, ch);

			StartBackgroundEpgCap();
		}else{
			BOOL chChgErr = FALSE;
			if( this->tsOut.IsChChanging(&chChgErr) == TRUE ){
				if( chChgErr == TRUE ){
					//�G���[�̎��͍Đݒ�
					this->tsOut.SetChChangeEvent();
					_OutputDebugString(L"SetCh space %d, ch %d", space, ch);
					ret = this->bonUtil.SetCh(space, ch);

					StartBackgroundEpgCap();
				}
			}else{
				if( chChgErr == TRUE ){
					//�G���[�̎��͍Đݒ�
					this->tsOut.SetChChangeEvent();
					_OutputDebugString(L"SetCh space %d, ch %d", space, ch);
					ret = this->bonUtil.SetCh(space, ch);

					StartBackgroundEpgCap();
				}
			}
		}
	}else{
		OutputDebugString(L"Err GetNowCh");
	}
	return ret;
}

//�`�����l���ύX�����ǂ���
//�߂�l�F
// TRUE�i�ύX���j�AFALSE�i�����j
BOOL CBonCtrl::IsChChanging(BOOL* chChgErr)
{
	return this->tsOut.IsChChanging(chChgErr);
}

//���݂̃X�g���[����ID���擾����
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// ONID		[OUT]originalNetworkID
// TSID		[OUT]transportStreamID
BOOL CBonCtrl::GetStreamID(
	WORD* ONID,
	WORD* TSID
	)
{
	return this->tsOut.GetStreamID(ONID, TSID);
}

//�V�O�i�����x���̎擾
//�߂�l�F
// �V�O�i�����x��
float CBonCtrl::GetSignalLevel()
{
	float ret = this->bonUtil.GetSignalLevel();
	this->tsOut.SetSignalLevel(ret);
	return ret;
}

//BonDriver�����[�h��̏���������
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::_OpenBonDriver()
{
	if( this->recvThread == NULL ){
		//��M�X���b�h�N��
		ResetEvent(this->recvStopEvent);
		this->recvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
		SetThreadPriority( this->recvThread, THREAD_PRIORITY_NORMAL );
		ResumeThread(this->recvThread);
	}
	if( this->analyzeThread == NULL ){
		//��̓X���b�h�N��
		ResetEvent(this->analyzeStopEvent);
		this->analyzeThread = (HANDLE)_beginthreadex(NULL, 0, AnalyzeThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
		SetThreadPriority( this->analyzeThread, THREAD_PRIORITY_NORMAL );
		ResumeThread(this->analyzeThread);
	}

	return NO_ERR;
}

//���[�h���Ă���BonDriver�̊J���{��
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::_CloseBonDriver()
{
	if( this->epgCapBackThread != NULL ){
		::SetEvent(this->epgCapBackStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapBackThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapBackThread, 0xffffffff);
		}
		CloseHandle(this->epgCapBackThread);
		this->epgCapBackThread = NULL;
	}

	if( this->epgCapThread != NULL ){
		::SetEvent(this->epgCapStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapThread, 0xffffffff);
		}
		CloseHandle(this->epgCapThread);
		this->epgCapThread = NULL;
	}

	if( this->recvThread != NULL ){
		::SetEvent(this->recvStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->recvThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->recvThread, 0xffffffff);
		}
		CloseHandle(this->recvThread);
		this->recvThread = NULL;
	}
	if( this->analyzeThread != NULL ){
		::SetEvent(this->analyzeStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->analyzeThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->analyzeThread, 0xffffffff);
		}
		CloseHandle(this->analyzeThread);
		this->analyzeThread = NULL;
	}

	this->TSBuff.clear();
	this->TSBuffOffset = 0;

	return this->bonUtil.CloseBonDriver();
}

UINT WINAPI CBonCtrl::RecvThread(LPVOID param)
{
	CBonCtrl* sys = (CBonCtrl*)param;
	CPacketInit packetInit;
	while(1){
		if( ::WaitForSingleObject(sys->recvStopEvent, 0) != WAIT_TIMEOUT ){
			//�L�����Z�����ꂽ
			break;
		}
		BYTE *data = NULL;
		DWORD size = 0;
		DWORD remain = 0;
		{
			if( sys->bonUtil.GetTsStream(&data,&size,&remain) == TRUE ){
				if( size != 0 && data != NULL){
					BYTE* outData;
					DWORD outSize;
					if( packetInit.GetTSData(data, size, &outData, &outSize) == TRUE ){
						CBlockLock lock(&sys->buffLock);
						sys->TSBuff.erase(sys->TSBuff.begin(), sys->TSBuff.begin() + sys->TSBuffOffset);
						sys->TSBuffOffset = 0;
						if( sys->TSBuff.size() / 48128 > sys->tsBuffMaxCount ){
							sys->TSBuff.clear();
						}
						sys->TSBuff.insert(sys->TSBuff.end(), outData, outData + outSize);
					}
				}else{
					Sleep(10);
				}
			}else{
				Sleep(10);
			}
		}
	}
	return 0;
}

UINT WINAPI CBonCtrl::AnalyzeThread(LPVOID param)
{
	CBonCtrl* sys = (CBonCtrl*)param;

	while(1){
		if( ::WaitForSingleObject(sys->analyzeStopEvent, 0) != WAIT_TIMEOUT ){
			//�L�����Z�����ꂽ
			break;
		}

		//�o�b�t�@����f�[�^���o��
		BYTE data[48128];
		DWORD dataSize = 0;
		{
			CBlockLock lock(&sys->buffLock);
			if( sys->TSBuff.size() - sys->TSBuffOffset >= sizeof(data) ){
				//�K��188�̔{���Ŏ��o���Ȃ���΂Ȃ�Ȃ�
				dataSize = sizeof(data);
				memcpy(data, &sys->TSBuff[sys->TSBuffOffset], dataSize);
				sys->TSBuffOffset += dataSize;
			}
		}
		if( dataSize != 0 ){
			sys->tsOut.AddTSBuff(data, dataSize);
		}else{
			Sleep(10);
		}
	}
	return 0;
}

//EPG�f�[�^�̒~�Ϗ�Ԃ����Z�b�g����
void CBonCtrl::ClearSectionStatus()
{
	this->tsOut.ClearSectionStatus();
}

//EPG�f�[�^�̒~�Ϗ�Ԃ��擾����
//�߂�l�F
// �X�e�[�^�X
//�����F
// l_eitFlag		[IN]L-EIT�̃X�e�[�^�X���擾
EPG_SECTION_STATUS CBonCtrl::GetSectionStatus(
	BOOL l_eitFlag
	)
{
	return this->tsOut.GetSectionStatus(l_eitFlag);
}

//���X�g���[���̃T�[�r�X�ꗗ���擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// serviceList				[OUT]�T�[�r�X���̃��X�g
DWORD CBonCtrl::GetServiceListActual(
	vector<TS_SERVICE_INFO>* serviceList
	)
{
	DWORD _serviceListSize = 0;
	SERVICE_INFO* _serviceList = NULL;
	DWORD err = this->tsOut.GetServiceListActual(&_serviceListSize, &_serviceList);
	if( err == NO_ERR ){
		for( DWORD i=0; i<_serviceListSize; i++ ){
			TS_SERVICE_INFO item;
			item.ONID = _serviceList[i].original_network_id;
			item.TSID = _serviceList[i].transport_stream_id;
			item.SID = _serviceList[i].service_id;
			if( _serviceList[i].extInfo != NULL ){
				item.serviceType = _serviceList[i].extInfo->service_type;
				item.partialFlag = _serviceList[i].extInfo->partialReceptionFlag;
				if( _serviceList[i].extInfo->service_name != NULL ){
					item.serviceName = _serviceList[i].extInfo->service_name;
				}else{
					item.serviceName = L"";
				}
				if( _serviceList[i].extInfo->network_name != NULL ){
					item.networkName = _serviceList[i].extInfo->network_name;
				}else{
					item.networkName = L"";
				}
				item.remoteControlKeyID = _serviceList[i].extInfo->remote_control_key_id;
			}else{
				item.serviceType = 0;
				item.partialFlag = 0;
				item.serviceName = L"";
				item.networkName = L"";
				item.remoteControlKeyID = 0;
			}
			serviceList->push_back(item);
		}
	}

	return err;
}

//�T�[�r�X�ꗗ���擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// serviceList				[OUT]�T�[�r�X���̃��X�g
DWORD CBonCtrl::GetServiceList(
	vector<CH_DATA4>* serviceList
	)
{
	return this->chUtil.GetEnumService(serviceList);
}

//TS�X�g���[������p�R���g���[�����쐬����
//�߂�l�F
// �G���[�R�[�h
//�����F
// id			[OUT]���䎯��ID
BOOL CBonCtrl::CreateServiceCtrl(
	DWORD* id
	)
{
	return this->tsOut.CreateServiceCtrl(id);
}

//TS�X�g���[������p�R���g���[�����쐬����
//�߂�l�F
// �G���[�R�[�h
//�����F
// id			[IN]���䎯��ID
BOOL CBonCtrl::DeleteServiceCtrl(
	DWORD id
	)
{
	return this->tsOut.DeleteServiceCtrl(id);
}

//����Ώۂ̃T�[�r�X��ݒ肷��
BOOL CBonCtrl::SetServiceID(
	DWORD id,
	WORD serviceID
	)
{
	return this->tsOut.SetServiceID(id,serviceID);
}

BOOL CBonCtrl::GetServiceID(
	DWORD id,
	WORD* serviceID
	)
{
	return this->tsOut.GetServiceID(id,serviceID);
}

//UDP�ő��M���s��
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// id			[IN]���䎯��ID
// sendList		[IN/OUT]���M�惊�X�g�BNULL�Œ�~�BPort�͎��ۂɑ��M�Ɏg�p����Port���Ԃ�B�B
BOOL CBonCtrl::SendUdp(
	DWORD id,
	vector<NW_SEND_INFO>* sendList
	)
{
	return this->tsOut.SendUdp(id,sendList);
}

//TCP�ő��M���s��
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// id			[IN]���䎯��ID
// sendList		[IN/OUT]���M�惊�X�g�BNULL�Œ�~�BPort�͎��ۂɑ��M�Ɏg�p����Port���Ԃ�B
BOOL CBonCtrl::SendTcp(
	DWORD id,
	vector<NW_SEND_INFO>* sendList
	)
{
	return this->tsOut.SendTcp(id,sendList);
}

//�t�@�C���ۑ����J�n����
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// id					[IN]���䎯��ID
// fileName				[IN]�ۑ��t�@�C���p�X
// overWriteFlag		[IN]����t�@�C�������ݎ��ɏ㏑�����邩�ǂ����iTRUE�F����AFALSE�F���Ȃ��j
// pittariFlag			[IN]�҂����胂�[�h�iTRUE�F����AFALSE�F���Ȃ��j
// pittariONID			[IN]�҂����胂�[�h�Ř^�悷��ONID
// pittariTSID			[IN]�҂����胂�[�h�Ř^�悷��TSID
// pittariSID			[IN]�҂����胂�[�h�Ř^�悷��SID
// pittariEventID		[IN]�҂����胂�[�h�Ř^�悷��C�x���gID
// createSize			[IN]�t�@�C���쐬���Ƀf�B�X�N�ɗ\�񂷂�e��
// saveFolder			[IN]�g�p����t�H���_�ꗗ
// saveFolderSub		[IN]HDD�̋󂫂��Ȃ��Ȃ����ꍇ�Ɉꎞ�I�Ɏg�p����t�H���_
BOOL CBonCtrl::StartSave(
	DWORD id,
	wstring fileName,
	BOOL overWriteFlag,
	BOOL pittariFlag,
	WORD pittariONID,
	WORD pittariTSID,
	WORD pittariSID,
	WORD pittariEventID,
	ULONGLONG createSize,
	vector<REC_FILE_SET_INFO>* saveFolder,
	vector<wstring>* saveFolderSub
)
{
	BOOL ret = this->tsOut.StartSave(id, fileName, overWriteFlag, pittariFlag, pittariONID, pittariTSID, pittariSID, pittariEventID, createSize, saveFolder, saveFolderSub, writeBuffMaxCount);

	StartBackgroundEpgCap();

	return ret;
}

//�t�@�C���ۑ����I������
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// id			[IN]���䎯��ID
BOOL CBonCtrl::EndSave(
	DWORD id
	)
{
	return this->tsOut.EndSave(id);
}

//�X�N�����u�����������̓���ݒ�
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// enable		[IN] TRUE�i��������j�AFALSE�i�������Ȃ��j
BOOL CBonCtrl::SetScramble(
	DWORD id,
	BOOL enable
	)
{
	return this->tsOut.SetScramble(id, enable);
}

//�����ƃf�[�^�����܂߂邩�ǂ���
//�����F
// id					[IN]���䎯��ID
// enableCaption		[IN]������ TRUE�i�܂߂�j�AFALSE�i�܂߂Ȃ��j
// enableData			[IN]�f�[�^������ TRUE�i�܂߂�j�AFALSE�i�܂߂Ȃ��j
void CBonCtrl::SetServiceMode(
	DWORD id,
	BOOL enableCaption,
	BOOL enableData
	)
{
	this->tsOut.SetServiceMode(id, enableCaption, enableData);
}

//�G���[�J�E���g���N���A����
void CBonCtrl::ClearErrCount(
	DWORD id
	)
{
	this->tsOut.ClearErrCount(id);
}

//�h���b�v�ƃX�N�����u���̃J�E���g���擾����
//�����F
// drop				[OUT]�h���b�v��
// scramble			[OUT]�X�N�����u����
void CBonCtrl::GetErrCount(
	DWORD id,
	ULONGLONG* drop,
	ULONGLONG* scramble
	)
{
	this->tsOut.GetErrCount(id, drop, scramble);
}

//�w��T�[�r�X�̌���or����EPG�����擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// originalNetworkID		[IN]�擾�Ώۂ�originalNetworkID
// transportStreamID		[IN]�擾�Ώۂ�transportStreamID
// serviceID				[IN]�擾�Ώۂ�ServiceID
// nextFlag					[IN]TRUE�i���̔ԑg�j�AFALSE�i���݂̔ԑg�j
// epgInfo					[OUT]EPG���iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
DWORD CBonCtrl::GetEpgInfo(
	WORD originalNetworkID,
	WORD transportStreamID,
	WORD serviceID,
	BOOL nextFlag,
	EPGDB_EVENT_INFO* epgInfo
	)
{
	return this->tsOut.GetEpgInfo(originalNetworkID, transportStreamID, serviceID, nextFlag, epgInfo);
}

//�w��C�x���g��EPG�����擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// originalNetworkID		[IN]�擾�Ώۂ�originalNetworkID
// transportStreamID		[IN]�擾�Ώۂ�transportStreamID
// serviceID				[IN]�擾�Ώۂ�ServiceID
// eventID					[IN]�擾�Ώۂ�EventID
// pfOnlyFlag				[IN]p/f����̂݌������邩�ǂ���
// epgInfo					[OUT]EPG���iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
DWORD CBonCtrl::SearchEpgInfo(
	WORD originalNetworkID,
	WORD transportStreamID,
	WORD serviceID,
	WORD eventID,
	BYTE pfOnlyFlag,
	EPGDB_EVENT_INFO* epgInfo
	)
{
	return this->tsOut.SearchEpgInfo(originalNetworkID, transportStreamID, serviceID, eventID, pfOnlyFlag, epgInfo);
}

//PC���v�����Ƃ����X�g���[�����ԂƂ̍����擾����
//�߂�l�F
// ���̕b��
int CBonCtrl::GetTimeDelay(
	)
{
	return this->tsOut.GetTimeDelay();
}

//�^�撆���ǂ������擾����
// TRUE�i�^�撆�j�AFALSE�i�^�悵�Ă��Ȃ��j
BOOL CBonCtrl::IsRec()
{
	return this->tsOut.IsRec();
}
//�`�����l���X�L�������J�n����
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::StartChScan()
{
	if( this->tsOut.IsRec() == TRUE ){
		return ERR_FALSE;
	}
	if( this->epgSt_err == ST_WORKING ){
		return ERR_FALSE;
	}

	if( ::WaitForSingleObject(this->chScanThread, 0) == WAIT_OBJECT_0 ){
		CloseHandle(this->chScanThread);
		this->chScanThread = NULL;
	}

	DWORD ret = ERR_FALSE;
	if( this->chScanThread == NULL ){
		if( this->bonUtil.GetOpenBonDriverFileName().empty() == false ){
			ret = NO_ERR;
			this->chSt_space = 0;
			this->chSt_ch = 0;
			this->chSt_chName = L"";
			this->chSt_chkNum = 0;
			this->chSt_totalNum = 0;
			this->chSt_err = ST_WORKING;

			//��M�X���b�h�N��
			ResetEvent(this->chScanStopEvent);
			this->chScanThread = (HANDLE)_beginthreadex(NULL, 0, ChScanThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
			SetThreadPriority( this->chScanThread, THREAD_PRIORITY_NORMAL );
			ResumeThread(this->chScanThread);
		}
	}

	return ret;
}

//�`�����l���X�L�������L�����Z������
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::StopChScan()
{
	if( this->chScanThread != NULL ){
		::SetEvent(this->chScanStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->chScanThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->chScanThread, 0xffffffff);
		}
		CloseHandle(this->chScanThread);
		this->chScanThread = NULL;
	}

	return NO_ERR;
}

//�`�����l���X�L�����̏�Ԃ��擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// space		[OUT]�X�L�������̕���CH��space
// ch			[OUT]�X�L�������̕���CH��ch
// chName		[OUT]�X�L�������̕���CH�̖��O
// chkNum		[OUT]�`�F�b�N�ς݂̐�
// totalNum		[OUT]�`�F�b�N�Ώۂ̑���
DWORD CBonCtrl::GetChScanStatus(
	DWORD* space,
	DWORD* ch,
	wstring* chName,
	DWORD* chkNum,
	DWORD* totalNum
	)
{
	if( space != NULL ){
		*space = this->chSt_space;
	}
	if( ch != NULL ){
		*ch = this->chSt_ch;
	}
	if( chName != NULL ){
		*chName = this->chSt_chName;
	}
	if( chkNum != NULL ){
		*chkNum = this->chSt_chkNum;
	}
	if( totalNum != NULL ){
		*totalNum = this->chSt_totalNum;
	}
	return this->chSt_err;
}

UINT WINAPI CBonCtrl::ChScanThread(LPVOID param)
{
	CBonCtrl* sys = (CBonCtrl*)param;

	//TODO: chUtil��const�ɕۂ��Ă��Ȃ��̂ŃX���b�h���S���͔j�]���Ă���B�X�L�����������̖��Ȃ̂ŏC���͂��Ȃ����v����
	sys->chUtil.Clear();

	wstring settingPath;
	GetSettingPath(settingPath);
	wstring bonFileTitle;
	GetFileTitle(sys->bonUtil.GetOpenBonDriverFileName(), bonFileTitle);
	wstring tunerName = sys->bonUtil.GetTunerName();
	CheckFileName(tunerName);

	wstring chSet4 = settingPath + L"\\" + bonFileTitle + L"(" + tunerName + L").ChSet4.txt";
	wstring chSet5 = settingPath + L"\\ChSet5.txt";

	vector<CHK_CH_INFO> chkList;
	map<DWORD, BON_SPACE_INFO> spaceMap;
	if( sys->bonUtil.GetOriginalChList(&spaceMap) != NO_ERR ){
		sys->chSt_err = ST_COMPLETE;
		sys->chUtil.SaveChSet(chSet4, chSet5);
		return 0;
	}
	map<DWORD, BON_SPACE_INFO>::iterator itrSpace;
	for( itrSpace = spaceMap.begin(); itrSpace != spaceMap.end(); itrSpace++ ){
		sys->chSt_totalNum += (DWORD)itrSpace->second.chMap.size();
		map<DWORD, wstring>::iterator itrCh;
		for( itrCh = itrSpace->second.chMap.begin(); itrCh != itrSpace->second.chMap.end(); itrCh++ ){
			CHK_CH_INFO item;
			item.space = itrSpace->first;
			item.spaceName = itrSpace->second.spaceName;
			item.ch = itrCh->first;
			item.chName = itrCh->second;
			chkList.push_back(item);
		}
	}

	if( sys->chSt_totalNum == 0 ){
		sys->chSt_err = ST_COMPLETE;
		sys->chUtil.SaveChSet(chSet4, chSet5);
		return 0;
	}

	wstring folderPath;
	GetModuleFolderPath( folderPath );
	wstring iniPath = folderPath;
	iniPath += L"\\BonCtrl.ini";

	DWORD chChgTimeOut = GetPrivateProfileInt(L"CHSCAN", L"ChChgTimeOut", 9, iniPath.c_str());
	DWORD serviceChkTimeOut = GetPrivateProfileInt(L"CHSCAN", L"ServiceChkTimeOut", 8, iniPath.c_str());


	DWORD wait = 0;
	BOOL chkNext = TRUE;
	DWORD startTime = 0;
	DWORD chkWait = 0;
	DWORD chkCount = 0;
	BOOL firstChg = FALSE;

	while(1){
		if( ::WaitForSingleObject(sys->chScanStopEvent, wait) != WAIT_TIMEOUT ){
			//�L�����Z�����ꂽ
			sys->chSt_err = ST_CANCEL;
			break;
		}
		if( chkNext == TRUE ){
			sys->chSt_space = chkList[chkCount].space;
			sys->chSt_ch = chkList[chkCount].ch;
			sys->chSt_chName = chkList[chkCount].chName;
			sys->_SetCh(chkList[chkCount].space, chkList[chkCount].ch, TRUE);
			if( firstChg == FALSE ){
				firstChg = TRUE;
				sys->tsOut.ResetChChange();
			}
			startTime = GetTickCount();
			chkNext = FALSE;
			wait = 1000;
			chkWait = chChgTimeOut;
		}else{
			BOOL chChgErr = FALSE;
			if( sys->tsOut.IsChChanging(&chChgErr) == TRUE ){
				if( GetTickCount() - startTime > chkWait * 1000 ){
					//�`�����l���؂�ւ���8�b�ȏォ�����Ă�̂Ŗ��M���Ɣ��f
					OutputDebugString(L"��AutoScan Ch Change timeout\r\n");
					chkNext = TRUE;
				}
			}else{
				if( GetTickCount() - startTime > (chkWait + serviceChkTimeOut) * 1000 || chChgErr == TRUE){
					//�`�����l���؂�ւ������������ǃT�[�r�X�ꗗ�Ƃ�Ȃ��̂Ŗ��M���Ɣ��f
					OutputDebugString(L"��AutoScan GetService timeout\r\n");
					chkNext = TRUE;
				}else{
					//�T�[�r�X�ꗗ�̎擾���s��
					DWORD serviceListSize;
					SERVICE_INFO* serviceList;
					if( sys->tsOut.GetServiceListActual(&serviceListSize, &serviceList) == NO_ERR ){
						if( serviceListSize > 0 ){
							//�ꗗ�̎擾���ł���
							for( DWORD i=0 ;i<serviceListSize; i++ ){
								if( serviceList[i].extInfo != NULL ){
									if( serviceList[i].extInfo->service_name != NULL ){
										if( wcslen(serviceList[i].extInfo->service_name) > 0 ){
											sys->chUtil.AddServiceInfo(chkList[chkCount].space, chkList[chkCount].ch, chkList[chkCount].chName, &(serviceList[i]));
										}
									}
								}
							}
							chkNext = TRUE;
						}
					}
				}
			}
			if( chkNext == TRUE ){
				//���̃`�����l����
				chkCount++;
				sys->chSt_chkNum++;
				if( sys->chSt_totalNum <= chkCount ){
					//�S���`�F�b�N�I������̂ŏI��
					sys->chSt_err = ST_COMPLETE;
					sys->chUtil.SaveChSet(chSet4, chSet5);
					break;
				}
			}
		}
	}

	sys->chUtil.LoadChSet(chSet4, chSet5);

	return 0;
}

//EPG�擾�Ώۂ̃T�[�r�X�ꗗ���擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// chList		[OUT]EPG�擾����`�����l���ꗗ
DWORD CBonCtrl::GetEpgCapService(
	vector<EPGCAP_SERVICE_INFO>* chList
	)
{
	this->chUtil.GetEpgCapService(chList);
	return NO_ERR;
}

//EPG�擾���J�n����
//�߂�l�F
// �G���[�R�[�h
//�����F
// chList		[IN]EPG�擾����`�����l���ꗗ
DWORD CBonCtrl::StartEpgCap(
	vector<EPGCAP_SERVICE_INFO>* chList
	)
{
	if( this->tsOut.IsRec() == TRUE ){
		return ERR_FALSE;
	}
	if( this->chSt_err == ST_WORKING ){
		return ERR_FALSE;
	}

	StopBackgroundEpgCap();

	if( ::WaitForSingleObject(this->epgCapThread, 0) == WAIT_OBJECT_0 ){
		CloseHandle(this->epgCapThread);
		this->epgCapThread = NULL;
	}

	DWORD ret = ERR_FALSE;
	if( this->epgCapThread == NULL ){
		if( this->bonUtil.GetOpenBonDriverFileName().empty() == false ){
			ret = NO_ERR;
			this->epgCapChList = *chList;
			this->epgSt_err = ST_WORKING;
			this->epgSt_ch.ONID = 0xFFFF;
			this->epgSt_ch.TSID = 0xFFFF;
			this->epgSt_ch.SID = 0xFFFF;

			//��M�X���b�h�N��
			ResetEvent(this->epgCapStopEvent);
			this->epgCapThread = (HANDLE)_beginthreadex(NULL, 0, EpgCapThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
			SetThreadPriority( this->epgCapThread, THREAD_PRIORITY_NORMAL );
			ResumeThread(this->epgCapThread);
		}
	}

	return ret;
}

//EPG�擾���~����
//�߂�l�F
// �G���[�R�[�h
DWORD CBonCtrl::StopEpgCap(
	)
{
	if( this->epgCapThread != NULL ){
		::SetEvent(this->epgCapStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapThread, 0xffffffff);
		}
		CloseHandle(this->epgCapThread);
		this->epgCapThread = NULL;
	}

	return NO_ERR;
}

//EPG�擾�̃X�e�[�^�X���擾����
//�߂�l�F
// �G���[�R�[�h
//�����F
// info			[OUT]�擾���̃T�[�r�X
DWORD CBonCtrl::GetEpgCapStatus(
	EPGCAP_SERVICE_INFO* info
	)
{
	if( info != NULL ){
		*info = this->epgSt_ch;
	}
	return this->epgSt_err;
}

UINT WINAPI CBonCtrl::EpgCapThread(LPVOID param)
{
	CBonCtrl* sys = (CBonCtrl*)param;

	BOOL chkNext = TRUE;
	BOOL startCap = FALSE;
	DWORD wait = 0;
	DWORD startTime = 0;
	DWORD chkCount = 0;
	DWORD chkWait = 8;

	BOOL chkBS = FALSE;
	BOOL chkCS1 = FALSE;
	BOOL chkCS2 = FALSE;

	wstring folderPath;
	GetModuleFolderPath( folderPath );
	wstring iniPath = folderPath;
	iniPath += L"\\BonCtrl.ini";

	DWORD timeOut = GetPrivateProfileInt(L"EPGCAP", L"EpgCapTimeOut", 15, iniPath.c_str());
	BOOL saveTimeOut = GetPrivateProfileInt(L"EPGCAP", L"EpgCapSaveTimeOut", 0, iniPath.c_str());

	//Common.ini�͈�ʂɊO���v���Z�X���ύX����\���̂���(�͂���)���̂Ȃ̂ŁA���p�̒��O�Ƀ`�F�b�N����
	wstring commonIniPath;
	GetCommonIniPath(commonIniPath);
	BOOL BSBasic = GetPrivateProfileInt(L"SET", L"BSBasicOnly", 1, commonIniPath.c_str());
	BOOL CS1Basic = GetPrivateProfileInt(L"SET", L"CS1BasicOnly", 1, commonIniPath.c_str());
	BOOL CS2Basic = GetPrivateProfileInt(L"SET", L"CS2BasicOnly", 1, commonIniPath.c_str());

	while(1){
		if( ::WaitForSingleObject(sys->epgCapStopEvent, wait) != WAIT_TIMEOUT ){
			//�L�����Z�����ꂽ
			sys->epgSt_err = ST_CANCEL;
			sys->tsOut.StopSaveEPG(FALSE);
			break;
		}
		if( chkNext == TRUE ){
			if( sys->tsOut.IsChChanging(NULL) == TRUE ){
				Sleep(200);
				continue;
			}
			DWORD space = 0;
			DWORD ch = 0;
			sys->chUtil.GetCh(sys->epgCapChList[chkCount].ONID, sys->epgCapChList[chkCount].TSID, space, ch);
			sys->_SetCh(space, ch);
			startTime = GetTickCount();
			chkNext = FALSE;
			startCap = FALSE;
			wait = 1000;
			if( sys->epgCapChList[chkCount].ONID == 4 ){
				chkBS = TRUE;
			}else if( sys->epgCapChList[chkCount].ONID == 6 ){
				chkCS1 = TRUE;
			}else if( sys->epgCapChList[chkCount].ONID == 7 ){
				chkCS2 = TRUE;
			}
			sys->epgSt_ch = sys->epgCapChList[chkCount];
		}else{
			BOOL chChgErr = FALSE;
			if( sys->tsOut.IsChChanging(&chChgErr) == TRUE ){
				if( GetTickCount() - startTime > chkWait * 1000 ){
					//�`�����l���؂�ւ���10�b�ȏォ�����Ă�̂Ŗ��M���Ɣ��f
					chkNext = TRUE;
				}
			}else{
				if( GetTickCount() - startTime > (chkWait + timeOut * 60) * 1000 || chChgErr == TRUE){
					//15���ȏォ�����Ă���Ȃ��~
					sys->tsOut.StopSaveEPG(saveTimeOut);
					chkNext = TRUE;
					wait = 0;
					_OutputDebugString(L"++%d����EPG�擾�������� or Ch�ύX�ŃG���[", timeOut);
				}else if( GetTickCount() - startTime > chkWait * 1000 ){
					//�؂�ւ�����15�b�ȏ�߂��Ă���̂Ŏ擾����
					if( startCap == FALSE ){
						//�擾�J�n
						startCap = TRUE;
						wstring epgDataPath = L"";
						sys->GetEpgDataFilePath(sys->epgCapChList[chkCount].ONID, sys->epgCapChList[chkCount].TSID, epgDataPath, BSBasic, CS1Basic, CS2Basic);
						sys->tsOut.StartSaveEPG(epgDataPath);
						sys->tsOut.ClearSectionStatus();
						wait = 60*1000;
					}else{
						//�~�Ϗ�ԃ`�F�b�N
						BOOL leitFlag = sys->chUtil.IsPartial(sys->epgCapChList[chkCount].ONID, sys->epgCapChList[chkCount].TSID, sys->epgCapChList[chkCount].SID);
						EPG_SECTION_STATUS status = sys->tsOut.GetSectionStatus(leitFlag);
						if( sys->epgCapChList[chkCount].ONID == 4 && BSBasic == TRUE ){
							if( status == EpgBasicAll || status == EpgHEITAll ){
								chkNext = TRUE;
							}
						}else if( sys->epgCapChList[chkCount].ONID == 6 && CS1Basic == TRUE ){
							if( status == EpgBasicAll || status == EpgHEITAll ){
								chkNext = TRUE;
							}
						}else if( sys->epgCapChList[chkCount].ONID == 7 && CS2Basic == TRUE ){
							if( status == EpgBasicAll || status == EpgHEITAll ){
								chkNext = TRUE;
							}
						}else{
							if( leitFlag == FALSE && status == EpgHEITAll ){
								chkNext = TRUE;
							}else if( leitFlag == TRUE && status == EpgLEITAll ){
								chkNext = TRUE;
							}
						}
						if( chkNext == TRUE ){
							sys->tsOut.StopSaveEPG(TRUE);
							wait = 0;
						}else{
							wait = 10*1000;
						}
					}
				}
			}
			if( chkNext == TRUE ){
				//���̃`�����l����
				chkCount++;
				if( sys->epgCapChList.size() <= chkCount ){
					//�S���`�F�b�N�I������̂ŏI��
					sys->epgSt_err = ST_COMPLETE;
					return 0;
				}
				//BS 1�`�����l���̂݁H
				if( sys->epgCapChList[chkCount].ONID == 4 && BSBasic == TRUE && chkBS == TRUE){
					while(chkCount<(DWORD)sys->epgCapChList.size()){
						if( sys->epgCapChList[chkCount].ONID != 4 ){
							break;
						}
						chkCount++;
						if( sys->epgCapChList.size() <= chkCount ){
							//�S���`�F�b�N�I������̂ŏI��
							sys->epgSt_err = ST_COMPLETE;
							return 0;
						}
					}
				}
				//CS1 1�`�����l���̂݁H
				if( sys->epgCapChList[chkCount].ONID == 6 && CS1Basic == TRUE && chkCS1 == TRUE ){
					while(chkCount<(DWORD)sys->epgCapChList.size()){
						if( sys->epgCapChList[chkCount].ONID != 6 ){
							break;
						}
						chkCount++;
						if( sys->epgCapChList.size() <= chkCount ){
							//�S���`�F�b�N�I������̂ŏI��
							sys->epgSt_err = ST_COMPLETE;
							return 0;
						}
					}
				}
				//CS2 1�`�����l���̂݁H
				if( sys->epgCapChList[chkCount].ONID == 7 && CS2Basic == TRUE && chkCS2 == TRUE ){
					while(chkCount<(DWORD)sys->epgCapChList.size()){
						if( sys->epgCapChList[chkCount].ONID != 7 ){
							break;
						}
						chkCount++;
						if( sys->epgCapChList.size() <= chkCount ){
							//�S���`�F�b�N�I������̂ŏI��
							sys->epgSt_err = ST_COMPLETE;
							return 0;
						}
					}
				}
			}
		}
	}
	return 0;
}

void CBonCtrl::GetEpgDataFilePath(WORD ONID, WORD TSID, wstring& epgDataFilePath, BOOL BSBasic, BOOL CS1Basic, BOOL CS2Basic)
{
	wstring epgDataFolderPath = L"";
	GetSettingPath(epgDataFolderPath);
	epgDataFolderPath += EPG_SAVE_FOLDER;

	if( ONID == 4 && BSBasic == TRUE ){
		Format(epgDataFilePath, L"%s\\%04XFFFF_epg.dat", epgDataFolderPath.c_str(), ONID);
	}else if( ONID == 6 && CS1Basic == TRUE ){
		Format(epgDataFilePath, L"%s\\%04XFFFF_epg.dat", epgDataFolderPath.c_str(), ONID);
	}else if( ONID == 7 && CS2Basic == TRUE ){
		Format(epgDataFilePath, L"%s\\%04XFFFF_epg.dat", epgDataFolderPath.c_str(), ONID);
	}else{
		Format(epgDataFilePath, L"%s\\%04X%04X_epg.dat", epgDataFolderPath.c_str(), ONID, TSID);
	}
}

//�^�撆�̃t�@�C���̃t�@�C���p�X���擾����
//�����F
// id					[IN]���䎯��ID
// filePath				[OUT]�ۑ��t�@�C����
// subRecFlag			[OUT]�T�u�^�悪�����������ǂ���
void CBonCtrl::GetSaveFilePath(
	DWORD id,
	wstring* filePath,
	BOOL* subRecFlag
	)
{
	this->tsOut.GetSaveFilePath(id, filePath, subRecFlag);
}

//�h���b�v�ƃX�N�����u���̃J�E���g��ۑ�����
//�����F
// id					[IN]���䎯��ID
// filePath				[IN]�ۑ��t�@�C����
void CBonCtrl::SaveErrCount(
	DWORD id,
	wstring filePath
	)
{
	this->tsOut.SaveErrCount(id, filePath);
}

//�^�撆�̃t�@�C���̏o�̓T�C�Y���擾����
//�����F
// id					[IN]���䎯��ID
// writeSize			[OUT]�ۑ��t�@�C����
void CBonCtrl::GetRecWriteSize(
	DWORD id,
	__int64* writeSize
	)
{
	this->tsOut.GetRecWriteSize(id, writeSize);
}

//�o�b�N�O���E���h�ł�EPG�擾�ݒ�
//�����F
// enableLive	[IN]�������Ɏ擾����
// enableRec	[IN]�^�撆�Ɏ擾����
// enableRec	[IN]EPG�擾����`�����l���ꗗ
// BSBasic		[IN]BS�łP�`�����l�������{���̂ݎ擾���邩�ǂ���
// CS1Basic		[IN]CS1�łP�`�����l�������{���̂ݎ擾���邩�ǂ���
// CS2Basic		[IN]CS2�łP�`�����l�������{���̂ݎ擾���邩�ǂ���
// backStartWaitSec	[IN]Ch�؂�ւ��A�^��J�n��A�o�b�N�O���E���h�ł�EPG�擾���J�n����܂ł̕b��
void CBonCtrl::SetBackGroundEpgCap(
	BOOL enableLive,
	BOOL enableRec,
	BOOL BSBasic,
	BOOL CS1Basic,
	BOOL CS2Basic,
	DWORD backStartWaitSec
	)
{
	this->enableLiveEpgCap = enableLive;
	this->enableRecEpgCap = enableRec;
	this->epgCapBackBSBasic = BSBasic;
	this->epgCapBackCS1Basic = CS1Basic;
	this->epgCapBackCS2Basic = CS2Basic;
	this->epgCapBackStartWaitSec = backStartWaitSec;

	StartBackgroundEpgCap();
}

void CBonCtrl::StartBackgroundEpgCap()
{
	StopBackgroundEpgCap();
	if( this->epgCapBackThread == NULL && this->epgCapThread == NULL && this->chScanThread == NULL ){
		if( this->bonUtil.GetOpenBonDriverFileName().empty() == false ){
			//��M�X���b�h�N��
			ResetEvent(this->epgCapBackStopEvent);
			this->epgCapBackThread = (HANDLE)_beginthreadex(NULL, 0, EpgCapBackThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
			SetThreadPriority( this->epgCapBackThread, THREAD_PRIORITY_NORMAL );
			ResumeThread(this->epgCapBackThread);
		}
	}
}

void CBonCtrl::StopBackgroundEpgCap()
{
	if( this->epgCapBackThread != NULL ){
		::SetEvent(this->epgCapBackStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->epgCapBackThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(this->epgCapBackThread, 0xffffffff);
		}
		CloseHandle(this->epgCapBackThread);
		this->epgCapBackThread = NULL;
	}
}

UINT WINAPI CBonCtrl::EpgCapBackThread(LPVOID param)
{
	wstring folderPath;
	GetModuleFolderPath( folderPath );
	wstring iniPath = folderPath;
	iniPath += L"\\BonCtrl.ini";

	DWORD timeOut = GetPrivateProfileInt(L"EPGCAP", L"EpgCapTimeOut", 15, iniPath.c_str());
	BOOL saveTimeOut = GetPrivateProfileInt(L"EPGCAP", L"EpgCapSaveTimeOut", 0, iniPath.c_str());

	CBonCtrl* sys = (CBonCtrl*)param;
	if( ::WaitForSingleObject(sys->epgCapBackStopEvent, sys->epgCapBackStartWaitSec*1000) != WAIT_TIMEOUT ){
		//�L�����Z�����ꂽ
		return 0;
	}

	if( sys->tsOut.IsRec() == TRUE ){
		if( sys->enableRecEpgCap == FALSE ){
			return 0;
		}
	}else{
		if( sys->enableLiveEpgCap == FALSE ){
			return 0;
		}
	}

	DWORD startTime = GetTickCount();

	wstring epgDataPath = L"";
	WORD ONID;
	WORD TSID;
	sys->tsOut.GetStreamID(&ONID, &TSID);

	if( sys->chUtil.IsEpgCapService(ONID, TSID) == FALSE ){
		return 0;
	}

	sys->GetEpgDataFilePath(ONID, TSID, epgDataPath, sys->epgCapBackBSBasic, sys->epgCapBackCS1Basic, sys->epgCapBackCS2Basic);
	sys->tsOut.StartSaveEPG(epgDataPath);
	sys->tsOut.ClearSectionStatus();

	if( ::WaitForSingleObject(sys->epgCapBackStopEvent, 60*1000) != WAIT_TIMEOUT ){
		//�L�����Z�����ꂽ
		sys->tsOut.StopSaveEPG(FALSE);
		return 0;
	}
	while(1){
		//�~�Ϗ�ԃ`�F�b�N
		BOOL chkNext = FALSE;
		BOOL leitFlag = sys->chUtil.IsPartial(ONID, TSID, sys->lastSID);
		EPG_SECTION_STATUS status = sys->tsOut.GetSectionStatus(leitFlag);
		if( ONID == 4 && sys->epgCapBackBSBasic == TRUE ){
			if( status == EpgBasicAll || status == EpgHEITAll ){
				chkNext = TRUE;
			}
		}else if( ONID == 6 && sys->epgCapBackCS1Basic == TRUE ){
			if( status == EpgBasicAll || status == EpgHEITAll ){
				chkNext = TRUE;
			}
		}else if( ONID == 7 && sys->epgCapBackCS2Basic == TRUE ){
			if( status == EpgBasicAll || status == EpgHEITAll ){
				chkNext = TRUE;
			}
		}else{
			if( leitFlag == FALSE && status == EpgHEITAll ){
				chkNext = TRUE;
			}else if( leitFlag == TRUE && status == EpgLEITAll ){
				chkNext = TRUE;
			}
		}

		if( chkNext == TRUE ){
			sys->tsOut.StopSaveEPG(TRUE);
			CSendCtrlCmd cmd;
			cmd.SetConnectTimeOut(1000);
			cmd.SendReloadEpg();
			break;
		}else{
			if( GetTickCount() - startTime > timeOut * 60 * 1000 ){
				//15���ȏォ�����Ă���Ȃ��~
				sys->tsOut.StopSaveEPG(saveTimeOut);
				CSendCtrlCmd cmd;
				cmd.SetConnectTimeOut(1000);
				cmd.SendReloadEpg();
				_OutputDebugString(L"++%d����EPG�擾�������� or Ch�ύX�ŃG���[", timeOut);
				break;
			}
		}

		if( ::WaitForSingleObject(sys->epgCapBackStopEvent, 10*1000) != WAIT_TIMEOUT ){
			//�L�����Z�����ꂽ
			sys->tsOut.StopSaveEPG(FALSE);
			break;
		}
	}

	return 0;
}

BOOL CBonCtrl::GetViewStatusInfo(
	DWORD id,
	float* signal,
	DWORD* space,
	DWORD* ch,
	ULONGLONG* drop,
	ULONGLONG* scramble
	)
{
	BOOL ret = FALSE;

	this->tsOut.GetErrCount(id, drop, scramble);

	*signal = this->bonUtil.GetSignalLevel();
	this->tsOut.SetSignalLevel(*signal);

	if( this->bonUtil.GetNowCh(space, ch) == TRUE ){
		ret = TRUE;
	}

	return ret;
}
