#include "StdAfx.h"
#include "TunerBankCtrl.h"
#include "../../Common/EpgTimerUtil.h"
#include "../../Common/SendCtrlCmd.h"
#include "../../Common/PathUtil.h"
#include "../../Common/ReNamePlugInUtil.h"
#include "../../Common/BlockLock.h"
#include "../../Common/TimeUtil.h"
#include <tlhelp32.h>

CTunerBankCtrl::CTunerBankCtrl(DWORD tunerID_, LPCWSTR bonFileName_, const vector<CH_DATA4>& chList_, CNotifyManager& notifyManager_, CEpgDBManager& epgDBManager_)
	: tunerID(tunerID_)
	, bonFileName(bonFileName_)
	, chList(chList_)
	, notifyManager(notifyManager_)
	, epgDBManager(epgDBManager_)
	, hTunerProcess(NULL)
	, specialState(TR_IDLE)
	, delayTime(0)
	, epgCapDelayTime(0)
{
	InitializeCriticalSection(&this->watchContext.lock);
	this->watchContext.count = 0;
}

CTunerBankCtrl::~CTunerBankCtrl()
{
	if( this->hTunerProcess ){
		CloseHandle(this->hTunerProcess);
	}
	DeleteCriticalSection(&this->watchContext.lock);
}

void CTunerBankCtrl::ReloadSetting()
{
	//���W���[��ini�ȊO�̃p�����[�^�͕K�v�ȂƂ��ɂ��̏�Ŏ擾����
	wstring iniPath;
	GetModuleIniPath(iniPath);
	this->recWakeTime = (__int64)GetPrivateProfileInt(L"SET", L"RecAppWakeTime", 2, iniPath.c_str()) * 60 * I64_1SEC;
	this->recWakeTime = max(this->recWakeTime, READY_MARGIN * I64_1SEC);
	this->recMinWake = GetPrivateProfileInt(L"SET", L"RecMinWake", 1, iniPath.c_str()) != 0;
	this->recView = GetPrivateProfileInt(L"SET", L"RecView", 1, iniPath.c_str()) != 0;
	this->recNW = GetPrivateProfileInt(L"SET", L"RecNW", 0, iniPath.c_str()) != 0;
	this->backPriority = GetPrivateProfileInt(L"SET", L"BackPriority", 1, iniPath.c_str()) != 0;
	this->saveProgramInfo = GetPrivateProfileInt(L"SET", L"PgInfoLog", 0, iniPath.c_str()) != 0;
	this->saveErrLog = GetPrivateProfileInt(L"SET", L"DropLog", 0, iniPath.c_str()) != 0;
	this->recOverWrite = GetPrivateProfileInt(L"SET", L"RecOverWrite", 0, iniPath.c_str()) != 0;
	int pr = GetPrivateProfileInt(L"SET", L"ProcessPriority", 3, iniPath.c_str());
	this->processPriority =
		pr == 0 ? REALTIME_PRIORITY_CLASS :
		pr == 1 ? HIGH_PRIORITY_CLASS :
		pr == 2 ? ABOVE_NORMAL_PRIORITY_CLASS :
		pr == 3 ? NORMAL_PRIORITY_CLASS :
		pr == 4 ? BELOW_NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS;
	this->keepDisk = GetPrivateProfileInt(L"SET", L"KeepDisk", 1, iniPath.c_str()) != 0;
	this->recNameNoChkYen = GetPrivateProfileInt(L"SET", L"NoChkYen", 0, iniPath.c_str()) != 0;
	this->recNamePlugInFileName.clear();
	if( GetPrivateProfileInt(L"SET", L"RecNamePlugIn", 0, iniPath.c_str()) != 0 ){
		WCHAR buff[512];
		GetPrivateProfileString(L"SET", L"RecNamePlugInFile", L"RecName_Macro.dll", buff, 512, iniPath.c_str());
		this->recNamePlugInFileName = buff;
	}
}

bool CTunerBankCtrl::AddReserve(const TUNER_RESERVE& reserve)
{
	if( reserve.reserveID == 0 ||
	    this->reserveMap.count(reserve.reserveID) != 0 ||
	    reserve.recMode > RECMODE_VIEW ){
		return false;
	}
	TUNER_RESERVE& r = this->reserveMap.insert(std::make_pair(reserve.reserveID, reserve)).first->second;
	r.startOrder = (r.startTime - r.startMargin) / I64_1SEC << 16 | r.reserveID & 0xFFFF;
	r.effectivePriority = (this->backPriority ? -1 : 1) * ((__int64)((this->backPriority ? r.priority : ~r.priority) & 7) << 60 | r.startOrder);
	r.state = TR_IDLE;
	return true;
}

bool CTunerBankCtrl::ChgCtrlReserve(TUNER_RESERVE* reserve)
{
	map<DWORD, TUNER_RESERVE>::iterator itr = this->reserveMap.find(reserve->reserveID);
	if( itr != this->reserveMap.end() && itr->second.state != TR_IDLE ){
		//�����p�����[�^��ޔ�
		TUNER_RESERVE save = itr->second;
		//�ύX�ł��Ȃ��t�B�[���h���㏑��
		reserve->onid = save.onid;
		reserve->tsid = save.tsid;
		reserve->sid = save.sid;
		//�v���O�����\��ւ̕ύX�̂ݔF�߂�
		if( reserve->eid != 0xFFFF ){
			reserve->eid = save.eid;
		}
		reserve->recMode = save.recMode;
		reserve->priority = save.priority;
		reserve->enableCaption = save.enableCaption;
		reserve->enableData = save.enableData;
		reserve->pittari = save.pittari;
		reserve->partialRecMode = save.partialRecMode;
		reserve->recFolder = save.recFolder;
		reserve->partialRecFolder = save.partialRecFolder;
		//����ړ��͒��ӁB�Ȃ��O���ړ��͂ǂꂾ���傫���Ă�����Check()�ŗ\��I�����邾���Ȃ̂Ŗ��Ȃ�
		if( reserve->startTime - reserve->startMargin > save.startTime - save.startMargin ){
			__int64 now = GetNowI64Time() + this->delayTime;
			if( reserve->startTime - reserve->startMargin - 60 * I64_1SEC > now ){
				reserve->startTime = save.startTime;
				reserve->startMargin = save.startMargin;
			}
		}
		TUNER_RESERVE& r = itr->second = *reserve;
		//�����p�����[�^�𕜌�
		r.startOrder = (r.startTime - r.startMargin) / I64_1SEC << 16 | r.reserveID & 0xFFFF;
		r.effectivePriority = (this->backPriority ? -1 : 1) * ((__int64)((this->backPriority ? r.priority : ~r.priority) & 7) << 60 | r.startOrder);
		r.state = save.state;
		r.ctrlID[0] = save.ctrlID[0];
		r.ctrlID[1] = save.ctrlID[1];
		r.notStartHead = save.notStartHead;
		r.appendPgInfo = save.appendPgInfo;
		r.savedPgInfo = save.savedPgInfo;
		r.epgStartTime = save.epgStartTime;
		r.epgEventName = save.epgEventName;
		return true;
	}
	return false;
}

bool CTunerBankCtrl::DelReserve(DWORD reserveID)
{
	map<DWORD, TUNER_RESERVE>::iterator itr = this->reserveMap.find(reserveID);
	if( itr != this->reserveMap.end() ){
		if( itr->second.state != TR_IDLE ){
			//hTunerProcess�͕K��!NULL
			CWatchBlock watchBlock(&this->watchContext);
			CSendCtrlCmd ctrlCmd;
			ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
			for( int i = 0; i < 2; i++ ){
				if( itr->second.ctrlID[i] != 0 ){
					if( itr->second.state == TR_REC && itr->second.recMode != RECMODE_VIEW ){
						SET_CTRL_REC_STOP_PARAM param;
						param.ctrlID = itr->second.ctrlID[i];
						param.saveErrLog = this->saveErrLog;
						SET_CTRL_REC_STOP_RES_PARAM resVal;
						ctrlCmd.SendViewStopRec(param, &resVal);
					}
					ctrlCmd.SendViewDeleteCtrl(itr->second.ctrlID[i]);
				}
			}
			if( itr->second.state == TR_REC ){
				//�^��I���ɔ�����GUI�L�[�v���������ꂽ��������Ȃ�
				this->tunerResetLock = true;
			}
		}
		this->reserveMap.erase(itr);
		return true;
	}
	return false;
}

void CTunerBankCtrl::ClearNoCtrl(__int64 startTime)
{
	for( map<DWORD, TUNER_RESERVE>::iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); ){
		if( itr->second.state == TR_IDLE && itr->second.startTime - itr->second.startMargin >= startTime ){
			this->reserveMap.erase(itr++);
		}else{
			itr++;
		}
	}
}

vector<DWORD> CTunerBankCtrl::GetReserveIDList() const
{
	vector<DWORD> list;
	list.reserve(this->reserveMap.size());
	for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
		list.push_back(itr->first);
	}
	return list;
}

vector<CTunerBankCtrl::CHECK_RESULT> CTunerBankCtrl::Check(vector<DWORD>* startedReserveIDList)
{
	vector<CHECK_RESULT> retList;

	if( this->hTunerProcess && WaitForSingleObject(this->hTunerProcess, 0) != WAIT_TIMEOUT ){
		//�`���[�i���\����������ꂽ
		CloseTuner();
		this->specialState = TR_IDLE;
		//TR_IDLE�łȂ��S�\��𑒂�
		for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); ){
			if( itr->second.state != TR_IDLE ){
				CHECK_RESULT ret;
				ret.type = CHECK_ERR_REC;
				ret.reserveID = itr->first;
				retList.push_back(ret);
				this->reserveMap.erase(itr++);
			}else{
				itr++;
			}
		}
	}

	CWatchBlock watchBlock(&this->watchContext);
	CSendCtrlCmd ctrlCmd;
	if( this->hTunerProcess ){
		//�`���[�i�N�����ɂ͂�����ēx�ĂԂ���
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
	}

	if( this->specialState == TR_EPGCAP ){
		DWORD status;
		if( ctrlCmd.SendViewGetStatus(&status) == CMD_SUCCESS ){
			if( status != VIEW_APP_ST_GET_EPG ){
				//�擾�I�����
				OutputDebugString(L"epg end\r\n");
				CloseTuner();
				this->specialState = TR_IDLE;
			}
		}else{
			//�G���[
			OutputDebugString(L"epg err\r\n");
			CloseTuner();
			this->specialState = TR_IDLE;
		}
	}else if( this->specialState == TR_NWTV ){
		//�l�b�g���[�N���[�h�ł�GUI�L�[�v�ł��Ȃ��̂�BonDriver���ύX����邩������Ȃ�
		//BonDriver���ύX���ꂽ�`���[�i�͂��̃o���N�̊Ǘ����ɒu���Ȃ��̂ŁA�l�b�g���[�N���[�h����������
		wstring bonDriver;
		if( ctrlCmd.SendViewGetBonDrivere(&bonDriver) == CMD_SUCCESS && CompareNoCase(bonDriver, this->bonFileName) != 0 ){
			if( ctrlCmd.SendViewSetID(-1) == CMD_SUCCESS ){
				CBlockLock lock(&this->watchContext.lock);
				CloseHandle(this->hTunerProcess);
				this->hTunerProcess = NULL;
				this->specialState = TR_IDLE;
			}else{
				//ID���D�Ɏ��s�����̂ŏ����Ă��炤�����Ȃ�
				CloseNWTV();
			}
			//TODO: �ėp�̃��O�p���b�Z�[�W�����݂��Ȃ��̂ŁA��ނ𓾂�NOTIFY_UPDATE_REC_END�Ōx������
			this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_REC_END,
				L"BonDriver���ύX���ꂽ����Network���[�h���������܂���\r\n�ύX����BonDriver�ɘ^��̗\�肪�Ȃ������ӂ��Ă�������");
		}
	}else if( this->hTunerProcess && this->tunerChLocked == false ){
		//GUI�L�[�v����Ă��Ȃ��̂�BonDriver���ύX����邩������Ȃ�
		wstring bonDriver;
		if( ctrlCmd.SendViewGetBonDrivere(&bonDriver) == CMD_SUCCESS && CompareNoCase(bonDriver, this->bonFileName) != 0 ){
			if( ctrlCmd.SendViewSetID(-1) == CMD_SUCCESS ){
				CBlockLock lock(&this->watchContext.lock);
				CloseHandle(this->hTunerProcess);
				this->hTunerProcess = NULL;
			}else{
				//ID���D�Ɏ��s�����̂ŏ����Ă��炤�����Ȃ�
				CloseTuner();
			}
			//TR_IDLE�łȂ��S�\��𑒂�
			for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); ){
				if( itr->second.state != TR_IDLE ){
					CHECK_RESULT ret;
					ret.type = CHECK_ERR_REC;
					ret.reserveID = itr->first;
					retList.push_back(ret);
					this->reserveMap.erase(itr++);
				}else{
					itr++;
				}
			}
		}
	}

	this->delayTime = 0;
	this->epgCapDelayTime = 0;
	if( this->hTunerProcess && this->specialState != TR_NWTV ){
		//PC���v�Ƃ̌덷�擾
		int delaySec;
		if( ctrlCmd.SendViewGetDelay(&delaySec) == CMD_SUCCESS ){
			//������l��͂�ł������Ȃ��ƂɂȂ�Ȃ��悤�AEPG�擾���̒l�͏�ԑJ�ڂ̎Q�l�ɂ��Ȃ�
			if( this->specialState == TR_EPGCAP ){
				this->epgCapDelayTime = delaySec * I64_1SEC;
			}else{
				this->delayTime = delaySec * I64_1SEC;
			}
		}
	}
	__int64 now = GetNowI64Time() + this->delayTime;

	//�I�����Ԃ��߂����\���������ATR_IDLE->TR_READY�ȊO�̑J�ڂ�����
	vector<pair<__int64, DWORD>> idleList;
	bool ngResetLock = false;
	for( map<DWORD, TUNER_RESERVE>::iterator itrRes = this->reserveMap.begin(); itrRes != this->reserveMap.end(); ){
		TUNER_RESERVE& r = itrRes->second;
		CHECK_RESULT ret;
		ret.type = 0;
		switch( r.state ){
		case TR_IDLE:
			if( r.startTime + r.endMargin + r.durationSecond * I64_1SEC < now ){
				ret.type = CHECK_ERR_PASS;
			}
			//�J�n�����b���x�Ȃ̂ŁA�O��֌W���m���ɂ��邽�ߊJ�n���Ԃ͕K���b���x�ň���
			else if( (r.startTime - r.startMargin - this->recWakeTime) / I64_1SEC < now / I64_1SEC ){
				//�^��J�nrecWakeTime�O�`
				idleList.push_back(std::make_pair(r.startOrder, r.reserveID));
			}
			break;
		case TR_READY:
			if( r.startTime + r.endMargin + r.durationSecond * I64_1SEC < now ){
				for( int i = 0; i < 2; i++ ){
					if( r.ctrlID[i] != 0 ){
						ctrlCmd.SendViewDeleteCtrl(r.ctrlID[i]);
					}
				}
				ret.type = CHECK_ERR_PASS;
			}
			//�p�C�v�R�}���h�ɂ̓`�����l���ύX�̊����𒲂ׂ�d�g�݂��Ȃ��̂ŁA�Ó��Ȏ��Ԃ����҂�
			else if( GetTickCount() - this->tunerChChgTick > 5000 && r.startTime - r.startMargin < now ){
				//�^��J�n�`
				if( RecStart(r, now) ){
					//�r������J�n���ꂽ��
					r.notStartHead = r.startTime - r.startMargin + 60 * I64_1SEC < now;
					r.appendPgInfo = false;
					r.savedPgInfo = false;
					r.state = TR_REC;
					if( r.recMode == RECMODE_VIEW ){
						//�����\��łȂ��\��1�ł�����΁u�������[�h�v�ɂ��Ȃ�
						map<DWORD, TUNER_RESERVE>::const_iterator itr;
						for( itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
							if( itr->second.state != TR_IDLE && itr->second.recMode != RECMODE_VIEW ){
								break;
							}
						}
						if( itr == this->reserveMap.end() ){
							//�u�������[�h�v�ɂ����GUI�L�[�v����������Ă��܂����߃`�����l����c�����邱�Ƃ͂ł��Ȃ�
							ctrlCmd.SendViewSetStandbyRec(2);
							this->tunerChLocked = false;
							if( this->recView ){
								ctrlCmd.SendViewExecViewApp();
							}
						}
					}
					if( startedReserveIDList ){
						startedReserveIDList->push_back(r.reserveID);
					}
				}else{
					//�J�n�ł��Ȃ�����
					ret.type = CHECK_ERR_RECSTART;
				}
			}
			break;
		case TR_REC:
			{
				//�X�e�[�^�X�m�F
				DWORD status;
				if( r.recMode != RECMODE_VIEW && ctrlCmd.SendViewGetStatus(&status) == CMD_SUCCESS && status != VIEW_APP_ST_REC ){
					//�L�����Z�����ꂽ�H
					ret.type = CHECK_ERR_REC;
					this->tunerResetLock = true;
				}else if( r.startTime + r.endMargin + r.durationSecond * I64_1SEC < now ){
					ret.type = CHECK_ERR_REC;
					ret.continueRec = false;
					ret.drops = 0;
					ret.scrambles = 0;
					bool isMainCtrl = true;
					for( int i = 0; i < 2; i++ ){
						if( r.ctrlID[i] != 0 ){
							if( r.recMode == RECMODE_VIEW ){
								if( isMainCtrl ){
									ret.type = CHECK_END;
								}
							}else{
								SET_CTRL_REC_STOP_PARAM param;
								param.ctrlID = r.ctrlID[i];
								param.saveErrLog = this->saveErrLog;
								SET_CTRL_REC_STOP_RES_PARAM resVal;
								if( ctrlCmd.SendViewStopRec(param, &resVal) != CMD_SUCCESS ){
									if( isMainCtrl ){
										ret.type = CHECK_ERR_RECEND;
									}
								}else if( isMainCtrl ){
									ret.type = resVal.subRecFlag ? CHECK_END_END_SUBREC :
									           r.notStartHead ? CHECK_END_NOT_START_HEAD :
									           r.savedPgInfo == false ? CHECK_END_NOT_FIND_PF : CHECK_END;
									ret.recFilePath = resVal.recFilePath;
									ret.drops = resVal.drop;
									ret.scrambles = resVal.scramble;
									ret.epgStartTime = r.epgStartTime;
									ret.epgEventName = r.epgEventName;
								}
							}
							ctrlCmd.SendViewDeleteCtrl(r.ctrlID[i]);
							isMainCtrl = false;
						}
					}
					//�^��I���ɔ�����GUI�L�[�v���������ꂽ��������Ȃ�
					this->tunerResetLock = true;
				}else{
					//�ԑg���m�F
					if( r.savedPgInfo == false && r.recMode != RECMODE_VIEW ){
						GET_EPG_PF_INFO_PARAM val;
						val.ONID = r.onid;
						val.TSID = r.tsid;
						val.SID = r.sid;
						val.pfNextFlag = FALSE;
						EPGDB_EVENT_INFO resVal;
						if( ctrlCmd.SendViewGetEventPF(&val, &resVal) == CMD_SUCCESS &&
						    resVal.StartTimeFlag && resVal.DurationFlag &&
						    ConvertI64Time(resVal.start_time) <= r.startTime + 30 * I64_1SEC &&
						    r.startTime + 30 * I64_1SEC < ConvertI64Time(resVal.start_time) + resVal.durationSec * I64_1SEC &&
						    (r.eid == 0xFFFF || r.eid == resVal.event_id) ){
							//�J�n���Ԃ���30�b�͉߂��Ă���̂ł��̔ԑg��񂪘^�撆�̂��̂̂͂�
							r.savedPgInfo = true;
							r.epgStartTime = resVal.start_time;
							r.epgEventName = resVal.shortInfo ? resVal.shortInfo->event_name : L"";
							//�����H��APR(���s)���܂ނ���
							Replace(r.epgEventName, L"\r\n", L"");
							if( this->saveProgramInfo ){
								for( int i = 0; i < 2; i++ ){
									wstring recPath;
									if( r.ctrlID[i] != 0 && ctrlCmd.SendViewGetRecFilePath(r.ctrlID[i], &recPath) == CMD_SUCCESS ){
										SaveProgramInfo(recPath.c_str(), resVal, r.appendPgInfo);
									}
								}
							}
						}
					}
					//�܂��^�撆�̗\�񂪂���̂�GUI�L�[�v���Đݒ肵�Ă͂����Ȃ�
					ngResetLock = true;
				}
			}
			break;
		}
		if( ret.type != 0 ){
			ret.reserveID = itrRes->first;
			retList.push_back(ret);
			this->reserveMap.erase(itrRes++);
		}else{
			itrRes++;
		}
	}

	//TR_IDLE->TR_READY�̑J�ڂ�҂\����J�n���ɕ��ׂ�
	std::sort(idleList.begin(), idleList.end());

	//TR_IDLE->TR_READY(TR_REC)�̑J�ڂ�����
	for( vector<pair<__int64, DWORD>>::const_iterator itrIdle = idleList.begin(); itrIdle != idleList.end(); itrIdle++ ){
		map<DWORD, TUNER_RESERVE>::iterator itrRes = this->reserveMap.find(itrIdle->second);
		TUNER_RESERVE& r = itrRes->second;
		CHECK_RESULT ret;
		ret.type = 0;
		if( this->hTunerProcess == NULL ){
			//�`���[�i���N������
			SET_CH_INFO initCh;
			initCh.ONID = r.onid;
			initCh.TSID = r.tsid;
			initCh.SID = r.sid;
			initCh.useSID = TRUE;
			initCh.useBonCh = FALSE;
			bool nwUdpTcp = this->recNW || r.recMode == RECMODE_VIEW;
			if( OpenTuner(this->recMinWake, nwUdpTcp, nwUdpTcp, true, &initCh) ||
			    CloseOtherTuner() && OpenTuner(this->recMinWake, nwUdpTcp, nwUdpTcp, true, &initCh) ){
				this->tunerONID = r.onid;
				this->tunerTSID = r.tsid;
				this->tunerChLocked = true;
				this->tunerResetLock = false;
				this->tunerChChgTick = GetTickCount();
				this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_PRE_REC_START, this->bonFileName);
				ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
			}else{
				//�N���ł��Ȃ�����
				ret.type = CHECK_ERR_OPEN;
			}
		}
		if( this->hTunerProcess && (r.startTime - r.startMargin) / I64_1SEC - READY_MARGIN < now / I64_1SEC ){
			//�^��J�nREADY_MARGIN�b�O�`
			//����ł͘^�搧��쐬�͒ʏ�^�掞60�b�O�A���荞�ݘ^�掞15�b�O����
			//�쐬��O�|������K�v�͓��ɂȂ��̂ƁA�`�����l���ύX����EIT[p/f]�擾�܂ł̎��Ԃ��m�ۂł���悤���̕b���ɂ���
			if( this->specialState == TR_EPGCAP ){
				//EPG�擾���L�����Z��(�J�ڒ��f)
				OutputDebugString(L"epg cancel\r\n");
				//CSendCtrlCmd::SendViewEpgCapStop()�͑���Ȃ�(�����Ƀ`���[�i����̂ňӖ����Ȃ�����)
				CloseTuner();
				this->specialState = TR_IDLE;
				break;
			}else if( this->specialState == TR_NWTV ){
				//�l�b�g���[�N���[�h������
				wstring bonDriver;
				DWORD status;
				if( ctrlCmd.SendViewGetBonDrivere(&bonDriver) == CMD_SUCCESS && CompareNoCase(bonDriver, this->bonFileName) == 0 &&
				    ctrlCmd.SendViewGetStatus(&status) == CMD_SUCCESS && (status == VIEW_APP_ST_NORMAL || status == VIEW_APP_ST_ERR_CH_CHG) ){
					//�v���Z�X�������p��
					this->tunerONID = r.onid;
					this->tunerTSID = r.tsid;
					this->tunerChLocked = false;
					this->tunerResetLock = false;
					this->specialState = TR_IDLE;
				}else{
					//�l�b�g���[�N���[�h�I��(�J�ڒ��f)
					CloseNWTV();
					break;
				}
			}
			if( this->tunerONID != r.onid || this->tunerTSID != r.tsid ){
				//�`�����l���Ⴄ�̂ŁATR_IDLE�łȂ��S�\��̗D��x���ׂ�
				map<DWORD, TUNER_RESERVE>::const_iterator itr;
				for( itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
					if( itr->second.state != TR_IDLE && itr->second.effectivePriority < r.effectivePriority ){
						break;
					}
				}
				if( itr == this->reserveMap.end() ){
					//TR_IDLE�łȂ��S�\��͎��������ア�̂ő���
					for( itr = this->reserveMap.begin(); itr != this->reserveMap.end(); ){
						if( itr->second.state != TR_IDLE ){
							CHECK_RESULT retOther;
							retOther.type = CHECK_ERR_REC;
							retOther.reserveID = itr->first;
							retOther.continueRec = false;
							retOther.drops = 0;
							retOther.scrambles = 0;
							bool isMainCtrl = true;
							for( int i = 0; i < 2; i++ ){
								if( itr->second.ctrlID[i] != 0 ){
									if( itr->second.state == TR_REC ){
										if( isMainCtrl ){
											retOther.type = CHECK_END_NEXT_START_END;
										}
										if( itr->second.recMode != RECMODE_VIEW ){
											SET_CTRL_REC_STOP_PARAM param;
											param.ctrlID = itr->second.ctrlID[i];
											param.saveErrLog = this->saveErrLog;
											SET_CTRL_REC_STOP_RES_PARAM resVal;
											if( ctrlCmd.SendViewStopRec(param, &resVal) != CMD_SUCCESS ){
												if( isMainCtrl ){
													retOther.type = CHECK_ERR_RECEND;
												}
											}else if( isMainCtrl ){
												retOther.recFilePath = resVal.recFilePath;
												retOther.drops = resVal.drop;
												retOther.scrambles = resVal.scramble;
											}
										}
									}
									ctrlCmd.SendViewDeleteCtrl(itr->second.ctrlID[i]);
									isMainCtrl = false;
								}
							}
							retList.push_back(retOther);
							this->reserveMap.erase(itr++);
						}else{
							itr++;
						}
					}
					this->tunerONID = r.onid;
					this->tunerTSID = r.tsid;
					this->tunerChLocked = false;
					this->tunerResetLock = false;
				}
			}
			if( this->tunerONID == r.onid && this->tunerTSID == r.tsid ){
				if( this->tunerChLocked == false ){
					//�`�����l���ύX
					SET_CH_INFO chgCh;
					chgCh.ONID = r.onid;
					chgCh.TSID = r.tsid;
					chgCh.SID = r.sid;
					chgCh.useSID = TRUE;
					chgCh.useBonCh = FALSE;
					//�u�\��^��ҋ@���v
					ctrlCmd.SendViewSetStandbyRec(1);
					if( ctrlCmd.SendViewSetCh(&chgCh) == CMD_SUCCESS ){
						this->tunerChLocked = true;
						this->tunerResetLock = false;
						this->tunerChChgTick = GetTickCount();
					}
				}
				if( this->tunerChLocked ){
					//����`�����l���Ȃ̂Ř^�搧����쐬�ł���
					bool continueRec = false;
					for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
						if( itr->second.continueRecFlag &&
						    itr->second.state == TR_REC &&
						    itr->second.sid == r.sid &&
						    itr->second.recMode != RECMODE_VIEW &&
						    itr->second.recMode == r.recMode &&
						    itr->second.enableCaption == r.enableCaption &&
						    itr->second.enableData == r.enableData &&
						    itr->second.partialRecMode == r.partialRecMode ){
							//�A���^��Ȃ̂ŁA���ꐧ��ID�Ř^��J�n���ꂽ���Ƃɂ���BTR_REC�܂őJ�ڂ���̂Œ���
							r.state = TR_REC;
							r.ctrlID[0] = itr->second.ctrlID[0];
							r.ctrlID[1] = itr->second.ctrlID[1];
							r.notStartHead = r.startTime - r.startMargin + 60 * I64_1SEC < now;
							r.appendPgInfo = itr->second.appendPgInfo || itr->second.savedPgInfo;
							r.savedPgInfo = false;
							//���p�����𑒂�
							CHECK_RESULT retOther;
							retOther.type = CHECK_ERR_REC;
							retOther.reserveID = itr->first;
							retOther.continueRec = true;
							retOther.drops = 0;
							retOther.scrambles = 0;
							for( int i = 0; i < 2; i++ ){
								if( itr->second.ctrlID[i] != 0 ){
									if( ctrlCmd.SendViewGetRecFilePath(itr->second.ctrlID[i], &retOther.recFilePath) == CMD_SUCCESS ){
										retOther.type = itr->second.notStartHead ? CHECK_END_NOT_START_HEAD :
										                itr->second.savedPgInfo == false ? CHECK_END_NOT_FIND_PF : CHECK_END;
										retOther.epgStartTime = itr->second.epgStartTime;
										retOther.epgEventName = itr->second.epgEventName;
									}
									break;
								}
							}
							retList.push_back(retOther);
							this->reserveMap.erase(itr);
							continueRec = true;
							break;
						}
					}
					if( continueRec == false ){
						if( CreateCtrl(&r.ctrlID[0], &r.ctrlID[1], r) ){
							r.state = TR_READY;
						}else{
							//�쐬�ł��Ȃ�����
							ret.type = CHECK_ERR_CTRL;
						}
					}
				}
			}
		}
		if( ret.type != 0 ){
			ret.reserveID = itrRes->first;
			retList.push_back(ret);
			this->reserveMap.erase(itrRes);
		}
	}

	if( IsNeedOpenTuner() == false ){
		//�`���[�i���K�v�Ȃ��Ȃ���
		CloseTuner();
	}
	if( this->hTunerProcess && this->specialState == TR_IDLE && this->tunerResetLock ){
		if( ngResetLock == false ){
			//�u�\��^��ҋ@���v
			ctrlCmd.SendViewSetStandbyRec(1);
		}
		this->tunerResetLock = false;
	}
	return retList;
}

bool CTunerBankCtrl::IsNeedOpenTuner() const
{
	if( this->specialState != TR_IDLE ){
		return true;
	}
	//�߂�l�̐U����h������delayTime���l�����Ă͂����Ȃ�
	__int64 now = GetNowI64Time();
	for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
		if( itr->second.state != TR_IDLE || (itr->second.startTime - itr->second.startMargin - this->recWakeTime) / I64_1SEC < now / I64_1SEC ){
			return true;
		}
	}
	return false;
}

bool CTunerBankCtrl::FindPartialService(WORD onid, WORD tsid, WORD sid, WORD* partialSID, wstring* serviceName) const
{
	vector<CH_DATA4>::const_iterator itr;
	for( itr = this->chList.begin(); itr != this->chList.end(); itr++ ){
		if( itr->originalNetworkID == onid && itr->transportStreamID == tsid && itr->partialFlag != FALSE ){
			if( itr->serviceID != sid ){
				if( partialSID != NULL ){
					*partialSID = itr->serviceID;
				}
				if( serviceName != NULL ){
					*serviceName = itr->serviceName;
				}
				return true;
			}
		}
	}
	return false;
}

bool CTunerBankCtrl::CreateCtrl(DWORD* ctrlID, DWORD* partialCtrlID, const TUNER_RESERVE& reserve) const
{
	if( this->hTunerProcess == NULL ){
		return false;
	}
	BYTE partialRecMode = reserve.recMode == RECMODE_VIEW ? 0 : min(reserve.partialRecMode, 2);
	WORD partialSID = 0;
	if( partialRecMode == 1 || partialRecMode == 2 ){
		if( FindPartialService(reserve.onid, reserve.tsid, reserve.sid, &partialSID, NULL) == false ){
			partialSID = 0;
			if( reserve.partialRecMode == 2 ){
				return false;
			}
		}
	}
	CSendCtrlCmd ctrlCmd;
	ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
	DWORD newID;
	if( ctrlCmd.SendViewCreateCtrl(&newID) != CMD_SUCCESS ){
		return false;
	}

	if( partialRecMode == 2 ){
		//������M�̂�
		*ctrlID = 0;
		*partialCtrlID = newID;
	}else{
		*ctrlID = newID;
		*partialCtrlID = 0;
		if( partialRecMode == 1 && partialSID != 0 && ctrlCmd.SendViewCreateCtrl(partialCtrlID) != CMD_SUCCESS ){
			partialCtrlID = 0;
		}
	}
	SET_CTRL_MODE param;
	if( *ctrlID != 0 ){
		//�ʏ�
		param.ctrlID = *ctrlID;
		param.SID = reserve.recMode == RECMODE_ALL || reserve.recMode == RECMODE_ALL_NOB25 ? 0xFFFF : reserve.sid;
		param.enableScramble = reserve.recMode != RECMODE_ALL_NOB25 && reserve.recMode != RECMODE_SERVICE_NOB25;
		param.enableCaption = reserve.enableCaption;
		param.enableData = reserve.enableData;
		ctrlCmd.SendViewSetCtrlMode(param);
	}
	if( *partialCtrlID != 0 ){
		//������M
		param.ctrlID = *partialCtrlID;
		param.SID = partialSID;
		param.enableScramble = reserve.recMode != RECMODE_ALL_NOB25 && reserve.recMode != RECMODE_SERVICE_NOB25;
		param.enableCaption = reserve.enableCaption;
		param.enableData = reserve.enableData;
		ctrlCmd.SendViewSetCtrlMode(param);
	}
	SYSTEMTIME st;
	ConvertSystemTime(reserve.startTime, &st);
	wstring msg;
	Format(msg, L"%s %04d/%02d/%02d %02d:%02d:%02d�` %s", reserve.stationName.c_str(),
	       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, reserve.title.c_str());
	this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_PRE_REC_START, msg);
	return true;
}

void CTunerBankCtrl::SaveProgramInfo(LPCWSTR recPath, const EPGDB_EVENT_INFO& info, bool append) const
{
	wstring iniCommonPath;
	GetCommonIniPath(iniCommonPath);
	WCHAR buff[512];
	GetPrivateProfileString(L"SET", L"RecInfoFolder", L"", buff, 512, iniCommonPath.c_str());
	wstring infoFolder = buff;
	ChkFolderPath(infoFolder);

	wstring savePath;
	if( infoFolder.empty() ){
		savePath = recPath;
	}else{
		GetFileName(recPath, savePath);
		savePath = infoFolder + L"\\" + savePath;
	}
	savePath += L".program.txt";

	wstring serviceName;
	for( size_t i = 0; i < this->chList.size(); i++ ){
		if( this->chList[i].originalNetworkID == info.original_network_id &&
		    this->chList[i].transportStreamID == info.transport_stream_id &&
		    this->chList[i].serviceID == info.service_id ){
			serviceName = this->chList[i].serviceName;
			break;
		}
	}
	wstring outTextW;
	_ConvertEpgInfoText2(&info, outTextW, serviceName);
	string outText;
	WtoA(outTextW, outText);

	HANDLE hFile = _CreateDirectoryAndFile(savePath.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE ){
		if( append ){
			SetFilePointer(hFile, 0, NULL, FILE_END);
			outText = "\r\n-----------------------\r\n" + outText;
		}
		DWORD dwWrite;
		WriteFile(hFile, outText.c_str(), (DWORD)outText.size(), &dwWrite, NULL);
		CloseHandle(hFile);
	}
}

bool CTunerBankCtrl::RecStart(const TUNER_RESERVE& reserve, __int64 now) const
{
	if( this->hTunerProcess == NULL ){
		return false;
	}
	if( reserve.recMode == RECMODE_VIEW ){
		return true;
	}
	CSendCtrlCmd ctrlCmd;
	ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
	bool isMainCtrl = true;
	for( int i = 0; i < 2; i++ ){
		if( reserve.ctrlID[i] != 0 ){
			SET_CTRL_REC_PARAM param;
			param.ctrlID = reserve.ctrlID[i];
			//saveFolder[].recFileName����łȂ����肱�̃t�B�[���h�����p����邱�Ƃ͂Ȃ�
			param.fileName = L"padding.ts";
			//�����o�͗p�t�@�C����
			param.saveFolder = i == 0 ? reserve.recFolder : reserve.partialRecFolder;
			if( param.saveFolder.empty() ){
				param.saveFolder.resize(1);
				wstring commonIniPath;
				GetCommonIniPath(commonIniPath);
				WCHAR buff[512];
				GetRecFolderPath(param.saveFolder[0].recFolder);
				GetPrivateProfileString(L"SET", L"RecWritePlugIn0", L"", buff, 512, commonIniPath.c_str());
				param.saveFolder[0].writePlugIn = buff;
				param.saveFolder[0].recNamePlugIn = this->recNamePlugInFileName;
			}else{
				for( size_t j = 0; j < param.saveFolder.size(); j++ ){
					if( param.saveFolder[j].recNamePlugIn.empty() ){
						param.saveFolder[j].recNamePlugIn = this->recNamePlugInFileName;
					}
				}
			}
			//recNamePlugIn��W�J���Ď��t�@�C�������Z�b�g
			for( size_t j = 0; j < param.saveFolder.size(); j++ ){
				param.saveFolder[j].recFileName.clear();
				if( param.saveFolder[j].recNamePlugIn.empty() == false ){
					WORD sid = reserve.sid;
					WORD eid = reserve.eid;
					wstring stationName = reserve.stationName;
					if( i != 0 ){
						FindPartialService(reserve.onid, reserve.tsid, reserve.sid, &sid, &stationName);
						eid = 0xFFFF;
					}
					wstring plugInPath;
					GetModuleFolderPath(plugInPath);
					plugInPath += L"\\RecName\\";
					{
						PLUGIN_RESERVE_INFO info;
						ConvertSystemTime(reserve.startTime, &info.startTime);
						info.durationSec = reserve.durationSecond;
						wcscpy_s(info.eventName, reserve.title.c_str());
						info.ONID = reserve.onid;
						info.TSID = reserve.tsid;
						info.SID = sid;
						info.EventID = eid;
						wcscpy_s(info.serviceName, stationName.c_str());
						wcscpy_s(info.bonDriverName, this->bonFileName.c_str());
						info.bonDriverID = this->tunerID >> 16;
						info.tunerID = this->tunerID & 0xFFFF;
						EPG_EVENT_INFO* epgInfo = NULL;
						if( info.EventID != 0xFFFF ){
							EPGDB_EVENT_INFO epgDBInfo;
							if( this->epgDBManager.SearchEpg(info.ONID, info.TSID, info.SID, info.EventID, &epgDBInfo) != FALSE ){
								epgInfo = new EPG_EVENT_INFO;
								CopyEpgInfo(epgInfo, &epgDBInfo);
							}
						}
						info.reserveID = reserve.reserveID;
						info.epgInfo = epgInfo;
						info.sizeOfStruct = 0;
						WCHAR name[512];
						DWORD size = 512;
						if( CReNamePlugInUtil::ConvertRecName3(&info, param.saveFolder[j].recNamePlugIn.c_str(), plugInPath.c_str(), name, &size) ){
							param.saveFolder[j].recFileName = name;
							CheckFileName(param.saveFolder[j].recFileName, this->recNameNoChkYen);
						}
						delete epgInfo;
					}
					param.saveFolder[j].recNamePlugIn.clear();
				}
				//���t�@�C�����͋�ɂ��Ȃ�
				if( param.saveFolder[j].recFileName.empty() ){
					SYSTEMTIME st;
					ConvertSystemTime(max(reserve.startTime, now), &st);
					Format(param.saveFolder[j].recFileName, L"%04d%02d%02d%02d%02d%02X%02X%02d-%s.ts",
					       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
					       this->tunerID >> 16, this->tunerID & 0xFFFF, param.ctrlID, reserve.title.c_str());
					CheckFileName(param.saveFolder[j].recFileName);
				}
			}
			param.overWriteFlag = this->recOverWrite;
			param.pittariFlag = reserve.eid != 0xFFFF && reserve.pittari;
			param.pittariONID = reserve.onid;
			param.pittariTSID = reserve.tsid;
			param.pittariSID = reserve.sid;
			param.pittariEventID = reserve.eid;
			DWORD bitrate = 0;
			if( this->keepDisk ){
				_GetBitrate(reserve.onid, reserve.tsid, reserve.sid, &bitrate);
			}
			param.createSize = (ULONGLONG)(bitrate / 8) * 1000 * reserve.durationSecond;
			if( ctrlCmd.SendViewStartRec(param) != CMD_SUCCESS && isMainCtrl ){
				break;
			}
			isMainCtrl = false;
		}
	}
	if( isMainCtrl == false ){
		SYSTEMTIME st;
		ConvertSystemTime(reserve.startTime, &st);
		wstring msg;
		Format(msg, L"%s %04d/%02d/%02d %02d:%02d:%02d\r\n%s", reserve.stationName.c_str(),
		       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, reserve.title.c_str());
		this->notifyManager.AddNotifyMsg(NOTIFY_UPDATE_REC_START, msg);
		return true;
	}
	return false;
}

CTunerBankCtrl::TR_STATE CTunerBankCtrl::GetState() const
{
	TR_STATE state = TR_IDLE;
	if( this->hTunerProcess ){
		state = TR_OPEN;
		if( this->specialState != TR_IDLE ){
			state = this->specialState;
		}else{
			for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
				if( itr->second.state == TR_REC ){
					state = TR_REC;
					break;
				}else if( itr->second.state == TR_READY ){
					state = TR_READY;
				}
			}
		}
	}
	return state;
}

__int64 CTunerBankCtrl::GetNearestReserveTime() const
{
	__int64 minTime = LLONG_MAX;
	for( map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.begin(); itr != this->reserveMap.end(); itr++ ){
		minTime = min(itr->second.startTime - itr->second.startMargin, minTime);
	}
	return minTime;
}

bool CTunerBankCtrl::StartEpgCap(const vector<SET_CH_INFO>& setChList)
{
	if( setChList.empty() == false && this->hTunerProcess == NULL ){
		//EPG�擾�ɂ��Ă̓`���[�i�̍ė��p�͂��Ȃ�
		if( OpenTuner(this->recMinWake, false, false, true, NULL) ){
			CWatchBlock watchBlock(&this->watchContext);
			CSendCtrlCmd ctrlCmd;
			ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
			//EPG�擾�J�n
			if( ctrlCmd.SendViewEpgCapStart(&setChList) == CMD_SUCCESS ){
				this->specialState = TR_EPGCAP;
				return true;
			}
			CloseTuner();
		}
	}
	return false;
}

bool CTunerBankCtrl::GetCurrentChID(WORD* onid, WORD* tsid) const
{
	if( this->hTunerProcess && this->specialState == TR_IDLE ){
		*onid = this->tunerONID;
		*tsid = this->tunerTSID;
		return true;
	}
	return false;
}

bool CTunerBankCtrl::SearchEpgInfo(WORD sid, WORD eid, EPGDB_EVENT_INFO* resVal) const
{
	if( this->hTunerProcess && this->specialState == TR_IDLE ){
		CWatchBlock watchBlock(&this->watchContext);
		CSendCtrlCmd ctrlCmd;
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
		SEARCH_EPG_INFO_PARAM val;
		val.ONID = this->tunerONID;
		val.TSID = this->tunerTSID;
		val.SID = sid;
		val.eventID = eid;
		val.pfOnlyFlag = 0;
		if( ctrlCmd.SendViewSearchEvent(&val, resVal) == CMD_SUCCESS ){
			if( resVal->shortInfo ){
				//�����H��APR(���s)���܂ނ���
				Replace(resVal->shortInfo->event_name, L"\r\n", L"");
			}
			return true;
		}
	}
	return false;
}

int CTunerBankCtrl::GetEventPF(WORD sid, bool pfNextFlag, EPGDB_EVENT_INFO* resVal) const
{
	//�`�����l���ύX��v�����Ă���ŏ���EIT[p/f]���͂��Ó��Ȏ��Ԃ����҂�
	//TODO: �����\��(=GUI�L�[�v����Ă��Ȃ��Ƃ�)�Ƀ`�����l���ύX�����ƍŐV�̏��łȂ��Ȃ�\��������B���d�l�ł͉�����Ȃ�
	if( this->hTunerProcess && this->specialState == TR_IDLE && (this->tunerChLocked == false || GetTickCount() - this->tunerChChgTick > 8000) ){
		CWatchBlock watchBlock(&this->watchContext);
		CSendCtrlCmd ctrlCmd;
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
		GET_EPG_PF_INFO_PARAM val;
		val.ONID = this->tunerONID;
		val.TSID = this->tunerTSID;
		val.SID = sid;
		val.pfNextFlag = pfNextFlag;
		DWORD ret = ctrlCmd.SendViewGetEventPF(&val, resVal);
		if( ret == CMD_SUCCESS ){
			if( resVal->shortInfo ){
				//�����H��APR(���s)���܂ނ���
				Replace(resVal->shortInfo->event_name, L"\r\n", L"");
			}
			return 0;
		}else if( ret == CMD_ERR && (this->tunerChLocked == false || GetTickCount() - this->tunerChChgTick > 15000) ){
			return 1;
		}
		//�ŏ���TOT���͂��܂ł́A����̂ɏ�����\��������
	}
	return 2;
}

__int64 CTunerBankCtrl::DelayTime() const
{
	return this->specialState == TR_EPGCAP ? this->epgCapDelayTime : this->delayTime;
}

bool CTunerBankCtrl::SetNWTVCh(bool nwUdp, bool nwTcp, const SET_CH_INFO& chInfo)
{
	if( this->hTunerProcess == NULL ){
		if( OpenTuner(true, nwUdp, nwTcp, false, &chInfo) ){
			this->specialState = TR_NWTV;
			return true;
		}
	}else if( this->specialState == TR_NWTV ){
		CWatchBlock watchBlock(&this->watchContext);
		CSendCtrlCmd ctrlCmd;
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
		ctrlCmd.SendViewSetCh(&chInfo);
		return true;
	}
	return false;
}

void CTunerBankCtrl::CloseNWTV()
{
	if( this->hTunerProcess && this->specialState == TR_NWTV ){
		CloseTuner();
		this->specialState = TR_IDLE;
	}
}

bool CTunerBankCtrl::GetRecFilePath(DWORD reserveID, wstring& filePath, DWORD* ctrlID, DWORD* processID) const
{
	map<DWORD, TUNER_RESERVE>::const_iterator itr = this->reserveMap.find(reserveID);
	if( itr != this->reserveMap.end() && itr->second.state == TR_REC && itr->second.recMode != RECMODE_VIEW ){
		CWatchBlock watchBlock(&this->watchContext);
		CSendCtrlCmd ctrlCmd;
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
		wstring recFilePath;
		DWORD mainCtrlID = itr->second.ctrlID[0] ? itr->second.ctrlID[0] : itr->second.ctrlID[1];
		if( ctrlCmd.SendViewGetRecFilePath(mainCtrlID, &recFilePath) == CMD_SUCCESS ){
			filePath = recFilePath;
			*ctrlID = mainCtrlID;
			*processID = this->tunerPid;
			return true;
		}
	}
	return false;
}

bool CTunerBankCtrl::OpenTuner(bool minWake, bool nwUdp, bool nwTcp, bool standbyRec, const SET_CH_INFO* initCh)
{
	if( this->hTunerProcess ){
		return false;
	}
	wstring commonIniPath;
	GetCommonIniPath(commonIniPath);
	wstring strIni;
	GetModuleFolderPath(strIni);
	strIni += L"\\ViewApp.ini";
	WCHAR buff[512];

	GetPrivateProfileString(L"SET", L"RecExePath", L"", buff, 512, commonIniPath.c_str());
	wstring strExecute = buff;
	if( strExecute.empty() ){
		GetModuleFolderPath(strExecute);
		strExecute += L"\\EpgDataCap_Bon.exe";
	}

	GetPrivateProfileString(L"APP_CMD_OPT", L"Bon", L"-d", buff, 512, strIni.c_str());
	wstring strParam = wstring(L" ") + buff + L" " + this->bonFileName;

	if( minWake ){
		GetPrivateProfileString(L"APP_CMD_OPT", L"Min", L"-min", buff, 512, strIni.c_str());
		strParam += L" ";
		strParam += buff;
	}
	//����"-noview"�͈���Ȃ�(���܂̂Ƃ��뉽�̌��ʂ��Ȃ�����)
	if( nwUdp == false && nwTcp == false ){
		GetPrivateProfileString(L"APP_CMD_OPT", L"NetworkOff", L"-nonw", buff, 512, strIni.c_str());
		strParam += L" ";
		strParam += buff;
	}else{
		strParam += nwUdp ? L" -nwudp" : L"";
		strParam += nwTcp ? L" -nwtcp" : L"";
	}

	//����ƈقȂ�C�x���g�I�u�W�F�N�g"Global\\EpgTimerSrv_OpenTuner_Event"�ɂ��r������͂��Ȃ�
	//�܂��A�V�F�����������(���T�[�r�X���[�h�łȂ�����)GUI�o�R�łȂ�����CreateProcess()����̂Œ���
	if( GetShellWindow() == NULL ){
		OutputDebugString(L"GetShellWindow() failed\r\n");
		//�\���ł��Ȃ��\���������̂�GUI�o�R�ŋN�����Ă݂�
		CSendCtrlCmd ctrlCmd;
		map<DWORD, DWORD> registGUIMap;
		this->notifyManager.GetRegistGUI(&registGUIMap);
		for( map<DWORD, DWORD>::iterator itr = registGUIMap.begin(); itr != registGUIMap.end(); itr++ ){
			ctrlCmd.SetPipeSetting(CMD2_GUI_CTRL_WAIT_CONNECT, CMD2_GUI_CTRL_PIPE, itr->first);
			if( ctrlCmd.SendGUIExecute(L'"' + strExecute + L'"' + strParam, &this->tunerPid) == CMD_SUCCESS ){
				//�n���h���J���O�ɏI�����邩������Ȃ�
				this->hTunerProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_SET_INFORMATION, FALSE, this->tunerPid);
				if( this->hTunerProcess ){
					SetPriorityClass(this->hTunerProcess, this->processPriority);
					break;
				}
			}
		}
	}
	if( this->hTunerProcess == NULL ){
		PROCESS_INFORMATION pi;
		STARTUPINFO si = {};
		si.cb = sizeof(si);
		vector<WCHAR> strBuff(strParam.c_str(), strParam.c_str() + strParam.size() + 1);
		if( CreateProcess(strExecute.c_str(), &strBuff.front(), NULL, NULL, FALSE, this->processPriority, NULL, NULL, &si, &pi) != FALSE ){
			CloseHandle(pi.hThread);
			this->hTunerProcess = pi.hProcess;
			this->tunerPid = pi.dwProcessId;
		}
	}
	if( this->hTunerProcess ){
		//ID�̃Z�b�g
		CWatchBlock watchBlock(&this->watchContext);
		CSendCtrlCmd ctrlCmd;
		ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
		ctrlCmd.SetConnectTimeOut(0);
		//�N�������܂ōő�30�b�قǑ҂�
		for( int i = 0; i < 300; i++ ){
			Sleep(100);
			if( WaitForSingleObject(this->hTunerProcess, 0) != WAIT_TIMEOUT ){
				break;
			}else if( ctrlCmd.SendViewSetID(this->tunerID) == CMD_SUCCESS ){
				ctrlCmd.SetConnectTimeOut(CONNECT_TIMEOUT);
				//�N���X�e�[�^�X���m�F
				DWORD status;
				if( ctrlCmd.SendViewGetStatus(&status) == CMD_SUCCESS && status == VIEW_APP_ST_ERR_BON ){
					break;
				}
				if( standbyRec ){
					//�u�\��^��ҋ@���v
					ctrlCmd.SendViewSetStandbyRec(1);
				}
				if( initCh ){
					ctrlCmd.SendViewSetCh(initCh);
				}
				return true;
			}
		}
		CloseTuner();
	}
	return false;
}

void CTunerBankCtrl::CloseTuner()
{
	if( this->hTunerProcess ){
		if( WaitForSingleObject(this->hTunerProcess, 0) == WAIT_TIMEOUT ){
			CWatchBlock watchBlock(&this->watchContext);
			CSendCtrlCmd ctrlCmd;
			ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
			if( ctrlCmd.SendViewAppClose() != CMD_SUCCESS || WaitForSingleObject(this->hTunerProcess, 30000) == WAIT_TIMEOUT ){
				//�Ԃ��E��
				TerminateProcess(this->hTunerProcess, 0xFFFFFFFF);
			}
		}
		CBlockLock lock(&this->watchContext.lock);
		CloseHandle(this->hTunerProcess);
		this->hTunerProcess = NULL;
	}
}

bool CTunerBankCtrl::CloseOtherTuner()
{
	if( this->hTunerProcess ){
		return false;
	}
	vector<DWORD> pidList;
	//Toolhelp�X�i�b�v�V���b�g���쐬����
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if( hSnapshot != INVALID_HANDLE_VALUE ){
		PROCESSENTRY32 procent;
		procent.dwSize = sizeof(PROCESSENTRY32);
		if( Process32First(hSnapshot, &procent) != FALSE ){
			do{
				pidList.push_back(procent.th32ProcessID);
			}while( Process32Next(hSnapshot, &procent) != FALSE );
		}
		CloseHandle(hSnapshot);
	}
	bool closed = false;

	//�N�����Ŏg������̒T��
	for( size_t i = 0; closed == false && i < pidList.size(); i++ ){
		//����ƈقȂ�C���[�W���ł͂Ȃ��ڑ��ҋ@�p�C�x���g�̗L���Ŕ��f����̂Œ���
		wstring eventName;
		Format(eventName, L"%s%d", CMD2_VIEW_CTRL_WAIT_CONNECT, pidList[i]);
		HANDLE waitEvent = OpenEvent(SYNCHRONIZE, FALSE, eventName.c_str());
		if( waitEvent ){
			CloseHandle(waitEvent);
			//����̃t���[�Y�ɑΏ����邽�߈ꎞ�I�ɂ��̃o���N�̊Ǘ����ɒu��
			this->tunerPid = pidList[i];
			this->hTunerProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, this->tunerPid);
			if( this->hTunerProcess ){
				CSendCtrlCmd ctrlCmd;
				ctrlCmd.SetPipeSetting(CMD2_VIEW_CTRL_WAIT_CONNECT, CMD2_VIEW_CTRL_PIPE, this->tunerPid);
				wstring bonDriver;
				int id;
				DWORD status;
				//�^�撆�̂��̂͒D��Ȃ��̂Œ���
				if( ctrlCmd.SendViewGetBonDrivere(&bonDriver) == CMD_SUCCESS && CompareNoCase(bonDriver, this->bonFileName) == 0 &&
				    ctrlCmd.SendViewGetID(&id) == CMD_SUCCESS && id == -1 &&
				    ctrlCmd.SendViewGetStatus(&status) == CMD_SUCCESS &&
				    (status == VIEW_APP_ST_NORMAL || status == VIEW_APP_ST_GET_EPG || status == VIEW_APP_ST_ERR_CH_CHG) ){
					ctrlCmd.SendViewAppClose();
					//10�b�����I����҂�
					WaitForSingleObject(this->hTunerProcess, 10000);
					closed = true;
				}
				CBlockLock lock(&this->watchContext.lock);
				CloseHandle(this->hTunerProcess);
				this->hTunerProcess = NULL;
			}
		}
	}
	//TVTest�Ŏg���Ă���̂��邩�`�F�b�N
	for( size_t i = 0; closed == false && i < pidList.size(); i++ ){
		wstring eventName;
		Format(eventName, L"%s%d", CMD2_TVTEST_CTRL_WAIT_CONNECT, pidList[i]);
		HANDLE waitEvent = OpenEvent(SYNCHRONIZE, FALSE, eventName.c_str());
		if( waitEvent ){
			CloseHandle(waitEvent);
			this->tunerPid = pidList[i];
			this->hTunerProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, this->tunerPid);
			if( this->hTunerProcess ){
				CSendCtrlCmd ctrlCmd;
				ctrlCmd.SetPipeSetting(CMD2_TVTEST_CTRL_WAIT_CONNECT, CMD2_TVTEST_CTRL_PIPE, this->tunerPid);
				wstring bonDriver;
				if( ctrlCmd.SendViewGetBonDrivere(&bonDriver) == CMD_SUCCESS && CompareNoCase(bonDriver, this->bonFileName) == 0 ){
					ctrlCmd.SendViewAppClose();
					WaitForSingleObject(this->hTunerProcess, 10000);
					closed = true;
				}
				CBlockLock lock(&this->watchContext.lock);
				CloseHandle(this->hTunerProcess);
				this->hTunerProcess = NULL;
			}
		}
	}
	return closed;
}

void CTunerBankCtrl::Watch()
{
	//�`���[�i���t���[�Y����悤�Ȕ�펖�Ԃł�CSendCtrlCmd�̃^�C���A�E�g�͓��ĂɂȂ�Ȃ�
	//CWatchBlock�ň͂�ꂽ��Ԃ�40�b�̃^�C���A�E�g�ŊĎ����āA�K�v�Ȃ狭���I������
	CBlockLock lock(&this->watchContext.lock);
	if( this->watchContext.count != 0 && GetTickCount() - this->watchContext.tick > 40000 ){
		if( this->hTunerProcess ){
			//���Ȃ��Ƃ�hTunerProcess�͂܂�CloseHandle()����Ă��Ȃ�
			TerminateProcess(this->hTunerProcess, 0xFFFFFFFF);
			_OutputDebugString(L"CTunerBankCtrl::Watch(): Terminated TunerID=0x%08x\r\n", this->tunerID);
		}
	}
}

CTunerBankCtrl::CWatchBlock::CWatchBlock(WATCH_CONTEXT* context_)
	: context(context_)
{
	CBlockLock lock(&this->context->lock);
	if( ++this->context->count == 1 ){
		this->context->tick = GetTickCount();
	}
}

CTunerBankCtrl::CWatchBlock::~CWatchBlock()
{
	CBlockLock lock(&this->context->lock);
	this->context->count--;
}
