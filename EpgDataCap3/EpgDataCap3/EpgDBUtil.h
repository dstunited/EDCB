#pragma once

#include "../../Common/Util.h"
#include "./Table/TableUtil.h"
#include "../../Common/EpgDataCap3Def.h"

class CEpgDBUtil
{
public:
	CEpgDBUtil(void);
	~CEpgDBUtil(void);

	BOOL AddEIT(WORD PID, CEITTable* eit);
	BOOL AddEIT_SD(WORD PID, CEITTable_SD* eit);
	BOOL AddEIT_SD2(WORD PID, CEITTable_SD2* eit);

	BOOL AddServiceList(CNITTable* nit);
	BOOL AddServiceList(WORD TSID, CSITTable* sit);
	BOOL AddSDT(CSDTTable* sdt);

	void SetStreamChangeEvent();

	//EPG�f�[�^�̒~�Ϗ�Ԃ����Z�b�g����
	void ClearSectionStatus();

	//EPG�f�[�^�̒~�Ϗ�Ԃ��擾����
	//�߂�l�F
	// �X�e�[�^�X
	//�����F
	// l_eitFlag		[IN]L-EIT�̃X�e�[�^�X���擾
	EPG_SECTION_STATUS GetSectionStatus(BOOL l_eitFlag);

	//�w��T�[�r�X�̑SEPG�����擾����
	//�����F
	// originalNetworkID		[IN]�擾�Ώۂ�originalNetworkID
	// transportStreamID		[IN]�擾�Ώۂ�transportStreamID
	// serviceID				[IN]�擾�Ώۂ�ServiceID
	// epgInfoListSize			[OUT]epgInfoList�̌�
	// epgInfoList				[OUT]EPG���̃��X�g�iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	BOOL GetEpgInfoList(
		WORD originalNetworkID,
		WORD transportStreamID,
		WORD serviceID,
		DWORD* epgInfoListSize,
		EPG_EVENT_INFO** epgInfoList
		);

	//�w��T�[�r�X�̑SEPG����񋓂���
	BOOL EnumEpgInfoList(
		WORD originalNetworkID,
		WORD transportStreamID,
		WORD serviceID,
		BOOL (CALLBACK *enumEpgInfoListProc)(DWORD, EPG_EVENT_INFO*, LPVOID),
		LPVOID param
		);

	//�~�ς��ꂽEPG���̂���T�[�r�X�ꗗ���擾����
	//SERVICE_EXT_INFO�̏��͂Ȃ��ꍇ������
	//�����F
	// serviceListSize			[OUT]serviceList�̌�
	// serviceList				[OUT]�T�[�r�X���̃��X�g�iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	void GetServiceListEpgDB(
		DWORD* serviceListSize,
		SERVICE_INFO** serviceList
		);

	//�w��T�[�r�X�̌���or����EPG�����擾����
	//�����F
	// originalNetworkID		[IN]�擾�Ώۂ�originalNetworkID
	// transportStreamID		[IN]�擾�Ώۂ�transportStreamID
	// serviceID				[IN]�擾�Ώۂ�ServiceID
	// nextFlag					[IN]TRUE�i���̔ԑg�j�AFALSE�i���݂̔ԑg�j
	// epgInfo					[OUT]EPG���iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	BOOL GetEpgInfo(
		WORD originalNetworkID,
		WORD transportStreamID,
		WORD serviceID,
		BOOL nextFlag,
		EPG_EVENT_INFO** epgInfo
		);

	//�w��C�x���g��EPG�����擾����
	//�����F
	// originalNetworkID		[IN]�擾�Ώۂ�originalNetworkID
	// transportStreamID		[IN]�擾�Ώۂ�transportStreamID
	// serviceID				[IN]�擾�Ώۂ�ServiceID
	// EventID					[IN]�擾�Ώۂ�EventID
	// pfOnlyFlag				[IN]p/f����̂݌������邩�ǂ���
	// epgInfo					[OUT]EPG���iDLL���Ŏ����I��delete����B���Ɏ擾���s���܂ŗL���j
	BOOL SearchEpgInfo(
		WORD originalNetworkID,
		WORD transportStreamID,
		WORD serviceID,
		WORD eventID,
		BYTE pfOnlyFlag,
		EPG_EVENT_INFO** epgInfo
		);

protected:
	CRITICAL_SECTION dbLock;

	struct SI_TAG{
		BYTE tableID;		//�f�[�^�ǉ�����table_id �i�D��x 0x4E > 0x50-0x5F > 0x4F > 0x60-0x6F)
		BYTE version;		//�f�[�^�ǉ����̃o�[�W����
	};
	struct SHORT_EVENT_INFO : EPG_SHORT_EVENT_INFO, SI_TAG{
	};
	struct EXTENDED_EVENT_INFO : EPG_EXTENDED_EVENT_INFO, SI_TAG{
	};
	struct CONTEN_INFO : EPG_CONTEN_INFO, SI_TAG{
	};
	struct COMPONENT_INFO : EPG_COMPONENT_INFO, SI_TAG{
	};
	struct AUDIO_COMPONENT_INFO : EPG_AUDIO_COMPONENT_INFO, SI_TAG{
	};
	struct EVENTGROUP_INFO : EPG_EVENTGROUP_INFO, SI_TAG{
	};
	struct EVENT_INFO{
		WORD ONID;
		WORD TSID;
		WORD SID;
		WORD event_id;
		BYTE StartTimeFlag;	//start_time�̒l���L�����ǂ���
		SYSTEMTIME start_time;
		BYTE DurationFlag; //duration�̒l���L�����ǂ���
		DWORD durationSec;
		BYTE freeCAFlag;
		BYTE pfFlag;
		SHORT_EVENT_INFO* shortInfo;
		EXTENDED_EVENT_INFO* extInfo;
		CONTEN_INFO* contentInfo;
		COMPONENT_INFO* componentInfo;
		AUDIO_COMPONENT_INFO* audioInfo;
		EVENTGROUP_INFO* eventGroupInfo;
		EVENTGROUP_INFO* eventRelayInfo;
		EVENT_INFO(void){
			pfFlag = FALSE;
			shortInfo = NULL;
			extInfo = NULL;
			contentInfo = NULL;
			componentInfo = NULL;
			audioInfo = NULL;
			eventGroupInfo = NULL;
			eventRelayInfo = NULL;
		}
		~EVENT_INFO(void){
			SAFE_DELETE(shortInfo);
			SAFE_DELETE(extInfo);
			SAFE_DELETE(contentInfo);
			SAFE_DELETE(componentInfo);
			SAFE_DELETE(audioInfo);
			SAFE_DELETE(eventGroupInfo);
			SAFE_DELETE(eventRelayInfo);
		}
	private:
		EVENT_INFO(const EVENT_INFO&);
		EVENT_INFO& operator=(const EVENT_INFO&);
	};
	struct SERVICE_EVENT_INFO{
		map<WORD, EVENT_INFO*> eventMap;
		EVENT_INFO* nowEvent;	//map����delete�����̂�delete����K�v�Ȃ�
		EVENT_INFO* nextEvent;	//map����delete�����̂�delete����K�v�Ȃ�
		SERVICE_EVENT_INFO(void){
			nowEvent = NULL;
			nextEvent = NULL;
		}
	};
	struct SECTION_FLAG_INFO{
		DWORD sectionFlag;
		DWORD maxFlag;
		SECTION_FLAG_INFO(void){
			sectionFlag = 0;
			maxFlag = 0;
		}
	};
	struct SECTION_STATUS_INFO{
		BYTE HEITFlag;
		BYTE last_section_numberBasic;
		BYTE last_table_idBasic;
		BYTE last_section_numberExt;
		BYTE last_table_idExt;
		map<WORD, SECTION_FLAG_INFO> sectionBasicMap;	//�L�[ TableID�A�t���O
		map<WORD, SECTION_FLAG_INFO> sectionExtMap;	//�L�[ TableID�A�t���O
		SECTION_STATUS_INFO(void){
			HEITFlag = TRUE;
			last_section_numberBasic = 0;
			last_table_idBasic = 0;
			last_section_numberExt = 0;
			last_table_idExt = 0;
		}
	};
	map<ULONGLONG, SERVICE_EVENT_INFO> serviceEventMap;
	map<ULONGLONG, SECTION_STATUS_INFO> sectionMap;
	map<ULONGLONG, BYTE> serviceList;

	map<ULONGLONG, SERVICE_EVENT_INFO> serviceEventMapSD;


	typedef struct _DB_SERVICE_INFO{
		WORD original_network_id;	//original_network_id
		WORD transport_stream_id;	//transport_stream_id
		WORD service_id;			//service_id
		BYTE service_type;
		BYTE partialReceptionFlag;
		wstring service_provider_name;
		wstring service_name;
		_DB_SERVICE_INFO(void){
			service_type = 0;
			partialReceptionFlag = FALSE;
			service_provider_name = L"";
			service_name = L"";
		};
		~_DB_SERVICE_INFO(void){
		};
	}DB_SERVICE_INFO;
	typedef struct _DB_TS_INFO{
		WORD original_network_id;	//original_network_id
		WORD transport_stream_id;	//transport_stream_id
		wstring network_name;
		wstring ts_name;
		BYTE remote_control_key_id;
		map<WORD,DB_SERVICE_INFO> serviceList;
		_DB_TS_INFO(void){
			network_name = L"";
			ts_name = L"";
			remote_control_key_id = 0;
		};
		~_DB_TS_INFO(void){
		};
	}DB_TS_INFO;
	map<DWORD, DB_TS_INFO> serviceInfoList;

	DWORD sectionNowFlag;

	EPG_EVENT_INFO* epgInfoList;

	EPG_EVENT_INFO* epgInfo;

	EPG_EVENT_INFO* searchEpgInfo;

	SERVICE_INFO* serviceDBList;
protected:
	void Clear();
	
	static BOOL AddShortEvent(BYTE table_id, BYTE version_number, EVENT_INFO* eventInfo, AribDescriptor::CDescriptor* shortEvent, BOOL skySDFlag);
	static BOOL AddExtEvent(BYTE table_id, BYTE version_number, EVENT_INFO* eventInfo, vector<AribDescriptor::CDescriptor*>* descriptorList, BOOL skySDFlag);
	static BOOL AddContent(BYTE table_id, BYTE version_number, EVENT_INFO* eventInfo, AribDescriptor::CDescriptor* content, BOOL skySDFlag);
	static BOOL AddComponent(BYTE table_id, BYTE version_number, EVENT_INFO* eventInfo, AribDescriptor::CDescriptor* component, BOOL skySDFlag);
	static BOOL AddAudioComponent(BYTE table_id, BYTE version_number, EVENT_INFO* eventInfo, vector<AribDescriptor::CDescriptor*>* descriptorList, BOOL skySDFlag);
	static BOOL AddEventGroup(CEITTable* eit, EVENT_INFO* eventInfo, AribDescriptor::CDescriptor* eventGroup);
	static BOOL AddEventRelay(CEITTable* eit, EVENT_INFO* eventInfo, AribDescriptor::CDescriptor* eventGroup);
	static BOOL CheckUpdate(BYTE eit_table_id, BYTE eit_version_number, BYTE tableID, BYTE version, BOOL skySDFlag);

	static BOOL CheckUpdate_SD(BYTE eit_table_id, BYTE eit_version_number, BYTE tableID, BYTE version);

	BOOL AddSDEventMap(CEITTable_SD* eit);

	BOOL CheckSectionAll(map<WORD, SECTION_FLAG_INFO>* sectionMap, BOOL leitFlag = FALSE);

	void CopyEpgInfo(EPG_EVENT_INFO* destInfo, EVENT_INFO* srcInfo);
};
