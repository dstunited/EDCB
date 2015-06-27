#include "StdAfx.h"
#include "WriteTSFile.h"
#include <process.h>

#include "../Common/PathUtil.h"
#include "../Common/BlockLock.h"

CWriteTSFile::CWriteTSFile(void)
{
	InitializeCriticalSection(&this->outThreadLock);
	this->TSBuffOffset = 0;

    this->outThread = NULL;
    this->outStopFlag = FALSE;
	this->writeTotalSize = 0;
	this->maxBuffCount = -1;
}

CWriteTSFile::~CWriteTSFile(void)
{
	if( this->outThread != NULL ){
		this->outStopFlag = TRUE;
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->outThread, 10000) == WAIT_TIMEOUT ){
			::TerminateThread(this->outThread, 0xffffffff);
		}
		CloseHandle(this->outThread);
		this->outThread = NULL;
	}


	DeleteCriticalSection(&this->outThreadLock);
	for( size_t i=0; i<this->fileList.size(); i++ ){
		SAFE_DELETE(this->fileList[i]);
	}
	this->fileList.clear();
}

//�t�@�C���ۑ����J�n����
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// fileName				[IN]�ۑ��t�@�C����
// overWriteFlag		[IN]����t�@�C�������ݎ��ɏ㏑�����邩�ǂ����iTRUE�F����AFALSE�F���Ȃ��j
// createSize			[IN]�t�@�C���쐬���Ƀf�B�X�N�ɗ\�񂷂�e��
// saveFolder			[IN]�g�p����t�H���_�ꗗ
// saveFolderSub		[IN]HDD�̋󂫂��Ȃ��Ȃ����ꍇ�Ɉꎞ�I�Ɏg�p����t�H���_
BOOL CWriteTSFile::StartSave(
	wstring fileName,
	BOOL overWriteFlag,
	ULONGLONG createSize,
	vector<REC_FILE_SET_INFO>* saveFolder,
	vector<wstring>* saveFolderSub,
	int maxBuffCount
)
{
	BOOL ret = TRUE;

	this->maxBuffCount = maxBuffCount;

	if( saveFolder->size() == 0 ){
		_OutputDebugString(L"CWriteTSFile::StartSave Err saveFolder 0");
		return FALSE;
	}
	
	if( this->outThread == NULL && this->fileList.size() == 0 ){
		this->writeTotalSize = 0;
		this->subRecFlag = FALSE;
		vector<REC_FILE_SET_INFO> saveFolder_ = *saveFolder;
		this->saveFolderSub = *saveFolderSub;
		BOOL firstFreeChek = TRUE;
		for( size_t i=0; i<saveFolder_.size(); i++ ){
			SAVE_INFO* item = new SAVE_INFO;
			if( saveFolder_[i].writePlugIn.size() == 0 ){
				saveFolder_[i].writePlugIn = L"Write_Default.dll";
			}
			wstring plugInPath = L"";
			GetModuleFolderPath(plugInPath);
			plugInPath += L"\\Write\\";
			plugInPath += saveFolder_[i].writePlugIn.c_str();

			item->writeUtil = new CWritePlugInUtil;
			if(item->writeUtil->Initialize(plugInPath.c_str() ) == FALSE ){
				_OutputDebugString(L"CWriteTSFile::StartSave Err 3");
				SAFE_DELETE(item);
			}else{
				wstring folderPath = saveFolder_[i].recFolder;
				if( CompareNoCase(saveFolder_[i].writePlugIn, L"Write_Default.dll" ) == 0 ){
					//�f�t�H���g�̏ꍇ�͋󂫗e�ʂ����炩���߃`�F�b�N
					if( createSize > 0 ){
						if( ChkFreeFolder(createSize, saveFolder_[i].recFolder) == FALSE ){
							if( GetFreeFolder(createSize, folderPath) == TRUE ){
								//�󂫂Ȃ������̂ŃT�u�t�H���_�ɘ^��
								this->subRecFlag = TRUE;
							}
						}
					}
				}
				ChkFolderPath(folderPath);

				wstring recPath;
				recPath = folderPath;
				recPath += L"\\";
				if( saveFolder_[i].recFileName.size() == 0 ){
					recPath += fileName;
					item->recFileName = fileName;
				}else{
					recPath += saveFolder_[i].recFileName;
					item->recFileName = saveFolder_[i].recFileName;
				}
				//�J�n
				BOOL startRes = item->writeUtil->StartSave(recPath.c_str(), overWriteFlag, createSize);
				if( startRes == FALSE ){
					_OutputDebugString(L"CWriteTSFile::StartSave Err 2");
					//�G���[���T�u�t�H���_�Ń��g���C
					if( GetFreeFolder(createSize, folderPath) == TRUE ){
						//�󂫂Ȃ������̂ŃT�u�t�H���_�ɘ^��
						this->subRecFlag = TRUE;
					}
					ChkFolderPath(folderPath);
					recPath = folderPath;
					recPath += L"\\";
					if( saveFolder_[i].recFileName.size() == 0 ){
						recPath += fileName;
						item->recFileName = fileName;
					}else{
						recPath += saveFolder_[i].recFileName;
						item->recFileName = saveFolder_[i].recFileName;
					}
					startRes = item->writeUtil->StartSave(recPath.c_str(), overWriteFlag, createSize);
				}
				if( startRes == TRUE ){
					WCHAR saveFilePath[512] = L"";
					DWORD saveFilePathSize = 512;
					item->writeUtil->GetSaveFilePath(saveFilePath, &saveFilePathSize);

					item->recFilePath = saveFilePath;
					item->freeChk = firstFreeChek;
					item->overWriteFlag = overWriteFlag;
					this->fileList.push_back(item);
					if( i==0 ){
						this->mainSaveFilePath = saveFilePath;
					}
					firstFreeChek = FALSE;
				}else{
					SAFE_DELETE(item);
				}
			}
		}

		if( this->fileList.size() > 0 ){
			//��M�X���b�h�N��
			this->outStopFlag = FALSE;
			this->outThread = (HANDLE)_beginthreadex(NULL, 0, OutThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
			SetThreadPriority( this->outThread, THREAD_PRIORITY_NORMAL );
			ResumeThread(this->outThread);
		}else{
			_OutputDebugString(L"CWriteTSFile::StartSave Err fileList 0");
			ret = FALSE;
		}
	}else{
		_OutputDebugString(L"CWriteTSFile::StartSave Err 1");
		ret = FALSE;
	}

	return ret;
}

//�ۑ��T�u�t�H���_����󂫂̂���t�H���_�p�X���擾
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// needFreeSize			[IN]�Œ�K�v�ȋ󂫃T�C�Y
// freeFolderPath		[OUT]���������t�H���_
BOOL CWriteTSFile::GetFreeFolder(
	ULONGLONG needFreeSize,
	wstring& freeFolderPath
)
{
	BOOL ret = FALSE;

	for( int i = 0; i < (int)this->saveFolderSub.size(); i++ ){
		ULARGE_INTEGER stFree;
		if( _GetDiskFreeSpaceEx( this->saveFolderSub[i].c_str(), &stFree, NULL, NULL ) != FALSE ){
			if( stFree.QuadPart > needFreeSize ){
				freeFolderPath = this->saveFolderSub[i];
				ChkFolderPath(freeFolderPath);
				ret = TRUE;
				break;
			}
		}
	}
	return ret;
}

//�w��t�H���_�̋󂫂����邩�`�F�b�N
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// needFreeSize			[IN]�Œ�K�v�ȋ󂫃T�C�Y
// chkFolderPath		[IN]�w��t�H���_
BOOL CWriteTSFile::ChkFreeFolder(
	ULONGLONG needFreeSize,
	wstring chkFolderPath
)
{
	BOOL ret = FALSE;

	ULARGE_INTEGER stFree;
	if( _GetDiskFreeSpaceEx( chkFolderPath.c_str(), &stFree, NULL, NULL ) != FALSE ){
		if( stFree.QuadPart > needFreeSize ){
			ret = TRUE;
		}
	}
	return ret;
}

//�t�@�C���ۑ����I������
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
BOOL CWriteTSFile::EndSave()
{
	BOOL ret = TRUE;

	if( this->outThread != NULL ){
		this->outStopFlag = TRUE;
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(this->outThread, 10000) == WAIT_TIMEOUT ){
			::TerminateThread(this->outThread, 0xffffffff);
		}
		CloseHandle(this->outThread);
		this->outThread = NULL;
	}

	//�c���Ă���o�b�t�@�������o��
	{
		CBlockLock lock(&this->outThreadLock);
		this->TSBuff.erase(this->TSBuff.begin(), this->TSBuff.begin() + this->TSBuffOffset);
		this->TSBuffOffset = 0;
		while( this->TSBuff.empty() == false ){
			DWORD dataSize = (DWORD)min(this->TSBuff.size(), 48128);
			for( size_t i=0; i<this->fileList.size(); i++ ){
				if( this->fileList[i]->writeUtil != NULL ){
					DWORD write = 0;
					this->fileList[i]->writeUtil->AddTSBuff( &this->TSBuff.front(), dataSize, &write);
				}
			}
			this->TSBuff.erase(this->TSBuff.begin(), this->TSBuff.begin() + dataSize);
		}
	}

	for( size_t i=0; i<this->fileList.size(); i++ ){
		if( this->fileList[i]->writeUtil != NULL ){
			this->fileList[i]->writeUtil->StopSave();
			this->fileList[i]->writeUtil->UnInitialize();
			SAFE_DELETE(this->fileList[i]);
		}
	}
	this->fileList.clear();

	return ret;
}

//�o�͗pTS�f�[�^�𑗂�
//�߂�l�F
// TRUE�i�����j�AFALSE�i���s�j
//�����F
// data		[IN]TS�f�[�^
// size		[IN]data�̃T�C�Y
BOOL CWriteTSFile::AddTSBuff(
	BYTE* data,
	DWORD size
	)
{
	if( data == NULL || size == 0 || this->outThread == NULL){
		return FALSE;
	}

	BOOL ret = TRUE;

	{
		CBlockLock lock(&this->outThreadLock);
		this->TSBuff.erase(this->TSBuff.begin(), this->TSBuff.begin() + this->TSBuffOffset);
		this->TSBuffOffset = 0;
		if( this->maxBuffCount > 0 ){
			if( this->TSBuff.size() / 48128 > (size_t)this->maxBuffCount ){
				_OutputDebugString(L"��writeBuffList MaxOver");
				this->TSBuff.clear();
			}
		}
		this->TSBuff.insert(this->TSBuff.end(), data, data + size);
	}
	return ret;
}

UINT WINAPI CWriteTSFile::OutThread(LPVOID param)
{
	CWriteTSFile* sys = (CWriteTSFile*)param;
	while( sys->outStopFlag == FALSE ){
		//�o�b�t�@����f�[�^���o��
		BYTE data[48128];
		DWORD dataSize = 0;
		{
			CBlockLock lock(&sys->outThreadLock);
			if( sys->TSBuff.size() - sys->TSBuffOffset >= sizeof(data) ){
				//�K��188�̔{���Ŏ��o���Ȃ���΂Ȃ�Ȃ�
				dataSize = sizeof(data);
				memcpy(data, &sys->TSBuff[sys->TSBuffOffset], dataSize);
				sys->TSBuffOffset += dataSize;
			}
		}

		if( dataSize != 0 ){
			for( size_t i=0; i<sys->fileList.size(); i++ ){
				{
					if( sys->fileList[i]->writeUtil != NULL ){
						DWORD write = 0;
						if( sys->fileList[i]->writeUtil->AddTSBuff( data, dataSize, &write) == FALSE ){
							//�󂫂��Ȃ��Ȃ���
							{
								CBlockLock lock(&sys->outThreadLock);
								sys->writeTotalSize = -1;
							}
							sys->fileList[i]->writeUtil->StopSave();

							if( sys->fileList[i]->freeChk == TRUE ){
								//���̋󂫂�T��
								wstring freeFolderPath = L"";
								if( sys->GetFreeFolder(200*1024*1024, freeFolderPath) == TRUE ){
									wstring recFilePath = freeFolderPath;
									recFilePath += L"\\";
									recFilePath += sys->fileList[i]->recFileName;

									//�J�n
									if( sys->fileList[i]->writeUtil->StartSave(recFilePath.c_str(), sys->fileList[i]->overWriteFlag, 0) == FALSE ){
										//���s�����̂ŏI���
										SAFE_DELETE(sys->fileList[i]->writeUtil);
									}else{
										WCHAR saveFilePath[512] = L"";
										DWORD saveFilePathSize = 512;
										sys->fileList[i]->writeUtil->GetSaveFilePath(saveFilePath, &saveFilePathSize);
										sys->fileList[i]->subRecPath.push_back(saveFilePath);
										sys->subRecFlag = TRUE;

										if( dataSize > write ){
											sys->fileList[i]->writeUtil->AddTSBuff( data+write, dataSize-write, &write);
										}
									}
								}
							}
						}
					}
				}
			}

			{
				CBlockLock lock(&sys->outThreadLock);
				sys->writeTotalSize += dataSize;
			}
		}else{
			Sleep(100);
		}
	}
	return 0;
}

//�^�撆�̃t�@�C���̃t�@�C���p�X���擾����
//�����F
// filePath			[OUT]�ۑ��t�@�C����
// subRecFlag		[OUT]�T�u�^�悪�����������ǂ���
void CWriteTSFile::GetSaveFilePath(
	wstring* filePath,
	BOOL* subRecFlag
	)
{
	*filePath = this->mainSaveFilePath;
	*subRecFlag = this->subRecFlag;
}

//�^�撆�̃t�@�C���̏o�̓T�C�Y���擾����
//�����F
// writeSize			[OUT]�ۑ��t�@�C����
void CWriteTSFile::GetRecWriteSize(
	__int64* writeSize
	)
{
	if( writeSize != NULL ){
		CBlockLock lock(&this->outThreadLock);
		*writeSize = this->writeTotalSize;
	}
}
