//-------------------------------------------------------------------------------------------------
// <copyright file="dirutil.cpp" company="Microsoft">
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
//    Directory helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


/*******************************************************************
 DirExists

*******************************************************************/
extern "C" BOOL DAPI DirExists(
    __in LPCWSTR wzPath, 
    __out_opt DWORD *pdwAttributes
    )
{
    Assert(wzPath);

    BOOL fExists = FALSE;

    DWORD dwAttributes = ::GetFileAttributesW(wzPath);
    if (0xFFFFFFFF == dwAttributes) // TODO: figure out why "INVALID_FILE_ATTRIBUTES" can't be used here
    {
        ExitFunction();
    }

    if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (pdwAttributes)
        {
            *pdwAttributes = dwAttributes;
        }

        fExists = TRUE;
    }

LExit:
    return fExists;
}


/*******************************************************************
 DirCreateTempPath

 *******************************************************************/
extern "C" HRESULT DAPI DirCreateTempPath(
    __in LPCWSTR wzPrefix,
    __in LPWSTR wzPath,
    __in DWORD cchPath
    )
{
    Assert(wzPrefix);
    Assert(wzPath);

    HRESULT hr = S_OK;

    WCHAR wzDir[MAX_PATH];
    WCHAR wzFile[MAX_PATH];
    DWORD cch = 0;

    cch = ::GetTempPathW(countof(wzDir), wzDir);
    if (!cch || cch >= countof(wzDir))
    {
        ExitWithLastError(hr, "Failed to GetTempPath.");
    }

    if (!::GetTempFileNameW(wzDir, wzPrefix, 0, wzFile))
    {
        ExitWithLastError(hr, "Failed to GetTempFileName.");
    }

    hr = ::StringCchCopyW(wzPath, cchPath, wzFile);

LExit:
    return hr;
}


/*******************************************************************
 DirEnsureExists

*******************************************************************/
extern "C" HRESULT DAPI DirEnsureExists(
    __in LPCWSTR wzPath, 
    __in_opt LPSECURITY_ATTRIBUTES psa
    )
{
    HRESULT hr = S_OK;
    UINT er;

    // try to create this directory
    if (!::CreateDirectoryW(wzPath, psa))
    {
        // if the directory already exists, bail
        er = ::GetLastError();
        if (ERROR_ALREADY_EXISTS == er)
        {
            ExitFunction1(hr = S_OK);
        }

        // get the parent path and try to create it
        LPWSTR pwzLastSlash = NULL;
        for (LPWSTR pwz = const_cast<LPWSTR>(wzPath); *pwz; pwz++)
        {
            if (*pwz == L'\\')
            {
                pwzLastSlash = pwz;
            }
        }

        // if there is no parent directory fail
        ExitOnNullDebugTrace(pwzLastSlash, hr, HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "cannot find parent path");

        *pwzLastSlash = L'\0';	// null terminate the parent path
        hr = DirEnsureExists(wzPath, psa);   // recurse!
        *pwzLastSlash = L'\\';  // put the slash back
        ExitOnFailureDebugTrace1(hr, "failed to create path: %S", wzPath);

        // try to create the directory now that all parents are created
        if (!::CreateDirectoryW(wzPath, psa))
        {
            // if the directory already exists for some reason no error
            er = ::GetLastError();
            if (ERROR_ALREADY_EXISTS == er)
            {
                hr = S_FALSE;
            }
            else
            {
                hr = HRESULT_FROM_WIN32(er);
            }
        }
        else
        {
            hr = S_OK;
        }
    }

LExit:
    return hr;
}


/*******************************************************************
 DirEnsureDelete - removes an entire directory structure

 NOTE: path must be in canonical form
*******************************************************************/
extern "C" HRESULT DAPI DirEnsureDelete(
    __in LPCWSTR wzPath,
    __in BOOL fDeleteFiles,
    __in BOOL fRecurse
    )
{
    Assert(wzPath && *wzPath);

    HRESULT hr = S_OK;
    DWORD er;

    DWORD dwAttrib;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    LPWSTR pwzDelete = NULL;
    WIN32_FIND_DATAW wfd;

    if (-1 == (dwAttrib = ::GetFileAttributesW(wzPath)))
    {
        ExitOnLastError1(hr, "Failed to get attributes for path: %S", wzPath);
    }

    if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (dwAttrib & FILE_ATTRIBUTE_READONLY)
        {
            if (!::SetFileAttributesW(wzPath, FILE_ATTRIBUTE_NORMAL))
            {
                ExitOnLastError1(hr, "Failed to remove read-only attribute from path: %S", wzPath);
            }
        }

        // If we're not recursing, just exit and we'll try to delete this path alone down
        // below.
        if (!fDeleteFiles && !fRecurse)
        {
            ExitFunction1(hr = S_OK);
        }

        // Delete everything in this directory.
        hr = StrAllocString(&pwzDelete, wzPath, 0);
        ExitOnFailure1(hr, "Failed to copy source string: %S", wzPath);
        hr = StrAllocConcat(&pwzDelete, L"\\*.*", 0);
        ExitOnFailure1(hr, "Failed to concat wild cards to string: %S", pwzDelete);

        hFind = ::FindFirstFileW(pwzDelete, &wfd);
        if (INVALID_HANDLE_VALUE == hFind)
        {
            ExitOnLastError1(hr, "failed to get first file in directory: %S", wzPath);
        }

        do
        {
            // Skip the dot directories.
            if (L'.' == wfd.cFileName[0] && (L'\0' == wfd.cFileName[1] || (L'.' == wfd.cFileName[1] && L'\0' == wfd.cFileName[2])))
            {
                continue;
            }

            hr = StrAllocString(&pwzDelete, wzPath, 0);
            ExitOnFailure1(hr, "Failed to copy source string: %S", wzPath);
            hr = StrAllocConcat(&pwzDelete, wfd.cFileName, 0);
            ExitOnFailure2(hr, "Failed to concat filename '%S' to string: %S", wfd.cFileName, pwzDelete);

            if (fRecurse && wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                hr = StrAllocConcat(&pwzDelete, L"\\", 0);
                ExitOnFailure1(hr, "Failed to concat \\ to string: %S", pwzDelete);

                hr = DirEnsureDelete(pwzDelete, fDeleteFiles, fRecurse); // recursive call
                ExitOnFailure1(hr, "Failed to delete: %S", pwzDelete);
            }
            else if (fDeleteFiles)  // this is a file, just delete it
            {
                if (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY || wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN || wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
                {
                    if (!::SetFileAttributesW(pwzDelete, FILE_ATTRIBUTE_NORMAL))
                    {
                        ExitOnLastError1(hr, "Failed to remove attributes from file: %S", pwzDelete);
                    }
                }

                if (!::DeleteFileW(pwzDelete))
                {
                    ExitOnLastError1(hr, "Failed to delete file: %S", pwzDelete);
                }
            }
        } while (::FindNextFileW(hFind, &wfd));

        er = ::GetLastError();
        if (ERROR_NO_MORE_FILES == er)
        {
            hr = S_OK;
        }
        else
        {
            hr = HRESULT_FROM_WIN32(er);
            ExitOnFailure1(hr, "Failed while looping through files in directory: %S", wzPath);
        }
    }
    else
    {
        hr = E_UNEXPECTED;
        ExitOnFailure1(hr, "Directory delete cannot delete file: %S", wzPath);
    }

    Assert(S_OK == hr);
LExit:
    if (INVALID_HANDLE_VALUE != hFind)
    {
        ::FindClose(hFind);
    }

    ReleaseStr(pwzDelete);

    // If all is well so far, try to remove the directory
    if (SUCCEEDED(hr) && !::RemoveDirectoryW(wzPath))
    {
        er = ::GetLastError();
        hr = HRESULT_FROM_WIN32(er);
    }

    return hr;
}
