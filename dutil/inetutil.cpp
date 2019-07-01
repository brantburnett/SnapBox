//-------------------------------------------------------------------------------------------------
// <copyright file="inetutil.cpp" company="Microsoft">
//    Copyright (c) Microsoft Corporation.  All rights reserved.
//    
//    The use and distribution terms for this software are covered by the
//    Common Public License 1.0 (http://opensource.org/licenses/cpl.php)
//    which can be found in the file CPL.TXT at the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by
//    the terms of this license.
//    
//    You must not remove this notice, or any other, from this software.
// </copyright>
// 
// <summary>
//    Internet utilites.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


/*******************************************************************
 InternetGetSizeByHandle - returns size of file by url handle

********************************************************** robmen */
extern "C" HRESULT DAPI InternetGetSizeByHandle(
	__in HINTERNET hiFile,
	__out LONGLONG* pllSize
	)
{
	Assert(pllSize);

	HRESULT hr = S_OK;
	DWORD dwSize;
	DWORD cb;

	cb = sizeof(dwSize);
	if (!::HttpQueryInfoW(hiFile, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, reinterpret_cast<LPVOID>(&dwSize), &cb, NULL))
		ExitOnLastError(hr, "failed to get size for internet file handle");

	*pllSize = dwSize;
LExit:
	return hr;
}

/*******************************************************************
 InetGetCreateTimeByHandle - returns url creation time

********************************************************** robmen */
extern "C" HRESULT DAPI InternetGetCreateTimeByHandle(
	__in HINTERNET hiFile,
	__out LPFILETIME pft
	)
{
	Assert(pft);

	HRESULT hr = S_OK;
	SYSTEMTIME st;
	DWORD cb;

	memset(&st, 0, sizeof(st));
	cb = sizeof(st);
	if (!::HttpQueryInfoW(hiFile, HTTP_QUERY_LAST_MODIFIED | HTTP_QUERY_FLAG_SYSTEMTIME, reinterpret_cast<LPVOID>(&st), &cb, NULL))
		ExitOnLastError(hr, "failed to get create time for internet file handle");

	if (!::SystemTimeToFileTime(&st, pft))
		ExitOnLastError(hr, "failed to convert system time to file time");
LExit:
	return hr;
}

