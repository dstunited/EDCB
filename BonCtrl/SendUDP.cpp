#include "StdAfx.h"
#include "SendUDP.h"

static const int SNDBUF_SIZE = 3 * 1024 * 1024;

CSendUDP::CSendUDP(void)
{
	WSAData wsaData;
	WSAStartup(MAKEWORD(2,0), &wsaData);
}

CSendUDP::~CSendUDP(void)
{
	CloseUpload();

	WSACleanup();

}

BOOL CSendUDP::StartUpload( vector<NW_SEND_INFO>* List )
{
	if( SockList.size() > 0 ){
		return FALSE;
	}

	for( int i=0; i<(int)List->size(); i++ ){
		SOCKET_DATA Item;
		Item.sock = socket(AF_INET, SOCK_DGRAM, 0);
		if( Item.sock == INVALID_SOCKET ){
			CloseUpload();
			return FALSE;
		}
		//�m���u���b�L���O���[�h��
		ULONG x = 1;
		if( ioctlsocket(Item.sock,FIONBIO, &x) == SOCKET_ERROR ||
		    setsockopt(Item.sock, SOL_SOCKET, SO_SNDBUF, (const char *)&SNDBUF_SIZE, sizeof(SNDBUF_SIZE)) == SOCKET_ERROR ){
			closesocket(Item.sock);
			CloseUpload();
			return FALSE;
		}

		if( (*List)[i].broadcastFlag == FALSE ){
			Item.addr.sin_family = AF_INET;
			Item.addr.sin_port = htons((WORD)(*List)[i].port);
			string strA="";
			WtoA((*List)[i].ipString, strA);
			Item.addr.sin_addr.S_un.S_addr = inet_addr(strA.c_str());

			//DWORD dwHost = inet_addr("127.0.0.1");
			//setsockopt(Item.sock, IPPROTO_IP,IP_MULTICAST_IF,(char *)&dwHost, sizeof(DWORD));
		}else{
			BOOL b=1;
			Item.addr.sin_family = AF_INET;
			Item.addr.sin_port = htons((WORD)(*List)[i].port);
			string strA="";
			WtoA((*List)[i].ipString, strA);
			Item.addr.sin_addr.S_un.S_addr = inet_addr(strA.c_str());
			setsockopt(Item.sock,SOL_SOCKET, SO_BROADCAST, (char *)&b, sizeof(b));
		}
		SockList.push_back(Item);
	}

	return TRUE;
}

BOOL CSendUDP::CloseUpload()
{
	for( int i=0; i<(int)SockList.size(); i++ ){
		closesocket(SockList[i].sock);
	}
	SockList.clear();

	return TRUE;
}

void CSendUDP::SendData(BYTE* pbBuff, DWORD dwSize)
{
	for( DWORD dwRead=0; dwRead<dwSize; ){
		//�y�C���[�h�����BBonDriver_UDP�̎�M�T�C�Y48128�ȉ��łȂ���΂Ȃ�Ȃ�
		int iSendSize = min(128 * 188, dwSize - dwRead);
		for( size_t i=0; i<SockList.size(); i++ ){
			int iRet = sendto(SockList[i].sock, (char*)(pbBuff + dwRead), iSendSize, 0, (struct sockaddr *)&SockList[i].addr, sizeof(SockList[i].addr));
			if( iRet == SOCKET_ERROR ){
				if( WSAGetLastError() == WSAEWOULDBLOCK ){
					//���M�������ǂ�����SNDBUF_SIZE�Ŏw�肵���o�b�t�@���s���Ă��܂���
					//�ш悪����Ȃ��Ƃ��͂ǂ����~���Ă��h���b�v���邵���Ȃ��̂ŁASleep()�ɂ��t���[����͂��Ȃ�
					OutputDebugString(L"Dropped\r\n");
				}
			}
		}
		dwRead += iSendSize;
	}
}