#include "StdAfx.h"
#include "ReserveManager.h"
#include <process.h>
#include "../../Common/PathUtil.h"
#include "../../Common/ReNamePlugInUtil.h"
#include "../../Common/EpgTimerUtil.h"
#include "../../Common/TimeUtil.h"
#include "../../Common/BlockLock.h"

CReserveManager::CReserveManager(CNotifyManager& notifyManager_, CEpgDBManager& epgDBManager_)
	: notifyManager(notifyManager_)
	, epgDBManager(epgDBManager_)
	, batManager(notifyManager_)
	, checkCount(0)
	, epgCapRequested(false)
	, epgCapWork(false)
	, reserveModified(false)
	, watchdogStopEvent(NULL)
	, watchdogThread(NULL)
{
	InitializeCriticalSection(&this->managerLock);
}

CReserveManager::~CReserveManager(void)
{
	DeleteCriticalSection(&this->managerLock);
}

void CReserveManager::Initialize()
{
	this->tunerManager.ReloadTuner();
	this->tunerManager.GetEnumTunerBank(&this->tunerBankMap, this->notifyManager, this->epgDBManager);
	this->lastCheckEpgCap = GetNowI64Time();

	wstring settingPath;
	GetSettingPath(settingPath);
	this->reserveText.ParseText((settingPath + L"\\" + RESERVE_TEXT_NAME).c_str());

	ReloadSetting();
	this->recInfoText.ParseText((settingPath + L"\\" + REC_INFO_TEXT_NAME).c_str());
	this->recInfo2Text.ParseText((settingPath + L"\\" + REC_INFO2_TEXT_NAME).c_str());

	this->watchdogStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if( this->watchdogStopEvent ){
		this->watchdogThread = (HANDLE)_beginthreadex(NULL, 0, WatchdogThread, this, 0, NULL);
	}
}

void CReserveManager::Finalize()
{
	if( this->watchdogThread ){
		SetEvent(this->watchdogStopEvent);
		if( WaitForSingleObject(this->watchdogThread, 15000) == WAIT_TIMEOUT ){
			TerminateThread(this->watchdogThread, 0xffffffff);
		}
		CloseHandle(this->watchdogThread);
		this->watchdogThread = NULL;
	}
	if( this->watchdogStopEvent ){
		CloseHandle(this->watchdogStopEvent);
		this->watchdogStopEvent = NULL;
	}
	for( map<DWORD, CTunerBankCtrl*>::iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		SAFE_DELETE(itr->second);
	}
}

void CReserveManager::ReloadSetting()
{
	CBlockLock lock(&this->managerLock);

	wstring iniPath;
	GetModuleIniPath(iniPath);
	wstring commonIniPath;
	GetCommonIniPath(commonIniPath);
	wstring viewIniPath;
	GetModuleFolderPath(viewIniPath);
	viewIniPath += L"\\EpgDataCap_Bon.ini";
	wstring settingPath;
	GetSettingPath(settingPath);
	WCHAR buff[1024];

	this->chUtil.ParseText((settingPath + L"\\ChSet5.txt").c_str());

	this->ngCapTimeSec = GetPrivateProfileInt(L"SET", L"NGEpgCapTime", 20, iniPath.c_str()) * 60;
	this->ngCapTunerTimeSec = GetPrivateProfileInt(L"SET", L"NGEpgCapTunerTime", 20, iniPath.c_str()) * 60;
	this->epgCapTimeSync = GetPrivateProfileInt(L"SET", L"TimeSync", 0, iniPath.c_str()) != 0;
	this->epgCapTimeList.clear();
	int count = GetPrivateProfileInt(L"EPG_CAP", L"Count", 0, iniPath.c_str());
	for( int i = 0; i < count; i++ ){
		WCHAR key[64];
		wsprintf(key, L"%dSelect", i);
		if( GetPrivateProfileInt(L"EPG_CAP", key, 0, iniPath.c_str()) != 0 ){
			wsprintf(key, L"%d", i);
			GetPrivateProfileString(L"EPG_CAP", key, L"", buff, 256, iniPath.c_str());
			//�j���w��ڔ���(w1=Mon,...,w7=Sun)
			unsigned int hour, minute, wday = 0;
			if( swscanf_s(buff, L"%u:%uw%u", &hour, &minute, &wday) >= 2 ){
				//�擾���(bit0(LSB)=BS,bit1=CS1,bit2=CS2)�B���l�̂Ƃ��͋��ʐݒ�ɏ]��
				wsprintf(key, L"%dBasicOnlyFlags", i);
				int basicOnlyFlags = GetPrivateProfileInt(L"EPG_CAP", key, -1, iniPath.c_str());
				basicOnlyFlags = basicOnlyFlags < 0 ? 0xFF : basicOnlyFlags & 7;
				if( wday == 0 ){
					//�j���w��Ȃ�
					for( int j = 0; j < 7; j++ ){
						this->epgCapTimeList.push_back(MAKELONG(((j * 24 + hour) * 60 + minute) % 10080, basicOnlyFlags));
					}
				}else{
					this->epgCapTimeList.push_back(MAKELONG(((wday * 24 + hour) * 60 + minute) % 10080, basicOnlyFlags));
				}
			}
		}
	}

	this->defStartMargin = GetPrivateProfileInt(L"SET", L"StartMargin", 5, iniPath.c_str());
	this->defEndMargin = GetPrivateProfileInt(L"SET", L"EndMargin", 2, iniPath.c_str());
	this->backPriority = GetPrivateProfileInt(L"SET", L"BackPriority", 1, iniPath.c_str()) != 0;

	this->recInfoText.SetKeepCount(
		GetPrivateProfileInt(L"SET", L"AutoDelRecInfo", 0, iniPath.c_str()) == 0 ? UINT_MAX :
		GetPrivateProfileInt(L"SET", L"AutoDelRecInfoNum", 100, iniPath.c_str()));
	this->recInfoText.SetRecInfoDelFile(GetPrivateProfileInt(L"SET", L"RecInfoDelFile", 0, commonIniPath.c_str()) != 0);
	GetPrivateProfileString(L"SET", L"RecInfoFolder", L"", buff, 512, commonIniPath.c_str());
	this->recInfoText.SetRecInfoFolder(buff);

	this->recInfo2Text.SetKeepCount(GetPrivateProfileInt(L"SET", L"RecInfo2Max", 1000, iniPath.c_str()));
	this->recInfo2DropChk = GetPrivateProfileInt(L"SET", L"RecInfo2DropChk", 15, iniPath.c_str());
	GetPrivateProfileString(L"SET", L"RecInfo2RegExp", L"", buff, 1024, iniPath.c_str());
	this->recInfo2RegExp = buff;

	this->defEnableCaption = GetPrivateProfileInt(L"SET", L"Caption", 1, viewIniPath.c_str()) != 0;
	this->defEnableData = GetPrivateProfileInt(L"SET", L"Data", 0, viewIniPath.c_str()) != 0;

	this->recNamePlugInFileName.clear();
	if( GetPrivateProfileInt(L"SET", L"RecNamePlugIn", 0, iniPath.c_str()) != 0 ){
		GetPrivateProfileString(L"SET", L"RecNamePlugInFile", L"RecName_Macro.dll", buff, 512, iniPath.c_str());
		this->recNamePlugInFileName = buff;
	}
	this->recNameNoChkYen = GetPrivateProfileInt(L"SET", L"NoChkYen", 0, iniPath.c_str()) != 0;

	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		itr->second->ReloadSetting();
	}
	ReloadBankMap();
}

vector<RESERVE_DATA> CReserveManager::GetReserveDataAll(bool getRecFileName) const
{
	CBlockLock lock(&this->managerLock);

	vector<RESERVE_DATA> list;
	list.reserve(this->reserveText.GetMap().size());
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		list.resize(list.size() + 1);
		GetReserveData(itr->first, &list.back(), getRecFileName);
	}
	return list;
}

vector<TUNER_RESERVE_INFO> CReserveManager::GetTunerReserveAll() const
{
	CBlockLock lock(&this->managerLock);

	vector<TUNER_RESERVE_INFO> list;
	list.reserve(this->tunerBankMap.size() + 1);
	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		list.resize(list.size() + 1);
		list.back().tunerID = itr->first;
		this->tunerManager.GetBonFileName(itr->first, list.back().tunerName);
		list.back().reserveList = itr->second->GetReserveIDList();
	}
	list.resize(list.size() + 1);
	list.back().tunerID = 0xFFFFFFFF;
	list.back().tunerName = L"�`���[�i�[�s��";
	vector<DWORD> &ngList = list.back().reserveList = GetNoTunerReserveAll();
	for( size_t i = 0; i < ngList.size(); i++ ){
		//�����\��́u�`���[�i�s���v�ł͂Ȃ�
		if( this->reserveText.GetMap().find(ngList[i])->second.recSetting.recMode == RECMODE_NO ){
			ngList.erase(ngList.begin() + i);
		}
	}
	return list;
}

vector<DWORD> CReserveManager::GetNoTunerReserveAll() const
{
	CBlockLock lock(&this->managerLock);

	vector<DWORD> list;
	list.reserve(this->reserveText.GetMap().size());
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		list.push_back(itr->first);
	}
	//�S�\�񂩂�o���N�ɑ��݂���\�������
	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		vector<DWORD> diffList = itr->second->GetReserveIDList();
		size_t k = 0;
		for( size_t i = 0, j = 0; i < list.size(); ){
			if( j >= diffList.size() || list[i] < diffList[j] ){
				list[k++] = list[i++];
			}else if( diffList[j] < list[i] ){
				j++;
			}else{
				i++;
			}
		}
		list.resize(k);
	}
	return list;
}

bool CReserveManager::GetReserveData(DWORD id, RESERVE_DATA* reserveData, bool getRecFileName) const
{
	CBlockLock lock(&this->managerLock);

	map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().find(id);
	if( itr != this->reserveText.GetMap().end() ){
		*reserveData = itr->second;
		reserveData->recFileNameList.clear();
		if( getRecFileName ){
			const vector<REC_FILE_SET_INFO>& folderList = reserveData->recSetting.recFolderList;
			vector<wstring>& nameList = reserveData->recFileNameList;
			if( folderList.empty() ){
				nameList.push_back(this->recNamePlugInFileName);
			}else{
				for( size_t i = 0; i < folderList.size(); i++ ){
					nameList.push_back(folderList[i].recNamePlugIn.empty() ? this->recNamePlugInFileName : folderList[i].recNamePlugIn);
				}
			}
			//recNamePlugIn��W�J���Ď��t�@�C�������Z�b�g
			for( size_t i = 0; i < nameList.size(); i++ ){
				if( nameList[i].empty() == false ){
					CReNamePlugInUtil plugIn;
					wstring plugInPath;
					GetModuleFolderPath(plugInPath);
					plugInPath += L"\\RecName\\" + nameList[i];
					nameList[i].clear();
					if( plugIn.Initialize(plugInPath.c_str()) != FALSE ){
						PLUGIN_RESERVE_INFO info;
						info.startTime = reserveData->startTime;
						info.durationSec = reserveData->durationSecond;
						wcscpy_s(info.eventName, reserveData->title.c_str());
						info.ONID = reserveData->originalNetworkID;
						info.TSID = reserveData->transportStreamID;
						info.SID = reserveData->serviceID;
						info.EventID = reserveData->eventID;
						wcscpy_s(info.serviceName, reserveData->stationName.c_str());
						//TODO: �`���[�i�Ɋւ�������Z�b�g
						wcscpy_s(info.bonDriverName, L"�`���[�i�[�s��");
						info.bonDriverID = 0xFFFF;
						info.tunerID = 0xFFFF;
						EPG_EVENT_INFO* epgInfo = NULL;
						if( info.EventID != 0xFFFF ){
							EPGDB_EVENT_INFO epgDBInfo;
							if( this->epgDBManager.SearchEpg(info.ONID, info.TSID, info.SID, info.EventID, &epgDBInfo) != FALSE ){
								epgInfo = new EPG_EVENT_INFO;
								CopyEpgInfo(epgInfo, &epgDBInfo);
							}
						}
						WCHAR name[512];
						DWORD size = 512;
						//ConvertRecName()�͌Ă΂Ȃ�(epgInfo==NULL�̏ꍇ�Ɠ����Ȃ̂�)
						if( plugIn.ConvertRecName2(&info, epgInfo, name, &size) != FALSE ){
							nameList[i] = name;
							CheckFileName(nameList[i], this->recNameNoChkYen);
						}
						delete epgInfo;
					}
				}
				//���t�@�C�����͋�ɂ��Ȃ�
				if( nameList[i].empty() ){
					SYSTEMTIME st = reserveData->startTime;
					Format(nameList[i], L"%04d%02d%02d%02d%02dFFFFFFFF0-%s.ts",
					       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, reserveData->title.c_str());
					CheckFileName(nameList[i]);
				}
			}
		}
		return true;
	}
	return false;
}

bool CReserveManager::AddReserveData(const vector<RESERVE_DATA>& reserveList, bool setComment, bool setReserveStatus)
{
	CBlockLock lock(&this->managerLock);

	bool modified = false;
	__int64 minStartTime = LLONG_MAX;
	__int64 now = GetNowI64Time();
	for( size_t i = 0; i < reserveList.size(); i++ ){
		RESERVE_DATA r = reserveList[i];
		//���łɏI�����Ă��Ȃ���
		if( now < ConvertI64Time(r.startTime) + r.durationSecond * I64_1SEC ){
			if( setComment == false ){
				r.comment.clear();
			}
			r.overlapMode = RESERVE_EXECUTE;
			if( setReserveStatus == false ){
				r.reserveStatus = ADD_RESERVE_NORMAL;
			}
			r.recFileNameList.clear();
			r.reserveID = this->reserveText.AddReserve(r);
			this->reserveModified = true;
			modified = true;
			if( r.recSetting.recMode != RECMODE_NO ){
				__int64 startTime;
				CalcEntireReserveTime(&startTime, NULL, r);
				minStartTime = min(startTime, minStartTime);
			}
		}
	}
	if( modified ){
		this->reserveText.SaveText();
		ReloadBankMap(minStartTime);
		this->notifyManager.AddNotify(NOTIFY_UPDATE_RESERVE_INFO);
		return true;
	}
	return false;
}

bool CReserveManager::ChgReserveData(const vector<RESERVE_DATA>& reserveList, bool setReserveStatus)
{
	CBlockLock lock(&this->managerLock);

	bool modified = false;
	__int64 minStartTime = LLONG_MAX;
	for( size_t i = 0; i < reserveList.size(); i++ ){
		RESERVE_DATA r = reserveList[i];
		map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().find(r.reserveID);
		if( itr != this->reserveText.GetMap().end() ){
			//�ύX�ł��Ȃ��t�B�[���h���㏑��
			r.comment = itr->second.comment;
			r.startTimeEpg = itr->second.startTimeEpg;
			if( setReserveStatus == false ){
				r.reserveStatus = itr->second.reserveStatus;
			}
			r.recFileNameList.clear();

			if( r.recSetting.recMode == RECMODE_NO ){
				if( itr->second.recSetting.recMode != RECMODE_NO ){
					//�o���N����폜
					for( map<DWORD, CTunerBankCtrl*>::const_iterator jtr = this->tunerBankMap.begin(); jtr != this->tunerBankMap.end(); jtr++ ){
						if( jtr->second->DelReserve(r.reserveID) ){
							break;
						}
					}
					r.overlapMode = RESERVE_EXECUTE;
					__int64 startTime;
					CalcEntireReserveTime(&startTime, NULL, itr->second);
					minStartTime = min(startTime, minStartTime);
				}
			}else{
				//�o���N�ɓn���\������쐬
				CTunerBankCtrl::TUNER_RESERVE tr;
				tr.reserveID = r.reserveID;
				tr.title = r.title;
				tr.stationName = r.stationName;
				tr.onid = r.originalNetworkID;
				tr.tsid = r.transportStreamID;
				tr.sid = r.serviceID;
				tr.eid = r.eventID;
				tr.recMode = r.recSetting.recMode;
				tr.priority = r.recSetting.priority;
				bool enableCaption = tr.enableCaption =
					r.recSetting.serviceMode & RECSERVICEMODE_SET ? (r.recSetting.serviceMode & RECSERVICEMODE_CAP) != 0 : this->defEnableCaption;
				bool enableData = tr.enableData =
					r.recSetting.serviceMode & RECSERVICEMODE_SET ? (r.recSetting.serviceMode & RECSERVICEMODE_DATA) != 0 : this->defEnableData;
				tr.pittari = r.recSetting.pittariFlag != 0;
				tr.partialRecMode = r.recSetting.partialRecFlag;
				tr.continueRecFlag = r.recSetting.continueRecFlag != 0;
				__int64 startTime, endTime;
				CalcEntireReserveTime(&startTime, &endTime, r);
				tr.startTime = ConvertI64Time(r.startTime);
				tr.durationSecond = r.durationSecond;
				__int64 startMargin = tr.startMargin = tr.startTime - startTime;
				__int64 endMargin = tr.endMargin = endTime - (tr.startTime + tr.durationSecond * I64_1SEC);
				tr.recFolder = r.recSetting.recFolderList;
				tr.partialRecFolder = r.recSetting.partialRecFolder;

				bool bankDeleted = false;
				map<DWORD, CTunerBankCtrl*>::const_iterator jtr;
				for( jtr = this->tunerBankMap.begin(); jtr != this->tunerBankMap.end(); jtr++ ){
					if( jtr->second->ChgCtrlReserve(&tr) ){
						//���̗\��͂��̃o���N�ɑҋ@��Ԃő��݂���
						if( tr.onid != r.originalNetworkID ||
						    tr.tsid != r.transportStreamID ||
						    tr.sid != r.serviceID ||
						    tr.eid != r.eventID ||
						    tr.startTime != ConvertI64Time(r.startTime) ||
						    tr.durationSecond != r.durationSecond ||
						    tr.startMargin != startMargin ||
						    tr.endMargin != endMargin ){
							//�K���ύX���ׂ��t�B�[���h��ύX�ł��Ȃ������̂őҋ@��Ԃ���������
							jtr->second->DelReserve(r.reserveID);
							bankDeleted = true;
						}else{
							//�K�������ύX����K�v�̂Ȃ��t�B�[���h�͑Ë�����
							r.title = tr.title;
							r.stationName = tr.stationName;
							r.recSetting.recMode = tr.recMode;
							r.recSetting.priority = tr.priority;
							if( tr.enableCaption != enableCaption || tr.enableData != enableData ){
								r.recSetting.serviceMode = 0;
								r.recSetting.serviceMode |= tr.enableCaption ? RECSERVICEMODE_SET | RECSERVICEMODE_CAP : 0;
								r.recSetting.serviceMode |= tr.enableData ? RECSERVICEMODE_SET | RECSERVICEMODE_DATA : 0;
							}
							r.recSetting.pittariFlag = tr.pittari;
							r.recSetting.partialRecFlag = tr.partialRecMode;
							r.recSetting.continueRecFlag = tr.continueRecFlag;
							r.recSetting.recFolderList = tr.recFolder;
							r.recSetting.partialRecFolder = tr.partialRecFolder;
							r.recSetting.tunerID = itr->second.recSetting.tunerID;
						}
						break;
					}
				}
				if( jtr == this->tunerBankMap.end() ){
					//���̗\��͑ҋ@��Ԃł͂Ȃ��̂ŒP���ɍ폜�ƒǉ��ōX�V�ł���
					for( jtr = this->tunerBankMap.begin(); jtr != this->tunerBankMap.end(); jtr++ ){
						if( jtr->second->DelReserve(tr.reserveID) ){
							jtr->second->AddReserve(tr);
							break;
						}
					}
				}

				//�����̃t�B�[���h�ɕω����Ȃ���΃o���N�z�u���č\�z����K�v�͂Ȃ�
				if( bankDeleted ||
				    r.originalNetworkID != itr->second.originalNetworkID ||
				    r.transportStreamID != itr->second.transportStreamID ||
				    r.serviceID != itr->second.serviceID ||
				    ConvertI64Time(r.startTime) != ConvertI64Time(itr->second.startTime) ||
				    r.durationSecond != itr->second.durationSecond ||
				    r.recSetting.recMode != itr->second.recSetting.recMode ||
				    r.recSetting.priority != itr->second.recSetting.priority ||
				    r.recSetting.startMargine != itr->second.recSetting.startMargine ||
				    r.recSetting.endMargine != itr->second.recSetting.endMargine ||
				    r.recSetting.tunerID != itr->second.recSetting.tunerID ){
					__int64 startTime, startTimeNext;
					CalcEntireReserveTime(&startTime, NULL, itr->second);
					CalcEntireReserveTime(&startTimeNext, NULL, r);
					minStartTime = min(min(startTime, startTimeNext), minStartTime);
				}
			}
			this->reserveText.ChgReserve(r);
			this->reserveModified = true;
			modified = true;
		}
	}
	if( modified ){
		this->reserveText.SaveText();
		ReloadBankMap(minStartTime);
		this->notifyManager.AddNotify(NOTIFY_UPDATE_RESERVE_INFO);
		return true;
	}
	return false;
}

void CReserveManager::DelReserveData(const vector<DWORD>& idList)
{
	CBlockLock lock(&this->managerLock);

	bool modified = false;
	__int64 minStartTime = LLONG_MAX;
	for( size_t i = 0; i < idList.size(); i++ ){
		map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().find(idList[i]);
		if( itr != this->reserveText.GetMap().end() ){
			if( itr->second.recSetting.recMode != RECMODE_NO ){
				//�o���N����폜
				for( map<DWORD, CTunerBankCtrl*>::const_iterator jtr = this->tunerBankMap.begin(); jtr != this->tunerBankMap.end(); jtr++ ){
					if( jtr->second->DelReserve(idList[i]) ){
						break;
					}
				}
				__int64 startTime;
				CalcEntireReserveTime(&startTime, NULL, itr->second);
				minStartTime = min(startTime, minStartTime);
			}
			this->reserveText.DelReserve(idList[i]);
			this->reserveModified = true;
			modified = true;
		}
	}
	if( modified ){
		this->reserveText.SaveText();
		ReloadBankMap(minStartTime);
		this->notifyManager.AddNotify(NOTIFY_UPDATE_RESERVE_INFO);
	}
}

vector<REC_FILE_INFO> CReserveManager::GetRecFileInfoAll() const
{
	CBlockLock lock(&this->managerLock);

	vector<REC_FILE_INFO> infoList;
	infoList.reserve(this->recInfoText.GetMap().size());
	for( map<DWORD, REC_FILE_INFO>::const_iterator itr = this->recInfoText.GetMap().begin(); itr != this->recInfoText.GetMap().end(); itr++ ){
		infoList.push_back(itr->second);
	}
	return infoList;
}

void CReserveManager::DelRecFileInfo(const vector<DWORD>& idList)
{
	CBlockLock lock(&this->managerLock);

	for( size_t i = 0; i < idList.size(); i++ ){
		this->recInfoText.DelRecInfo(idList[i]);
	}
	this->recInfoText.SaveText();
	this->notifyManager.AddNotify(NOTIFY_UPDATE_REC_INFO);
}

void CReserveManager::ChgProtectRecFileInfo(const vector<REC_FILE_INFO>& infoList)
{
	CBlockLock lock(&this->managerLock);

	for( size_t i = 0; i < infoList.size(); i++ ){
		this->recInfoText.ChgProtectRecInfo(infoList[i].id, infoList[i].protectFlag);
	}
	this->recInfoText.SaveText();
	this->notifyManager.AddNotify(NOTIFY_UPDATE_REC_INFO);
}

void CReserveManager::ReloadBankMap(__int64 reloadTime)
{
	CBlockLock lock(&this->managerLock);

	if( reloadTime == LLONG_MAX ){
		return;
	}
	OutputDebugString(L"Start ReloadBankMap\r\n");
	DWORD tick = GetTickCount();

	__int64 boundaryReloadTime = 0;

	//reloadTime���O�̗\����J�n���ԋt���Ƀ\�[�g
	multimap<__int64, const RESERVE_DATA*> sortTimeMap;
	sortTimeMap.insert(std::make_pair(-reloadTime, (RESERVE_DATA*)NULL));
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		if( itr->second.recSetting.recMode != RECMODE_NO ){
			__int64 startTime;
			CalcEntireReserveTime(&startTime, NULL, itr->second);
			if( startTime < reloadTime ){
				sortTimeMap.insert(std::make_pair(-startTime, &itr->second));
			}
		}
	}
	//READY_MARGIN�b�ȏ�̖��\�񎞊ԑт�T���B���\�񎞊ԑт���낾�����Ċ��蓖�Ă����OK
	for( multimap<__int64, const RESERVE_DATA*>::const_iterator itrRes, itrTime = sortTimeMap.begin(); itrTime != sortTimeMap.end(); itrTime++ ){
		for( (itrRes = itrTime)++; itrRes != sortTimeMap.end(); itrRes++ ){
			__int64 endTime;
			CalcEntireReserveTime(NULL, &endTime, *itrRes->second);
			if( endTime + CTunerBankCtrl::READY_MARGIN * I64_1SEC > -itrTime->first ){
				break;
			}
		}
		if( itrRes == sortTimeMap.end() ){
			boundaryReloadTime = -itrTime->first;
			break;
		}
	}

	//�J�n�ςݗ\�񃊃X�g
	vector<pair<DWORD, vector<DWORD>>> startedResList;
	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		//�ҋ@��Ԃɓ����Ă�����̈ȊO�N���A
		itr->second->ClearNoCtrl(boundaryReloadTime);
		startedResList.push_back(std::make_pair(itr->first, itr->second->GetReserveIDList()));
	}

	//boundaryReloadTime����̗\����J�n���ԋt���Ƀ\�[�g
	sortTimeMap.clear();
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		if( itr->second.recSetting.recMode != RECMODE_NO ){
			__int64 startTime;
			CalcEntireReserveTime(&startTime, NULL, itr->second);
			if( startTime >= boundaryReloadTime ){
				this->reserveText.SetOverlapMode(itr->first, RESERVE_NO_EXECUTE);
				sortTimeMap.insert(std::make_pair(-startTime, &itr->second));
			}
		}
	}
	//�\��𖳗\�񎞊ԑт��Ƃɑg�������ăo���N�z�u����(�g���ƂɓƗ����ď����ł���̂ő��x��z�u���萫������)
	for( multimap<__int64, const RESERVE_DATA*>::const_iterator itrRes, itrTime = sortTimeMap.begin(); itrTime != sortTimeMap.end(); ){
		for( (itrRes = itrTime)++; itrRes != sortTimeMap.end(); itrRes++ ){
			__int64 endTime;
			CalcEntireReserveTime(NULL, &endTime, *itrRes->second);
			if( endTime + CTunerBankCtrl::READY_MARGIN * I64_1SEC > -itrTime->first ){
				break;
			}
		}
		itrTime++;
		if( itrRes == sortTimeMap.end() ){
			//�o���N�����̗\��}�b�v
			multimap<__int64, const RESERVE_DATA*> sortResMap;
			for( itrRes = sortTimeMap.begin(); itrRes != itrTime; itrRes++ ){
				//�L�[�͎����D��x(�\��D��x<<60|�J�n��)
				__int64 startOrder = -itrRes->first / I64_1SEC << 16 | itrRes->second->reserveID & 0xFFFF;
				__int64 priority = (this->backPriority ? itrRes->second->recSetting.priority : ~itrRes->second->recSetting.priority) & 7;
				sortResMap.insert(std::make_pair((this->backPriority ? -1 : 1) * (priority << 60 | startOrder), itrRes->second));
			}
			itrTime = sortTimeMap.erase(sortTimeMap.begin(), itrTime);

			//�o���N���肵���\��}�b�v
			map<DWORD, vector<CHK_RESERVE_DATA>> bankResMap;
			for( size_t i = 0; i < startedResList.size(); i++ ){
				bankResMap.insert(std::make_pair(startedResList[i].first, vector<CHK_RESERVE_DATA>()));
				//�J�n�ςݗ\��͂��̂܂܃o���N����
				for( multimap<__int64, const RESERVE_DATA*>::const_iterator itr = sortResMap.begin(); itr != sortResMap.end(); ){
					if( std::find(startedResList[i].second.begin(), startedResList[i].second.end(), itr->second->reserveID) != startedResList[i].second.end() ){
						CHK_RESERVE_DATA item;
						CalcEntireReserveTime(&item.cutStartTime, &item.cutEndTime, *itr->second);
						item.cutStartTime -= CTunerBankCtrl::READY_MARGIN * I64_1SEC;
						item.startOrder = abs(itr->first) & 0x0FFFFFFFFFFFFFFFLL;
						item.effectivePriority = itr->first;
						item.started = true;
						item.r = itr->second;
						//�J�n�ςݗ\��͂��ׂăo���N���œ���`�����l���Ȃ̂�ChkInsertStatus()�͕s�v
						bankResMap[startedResList[i].first].push_back(item);
						sortResMap.erase(itr++);
					}else{
						itr++;
					}
				}
			}

			for( multimap<__int64, const RESERVE_DATA*>::const_iterator itr = sortResMap.begin(); itr != sortResMap.end(); ){
				CHK_RESERVE_DATA item;
				CalcEntireReserveTime(&item.cutStartTime, &item.cutEndTime, *itr->second);
				item.cutStartTime -= CTunerBankCtrl::READY_MARGIN * I64_1SEC;
				item.startOrder = abs(itr->first) & 0x0FFFFFFFFFFFFFFFLL;
				item.effectivePriority = itr->first;
				item.started = false;
				item.r = itr->second;
				if( itr->second->recSetting.tunerID != 0 ){
					//�`���[�iID�Œ�
					map<DWORD, vector<CHK_RESERVE_DATA>>::iterator itrBank = bankResMap.find(itr->second->recSetting.tunerID); 
					if( itrBank != bankResMap.end() &&
					    this->tunerManager.IsSupportService(itrBank->first, itr->second->originalNetworkID, itr->second->transportStreamID, itr->second->serviceID) ){
						CHK_RESERVE_DATA testItem = item;
						ChkInsertStatus(itrBank->second, testItem, false);
						if( testItem.cutEndTime - testItem.cutStartTime > CTunerBankCtrl::READY_MARGIN * I64_1SEC ){
							//�^�掞�Ԃ�����
							ChkInsertStatus(itrBank->second, item, true);
							itrBank->second.push_back(item);
							sortResMap.erase(itr++);
							continue;
						}
					}
				}else{
					//�����Ƃ��ǂ��Ǝv����o���N�Ɋ��蓖�Ă�
					map<DWORD, vector<CHK_RESERVE_DATA>>::iterator itrMin = bankResMap.end();
					__int64 costMin = LLONG_MAX;
					__int64 durationMin = 0;
					for( map<DWORD, vector<CHK_RESERVE_DATA>>::iterator jtr = bankResMap.begin(); jtr != bankResMap.end(); jtr++ ){
						if( this->tunerManager.IsSupportService(jtr->first, itr->second->originalNetworkID, itr->second->transportStreamID, itr->second->serviceID) ){
							CHK_RESERVE_DATA testItem = item;
							__int64 cost = ChkInsertStatus(jtr->second, testItem, false);
							if( cost < costMin ){
								itrMin = jtr;
								costMin = cost;
								durationMin = testItem.cutEndTime - testItem.cutStartTime;
							}
						}
					}
					if( itrMin != bankResMap.end() && durationMin > CTunerBankCtrl::READY_MARGIN * I64_1SEC ){
						//�^�掞�Ԃ�����
						ChkInsertStatus(itrMin->second, item, true);
						itrMin->second.push_back(item);
						sortResMap.erase(itr++);
						continue;
					}
				}
				itr++;
			}

			//���ۂɃo���N�ɒǉ�����
			for( map<DWORD, vector<CHK_RESERVE_DATA>>::const_iterator itr = bankResMap.begin(); itr != bankResMap.end(); itr++ ){
				for( size_t i = 0; i < itr->second.size(); i++ ){
					const RESERVE_DATA& r = *itr->second[i].r;
					__int64 startTime, endTime;
					CalcEntireReserveTime(&startTime, &endTime, r);
					//���Ԃ��Ԃ��L�^����(�Q�l���x�̏��)
					this->reserveText.SetOverlapMode(r.reserveID,
						itr->second[i].cutStartTime == startTime - CTunerBankCtrl::READY_MARGIN * I64_1SEC &&
						itr->second[i].cutEndTime == endTime ? RESERVE_EXECUTE : RESERVE_PILED_UP);
					//�o���N�ɓn���\������쐬
					CTunerBankCtrl::TUNER_RESERVE tr;
					tr.reserveID = r.reserveID;
					tr.title = r.title;
					tr.stationName = r.stationName;
					tr.onid = r.originalNetworkID;
					tr.tsid = r.transportStreamID;
					tr.sid = r.serviceID;
					tr.eid = r.eventID;
					tr.recMode = r.recSetting.recMode;
					tr.priority = r.recSetting.priority;
					tr.enableCaption = r.recSetting.serviceMode & RECSERVICEMODE_SET ? (r.recSetting.serviceMode & RECSERVICEMODE_CAP) != 0 : this->defEnableCaption;
					tr.enableData = r.recSetting.serviceMode & RECSERVICEMODE_SET ? (r.recSetting.serviceMode & RECSERVICEMODE_DATA) != 0 : this->defEnableData;
					tr.pittari = r.recSetting.pittariFlag != 0;
					tr.partialRecMode = r.recSetting.partialRecFlag;
					tr.continueRecFlag = r.recSetting.continueRecFlag != 0;
					tr.startTime = ConvertI64Time(r.startTime);
					tr.durationSecond = r.durationSecond;
					tr.startMargin = tr.startTime - startTime;
					tr.endMargin = endTime - (tr.startTime + tr.durationSecond * I64_1SEC);
					tr.recFolder = r.recSetting.recFolderList;
					tr.partialRecFolder = r.recSetting.partialRecFolder;
					this->tunerBankMap.find(itr->first)->second->AddReserve(tr);
				}
			}
		}
	}

	_OutputDebugString(L"End ReloadBankMap %dmsec\r\n", GetTickCount() - tick);
}

__int64 CReserveManager::ChkInsertStatus(vector<CHK_RESERVE_DATA>& bank, CHK_RESERVE_DATA& inItem, bool modifyBank) const
{
	//CBlockLock lock(&this->managerLock);

	bool overlapped = false;
	__int64 otherCosts[5] = {};

	for( size_t i = 0; i < bank.size(); i++ ){
		if( bank[i].r->originalNetworkID == inItem.r->originalNetworkID && bank[i].r->transportStreamID == inItem.r->transportStreamID ){
			//����`�����l��
			if( inItem.cutStartTime < bank[i].cutStartTime && bank[i].cutStartTime < inItem.cutEndTime ||
			    inItem.cutStartTime < bank[i].cutEndTime && bank[i].cutEndTime < inItem.cutEndTime ||
			    inItem.cutStartTime > bank[i].cutStartTime && bank[i].cutEndTime > inItem.cutEndTime ){
				//�d�Ȃ肪����
				overlapped = true;
			}
		}else{
			if( bank[i].effectivePriority < inItem.effectivePriority ){
				//���肪���D��x�Ȃ̂Ŏ����̗\�񎞊Ԃ����
				if( bank[i].startOrder > inItem.startOrder ){
					//���肪�x��ĊJ�n����̂Ŏ����̌�������
					__int64 cutEndTime = max(min(inItem.cutEndTime, bank[i].cutStartTime), inItem.cutStartTime);
					otherCosts[min(max(inItem.r->recSetting.priority, 1), 5) - 1] += inItem.cutEndTime - cutEndTime;
					inItem.cutEndTime = cutEndTime;
				}else{
					//�O�������
					__int64 cutStartTime = min(max(inItem.cutStartTime, bank[i].cutEndTime), inItem.cutEndTime);
					otherCosts[min(max(inItem.r->recSetting.priority, 1), 5) - 1] += cutStartTime - inItem.cutStartTime;
					inItem.cutStartTime = cutStartTime;
				}
			}else{
				//����̗\�񎞊Ԃ����
				if( inItem.startOrder > bank[i].startOrder ){
					//����̌�������
					__int64 cutEndTime = max(min(bank[i].cutEndTime, inItem.cutStartTime), bank[i].cutStartTime);
					otherCosts[min(max(bank[i].r->recSetting.priority, 1), 5) - 1] += bank[i].cutEndTime - cutEndTime;
					if( modifyBank ){
						bank[i].cutEndTime = cutEndTime;
					}
				}else{
					//�O�������
					_int64 cutStartTime = bank[i].started ? bank[i].cutEndTime : min(max(bank[i].cutStartTime, inItem.cutEndTime), bank[i].cutEndTime);
					otherCosts[min(max(bank[i].r->recSetting.priority, 1), 5) - 1] += cutStartTime - bank[i].cutStartTime;
					if( modifyBank ){
						bank[i].cutStartTime = cutStartTime;
					}
				}
			}
		}
	}

	//�D��x���Ƃɏd�݂����ăR�X�g���Z�o
	__int64 cost = 0;
	__int64 weight = 1;
	for( int i = 0; i < 5; i++ ){
		cost += min((otherCosts[i] + 10 * I64_1SEC - 1) / (10 * I64_1SEC), 5400 - 1) * weight;
		weight *= 5400;
	}
	if( cost == 0 && overlapped ){
		//TODO: �Ƃ肠�����ꗥ��-10�b�Ƃ��邪�A�d�Ȃ�x�����R�X�g�ɔ��f���Ă���������
		cost = -1;
	}
	return cost;
}

void CReserveManager::CalcEntireReserveTime(__int64* startTime, __int64* endTime, const RESERVE_DATA& data) const
{
	//CBlockLock lock(&this->managerLock);

	__int64 startTime_ = ConvertI64Time(data.startTime);
	__int64 endTime_ = startTime_ + data.durationSecond * I64_1SEC;
	__int64 startMargin = this->defStartMargin * I64_1SEC;
	__int64 endMargin = this->defEndMargin * I64_1SEC;
	if( data.recSetting.useMargineFlag != 0 ){
		startMargin = data.recSetting.startMargine * I64_1SEC;
		endMargin = data.recSetting.endMargine * I64_1SEC;
	}
	//�J�n�}�[�W���͌��̗\��I�������𒴂��ĕ��ł����Ă͂Ȃ�Ȃ�
	startMargin = max(startMargin, startTime_ - endTime_);
	//�I���}�[�W���͌��̗\��J�n�����𒴂��ĕ��ł����Ă͂Ȃ�Ȃ�
	endMargin = max(endMargin, startTime_ - min(startMargin, 0) - endTime_);
	if( startTime != NULL ){
		*startTime = startTime_ - startMargin;
	}
	if( endTime != NULL ){
		*endTime = endTime_ + endMargin;
	}
}

void CReserveManager::CheckTuijyu()
{
	CBlockLock lock(&this->managerLock);

	vector<RESERVE_DATA> chgList;
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		if( itr->second.eventID == 0xFFFF || itr->second.reserveStatus != ADD_RESERVE_NORMAL ){
			//�v���O�����\��A����эŐVEPG(�`���[�i����̏��)�ŕύX�ς݂̗\��͑ΏۊO
			continue;
		}
		//����ƈقȂ�recMode==RECMODE_NO�������B�܂�tuijyuuFlag�͈Ӗ����ω����Ă���̂Œ���
		EPGDB_EVENT_INFO info;
		if( this->epgDBManager.SearchEpg(itr->second.originalNetworkID, itr->second.transportStreamID, itr->second.serviceID, itr->second.eventID, &info) ){
			//�}�[�W�̓s����EIT[p/f]�R���̖��莞���̃C�x���g�������邩������Ȃ��������ł͖�������
			if( info.StartTimeFlag != 0 && info.DurationFlag != 0 ){
				__int64 startDiff = ConvertI64Time(info.start_time) - ConvertI64Time(itr->second.startTime);
				if( startDiff < -12 * 3600 * I64_1SEC || 12 * 3600 * I64_1SEC < startDiff ){
					//EventID�̍Ďg�p�ɔ����邽��12���Ԉȏ�̈ړ��͑ΏۊO
					continue;
				}
				RESERVE_DATA r = itr->second;
				bool chgRes = false;
				if( info.shortInfo != NULL && r.title != info.shortInfo->event_name ){
					r.title = info.shortInfo->event_name;
					chgRes = true;
				}
				if( ConvertI64Time(r.startTime) != ConvertI64Time(info.start_time) ){
					r.startTime = info.start_time;
					chgRes = true;
				}
				if( r.durationSecond != info.durationSec ){
					r.durationSecond = info.durationSec;
					chgRes = true;
				}
				if( chgRes ){
					chgList.push_back(r);
				}
			}
		}
	}
	if( chgList.empty() == false ){
		ChgReserveData(chgList);
	}
}

void CReserveManager::CheckTuijyuTuner()
{
	vector<DWORD> chgIDList;
	//tunerBankMap���̂��͔̂r������̑ΏۊO
	for( map<DWORD, CTunerBankCtrl*>::const_iterator itrBank = this->tunerBankMap.begin(); itrBank != this->tunerBankMap.end(); itrBank++ ){
		CBlockLock lock(&this->managerLock);

		WORD onid, tsid;
		if( itrBank->second->GetCurrentChID(&onid, &tsid) == false ){
			//���̃`���[�i�͋N�����Ă��Ȃ�
			continue;
		}
		vector<RESERVE_DATA> chgList;
		vector<RESERVE_DATA> relayAddList;
		const vector<pair<ULONGLONG, DWORD>>& cacheList = this->reserveText.GetSortByEventList();

		vector<pair<ULONGLONG, DWORD>>::const_iterator itrCache = std::lower_bound(
			cacheList.begin(), cacheList.end(), pair<ULONGLONG, DWORD>(_Create64Key2(onid, tsid, 0, 0), 0));
		for( ; itrCache != cacheList.end() && itrCache->first <= (ULONGLONG)_Create64Key2(onid, tsid, 0xFFFF, 0xFFFF); ){
			//�N�����̃`�����l���Ɉ�v����\���EIT[p/f]�Əƍ�����
			WORD sid = itrCache->first >> 16 & 0xFFFF;
			EPGDB_EVENT_INFO resPfVal[2];
			int nowSuccess = itrBank->second->GetEventPF(sid, false, &resPfVal[0]);
			int nextSuccess = itrBank->second->GetEventPF(sid, true, &resPfVal[1]);
			for( ; itrCache != cacheList.end() && itrCache->first <= (ULONGLONG)_Create64Key2(onid, tsid, sid, 0xFFFF); itrCache++ ){
				if( std::find(chgIDList.begin(), chgIDList.end(), itrCache->second) != chgIDList.end() ){
					//���̗\��͂��łɕύX�ς�
					continue;
				}
				map<DWORD, RESERVE_DATA>::const_iterator itrRes = this->reserveText.GetMap().find(itrCache->second);
				if( itrRes->second.eventID == 0xFFFF ||
				    itrRes->second.recSetting.recMode == RECMODE_NO ||
				    ConvertI64Time(itrRes->second.startTime) > GetNowI64Time() + 6 * 3600 * I64_1SEC ){
					//�v���O�����\��A�����\��A�����6���Ԉȏ��̗\��͑ΏۊO
					continue;
				}
				bool pfFound = false;
				for( int i = (nowSuccess == 0 ? 0 : 1); i < (nextSuccess == 0 ? 2 : 1); i++ ){
					const EPGDB_EVENT_INFO& info = resPfVal[i];
					if( info.event_id == itrRes->second.eventID && (info.StartTimeFlag != 0 || info.DurationFlag != 0) ){
						RESERVE_DATA r = itrRes->second;
						bool chgRes = false;
						if( info.shortInfo != NULL && r.title != info.shortInfo->event_name ){
							r.title = info.shortInfo->event_name;
							r.reserveStatus = ADD_RESERVE_CHG_PF;
							chgRes = true;
						}
						if( info.StartTimeFlag != 0 ){
							if( ConvertI64Time(r.startTime) != ConvertI64Time(info.start_time) ){
								r.startTime = info.start_time;
								r.reserveStatus = ADD_RESERVE_CHG_PF;
								chgRes = true;
							}
							if( info.DurationFlag == 0 ){
								//�p�����Ԗ���B����(present)���I���܂�5����؂�\���5���L�΂�
								if( i == 0 && ConvertI64Time(r.startTime) + r.durationSecond * I64_1SEC < GetNowI64Time() + 300 * I64_1SEC ){
									r.durationSecond += 300;
									r.reserveStatus = ADD_RESERVE_UNKNOWN_END;
									chgRes = true;
									OutputDebugString(L"��p/f �p�����Ԗ���̌��݃C�x���g�̗\���ύX���܂�\r\n");
								}
							}else if( r.reserveStatus == ADD_RESERVE_UNKNOWN_END || r.durationSecond != info.durationSec ){
								r.durationSecond = info.durationSec;
								r.reserveStatus = ADD_RESERVE_CHG_PF;
								chgRes = true;
							}
						}else{
							//�J�n��������B��(following)���J�n�܂�5����؂�\���5���ړ�
							if( i == 1 && ConvertI64Time(r.startTime) < GetNowI64Time() + 300 * I64_1SEC ){
								ConvertSystemTime(ConvertI64Time(r.startTime) + 300 * I64_1SEC, &r.startTime);
								r.reserveStatus = ADD_RESERVE_CHG_PF;
								chgRes = true;
								OutputDebugString(L"��p/f �J�n��������̎��C�x���g�̗\���ύX���܂�\r\n");
							}
							if( r.reserveStatus == ADD_RESERVE_UNKNOWN_END || r.durationSecond != info.durationSec ){
								r.durationSecond = info.durationSec;
								r.reserveStatus = ADD_RESERVE_CHG_PF;
								chgRes = true;
							}
						}
						if( chgRes ){
							chgIDList.push_back(r.reserveID);
							chgList.push_back(r);
						}
						//����(present)�ɂ��Ă̓C�x���g�����[���`�F�b�N
						if( i == 0 && r.recSetting.tuijyuuFlag && info.StartTimeFlag && info.DurationFlag && info.eventRelayInfo ){
							//�C�x���g�����[����
							vector<EPGDB_EVENT_DATA>::const_iterator itrR = info.eventRelayInfo->eventDataList.begin();
							for( ; itrR != info.eventRelayInfo->eventDataList.end(); itrR++ ){
								if( IsFindReserve(itrR->original_network_id, itrR->transport_stream_id, itrR->service_id, itrR->event_id) ){
									//�����[�ς�
									break;
								}
							}
							if( itrR == info.eventRelayInfo->eventDataList.end() ){
								OutputDebugString(L"EventRelayCheck\r\n");
								for( itrR = info.eventRelayInfo->eventDataList.begin(); itrR != info.eventRelayInfo->eventDataList.end(); itrR++ ){
									map<LONGLONG, CH_DATA5>::const_iterator itrCh = this->chUtil.GetMap().find(
										_Create64Key(itrR->original_network_id, itrR->transport_stream_id, itrR->service_id));
									if( itrCh != this->chUtil.GetMap().end() && relayAddList.empty() ){
										//�����[�ł���`�����l������
										RESERVE_DATA rr;
										rr.title = L"(�C�x���g�����[)" + r.title;
										//�����[���̏I�����Ԃ������[��̊J�n���ԂƂ���
										ConvertSystemTime(ConvertI64Time(info.start_time) + info.durationSec * I64_1SEC, &rr.startTime);
										rr.startTimeEpg = rr.startTime;
										rr.durationSecond = 600;
										rr.stationName = itrCh->second.serviceName;
										rr.originalNetworkID = itrR->original_network_id;
										rr.transportStreamID = itrR->transport_stream_id;
										rr.serviceID = itrR->service_id;
										rr.eventID = itrR->event_id;
										//�^��ݒ�̓����[���̗\����p��
										rr.recSetting = r.recSetting;
										rr.reserveStatus = ADD_RESERVE_RELAY;
										relayAddList.push_back(rr);
										OutputDebugString(L"���C�x���g�����[�ǉ�\r\n");
										break;
									}
								}
							}
						}
						pfFound = true;
						break;
					}
				}
				//EIT[p/f]���������擾�ł���󋵂�EIT[p/f]�ɂȂ����̂͒ʏ�`�F�b�N
				EPGDB_EVENT_INFO info;
				if( nowSuccess != 2 && nextSuccess != 2 && pfFound == false && itrBank->second->SearchEpgInfo(sid, itrRes->second.eventID, &info) ){
					if( info.StartTimeFlag != 0 && info.DurationFlag != 0 ){
						__int64 startDiff = ConvertI64Time(info.start_time) - ConvertI64Time(itrRes->second.startTime);
						if( startDiff < -12 * 3600 * I64_1SEC || 12 * 3600 * I64_1SEC < startDiff ){
							//EventID�̍Ďg�p�ɔ����邽��12���Ԉȏ�̈ړ��͑ΏۊO
							continue;
						}
						RESERVE_DATA r = itrRes->second;
						bool chgRes = false;
						if( info.shortInfo != NULL && r.title != info.shortInfo->event_name ){
							r.title = info.shortInfo->event_name;
							chgRes = true;
						}
						if( ConvertI64Time(r.startTime) != ConvertI64Time(info.start_time) ){
							r.startTime = info.start_time;
							chgRes = true;
						}
						if( r.durationSecond != info.durationSec ){
							r.durationSecond = info.durationSec;
							chgRes = true;
						}
						if( chgRes ){
							//EPG�ēǂݍ��݂ŕύX����Ȃ��悤�ɂ���
							r.reserveStatus = ADD_RESERVE_CHG_PF2;
							chgIDList.push_back(r.reserveID);
							chgList.push_back(r);
						}
					}
				}
			}
		}
		if( chgList.empty() == false ){
			ChgReserveData(chgList, true);
		}
		if( relayAddList.empty() == false ){
			AddReserveData(relayAddList, false, true);
		}
	}
}

void CReserveManager::CheckOverTimeReserve()
{
	CBlockLock lock(&this->managerLock);

	bool modified = false;
	__int64 now = GetNowI64Time();
	vector<DWORD> noList = GetNoTunerReserveAll();
	for( size_t i = 0; i < noList.size(); i++ ){
		map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().find(noList[i]);
		__int64 endTime;
		CalcEntireReserveTime(NULL, &endTime, itr->second);
		if( endTime < now ){
			//�I�����ԉ߂��Ă��܂��Ă���
			if( itr->second.recSetting.recMode != RECMODE_NO ){
				//�����̂��̂͌��ʂɎc���Ȃ�
				REC_FILE_INFO item;
				item = itr->second;
				item.recStatus = REC_END_STATUS_NO_TUNER;
				item.comment = L"�`���[�i�[�s���̂��ߎ��s���܂���";
				this->recInfoText.AddRecInfo(item);
			}
			this->reserveText.DelReserve(itr->first);
			this->reserveModified = true;
			modified = true;
		}
	}
	if( modified ){
		this->reserveText.SaveText();
		this->recInfoText.SaveText();
		this->notifyManager.AddNotify(NOTIFY_UPDATE_RESERVE_INFO);
		this->notifyManager.AddNotify(NOTIFY_UPDATE_REC_INFO);
	}
}

DWORD CReserveManager::Check()
{
	{
		this->checkCount++;

		bool isRec = false;
		bool isEpgCap = false;
		int shutdownMode = -1;
		//tunerBankMap���̂��͔̂r������̑ΏۊO
		for( map<DWORD, CTunerBankCtrl*>::const_iterator itrBank = this->tunerBankMap.begin(); itrBank != this->tunerBankMap.end(); itrBank++ ){
			CBlockLock lock(&this->managerLock);

			// �`���[�i�̗\���ԑJ�ڂ��s���A�\��I�����`�F�b�N����
			vector<CTunerBankCtrl::CHECK_RESULT> retList = itrBank->second->Check();
			CTunerBankCtrl::TR_STATE state = itrBank->second->GetState();
			isRec = isRec || state == CTunerBankCtrl::TR_REC;
			isEpgCap = isEpgCap || state == CTunerBankCtrl::TR_EPGCAP;
			bool modified = false;
			for( vector<CTunerBankCtrl::CHECK_RESULT>::const_iterator itrRet = retList.begin(); itrRet != retList.end(); itrRet++ ){
				map<DWORD, RESERVE_DATA>::const_iterator itrRes = this->reserveText.GetMap().find(itrRet->reserveID);
				if( itrRes != this->reserveText.GetMap().end() ){
					if( itrRet->type == CTunerBankCtrl::CHECK_END && itrRet->recFilePath.empty() == false &&
					    itrRet->drops < this->recInfo2DropChk && itrRet->epgEventName.empty() == false ){
						//�^��ς݂Ƃ��ēo�^
						PARSE_REC_INFO2_ITEM item;
						item.originalNetworkID = itrRes->second.originalNetworkID;
						item.transportStreamID = itrRes->second.transportStreamID;
						item.serviceID = itrRes->second.serviceID;
						item.startTime = itrRet->epgStartTime;
						item.eventName = itrRet->epgEventName;
						this->recInfo2Text.Add(item);
					}

					REC_FILE_INFO item;
					item = itrRes->second;
					if( itrRet->type <= CTunerBankCtrl::CHECK_END_NOT_START_HEAD ){
						item.recFilePath = itrRet->recFilePath;
						item.drops = itrRet->drops;
						item.scrambles = itrRet->scrambles;
					}
					switch( itrRet->type ){
					case CTunerBankCtrl::CHECK_END:
						if( ConvertI64Time(item.startTime) != ConvertI64Time(item.startTimeEpg) ){
							item.recStatus = REC_END_STATUS_CHG_TIME;
							item.comment = L"�J�n���Ԃ��ύX����܂���";
						}else{
							item.recStatus = REC_END_STATUS_NORMAL;
							item.comment = item.recFilePath.empty() ? L"�I��" : L"�^��I��";
						}
						break;
					case CTunerBankCtrl::CHECK_END_NOT_FIND_PF:
						item.recStatus = REC_END_STATUS_NOT_FIND_PF;
						item.comment = L"�^�撆�ɔԑg�����m�F�ł��܂���ł���";
						break;
					case CTunerBankCtrl::CHECK_END_NEXT_START_END:
						item.recStatus = REC_END_STATUS_NEXT_START_END;
						item.comment = L"���̗\��J�n�̂��߂ɃL�����Z������܂���";
						break;
					case CTunerBankCtrl::CHECK_END_END_SUBREC:
						item.recStatus = REC_END_STATUS_END_SUBREC;
						item.comment = L"�^��I���i�󂫗e�ʕs���ŕʃt�H���_�ւ̕ۑ��������j";
						break;
					case CTunerBankCtrl::CHECK_END_NOT_START_HEAD:
						item.recStatus = REC_END_STATUS_NOT_START_HEAD;
						item.comment = L"�ꕔ�̂ݘ^�悪���s���ꂽ�\��������܂�";
						break;
					case CTunerBankCtrl::CHECK_ERR_RECEND:
						item.recStatus = REC_END_STATUS_ERR_END2;
						item.comment = L"�t�@�C���ۑ��Œv���I�ȃG���[�����������\��������܂�";
						break;
					case CTunerBankCtrl::CHECK_ERR_REC:
						item.recStatus = REC_END_STATUS_ERR_END;
						item.comment = L"�^�撆�ɃL�����Z�����ꂽ�\��������܂�";
						break;
					case CTunerBankCtrl::CHECK_ERR_RECSTART:
					case CTunerBankCtrl::CHECK_ERR_CTRL:
						item.recStatus = REC_END_STATUS_ERR_RECSTART;
						item.comment = L"�^��J�n�����Ɏ��s���܂����i�󂫗e�ʕs���̉\������j";
						break;
					case CTunerBankCtrl::CHECK_ERR_OPEN:
						item.recStatus = REC_END_STATUS_OPEN_ERR;
						item.comment = L"�`���[�i�[�̃I�[�v���Ɏ��s���܂���";
						break;
					case CTunerBankCtrl::CHECK_ERR_PASS:
						item.recStatus = REC_END_STATUS_START_ERR;
						item.comment = L"�^�掞�ԂɋN�����Ă��Ȃ������\��������܂�";
						break;
					}
					this->recInfoText.AddRecInfo(item);

					//�o�b�`�����ǉ�
					if( (itrRet->type == CTunerBankCtrl::CHECK_END || itrRet->type == CTunerBankCtrl::CHECK_END_NEXT_START_END) && item.recFilePath.empty() == false &&
					    itrRes->second.recSetting.batFilePath.empty() == false && itrRet->continueRec == false ){
						BAT_WORK_INFO batInfo;
						batInfo.batFilePath = itrRes->second.recSetting.batFilePath;
						batInfo.suspendMode = itrRes->second.recSetting.suspendMode;
						batInfo.rebootFlag = itrRes->second.recSetting.rebootFlag;
						if( itrRes->second.comment.compare(0, 8, L"EPG�����\��(") == 0 && itrRes->second.comment.size() >= 9 ){
							batInfo.addKey.assign(itrRes->second.comment, 8, itrRes->second.comment.size() - 9);
						}
						batInfo.recFileInfo = item;
						this->batManager.AddBatWork(batInfo);
					}else if( itrRet->type != CTunerBankCtrl::CHECK_ERR_PASS ){
						shutdownMode = MAKEWORD(itrRes->second.recSetting.suspendMode, itrRes->second.recSetting.rebootFlag);
					}

					this->reserveText.DelReserve(itrRes->first);
					this->reserveModified = true;
					modified = true;

					//�\��I����ʒm
					SYSTEMTIME st = item.startTime;
					SYSTEMTIME stEnd;
					ConvertSystemTime(ConvertI64Time(st) + item.durationSecond * I64_1SEC, &stEnd);
					wstring msg;
					Format(msg, L"%s %04d/%02d/%02d %02d:%02d�`%02d:%02d\r\n%s\r\n%s",
					       item.serviceName.c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
					       stEnd.wHour, stEnd.wMinute, item.title.c_str(), item.comment.c_str());
					this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_REC_END, msg);
				}
			}
			if( modified ){
				CBlockLock lock(&this->managerLock);
				this->reserveText.SaveText();
				this->recInfoText.SaveText();
				this->recInfo2Text.SaveText();
				this->notifyManager.AddNotify(NOTIFY_UPDATE_RESERVE_INFO);
				this->notifyManager.AddNotify(NOTIFY_UPDATE_REC_INFO);
			}
		}
		if( this->checkCount % 30 == 0 ){
			CheckOverTimeReserve();
		}
		if( this->checkCount % 3 == 0 ){
			CheckTuijyuTuner();
		}
		__int64 idleMargin = GetNearestRecReserveTime() - GetNowI64Time();
		this->batManager.SetIdleMargin((DWORD)min(max(idleMargin / I64_1SEC, 0), MAXDWORD));
		this->notifyManager.SetNotifySrvStatus(isRec ? 1 : isEpgCap ? 2 : 0);

		BYTE suspendMode;
		BYTE rebootFlag;
		if( CheckEpgCap(isEpgCap) ){
			//EPG�擾����������
			this->notifyManager.AddNotify(NOTIFY_UPDATE_EPGCAP_END);
			return MAKELONG(0, CHECK_EPGCAP_END);
		}else if( this->batManager.GetWorkCount() == 0 && this->batManager.PopLastWorkSuspend(&suspendMode, &rebootFlag) ){
			//�o�b�`��������������
			return MAKELONG(MAKEWORD(suspendMode, rebootFlag), CHECK_NEED_SHUTDOWN);
		}else if( shutdownMode >= 0 && this->batManager.GetWorkCount() == 0 && this->batManager.IsWorking() == FALSE ){
			return MAKELONG(shutdownMode, CHECK_NEED_SHUTDOWN);
		}else if( this->reserveModified ){
			CBlockLock lock(&this->managerLock);
			if( this->reserveModified ){
				this->reserveModified = false;
				return MAKELONG(0, CHECK_RESERVE_MODIFIED);
			}
		}
	}
	return 0;
}

vector<DWORD> CReserveManager::GetEpgCapTunerIDList(__int64 now) const
{
	CBlockLock lock(&this->managerLock);

	//���p�\�ȃ`���[�i�̒��o
	vector<pair<vector<DWORD>, WORD>> tunerIDList;
	this->tunerManager.GetEnumEpgCapTuner(&tunerIDList);
	vector<DWORD> epgCapIDList;
	for( size_t i = 0; i < tunerIDList.size(); i++ ){
		WORD epgCapMax = tunerIDList[i].second;
		WORD ngCapCount = 0;
		for( size_t j = 0; j < tunerIDList[i].first.size() && epgCapMax > 0; j++ ){
			map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.find(tunerIDList[i].first[j]);
			CTunerBankCtrl::TR_STATE state = itr->second->GetState();
			__int64 minTime = itr->second->GetNearestReserveTime();
			if( this->ngCapTimeSec != 0 && (state != CTunerBankCtrl::TR_IDLE || minTime < now + this->ngCapTimeSec * I64_1SEC) ){
				//���s�����Ⴂ���Ȃ�
				ngCapCount++;
			}else if( state == CTunerBankCtrl::TR_IDLE && minTime > now + this->ngCapTunerTimeSec * I64_1SEC ){
				//�g����`���[�i
				epgCapIDList.push_back(itr->first);
				epgCapMax--;
			}
		}
		if( tunerIDList[i].second > tunerIDList[i].first.size() - ngCapCount ){
			epgCapIDList.clear();
			break;
		}
	}
	return epgCapIDList;
}

bool CReserveManager::RequestStartEpgCap()
{
	CBlockLock lock(&this->managerLock);

	if( this->epgCapRequested || this->epgCapWork || GetEpgCapTunerIDList(GetNowI64Time()).empty() ){
		return false;
	}
	this->epgCapRequested = true;
	return true;
}

bool CReserveManager::CheckEpgCap(bool isEpgCap)
{
	CBlockLock lock(&this->managerLock);

	bool doneEpgCap = false;
	__int64 now = GetNowI64Time();
	if( this->epgCapWork == false ){
		//����0�b���ׂ��^�C�~���O��EPG�擾�̃`�F�b�N���s��
		if( this->epgCapRequested || now / (60 * I64_1SEC) > this->lastCheckEpgCap / (60 * I64_1SEC) ){
			int basicOnlyFlags = -1;
			__int64 capTime = this->epgCapRequested ? now : GetNextEpgCapTime(now, &basicOnlyFlags);
			if( capTime <= now + 60 * I64_1SEC ){
				vector<DWORD> epgCapIDList = GetEpgCapTunerIDList(now);
				if( epgCapIDList.empty() == false ){
					if( capTime > now ){
						//�擾�J�n1���O
						//���̒ʒm�͂����܂ŎQ�l�B�J�n���Ȃ��̂ɒʒm����\�����A���̋t�����肦��
						this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_PRE_EPGCAP_START, L"�擾�J�n�P���O");
					}else{
						//�擾�J�n
						wstring iniCommonPath;
						GetCommonIniPath(iniCommonPath);
						int lastFlags = (GetPrivateProfileInt(L"SET", L"BSBasicOnly", 1, iniCommonPath.c_str()) != 0 ? 1 : 0) |
						                (GetPrivateProfileInt(L"SET", L"CS1BasicOnly", 1, iniCommonPath.c_str()) != 0 ? 2 : 0) |
						                (GetPrivateProfileInt(L"SET", L"CS2BasicOnly", 1, iniCommonPath.c_str()) != 0 ? 4 : 0);
						if( basicOnlyFlags >= 0 ){
							//�ꎞ�I�ɐݒ��ύX����EPG�擾�`���[�i���̋�����ς���
							//TODO: �p�C�v�R�}���h���g�����ׂ�
							this->epgCapBasicOnlyFlags = lastFlags;
							WritePrivateProfileString(L"SET", L"BSBasicOnly", basicOnlyFlags & 1 ? L"1" : L"0", iniCommonPath.c_str());
							WritePrivateProfileString(L"SET", L"CS1BasicOnly", basicOnlyFlags & 2 ? L"1" : L"0", iniCommonPath.c_str());
							WritePrivateProfileString(L"SET", L"CS2BasicOnly", basicOnlyFlags & 4 ? L"1" : L"0", iniCommonPath.c_str());
						}else{
							this->epgCapBasicOnlyFlags = -1;
							basicOnlyFlags = lastFlags;
						}
						//�e�`���[�i�ɐU�蕪��
						LONGLONG lastKey = -1;
						bool inBS = false;
						bool inCS1 = false;
						bool inCS2 = false;
						size_t listIndex = 0;
						vector<vector<SET_CH_INFO>> epgCapChList(epgCapIDList.size());
						for( map<LONGLONG, CH_DATA5>::const_iterator itr = this->chUtil.GetMap().begin(); itr != this->chUtil.GetMap().end(); itr++ ){
							if( itr->second.epgCapFlag == FALSE ||
							    lastKey >= 0 && lastKey == itr->first >> 16 ||
							    itr->second.originalNetworkID == 4 && (basicOnlyFlags & 1) && inBS ||
							    itr->second.originalNetworkID == 6 && (basicOnlyFlags & 2) && inCS1 ||
							    itr->second.originalNetworkID == 7 && (basicOnlyFlags & 4) && inCS2 ){
								continue;
							}
							lastKey = itr->first >> 16;
							SET_CH_INFO addCh;
							addCh.ONID = itr->second.originalNetworkID;
							addCh.TSID = itr->second.transportStreamID;
							addCh.SID = itr->second.serviceID;
							addCh.useSID = TRUE;
							addCh.useBonCh = FALSE;
							for( size_t i = 0; i < epgCapIDList.size(); i++ ){
								if( this->tunerManager.IsSupportService(epgCapIDList[listIndex], addCh.ONID, addCh.TSID, addCh.SID) ){
									epgCapChList[listIndex].push_back(addCh);
									inBS = inBS || addCh.ONID == 4;
									inCS1 = inCS1 || addCh.ONID == 6;
									inCS2 = inCS2 || addCh.ONID == 7;
									listIndex = (listIndex + 1) % epgCapIDList.size();
									break;
								}
								listIndex = (listIndex + 1) % epgCapIDList.size();
							}
						}
						for( size_t i = 0; i < epgCapIDList.size(); i++ ){
							this->tunerBankMap[epgCapIDList[i]]->StartEpgCap(epgCapChList[i]);
						}
						this->epgCapWork = true;
						this->epgCapSetTimeSync = false;
						this->notifyManager.AddNotify(NOTIFY_UPDATE_EPGCAP_START);
					}
				}
			}
		}
	}else{
		//EPG�擾��
		if( this->epgCapTimeSync && this->epgCapSetTimeSync == false ){
			//���v���킹(�vSE_SYSTEMTIME_NAME����)
			for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
				if( itr->second->GetState() == CTunerBankCtrl::TR_EPGCAP ){
					__int64 delay = itr->second->DelayTime();
					if( delay < -10 * I64_1SEC || 10 * I64_1SEC < delay ){
						SYSTEMTIME setTime;
						ConvertSystemTime(now + delay, &setTime);
						_OutputDebugString(L"��SetLocalTime %s%d\r\n", SetLocalTime(&setTime) ? L"" : L"err ", (int)(delay / I64_1SEC));
						this->epgCapSetTimeSync = true;
					}
				}
			}
		}
		if( isEpgCap == false ){
			//EPG�擾���̃`���[�i�������Ȃ����̂Ŏ擾����
			if( this->epgCapBasicOnlyFlags >= 0 ){
				//EPG�擾�J�n���̐ݒ�������߂�
				wstring iniCommonPath;
				GetCommonIniPath(iniCommonPath);
				WritePrivateProfileString(L"SET", L"BSBasicOnly", this->epgCapBasicOnlyFlags & 1 ? L"1" : L"0", iniCommonPath.c_str());
				WritePrivateProfileString(L"SET", L"CS1BasicOnly", this->epgCapBasicOnlyFlags & 2 ? L"1" : L"0", iniCommonPath.c_str());
				WritePrivateProfileString(L"SET", L"CS2BasicOnly", this->epgCapBasicOnlyFlags & 4 ? L"1" : L"0", iniCommonPath.c_str());
			}
			this->epgCapWork = false;
			doneEpgCap = true;
		}
	}
	this->epgCapRequested = false;
	this->lastCheckEpgCap = now;
	return doneEpgCap;
}

bool CReserveManager::IsActive() const
{
	CBlockLock lock(&this->managerLock);

	if( this->epgCapRequested || this->epgCapWork || this->batManager.GetWorkCount() != 0 || this->batManager.IsWorking() ){
		return true;
	}
	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		if( itr->second->GetState() != CTunerBankCtrl::TR_IDLE ){
			return true;
		}
	}
	return false;
}

__int64 CReserveManager::GetSleepReturnTime(__int64 baseTime) const
{
	CBlockLock lock(&this->managerLock);

	//�ł��߂��\��J�n�����𓾂�
	__int64 nextRec = LLONG_MAX;
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		if( itr->second.recSetting.recMode != RECMODE_NO ){
			__int64 startTime;
			CalcEntireReserveTime(&startTime, NULL, itr->second);
			if( startTime >= baseTime ){
				nextRec = min(nextRec, startTime);
			}
		}
	}
	__int64 capTime = GetNextEpgCapTime(baseTime + 60 * I64_1SEC);
	return min(nextRec, capTime);
}

__int64 CReserveManager::GetNearestRecReserveTime() const
{
	CBlockLock lock(&this->managerLock);

	__int64 minTime = LLONG_MAX;
	for( map<DWORD, RESERVE_DATA>::const_iterator itr = this->reserveText.GetMap().begin(); itr != this->reserveText.GetMap().end(); itr++ ){
		if( itr->second.recSetting.recMode != RECMODE_VIEW &&
		    itr->second.recSetting.recMode != RECMODE_NO ){
			__int64 startTime;
			CalcEntireReserveTime(&startTime, NULL, itr->second);
			minTime = min(startTime, minTime);
		}
	}
	return minTime;
}

__int64 CReserveManager::GetNextEpgCapTime(__int64 now, int* basicOnlyFlags) const
{
	CBlockLock lock(&this->managerLock);

	SYSTEMTIME st;
	ConvertSystemTime(now, &st);
	//���ݎ����ɑ΂�����j������̃I�t�Z�b�g(��)�B�b�ȉ��̐؂�̂Ăɒ���
	int baseTime = st.wDayOfWeek * 1440 + (int)(now / (60 * I64_1SEC) % 1440);
	//baseTime�Ƃ̍����ŏ���EPG�擾������T��
	int minDiff = INT_MAX;
	WORD minVal = 0;
	for( size_t i = 0; i < this->epgCapTimeList.size(); i++ ){
		int diff = (LOWORD(this->epgCapTimeList[i]) + 7 * 1440 - baseTime) % (7 * 1440);
		if( minDiff > diff ){
			minDiff = diff;
			minVal = HIWORD(this->epgCapTimeList[i]);
		}
	}
	if( minDiff == INT_MAX ){
		return LLONG_MAX;
	}
	if( basicOnlyFlags ){
		*basicOnlyFlags = minVal == 0xFF ? -1 : minVal;
	}
	return (now / (60 * I64_1SEC) + minDiff) * (60 * I64_1SEC);
}

bool CReserveManager::IsFindReserve(WORD onid, WORD tsid, WORD sid, WORD eid) const
{
	CBlockLock lock(&this->managerLock);

	const vector<pair<ULONGLONG, DWORD>>& sortList = this->reserveText.GetSortByEventList();

	vector<pair<ULONGLONG, DWORD>>::const_iterator itr = std::lower_bound(
		sortList.begin(), sortList.end(), pair<ULONGLONG, DWORD>(_Create64Key2(onid, tsid, sid, eid), 0));
	return itr != sortList.end() && itr->first == (ULONGLONG)_Create64Key2(onid, tsid, sid, eid);
}

bool CReserveManager::IsFindProgramReserve(WORD onid, WORD tsid, WORD sid, __int64 startTime, DWORD durationSec) const
{
	CBlockLock lock(&this->managerLock);

	const vector<pair<ULONGLONG, DWORD>>& sortList = this->reserveText.GetSortByEventList();

	vector<pair<ULONGLONG, DWORD>>::const_iterator itr = std::lower_bound(
		sortList.begin(), sortList.end(), pair<ULONGLONG, DWORD>(_Create64Key2(onid, tsid, sid, 0xFFFF), 0));
	for( ; itr != sortList.end() && itr->first == (ULONGLONG)_Create64Key2(onid, tsid, sid, 0xFFFF); itr++ ){
		map<DWORD, RESERVE_DATA>::const_iterator itrRes = this->reserveText.GetMap().find(itr->second);
		if( itrRes->second.durationSecond == durationSec && ConvertI64Time(itrRes->second.startTime) == startTime ){
			return true;
		}
	}
	return false;
}

vector<DWORD> CReserveManager::GetSupportServiceTuner(WORD onid, WORD tsid, WORD sid) const
{
	//tunerManager�͔r������̑ΏۊO
	vector<DWORD> idList;
	this->tunerManager.GetSupportServiceTuner(onid, tsid, sid, &idList);
	return idList;
}

bool CReserveManager::GetTunerCh(DWORD tunerID, WORD onid, WORD tsid, WORD sid, DWORD* space, DWORD* ch) const
{
	return this->tunerManager.GetCh(tunerID, onid, tsid, sid, space, ch) != FALSE;
}

wstring CReserveManager::GetTunerBonFileName(DWORD tunerID) const
{
	wstring bonFileName;
	this->tunerManager.GetBonFileName(tunerID, bonFileName);
	return bonFileName;
}

bool CReserveManager::IsOpenTuner(DWORD tunerID) const
{
	CBlockLock lock(&this->managerLock);

	map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.find(tunerID);
	return itr != this->tunerBankMap.end() && itr->second->GetState() != CTunerBankCtrl::TR_IDLE;
}

bool CReserveManager::SetNWTVCh(bool nwUdp, bool nwTcp, const SET_CH_INFO& chInfo, const vector<DWORD>& tunerIDList)
{
	CBlockLock lock(&this->managerLock);

	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		if( itr->second->GetState() == CTunerBankCtrl::TR_NWTV ){
			//���łɋN�����Ă���̂Ŏg������g��
			if( this->tunerManager.IsSupportService(itr->first, chInfo.ONID, chInfo.TSID, chInfo.SID) ){
				itr->second->SetNWTVCh(nwUdp, nwTcp, chInfo);
				return true;
			}
			itr->second->CloseNWTV();
			break;
		}
	}
	for( size_t i = 0; i < tunerIDList.size(); i++ ){
		if( this->tunerManager.IsSupportService(tunerIDList[i], chInfo.ONID, chInfo.TSID, chInfo.SID) ){
			map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.find(tunerIDList[i]);
			if( itr != this->tunerBankMap.end() && itr->second->SetNWTVCh(nwUdp, nwTcp, chInfo) ){
				return true;
			}
		}
	}
	return false;
}

bool CReserveManager::CloseNWTV()
{
	CBlockLock lock(&this->managerLock);

	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		if( itr->second->GetState() == CTunerBankCtrl::TR_NWTV ){
			itr->second->CloseNWTV();
			return true;
		}
	}
	return false;
}

bool CReserveManager::GetRecFilePath(DWORD reserveID, wstring& filePath, DWORD* ctrlID, DWORD* processID) const
{
	CBlockLock lock(&this->managerLock);

	for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = this->tunerBankMap.begin(); itr != this->tunerBankMap.end(); itr++ ){
		if( itr->second->GetRecFilePath(reserveID, filePath, ctrlID, processID) ){
			return true;
		}
	}
	return false;
}

bool CReserveManager::IsFindRecEventInfo(const EPGDB_EVENT_INFO& info, const EPGDB_SEARCH_KEY_INFO& key) const
{
	CBlockLock lock(&this->managerLock);
	bool ret = false;

	CoInitialize(NULL);
	try{
		IRegExpPtr regExp;
		regExp.CreateInstance(CLSID_RegExp);
		if( regExp != NULL && info.shortInfo != NULL ){
			wstring infoEventName = info.shortInfo->event_name;
			if( this->recInfo2RegExp.empty() == false ){
				regExp->PutGlobal(VARIANT_TRUE);
				regExp->PutPattern(_bstr_t(this->recInfo2RegExp.c_str()));
				_bstr_t rpl = regExp->Replace(_bstr_t(infoEventName.c_str()), _bstr_t());
				infoEventName = (LPCWSTR)rpl == NULL ? L"" : (LPCWSTR)rpl;
			}
			if( infoEventName.empty() == false && info.StartTimeFlag != 0 ){
				map<DWORD, PARSE_REC_INFO2_ITEM>::const_iterator itr;
				for( itr = this->recInfo2Text.GetMap().begin(); itr != this->recInfo2Text.GetMap().end(); itr++ ){
					if( ( key.chkRecNoService == 1 || itr->second.originalNetworkID == info.original_network_id &&
						itr->second.transportStreamID == info.transport_stream_id &&
						itr->second.serviceID == info.service_id ) &&
						ConvertI64Time(itr->second.startTime) + key.chkRecDay*24*60*60*I64_1SEC > ConvertI64Time(info.start_time) ){
						wstring eventName = itr->second.eventName;
						if( this->recInfo2RegExp.empty() == false ){
							_bstr_t rpl = regExp->Replace(_bstr_t(eventName.c_str()), _bstr_t());
							eventName = (LPCWSTR)rpl == NULL ? L"" : (LPCWSTR)rpl;
						}
						if( infoEventName == eventName ){
							ret = true;
							break;
						}
					}
				}
			}
		}
	}catch( _com_error& e ){
		_OutputDebugString(L"%s\r\n", e.ErrorMessage());
	}
	CoUninitialize();

	return ret;
}

bool CReserveManager::ChgAutoAddNoRec(WORD onid, WORD tsid, WORD sid, WORD eid)
{
	CBlockLock lock(&this->managerLock);

	vector<RESERVE_DATA> chgList;
	const vector<pair<ULONGLONG, DWORD>>& sortList = this->reserveText.GetSortByEventList();

	vector<pair<ULONGLONG, DWORD>>::const_iterator itr = std::lower_bound(
		sortList.begin(), sortList.end(), pair<ULONGLONG, DWORD>(_Create64Key2(onid, tsid, sid, eid), 0));
	for( ; itr != sortList.end() && itr->first == (ULONGLONG)_Create64Key2(onid, tsid, sid, eid); itr++ ){
		map<DWORD, RESERVE_DATA>::const_iterator itrRes = this->reserveText.GetMap().find(itr->second);
		if( itrRes->second.recSetting.recMode != RECMODE_NO && itrRes->second.comment.compare(0, 7, L"EPG�����\��") == 0 ){
			chgList.push_back(itrRes->second);
			chgList.back().recSetting.recMode = RECMODE_NO;
		}
	}
	return chgList.empty() == false && ChgReserveData(chgList);
}

vector<CH_DATA5> CReserveManager::GetChDataList() const
{
	CBlockLock lock(&this->managerLock);

	vector<CH_DATA5> list;
	list.reserve(this->chUtil.GetMap().size());
	for( map<LONGLONG, CH_DATA5>::const_iterator itr = this->chUtil.GetMap().begin(); itr != this->chUtil.GetMap().end(); itr++ ){
		list.push_back(itr->second);
	}
	return list;
}

UINT WINAPI CReserveManager::WatchdogThread(LPVOID param)
{
	CReserveManager* sys = (CReserveManager*)param;
	while( WaitForSingleObject(sys->watchdogStopEvent, 2000) == WAIT_TIMEOUT ){
		for( map<DWORD, CTunerBankCtrl*>::const_iterator itr = sys->tunerBankMap.begin(); itr != sys->tunerBankMap.end(); itr++ ){
			itr->second->Watch();
		}
	}
	return 0;
}
