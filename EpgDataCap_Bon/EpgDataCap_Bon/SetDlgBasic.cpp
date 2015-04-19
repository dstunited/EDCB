// SetDlgBasic.cpp : �����t�@�C��
//

#include "stdafx.h"
#include "EpgDataCap_Bon.h"
#include "SetDlgBasic.h"

#include "../../Common/PathUtil.h"
#include "../../Common/StringUtil.h"

// CSetDlgBasic �_�C�A���O

CSetDlgBasic::CSetDlgBasic()
	: m_hWnd(NULL)
{

}

CSetDlgBasic::~CSetDlgBasic()
{
}

BOOL CSetDlgBasic::Create(LPCTSTR lpszTemplateName, HWND hWndParent)
{
	return CreateDialogParam(GetModuleHandle(NULL), lpszTemplateName, hWndParent, DlgProc, (LPARAM)this) != NULL;
}


// CSetDlgBasic ���b�Z�[�W �n���h���[
BOOL CSetDlgBasic::OnInitDialog()
{
	// TODO:  �����ɏ�������ǉ����Ă�������
	wstring path;
	GetSettingPath(path);
	SetDlgItemText(m_hWnd, IDC_EDIT_SET_PATH, path.c_str());

	int iNum = GetPrivateProfileInt( L"SET", L"RecFolderNum", 0, commonIniPath.c_str() );
	if( iNum == 0 ){
		ListBox_AddString(GetDlgItem(IDC_LIST_REC_FOLDER), path.c_str());
	}else{
		for( int i = 0; i < iNum; i++ ){
			WCHAR key[64];
			wsprintf(key, L"RecFolderPath%d", i);
			WCHAR buff[512]=L"";
			GetPrivateProfileString( L"SET", key, L"", buff, 512, commonIniPath.c_str() );
			ListBox_AddString(GetDlgItem(IDC_LIST_REC_FOLDER), buff);
		}
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// ��O : OCX �v���p�e�B �y�[�W�͕K�� FALSE ��Ԃ��܂��B
}

void CSetDlgBasic::SaveIni()
{
	if( m_hWnd == NULL ){
		return;
	}

	WCHAR settingFolderPath[512] = L"";
	GetDlgItemText(m_hWnd, IDC_EDIT_SET_PATH, settingFolderPath, 512);
	wstring chkSettingPath = settingFolderPath;
	ChkFolderPath(chkSettingPath);
	wstring defSettingPath;
	GetDefSettingPath(defSettingPath);
	WritePrivateProfileString(L"SET", L"DataSavePath", CompareNoCase(chkSettingPath, defSettingPath) ? settingFolderPath : NULL, commonIniPath.c_str());
	_CreateDirectory(settingFolderPath);

	int iNum = ListBox_GetCount(GetDlgItem(IDC_LIST_REC_FOLDER));
	if( iNum == 1 ){
		WCHAR folder[512] = L"";
		int len = ListBox_GetTextLen(GetDlgItem(IDC_LIST_REC_FOLDER), 0);
		if( 0 <= len && len < 512 ){
			ListBox_GetText(GetDlgItem(IDC_LIST_REC_FOLDER), 0, folder);
		}
		wstring chkFolder = folder;
		ChkFolderPath(chkFolder);
		if( CompareNoCase(chkFolder, chkSettingPath) == 0 ){
			iNum = 0;
		}
	}
	WritePrivateProfileInt(L"SET", L"RecFolderNum", iNum, commonIniPath.c_str());
	for( int i = 0; i < iNum; i++ ){
		WCHAR key[64];
		wsprintf(key, L"RecFolderPath%d", i);
		WCHAR folder[512] = L"";
		int len = ListBox_GetTextLen(GetDlgItem(IDC_LIST_REC_FOLDER), i);
		if( 0 <= len && len < 512 ){
			ListBox_GetText(GetDlgItem(IDC_LIST_REC_FOLDER), i, folder);
		}
		WritePrivateProfileString(L"SET", key, folder, commonIniPath.c_str());
	}
}

void CSetDlgBasic::OnBnClickedButtonRecPath()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	BROWSEINFO bi;
	LPWSTR lpBuffer;
	LPITEMIDLIST pidlRoot = NULL;
	LPITEMIDLIST pidlBrowse;
	LPMALLOC lpMalloc = NULL;

	HRESULT hr = SHGetMalloc(&lpMalloc);
	if(FAILED(hr)) return;

	if ((lpBuffer = (LPWSTR) lpMalloc->Alloc(_MAX_PATH*2)) == NULL) {
		return;
	}
	WCHAR recFolderPath[512];
	if( GetDlgItemText(m_hWnd, IDC_EDIT_REC_FOLDER, recFolderPath, 512) <= 0 ){
		recFolderPath[0] = L'\0';
		if (!SUCCEEDED(SHGetSpecialFolderLocation( m_hWnd,CSIDL_DESKTOP,&pidlRoot ) )){ 
			lpMalloc->Free(lpBuffer);
			return;
		}
	}

	bi.hwndOwner = m_hWnd;
	bi.pidlRoot = pidlRoot;
	bi.pszDisplayName = lpBuffer;
	bi.lpszTitle = L"�^��t�@�C���ۑ��t�H���_��I�����Ă�������";
	bi.ulFlags = 0;
	bi.lpfn = NULL;
	bi.lParam = (LPARAM)recFolderPath;

	pidlBrowse = SHBrowseForFolder(&bi);
	if (pidlBrowse != NULL) {  
		if (SHGetPathFromIDList(pidlBrowse, lpBuffer)) {
			SetDlgItemText(m_hWnd, IDC_EDIT_REC_FOLDER, lpBuffer);
		}
		lpMalloc->Free(pidlBrowse);
	}
	if( pidlRoot != NULL ){
		lpMalloc->Free(pidlRoot); 
	}
	lpMalloc->Free(lpBuffer);
	lpMalloc->Release();
}


void CSetDlgBasic::OnBnClickedButtonRecAdd()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	WCHAR recFolderPath[512];
	if( GetDlgItemText(m_hWnd, IDC_EDIT_REC_FOLDER, recFolderPath, 512) <= 0 ){
		return ;
	}
	wstring addPath = recFolderPath;
	ChkFolderPath( addPath );

	//����t�H���_�����łɂ��邩�`�F�b�N
	int iNum = ListBox_GetCount(GetDlgItem(IDC_LIST_REC_FOLDER));
	BOOL findFlag = FALSE;
	for( int i = 0; i < iNum; i++ ){
		WCHAR folder[512] = L"";
		int len = ListBox_GetTextLen(GetDlgItem(IDC_LIST_REC_FOLDER), i);
		if( 0 <= len && len < 512 ){
			ListBox_GetText(GetDlgItem(IDC_LIST_REC_FOLDER), i, folder);
		}
		wstring strPath = folder;
		ChkFolderPath( strPath );

		if( CompareNoCase( addPath, strPath ) == 0 ){
			findFlag = TRUE;
			break;
		}
	}
	if( findFlag == FALSE ){
		ListBox_AddString(GetDlgItem(IDC_LIST_REC_FOLDER), addPath.c_str());
	}
}


void CSetDlgBasic::OnBnClickedButtonRecDel()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	int sel = ListBox_GetCurSel(GetDlgItem(IDC_LIST_REC_FOLDER));
	if( sel == LB_ERR ){
		return ;
	}
	ListBox_DeleteString(GetDlgItem(IDC_LIST_REC_FOLDER), sel);
}


void CSetDlgBasic::OnBnClickedButtonRecUp()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	HWND hItem = GetDlgItem(IDC_LIST_REC_FOLDER);
	int sel = ListBox_GetCurSel(hItem);
	if( sel == LB_ERR || sel == 0){
		return ;
	}
	WCHAR folder[512] = L"";
	int len = ListBox_GetTextLen(hItem, sel);
	if( 0 <= len && len < 512 ){
		ListBox_GetText(hItem, sel, folder);
		ListBox_DeleteString(hItem, sel);
		ListBox_InsertString(hItem, sel - 1, folder);
		ListBox_SetCurSel(hItem, sel - 1);
	}
}


void CSetDlgBasic::OnBnClickedButtonRecDown()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	HWND hItem = GetDlgItem(IDC_LIST_REC_FOLDER);
	int sel = ListBox_GetCurSel(hItem);
	if( sel == LB_ERR || sel == ListBox_GetCount(hItem) - 1 ){
		return ;
	}
	WCHAR folder[512] = L"";
	int len = ListBox_GetTextLen(hItem, sel);
	if( 0 <= len && len < 512 ){
		ListBox_GetText(hItem, sel, folder);
		ListBox_DeleteString(hItem, sel);
		ListBox_InsertString(hItem, sel + 1, folder);
		ListBox_SetCurSel(hItem, sel + 1);
	}
}


void CSetDlgBasic::OnBnClickedButtonSetPath()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	BROWSEINFO bi;
	LPWSTR lpBuffer;
	LPITEMIDLIST pidlRoot = NULL;
	LPITEMIDLIST pidlBrowse;
	LPMALLOC lpMalloc = NULL;

	HRESULT hr = SHGetMalloc(&lpMalloc);
	if(FAILED(hr)) return;

	if ((lpBuffer = (LPWSTR) lpMalloc->Alloc(_MAX_PATH*2)) == NULL) {
		return;
	}
	WCHAR settingFolderPath[512];
	if( GetDlgItemText(m_hWnd, IDC_EDIT_SET_PATH, settingFolderPath, 512) <= 0 ){
		settingFolderPath[0] = L'\0';
		if (!SUCCEEDED(SHGetSpecialFolderLocation( m_hWnd,CSIDL_DESKTOP,&pidlRoot ) )){ 
			lpMalloc->Free(lpBuffer);
			return;
		}
	}

	bi.hwndOwner = m_hWnd;
	bi.pidlRoot = pidlRoot;
	bi.pszDisplayName = lpBuffer;
	bi.lpszTitle = L"�ݒ�֌W�ۑ��t�H���_��I�����Ă�������";
	bi.ulFlags = 0;
	bi.lpfn = NULL;
	bi.lParam = (LPARAM)settingFolderPath;

	pidlBrowse = SHBrowseForFolder(&bi);
	if (pidlBrowse != NULL) {  
		if (SHGetPathFromIDList(pidlBrowse, lpBuffer)) {
			SetDlgItemText(m_hWnd, IDC_EDIT_SET_PATH, lpBuffer);
		}
		lpMalloc->Free(pidlBrowse);
	}
	if( pidlRoot != NULL ){
		lpMalloc->Free(pidlRoot); 
	}
	lpMalloc->Free(lpBuffer);
	lpMalloc->Release();
}


INT_PTR CALLBACK CSetDlgBasic::DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CSetDlgBasic* pSys = (CSetDlgBasic*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	if( pSys == NULL && uMsg != WM_INITDIALOG ){
		return FALSE;
	}
	switch( uMsg ){
	case WM_INITDIALOG:
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		pSys = (CSetDlgBasic*)lParam;
		pSys->m_hWnd = hDlg;
		return pSys->OnInitDialog();
	case WM_NCDESTROY:
		pSys->m_hWnd = NULL;
		break;
	case WM_COMMAND:
		switch( LOWORD(wParam) ){
		case IDC_BUTTON_REC_PATH:
			pSys->OnBnClickedButtonRecPath();
			break;
		case IDC_BUTTON_REC_ADD:
			pSys->OnBnClickedButtonRecAdd();
			break;
		case IDC_BUTTON_REC_DEL:
			pSys->OnBnClickedButtonRecDel();
			break;
		case IDC_BUTTON_REC_UP:
			pSys->OnBnClickedButtonRecUp();
			break;
		case IDC_BUTTON_REC_DOWN:
			pSys->OnBnClickedButtonRecDown();
			break;
		case IDC_BUTTON_SET_PATH:
			pSys->OnBnClickedButtonSetPath();
			break;
		}
		break;
	}
	return FALSE;
}

