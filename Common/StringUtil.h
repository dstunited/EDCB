#ifndef __STRING_UTIL_H__
#define __STRING_UTIL_H__

#include "Util.h"

void Format(string& strBuff, const char *format, ...);

void Format(wstring& strBuff, const WCHAR *format, ...);

void Replace(string& strBuff, const string& strOld, const string& strNew); //alias-safe

void Replace(wstring& strBuff, const wstring& strOld, const wstring& strNew); //alias-safe

void WtoA(const wstring& strIn, string& strOut);

void WtoUTF8(const wstring& strIn, string& strOut);

void AtoW(const string& strIn, wstring& strOut);

void UTF8toW(const string& strIn, wstring& strOut);

BOOL Separate(const string& strIn, const char *sep, string& strLeft, string& strRight); //alias-safe
inline BOOL Separate(const string& strIn, const string& strSep, string& strLeft, string& strRight){ return Separate(strIn, strSep.c_str(), strLeft, strRight); }

BOOL Separate(const wstring& strIn, const WCHAR *sep, wstring& strLeft, wstring& strRight); //alias-safe
inline BOOL Separate(const wstring& strIn, const wstring& strSep, wstring& strLeft, wstring& strRight){ return Separate(strIn, strSep.c_str(), strLeft, strRight); }

void ChkFolderPath(string& strPath);

void ChkFolderPath(wstring& strPath);

int CompareNoCase(const string& str1, const string& str2);

int CompareNoCase(const wstring& str1, const wstring& str2);

BOOL UrlDecode(LPCSTR src, DWORD srcSize, string& dest);

void Trim(string& strBuff);

void Trim(wstring& strBuff);

string Tolower(const string& src);

wstring Tolower(const wstring& src);

void EscapeXmlString(wstring& src);

#endif
