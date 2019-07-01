//-------------------------------------------------------------------------------------------------
// <copyright file="fileutil.cpp" company="Microsoft">
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
//    File helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

/*******************************************************************
 FileFromPath -  returns a pointer to the file part of the path

********************************************************************/
extern "C" LPWSTR DAPI FileFromPath(
    __in LPCWSTR wzPath
    )
{
    if (!wzPath)
        return NULL;

    LPWSTR wzFile = const_cast<LPWSTR>(wzPath);
    for (LPWSTR wz = wzFile; *wz; wz++)
    {
        // valid delineators
        //     \ => Windows path
        //     / => unix and URL path
        //     : => relative path from mapped root
        if (L'\\' == *wz || L'/' == *wz || L':' == *wz)
            wzFile = wz + 1;
    }

    return wzFile;
}


/*******************************************************************
 FileResolvePath - gets the full path to a file resolving environment
                   variables along the way.

********************************************************************/
extern "C" HRESULT DAPI FileResolvePath(
    __in LPCWSTR wzRelativePath,
    __out LPWSTR *ppwzFullPath
    )
{
    Assert(wzRelativePath && *wzRelativePath);

    HRESULT hr = S_OK;
    DWORD cch = 0;
    LPWSTR pwzExpandedPath = NULL;
    DWORD cchExpandedPath = 0;

    LPWSTR pwzFullPath = NULL;
    DWORD cchFullPath = 0;

    LPWSTR wzFileName = NULL;

    //
    // First, expand any environment variables.
    //
    cchExpandedPath = MAX_PATH;
    hr = StrAlloc(&pwzExpandedPath, cchExpandedPath);
    ExitOnFailure(hr, "Failed to allocate space for expanded path.");

    cch = ::ExpandEnvironmentStringsW(wzRelativePath, pwzExpandedPath, cchExpandedPath);
    if (0 == cch)
    {
        ExitWithLastError1(hr, "Failed to expand environment variables in string: %S", wzRelativePath);
    }
    else if (cchExpandedPath < cch)
    {
        cchExpandedPath = cch;
        hr = StrAlloc(&pwzExpandedPath, cchExpandedPath);
        ExitOnFailure(hr, "Failed to re-allocate more space for expanded path.");

        cch = ::ExpandEnvironmentStringsW(wzRelativePath, pwzExpandedPath, cchExpandedPath);
        if (0 == cch)
        {
            ExitWithLastError1(hr, "Failed to expand environment variables in string: %S", wzRelativePath);
        }
        else if (cchExpandedPath < cch)
        {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            ExitOnFailure(hr, "Failed to allocate buffer for expanded path.");
        }
    }

    //
    // Second, get the full path.
    //
    cchFullPath = MAX_PATH;
    hr = StrAlloc(&pwzFullPath, cchFullPath);
    ExitOnFailure(hr, "Failed to allocate space for full path.");

    cch = ::GetFullPathNameW(pwzExpandedPath, cchFullPath, pwzFullPath, &wzFileName);
    if (0 == cch)
    {
        ExitWithLastError1(hr, "Failed to get full path for string: %S", pwzExpandedPath);
    }
    else if (cchFullPath < cch)
    {
        cchFullPath = cch;
        hr = StrAlloc(&pwzFullPath, cchFullPath);
        ExitOnFailure(hr, "Failed to re-allocate more space for full path.");

        cch = ::GetFullPathNameW(pwzExpandedPath, cchFullPath, pwzFullPath, &wzFileName);
        if (0 == cch)
        {
            ExitWithLastError1(hr, "Failed to get full path for string: %S", pwzExpandedPath);
        }
        else if (cchFullPath < cch)
        {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            ExitOnFailure(hr, "Failed to allocate buffer for full path.");
        }
    }

    *ppwzFullPath = pwzFullPath;
    pwzFullPath = NULL;

LExit:
    ReleaseStr(pwzFullPath);
    ReleaseStr(pwzExpandedPath);

    return hr;
}


/*******************************************************************
FileStripExtension - Strip extension from filename
********************************************************************/
extern "C" HRESULT DAPI FileStripExtension(
__in LPCWSTR wzFileName,
__out LPWSTR *ppwzFileNameNoExtension
)
{
    Assert(wzFileName && *wzFileName);
   
    HRESULT hr = S_OK;
   
    SIZE_T cchFileName = wcslen(wzFileName);
   
    LPWSTR pwzFileNameNoExtension = NULL;
    DWORD cchFileNameNoExtension = 0;
   
    // Filename without extension can not be longer than _MAX_FNAME
    // Filename without extension should also not be longer than filename itself
    if (_MAX_FNAME > cchFileName)
    {
        cchFileNameNoExtension = (DWORD) cchFileName;
    }
    else
    {
        cchFileNameNoExtension = _MAX_FNAME;
    }
   
    hr = StrAlloc(&pwzFileNameNoExtension, cchFileNameNoExtension);
    ExitOnFailure(hr, "failed to allocate space for File Name without extension");
   
    // _wsplitpath_s can handle drive/path/filename/extension
    errno_t err = _wsplitpath_s(wzFileName, NULL, NULL, NULL, NULL, pwzFileNameNoExtension, cchFileNameNoExtension, NULL, NULL);
    if (0 != err)
    {
        hr = E_INVALIDARG;
        ExitOnFailure1(hr, "failed to parse filename: %S", wzFileName);
    }
   
    *ppwzFileNameNoExtension = pwzFileNameNoExtension;
    pwzFileNameNoExtension = NULL;
   
LExit:
    ReleaseStr(pwzFileNameNoExtension);
   
    return hr;
}


/*******************************************************************
 FileVersionFromString

*******************************************************************/
extern "C" HRESULT DAPI FileVersionFromString(
    __in LPCWSTR wzVersion,
    __out DWORD* pdwVerMajor,
    __out DWORD* pdwVerMinor
    )
{
    Assert(pdwVerMajor && pdwVerMinor);

    HRESULT hr = S_OK;
    LPCWSTR pwz = wzVersion;
    DWORD dw;

    *pdwVerMajor = 0;
    *pdwVerMinor = 0;

    dw = wcstoul(pwz, (WCHAR**)&pwz, 10);
    if (pwz && (L'.' == *pwz && dw < 0x10000) || !*pwz)
    {
        *pdwVerMajor = dw << 16;
        pwz++;
    }
    else
    {
        ExitFunction1(hr = S_FALSE);
    }

    dw = wcstoul(pwz, (WCHAR**)&pwz, 10);
    if (pwz && (L'.' == *pwz && dw < 0x10000) || !*pwz)
    {
        *pdwVerMajor |= dw;
        pwz++;
    }
    else
    {
        ExitFunction1(hr = S_FALSE);
    }

    dw = wcstoul(pwz, (WCHAR**)&pwz, 10);
    if (pwz && (L'.' == *pwz && dw < 0x10000) || !*pwz)
    {
        *pdwVerMinor = dw << 16;
        pwz++;
    }
    else
    {
        ExitFunction1(hr = S_FALSE);
    }

    dw = wcstoul(pwz, (WCHAR**)&pwz, 10);
    if (pwz && L'\0' == *pwz && dw < 0x10000)
    {
        *pdwVerMinor |= dw;
    }
    else
    {
        ExitFunction1(hr = S_FALSE);
    }

LExit:
    return hr;
}


/*******************************************************************
 FileSizeByHandle

********************************************************************/
extern "C" HRESULT DAPI FileSizeByHandle(
    __in HANDLE hFile,
    __out LONGLONG* pllSize
    )
{
    Assert(INVALID_HANDLE_VALUE != hFile && pllSize);
    HRESULT hr;
    LARGE_INTEGER li;

    *pllSize = 0;

    if (!::GetFileSizeEx(hFile, &li))
        ExitOnLastErrorDebugTrace(hr, "failed to get size of file to verify resource");

    *pllSize = li.QuadPart;
    hr = S_OK;
LExit:
    return hr;
}


/*******************************************************************
 FileExistsEx

********************************************************************/
extern "C" BOOL DAPI FileExistsEx(
    __in LPCWSTR wzPath,
    __out_opt DWORD *pdwAttributes
    )
{
    Assert(wzPath && *wzPath);
    BOOL fExists = FALSE;

    WIN32_FIND_DATAW fd;
    HANDLE hff;

    memset(&fd, 0, sizeof(fd));
    if (INVALID_HANDLE_VALUE != (hff = ::FindFirstFileW(wzPath, &fd)))
    {
        ::FindClose(hff);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (pdwAttributes)
                *pdwAttributes = fd.dwFileAttributes;
            fExists = TRUE;
        }
    }

    return fExists;
}


/*******************************************************************
 FileRead - read a file into memory

********************************************************************/
extern "C" HRESULT DAPI FileRead(
    __deref_out_bcount_full(*pcbDest) LPBYTE* ppbDest,
    __out DWORD* pcbDest,
    __in LPCWSTR wzSrcPath
    )
{
    HRESULT hr = FileReadPartial(ppbDest, pcbDest, wzSrcPath, FALSE, 0, 0xFFFFFFFF, FALSE);
    return hr;
}


/*******************************************************************
 FileReadUntil - read a file into memory with a maximum size

********************************************************************/
extern "C" HRESULT DAPI FileReadUntil(
    __deref_out_bcount_full(*pcbDest) LPBYTE* ppbDest,
    __out_range(<=, cbMaxRead) DWORD* pcbDest,
    __in LPCWSTR wzSrcPath,
    __in DWORD cbMaxRead
    )
{
    HRESULT hr = FileReadPartial(ppbDest, pcbDest, wzSrcPath, FALSE, 0, cbMaxRead, FALSE);
    return hr;
}


/*******************************************************************
 FileReadPartial - read a portion of a file into memory

********************************************************************/
extern "C" HRESULT DAPI FileReadPartial(
    __deref_out_bcount_full(*pcbDest) LPBYTE* ppbDest,
    __out_range(<=, cbMaxRead) DWORD* pcbDest,
    __in LPCWSTR wzSrcPath,
    __in BOOL fSeek,
    __in DWORD cbStartPosition,
    __in DWORD cbMaxRead,
    __in BOOL fPartialOK
    )
{
    HRESULT hr = S_OK;

    HANDLE hFile = INVALID_HANDLE_VALUE;
    LARGE_INTEGER liFileSize = { 0 };
    DWORD cbData = 0;
    BYTE* pbData = NULL;

    ExitOnNull(pcbDest, hr, E_INVALIDARG, "Invalid argument pcbDest");
    ExitOnNull(ppbDest, hr, E_INVALIDARG, "Invalid argument ppbDest");
    ExitOnNull(wzSrcPath, hr, E_INVALIDARG, "Invalid argument wzSrcPath");
    ExitOnNull(*wzSrcPath, hr, E_INVALIDARG, "*wzSrcPath is null");

    hFile = ::CreateFileW(wzSrcPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    ExitOnInvalidHandleWithLastError1(hFile, hr, "Failed to open file: %S", wzSrcPath);

    if (!::GetFileSizeEx(hFile, &liFileSize))
    {
        ExitWithLastError1(hr, "Failed to get size of file: %S", wzSrcPath);
    }

    if (fSeek)
    {
        if (cbStartPosition > liFileSize.QuadPart)
        {
            ExitOnFailure3(hr = E_INVALIDARG, "Start position %d bigger than file '%S' size %d", cbStartPosition, wzSrcPath, liFileSize.QuadPart);
        }

        DWORD dwErr = ::SetFilePointer(hFile, cbStartPosition, NULL, FILE_CURRENT);
        if (INVALID_SET_FILE_POINTER == dwErr)
        {
            ExitOnLastError1(hr, "Failed to seek position %d", cbStartPosition);
        }
    }
    else
    {
        cbStartPosition = 0;
    }

    if (fPartialOK)
    {
        cbData = cbMaxRead;
    }
    else
    {
        cbData = liFileSize.LowPart - cbStartPosition; // should only need the low part because we cap at DWORD
        if (cbMaxRead < liFileSize.QuadPart - cbStartPosition)
        {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            ExitOnFailure1(hr, "Failed to load file: %S, too large.", wzSrcPath);
        }
    }

    if (*ppbDest)
    {
        LPVOID pv = MemReAlloc(*ppbDest, cbData, TRUE);
        ExitOnNull1(pv, hr, E_OUTOFMEMORY, "Failed to re-allocate memory to read in file: %S", wzSrcPath);

        pbData = static_cast<BYTE*>(pv);
    }
    else
    {
        pbData = static_cast<BYTE*>(MemAlloc(cbData, TRUE));
        ExitOnNull1(pbData, hr, E_OUTOFMEMORY, "Failed to allocate memory to read in file: %S", wzSrcPath);
    }

    DWORD cbTotalRead = 0;
    DWORD cbRead = 0;
    do
    {
        DWORD cbRemaining = 0;
        hr = ::ULongSub(cbData, cbTotalRead, &cbRemaining);
        ExitOnFailure(hr, "Underflow calculating remaining buffer size.");

        if (!::ReadFile(hFile, pbData + cbTotalRead, cbRemaining, &cbRead, NULL))
        {
            ExitWithLastError1(hr, "Failed to read from file: %S", wzSrcPath);
        }

        cbTotalRead += cbRead;
    } while (cbRead);

    if (cbTotalRead != cbData)
    {
        hr = E_UNEXPECTED;
        ExitOnFailure1(hr, "Failed to completely read file: %S", wzSrcPath);
    }

    *ppbDest = pbData;
    pbData = NULL;
    *pcbDest = cbData;

LExit:
    ReleaseMem(pbData);
    ReleaseFile(hFile);

    return hr;
}


/*******************************************************************
 FileWrite - write a file from memory

********************************************************************/
extern "C" HRESULT DAPI FileWrite(
    __in_bcount(cbData) LPCBYTE pbData,
    __in DWORD cbData,
    __in LPCWSTR pwzFileName,
    __in DWORD dwFlagsAndAttributes,
    __out_opt HANDLE* pHandle
    )
{
    HRESULT hr = S_OK;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD cbDataWritten = 0;
    DWORD cbTotal = 0;

    // Open the file
    hFile = ::CreateFileW(pwzFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, dwFlagsAndAttributes, NULL);
    ExitOnInvalidHandleWithLastError1(hFile, hr, "Failed to open file: %S", pwzFileName);

    // Write out the old data
    while (cbTotal < cbData)
    {
        if (!::WriteFile(hFile, pbData + cbTotal, cbData - cbTotal, &cbDataWritten, NULL))
        {
            ExitOnLastError1(hr, "Failed to write to file: %S", pwzFileName);
        }

        cbTotal += cbDataWritten;
    }

    if (pHandle)
    {
        *pHandle = hFile;
        hFile = INVALID_HANDLE_VALUE;
    }

LExit:
    ReleaseFile(hFile);

    return hr;
}


/*******************************************************************
 FileEnsureCopy

*******************************************************************/
extern "C" HRESULT DAPI FileEnsureCopy(
    __in LPCWSTR wzSource,
    __in LPCWSTR wzTarget,
    __in BOOL fOverwrite
    )
{
    HRESULT hr = S_OK;
    DWORD er;

    // try to move the file first
    if (::CopyFileW(wzSource, wzTarget, !fOverwrite))
    {
        ExitFunction();  // we're done
    }

    er = ::GetLastError();  // check the error and do the right thing below
    if (!fOverwrite && (ERROR_FILE_EXISTS == er || ERROR_ALREADY_EXISTS == er))
    {
        // if not overwriting this is an expected error
        ExitFunction1(hr = S_FALSE);
    }
    else if (ERROR_PATH_NOT_FOUND == er)  // if the path doesn't exist
    {
        // try to create the directory then do the copy
        LPWSTR pwzLastSlash = NULL;
        for (LPWSTR pwz = const_cast<LPWSTR>(wzTarget); *pwz; pwz++)
        {
            if (*pwz == L'\\')
            {
                pwzLastSlash = pwz;
            }
        }

        if (pwzLastSlash)
        {
            *pwzLastSlash = L'\0';	// null terminate
            hr = DirEnsureExists(wzTarget, NULL);
            *pwzLastSlash = L'\\';	// now put the slash back
            ExitOnFailureDebugTrace2(hr, "failed to create directory while copying file: '%S' to: '%S'", wzSource, wzTarget);

            // try to move again
            if (!::CopyFileW(wzSource, wzTarget, fOverwrite))
            {
                ExitOnLastErrorDebugTrace2(hr, "failed to copy file: '%S' to: '%S'", wzSource, wzTarget);
            }
        }
        else // no path was specified so just return the error
            hr = HRESULT_FROM_WIN32(er);
    }
    else // unexpected error
        hr = HRESULT_FROM_WIN32(er);

LExit:
    return hr;
}


/*******************************************************************
 FileEnsureMove

*******************************************************************/
extern "C" HRESULT DAPI FileEnsureMove(
    __in LPCWSTR wzSource,
    __in LPCWSTR wzTarget,
    __in BOOL fOverwrite,
    __in BOOL fAllowCopy
    )
{
    HRESULT hr = S_OK;
    DWORD er;

    DWORD dwFlags = 0;

    if (fOverwrite)
    {
        dwFlags |= MOVEFILE_REPLACE_EXISTING;
    }
    if (fAllowCopy)
    {
        dwFlags |= MOVEFILE_COPY_ALLOWED;
    }

    // try to move the file first
    if (::MoveFileExW(wzSource, wzTarget, dwFlags))
    {
        ExitFunction();  // we're done
    }

    er = ::GetLastError();  // check the error and do the right thing below
    if (!fOverwrite && (ERROR_FILE_EXISTS == er || ERROR_ALREADY_EXISTS == er))
    {
        // if not overwriting this is an expected error
        ExitFunction1(hr = S_FALSE);
    }
    else if (ERROR_PATH_NOT_FOUND == er)  // if the path doesn't exist
    {
        // try to create the directory then do the copy
        LPWSTR pwzLastSlash = NULL;
        for (LPWSTR pwz = const_cast<LPWSTR>(wzTarget); *pwz; pwz++)
        {
            if (*pwz == L'\\')
            {
                pwzLastSlash = pwz;
            }
        }

        if (pwzLastSlash)
        {
            *pwzLastSlash = L'\0';	// null terminate
            hr = DirEnsureExists(wzTarget, NULL);
            *pwzLastSlash = L'\\';	// now put the slash back
            ExitOnFailureDebugTrace2(hr, "failed to create directory while moving file: '%S' to: '%S'", wzSource, wzTarget);

            // try to move again
            if (!::MoveFileExW(wzSource, wzTarget, dwFlags))
            {
                ExitOnLastErrorDebugTrace2(hr, "failed to move file: '%S' to: '%S'", wzSource, wzTarget);
            }
        }
        else // no path was specified so just return the error
            hr = HRESULT_FROM_WIN32(er);
    }
    else // unexpected error
        hr = HRESULT_FROM_WIN32(er);

LExit:
    return hr;
}

/*******************************************************************
 FileCreateTemp - creates an empty temp file

 NOTE: uses ANSI functions internally so it is Win9x safe
********************************************************************/
extern "C" HRESULT DAPI FileCreateTemp(
    __in LPCWSTR wzPrefix,
    __in LPCWSTR wzExtension,
    __deref_opt_out_z LPWSTR* ppwzTempFile,
    __out_opt HANDLE* phTempFile
    )
{
    Assert(wzPrefix && *wzPrefix);
    HRESULT hr;
    LPSTR pszTempPath = NULL;
    DWORD cchTempPath = MAX_PATH;

    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    LPSTR pszTempFile = NULL;

    int i = 0;

    hr = StrAnsiAlloc(&pszTempPath, cchTempPath);
    ExitOnFailure(hr, "failed to allocate memory for the temp path");
    ::GetTempPathA(cchTempPath, pszTempPath);

    for (i = 0; i < 1000 && INVALID_HANDLE_VALUE == hTempFile; i++)
    {
        hr = StrAnsiAllocFormatted(&pszTempFile, "%s%S%05d.%S", pszTempPath, wzPrefix, i, wzExtension);
        ExitOnFailure(hr, "failed to allocate memory for log file");

        hTempFile = ::CreateFileA(pszTempFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (INVALID_HANDLE_VALUE == hTempFile)
        {
            // if the file already exists, just try again
            hr = HRESULT_FROM_WIN32(::GetLastError());
            if (HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) == hr)
            {
                hr = S_OK;
                continue;
            }
            ExitOnFailureDebugTrace1(hr, "failed to create file: %S", pszTempFile);
        }
    }

    if (ppwzTempFile)
        hr = StrAllocStringAnsi(ppwzTempFile, pszTempFile, 0, CP_UTF8);
    if (phTempFile)
        *phTempFile = hTempFile;
LExit:
    if (FAILED(hr) || !phTempFile)
        ReleaseFile(hTempFile);

    ReleaseStr(pszTempFile);
    ReleaseStr(pszTempPath);

    return hr;
}


/*******************************************************************
 FileCreateTempW - creates an empty temp file

*******************************************************************/
extern "C" HRESULT DAPI FileCreateTempW(
    __in LPCWSTR wzPrefix,
    __in LPCWSTR wzExtension,
    __deref_opt_out_z LPWSTR* ppwzTempFile,
    __out_opt HANDLE* phTempFile
    )
{
    Assert(wzPrefix && *wzPrefix);
    HRESULT hr = E_FAIL;

    WCHAR wzTempPath[MAX_PATH];
    DWORD cchTempPath = countof(wzTempPath);
    LPWSTR pwzTempFile = NULL;

    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    int i = 0;

    if (!::GetTempPathW(cchTempPath, wzTempPath))
        ExitOnLastError(hr, "failed to get temp path");

    for (i = 0; i < 1000 && INVALID_HANDLE_VALUE == hTempFile; i++)
    {
        hr = StrAllocFormatted(&pwzTempFile, L"%s%s%05d.%s", wzTempPath, wzPrefix, i, wzExtension);
        ExitOnFailure(hr, "failed to allocate memory for log file");

        hTempFile = ::CreateFileW(pwzTempFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (INVALID_HANDLE_VALUE == hTempFile)
        {
            // if the file already exists, just try again
            hr = HRESULT_FROM_WIN32(::GetLastError());
            if (HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) == hr)
            {
                hr = S_OK;
                continue;
            }
            ExitOnFailureDebugTrace1(hr, "failed to create file: %S", pwzTempFile);
        }
    }

    if (phTempFile)
        *phTempFile = hTempFile;
    if (ppwzTempFile)
    {
        *ppwzTempFile = pwzTempFile;
        pwzTempFile = NULL;
    }

LExit:
    if (FAILED(hr) || !phTempFile)
        ReleaseFile(hTempFile);

    ReleaseStr(pwzTempFile);

    return hr;
}


/*******************************************************************
 FileVersion

********************************************************************/
extern "C" HRESULT DAPI FileVersion(
    __in LPCWSTR wzFilename,
    __out DWORD *pdwVerMajor,
    __out DWORD* pdwVerMinor
    )
{
    HRESULT hr = S_OK;

    DWORD dwHandle = 0;
    UINT cbVerBuffer = 0;
    LPVOID pVerBuffer = NULL;
    VS_FIXEDFILEINFO* pvsFileInfo = NULL;
    UINT cbFileInfo = 0;

    if (0 == (cbVerBuffer = ::GetFileVersionInfoSizeW(wzFilename, &dwHandle)))
    {
        ExitOnLastErrorDebugTrace1(hr, "failed to get version info for file: %S", wzFilename);
    }

    pVerBuffer = ::GlobalAlloc(GMEM_FIXED, cbVerBuffer);
    ExitOnNullDebugTrace1(pVerBuffer, hr, E_OUTOFMEMORY, "failed to allocate version info for file: %S", wzFilename);

    if (!::GetFileVersionInfoW(wzFilename, dwHandle, cbVerBuffer, pVerBuffer))
    {
        ExitOnLastErrorDebugTrace1(hr, "failed to get version info for file: %S", wzFilename);
    }

    if (!::VerQueryValueW(pVerBuffer, L"\\", (void**)&pvsFileInfo, &cbFileInfo))
    {
        ExitOnLastErrorDebugTrace1(hr, "failed to get version value for file: %S", wzFilename);
    }

    *pdwVerMajor = pvsFileInfo->dwFileVersionMS;
    *pdwVerMinor = pvsFileInfo->dwFileVersionLS;

LExit:
    if (pVerBuffer)
    {
        ::GlobalFree(pVerBuffer);
    }
    return hr;
}


/*******************************************************************
 FileIsSame

********************************************************************/
extern "C" HRESULT DAPI FileIsSame(
    __in LPCWSTR wzFile1,
    __in LPCWSTR wzFile2,
    __out LPBOOL lpfSameFile
    )
{
    HRESULT hr = S_OK;
    HANDLE hFile1 = NULL;
    HANDLE hFile2 = NULL;
    BY_HANDLE_FILE_INFORMATION fileInfo1 = { 0 };
    BY_HANDLE_FILE_INFORMATION fileInfo2 = { 0 };

    hFile1 = ::CreateFileW(wzFile1, FILE_READ_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    ExitOnInvalidHandleWithLastError1(hFile1, hr, "Failed to open file 1. File = '%S'", wzFile1);

    hFile2 = ::CreateFileW(wzFile2, FILE_READ_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    ExitOnInvalidHandleWithLastError1(hFile2, hr, "Failed to open file 2. File = '%S'", wzFile2);

    if (!::GetFileInformationByHandle(hFile1, &fileInfo1))
    {
        ExitWithLastError1(hr, "Failed to get information for file 1. File = '%S'", wzFile1);
    }

    if (!::GetFileInformationByHandle(hFile2, &fileInfo2))
    {
        ExitWithLastError1(hr, "Failed to get information for file 2. File = '%S'", wzFile2);
    }

    *lpfSameFile = fileInfo1.dwVolumeSerialNumber == fileInfo2.dwVolumeSerialNumber &&
        fileInfo1.nFileIndexHigh == fileInfo2.nFileIndexHigh &&
        fileInfo1.nFileIndexLow == fileInfo2.nFileIndexLow ? TRUE : FALSE;

LExit:
    ReleaseFile(hFile1);
    ReleaseFile(hFile2);

    return hr;
}

/*******************************************************************
 FileEnsureDelete - deletes a file, first removing read-only,
    hidden, or system attributes if necessary.
********************************************************************/
extern "C" HRESULT DAPI FileEnsureDelete(
    __in LPCWSTR wzFile
    )
{
    HRESULT hr = S_OK;

    DWORD dwAttrib = INVALID_FILE_ATTRIBUTES;
    if (FileExistsEx(wzFile, &dwAttrib))
    {
        if (dwAttrib & FILE_ATTRIBUTE_READONLY || dwAttrib & FILE_ATTRIBUTE_HIDDEN || dwAttrib & FILE_ATTRIBUTE_SYSTEM)
        {
            if (!::SetFileAttributesW(wzFile, FILE_ATTRIBUTE_NORMAL))
            {
                ExitOnLastError1(hr, "Failed to remove attributes from file: %S", wzFile);
            }
        }

        if (!::DeleteFileW(wzFile))
        {
            ExitOnLastError1(hr, "Failed to delete file: %S", wzFile);
        }
    }

LExit:
    return hr;
}

/*******************************************************************
 FileGetTime - Gets the file time of a specified file
********************************************************************/
extern "C" HRESULT DAPI FileGetTime(
    __in LPCWSTR wzFile,  
    __out_opt  LPFILETIME lpCreationTime,
    __out_opt  LPFILETIME lpLastAccessTime,
    __out_opt  LPFILETIME lpLastWriteTime
    )
{
    HRESULT hr = S_OK;
    HANDLE hFile = NULL;

    hFile = ::CreateFileW(wzFile, FILE_READ_ATTRIBUTES, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ExitOnInvalidHandleWithLastError1(hFile, hr, "Failed to open file. File = '%S'", wzFile);

    if (!::GetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime))
    {
        ExitWithLastError1(hr, "Failed to get file time for file. File = '%S'", wzFile);
    }

LExit:
    ReleaseFile(hFile);
    return hr;
}

/*******************************************************************
 FileSetTime - Sets the file time of a specified file
********************************************************************/
extern "C" HRESULT DAPI FileSetTime(
    __in LPCWSTR wzFile,  
    __in_opt  const FILETIME *lpCreationTime,
    __in_opt  const FILETIME *lpLastAccessTime,
    __in_opt  const FILETIME *lpLastWriteTime
    )
{
    HRESULT hr = S_OK;
    HANDLE hFile = NULL;

    hFile = ::CreateFileW(wzFile, FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ExitOnInvalidHandleWithLastError1(hFile, hr, "Failed to open file. File = '%S'", wzFile);

    if (!::SetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime))
    {
        ExitWithLastError1(hr, "Failed to set file time for file. File = '%S'", wzFile);
    }

LExit:
    ReleaseFile(hFile);
    return hr;
}

/*******************************************************************
 FileReSetTime - ReSets a file's last acess and modified time to the
 creation time of the file
********************************************************************/
extern "C" HRESULT DAPI FileResetTime(
    __in LPCWSTR wzFile
    )
{
    HRESULT hr = S_OK;
    HANDLE hFile = NULL;
    FILETIME ftCreateTime;

    hFile = ::CreateFileW(wzFile, FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ExitOnInvalidHandleWithLastError1(hFile, hr, "Failed to open file. File = '%S'", wzFile);
    
    if (!::GetFileTime(hFile, &ftCreateTime, NULL, NULL))
    {
        ExitWithLastError1(hr, "Failed to get file time for file. File = '%S'", wzFile);
    }

    if (!::SetFileTime(hFile, NULL, NULL, &ftCreateTime))
    {
        ExitWithLastError1(hr, "Failed to reset file time for file. File = '%S'", wzFile);
    }

LExit:
    ReleaseFile(hFile);
    return hr;
}