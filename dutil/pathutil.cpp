//-------------------------------------------------------------------------------------------------
// <copyright file="pathutil.cpp" company="Microsoft">
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
//    Path helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

#define PATH_GOOD_ENOUGH 64

/*******************************************************************
 PathFile -  returns a pointer to the file part of the path

********************************************************************/
extern "C" LPWSTR DAPI PathFile(
    __in LPCWSTR wzPath
    )
{
    if (!wzPath)
    {
        return NULL;
    }

    LPWSTR wzFile = const_cast<LPWSTR>(wzPath);
    for (LPWSTR wz = wzFile; *wz; wz++)
    {
        // valid delineators 
        //     \ => Windows path
        //     / => unix and URL path
        //     : => relative path from mapped root
        if (L'\\' == *wz || L'/' == *wz || (L':' == *wz && wz == wzPath + 1))
        {
            wzFile = wz + 1;
        }
    }

    return wzFile;
}


/*******************************************************************
 PathGetDirectory - extracts the directory from a path.

********************************************************************/
extern "C" HRESULT DAPI PathGetDirectory(
    __in LPCWSTR wzPath,
    __out LPWSTR *ppwzDirectory
    )
{
    HRESULT hr = S_OK;
    DWORD cchDirectory = 0;

    for (LPCWSTR wz = wzPath; *wz; wz++)
    {
        // valid delineators:
        //     \ => Windows path
        //     / => unix and URL path
        //     : => relative path from mapped root
        if (L'\\' == *wz || L'/' == *wz || (L':' == *wz && wz == wzPath + 1))
        {
            cchDirectory = static_cast<DWORD>(wz - wzPath) + 1;
        }
    }

    hr = StrAllocString(ppwzDirectory, wzPath, cchDirectory);
    ExitOnFailure(hr, "Failed to copy directory.");

LExit:
    return hr;
}


/*******************************************************************
 PathExpand - gets the full path to a file resolving environment
              variables along the way.

********************************************************************/
extern "C" HRESULT DAPI PathExpand(
    __out LPWSTR *ppwzFullPath,
    __in LPCWSTR wzRelativePath,
    __in DWORD dwResolveFlags
    )
{
    Assert(wzRelativePath && *wzRelativePath);

    HRESULT hr = S_OK;
    DWORD cch = 0;
    LPWSTR pwzExpandedPath = NULL;
    DWORD cchExpandedPath = 0;

    LPWSTR pwzFullPath = NULL;

    //
    // First, expand any environment variables.
    //
    if (dwResolveFlags & PATH_EXPAND_ENVIRONMENT)
    {
        cchExpandedPath = PATH_GOOD_ENOUGH;

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

        if (MAX_PATH < cch)
        {
            hr = PathPrefix(&pwzExpandedPath); // ignore invald arg from path prefix because this may not be a complete path yet
            if (E_INVALIDARG == hr)
            {
                hr = S_OK;
            }
            ExitOnFailure(hr, "Failed to prefix long path after expanding environment variables.");

            hr = StrMaxLength(pwzExpandedPath, reinterpret_cast<DWORD_PTR *>(&cchExpandedPath));
            ExitOnFailure(hr, "Failed to get max length of expanded path.");
        }
    }

    //
    // Second, get the full path.
    //
    if (dwResolveFlags & PATH_EXPAND_FULLPATH)
    {
        LPWSTR wzFileName = NULL;
        LPCWSTR wzPath = pwzExpandedPath ? pwzExpandedPath : wzRelativePath;
        DWORD cchFullPath = PATH_GOOD_ENOUGH < cchExpandedPath ? cchExpandedPath : PATH_GOOD_ENOUGH;

        hr = StrAlloc(&pwzFullPath, cchFullPath);
        ExitOnFailure(hr, "Failed to allocate space for full path.");

        cch = ::GetFullPathNameW(wzPath, cchFullPath, pwzFullPath, &wzFileName);
        if (0 == cch)
        {
            ExitWithLastError1(hr, "Failed to get full path for string: %S", wzPath);
        }
        else if (cchFullPath < cch)
        {
            cchFullPath = cch < MAX_PATH ? cch : cch + 7; // ensure space for "\\?\UNC" prefix if needed
            hr = StrAlloc(&pwzFullPath, cchFullPath);
            ExitOnFailure(hr, "Failed to re-allocate more space for full path.");

            cch = ::GetFullPathNameW(wzPath, cchFullPath, pwzFullPath, &wzFileName);
            if (0 == cch)
            {
                ExitWithLastError1(hr, "Failed to get full path for string: %S", wzPath);
            }
            else if (cchFullPath < cch)
            {
                hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                ExitOnFailure(hr, "Failed to allocate buffer for full path.");
            }
        }

        if (MAX_PATH < cch)
        {
            hr = PathPrefix(&pwzFullPath);
            ExitOnFailure(hr, "Failed to prefix long path after expanding.");
        }
    }
    else
    {
        pwzFullPath = pwzExpandedPath;
        pwzExpandedPath = NULL;
    }

    if (pwzFullPath)
    {
        *ppwzFullPath = pwzFullPath;
        pwzFullPath = NULL;
    }
    else
    {
        hr = StrAllocString(ppwzFullPath, wzRelativePath, 0);
        ExitOnFailure(hr, "Failed to copy relative path into full path.");
    }

LExit:
    ReleaseStr(pwzFullPath);
    ReleaseStr(pwzExpandedPath);

    return hr;
}


/*******************************************************************
 PathPrefix - prefixes a full path with \\?\ or \\?\UNC as 
              appropriate.

********************************************************************/
extern "C" HRESULT DAPI PathPrefix(
    __inout LPWSTR *ppwzFullPath
    )
{
    HRESULT hr = S_OK;
    LPWSTR pwzFullPath = *ppwzFullPath;

    if (((L'a' <= pwzFullPath[0] && L'z' >= pwzFullPath[0]) ||
         (L'A' <= pwzFullPath[0] && L'Z' >= pwzFullPath[0])) &&
        L':' == pwzFullPath[1] &&
        L'\\' == pwzFullPath[2]) // normal path
    {
        hr = StrAllocPrefix(ppwzFullPath, L"\\\\?\\", 4);
        ExitOnFailure(hr, "Failed to add prefix to file path.");
    }
    else if (L'\\' == pwzFullPath[0] && L'\\' == pwzFullPath[1]) // UNC
    {
        // ensure that we're not already prefixed
        if (!(L'?' == pwzFullPath[2] && L'\\' == pwzFullPath[3]))
        {
            DWORD_PTR cbFullPath;

            hr = StrSize(*ppwzFullPath, &cbFullPath);
            ExitOnFailure(hr, "Failed to get size of full path.");

            memmove(pwzFullPath, pwzFullPath + 1, cbFullPath - sizeof(WCHAR));

            hr = StrAllocPrefix(ppwzFullPath, L"\\\\?\\UNC", 7);
            ExitOnFailure(hr, "Failed to add prefix to UNC path.");
        }
    }
    else
    {
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "Invalid path provided to prefix.");
    }

LExit:
    return hr;
}


/*******************************************************************
 PathEnsureBackslashTerminated - appends a \ if path does not have it
                                 already.

********************************************************************/
extern "C" HRESULT DAPI PathFixedBackslashTerminate(
    __inout_ecount_z(cchPath) LPWSTR wzPath,
    __in DWORD_PTR cchPath
    )
{
    HRESULT hr = S_OK;
    size_t cLength = 0;

    hr = ::StringCchLengthW(wzPath, cchPath, &cLength);
    ExitOnFailure(hr, "Failed to get length of path.");

    if (cLength >= cchPath)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    else if (L'\\' != wzPath[cLength - 1])
    {
        wzPath[cLength] = L'\\';
        wzPath[cLength + 1] = L'\0';
    }

LExit:
    return hr;
}


extern "C" HRESULT DAPI PathBackslashTerminate(
    __inout LPWSTR* ppwzPath
    )
{
    Assert(ppwzPath);

    HRESULT hr = S_OK;
    DWORD_PTR cchPath = 0;
    size_t cLength = 0;

    hr = StrMaxLength(*ppwzPath, &cchPath);
    ExitOnFailure(hr, "Failed to get size of path string.");

    hr = ::StringCchLengthW(*ppwzPath, cchPath, &cLength);
    ExitOnFailure(hr, "Failed to get length of path.");

    if (L'\\' != (*ppwzPath)[cLength - 1])
    {
        hr = StrAllocConcat(ppwzPath, L"\\", 1);
        ExitOnFailure(hr, "Failed to concat backslash onto string.");
    }

LExit:
    return hr;
}


/*******************************************************************
 PathForCurrentProcess - gets the full path to the currently executing
                         process or (optinally) a module inside the process.

********************************************************************/
extern "C" HRESULT DAPI PathForCurrentProcess(
    __inout LPWSTR *ppwzFullPath,
    __in_opt HMODULE hModule
    )
{
    HRESULT hr = S_OK;
    DWORD cch = MAX_PATH;

    do
    {
        hr = StrAlloc(ppwzFullPath, cch);
        ExitOnFailure(hr, "Failed to allocate string for module path.");

        DWORD cchRequired = ::GetModuleFileNameW(hModule, *ppwzFullPath, cch);
        if (0 == cchRequired)
        {
            ExitWithLastError(hr, "Failed to get path for executing process.");
        }
        else if (cchRequired == cch)
        {
            cch = cchRequired + 1;
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }
        else
        {
            hr = S_OK;
        }
    } while (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) == hr);

LExit:
    return hr;
}


/*******************************************************************
 PathCreateTempFile

 Note: if wzDirectory is null, ::GetTempPath() will be used instead.
       if wzFileNameTemplate is null, GetTempFileName() will be used instead.
*******************************************************************/
extern "C" HRESULT DAPI PathCreateTempFile(
    __in_opt LPCWSTR wzDirectory,
    __in_opt __format_string LPCWSTR wzFileNameTemplate,
    __in DWORD dwUniqueCount,
    __in DWORD dwFileAttributes,
    __out_opt LPWSTR* ppwzTempFile,
    __out_opt HANDLE* phTempFile
    )
{
    AssertSz(0 < dwUniqueCount, "Must specify a non-zero unique count.");

    HRESULT hr = S_OK;

    LPWSTR pwzTempPath = NULL;
    DWORD cchTempPath = MAX_PATH;

    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    LPWSTR pwz = NULL;
    LPWSTR pwzTempFile = NULL;

    if (wzDirectory && *wzDirectory)
    {
        hr = StrAllocString(&pwzTempPath, wzDirectory, 0);
        ExitOnFailure(hr, "Failed to copy temp path.");
    }
    else
    {
        hr = StrAlloc(&pwzTempPath, cchTempPath);
        ExitOnFailure(hr, "Failed to allocate memory for the temp path.");

        if (!::GetTempPathW(cchTempPath, pwzTempPath))
        {
            ExitWithLastError(hr, "Failed to get temp path.");
        }
    }

    if (wzFileNameTemplate && *wzFileNameTemplate)
    {
        for (DWORD i = 1; i <= dwUniqueCount && INVALID_HANDLE_VALUE == hTempFile; ++i)
        {
            hr = StrAllocFormatted(&pwz, wzFileNameTemplate, i);
            ExitOnFailure(hr, "Failed to allocate memory for file template.");

            hr = StrAllocFormatted(&pwzTempFile, L"%s%s", pwzTempPath, pwz);
            ExitOnFailure(hr, "Failed to allocate temp file name.");

            hTempFile = ::CreateFileW(pwzTempFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_NEW, dwFileAttributes, NULL);
            if (INVALID_HANDLE_VALUE == hTempFile)
            {
                // if the file already exists, just try again
                hr = HRESULT_FROM_WIN32(::GetLastError());
                if (HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) == hr)
                {
                    hr = S_OK;
                }
                ExitOnFailure1(hr, "Failed to create file: %S", pwzTempFile);
            }
        }
    }

    // If we were not able to or we did not try to create a temp file, ask
    // the system to provide us a temp file using its built-in mechanism.
    if (INVALID_HANDLE_VALUE == hTempFile)
    {
        hr = StrAlloc(&pwzTempFile, MAX_PATH);
        ExitOnFailure(hr, "failed to allocate memory for the temp path");

        if (!::GetTempFileNameW(pwzTempPath, L"TMP", 0, pwzTempFile))
        {
            ExitWithLastError(hr, "Failed to create new temp file name.");
        }

        hTempFile = ::CreateFileW(pwzTempFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, dwFileAttributes, NULL);
        if (INVALID_HANDLE_VALUE == hTempFile)
        {
            ExitWithLastError1(hr, "Failed to open new temp file: %S", pwzTempFile);
        }
    }

    // If the caller wanted the temp file name or handle, return them here.
    if (ppwzTempFile)
    {
        *ppwzTempFile = pwzTempFile;
        pwzTempFile = NULL;
    }

    if (phTempFile)
    {
        *phTempFile = hTempFile;
        hTempFile = INVALID_HANDLE_VALUE;
    }

LExit:
    if (INVALID_HANDLE_VALUE != hTempFile)
    {
        ::CloseHandle(hTempFile);
    }

    ReleaseStr(pwz);
    ReleaseStr(pwzTempFile);
    ReleaseStr(pwzTempPath);

    return hr;
}


/*******************************************************************
 PathCreateTempDirectory

 Note: if wzDirectory is null, ::GetTempPath() will be used instead.
*******************************************************************/
extern "C" HRESULT DAPI PathCreateTempDirectory(
    __in_opt LPCWSTR wzDirectory,
    __in __format_string LPCWSTR wzDirectoryNameTemplate,
    __in DWORD dwUniqueCount,
    __out LPWSTR* ppwzTempDirectory
    )
{
    AssertSz(wzDirectoryNameTemplate && *wzDirectoryNameTemplate, "DirectorNameTemplate must be specified.");
    AssertSz(0 < dwUniqueCount, "Must specify a non-zero unique count.");

    HRESULT hr = S_OK;

    LPWSTR pwzTempPath = NULL;
    DWORD cchTempPath = MAX_PATH;

    LPWSTR pwz = NULL;
    LPWSTR pwzTempDirectory = NULL;

    if (wzDirectory && *wzDirectory)
    {
        hr = StrAllocString(&pwzTempPath, wzDirectory, 0);
        ExitOnFailure(hr, "Failed to copy temp path.");
    }
    else
    {
        hr = StrAlloc(&pwzTempPath, cchTempPath);
        ExitOnFailure(hr, "Failed to allocate memory for the temp path.");

        if (!::GetTempPathW(cchTempPath, pwzTempPath))
        {
            ExitWithLastError(hr, "Failed to get temp path.");
        }
    }

    for (DWORD i = 1; i <= dwUniqueCount; ++i)
    {
        hr = StrAllocFormatted(&pwz, wzDirectoryNameTemplate, i);
        ExitOnFailure(hr, "Failed to allocate memory for directory name template.");

        hr = StrAllocFormatted(&pwzTempDirectory, L"%s%s", pwzTempPath, pwz);
        ExitOnFailure(hr, "Failed to allocate temp directory name.");

        if (!::CreateDirectoryW(pwzTempDirectory, NULL))
        {
            DWORD er = ::GetLastError();
            if (ERROR_ALREADY_EXISTS == er)
            {
                hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
                continue;
            }
            else if (ERROR_PATH_NOT_FOUND == er)
            {
                hr = DirEnsureExists(pwzTempDirectory, NULL);
                break;
            }
            else
            {
                hr = HRESULT_FROM_WIN32(er);
                break;
            }
        }
        else
        {
            hr = S_OK;
            break;
        }
    }
    ExitOnFailure(hr, "Failed to create temp directory.");

    hr = PathBackslashTerminate(&pwzTempDirectory);
    ExitOnFailure(hr, "Failed to ensure temp directory is backslash terminated.");

    *ppwzTempDirectory = pwzTempDirectory;
    pwzTempDirectory = NULL;

LExit:
    ReleaseStr(pwz);
    ReleaseStr(pwzTempDirectory);
    ReleaseStr(pwzTempPath);

    return hr;
}

/*******************************************************************
 PathGetKnownFolder - returns the path to a well-known shell folder

*******************************************************************/
extern "C" HRESULT DAPI PathGetKnownFolder(
    __in int csidl,
    __out LPWSTR* psczKnownFolder
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczKnownFolder = NULL;

    hr = StrAlloc(&sczKnownFolder, MAX_PATH);
    ExitOnFailure(hr, "Failed to allocate memory for known folder.");

    hr = ::SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, sczKnownFolder);
    ExitOnFailure(hr, "Failed to get known folder path.");

    hr = PathBackslashTerminate(&sczKnownFolder);
    ExitOnFailure(hr, "Failed to ensure known folder path is backslash terminated.");

    *psczKnownFolder = sczKnownFolder;
    sczKnownFolder = NULL;

LExit:
    ReleaseStr(sczKnownFolder);

    return hr;
}


/*******************************************************************
 PathIsAbsolute - returns true if the path is absolute; false 
    otherwise.
*******************************************************************/
extern "C" BOOL DAPI PathIsAbsolute(
    __in LPCWSTR sczPath
    )
{
    DWORD dwLength = lstrlenW(sczPath);
    return (1 < dwLength) && (sczPath[0] == L'\\') || (sczPath[1] == L':');
}


/*******************************************************************
 PathConcat - like .NET's Path.Combine, lets you build up a path
    one piece -- file or directory -- at a time.

*******************************************************************/
extern "C" HRESULT DAPI PathConcat(
    __in_opt LPCWSTR sczPath1,
    __in_opt LPCWSTR sczPath2,
    __out LPWSTR* psczCombined
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczCombined= NULL;

    if (!sczPath2 || 0 == lstrlenW(sczPath2))
    {
        hr = StrAllocString(&sczCombined, sczPath1, 0);
        ExitOnFailure(hr, "Failed to copy just path1 to output.");
    }
    else if (!sczPath1 || 0 == lstrlenW(sczPath1) || PathIsAbsolute(sczPath2))
    {
        hr = StrAllocString(&sczCombined, sczPath2, 0);
        ExitOnFailure(hr, "Failed to copy just path2 to output.");
    }
    else
    {
        hr = StrAllocString(&sczCombined, sczPath1, 0);
        ExitOnFailure(hr, "Failed to copy path1 to output.");

        hr = PathBackslashTerminate(&sczCombined);
        ExitOnFailure(hr, "Failed to backslashify.");

        hr = StrAllocConcat(&sczCombined, sczPath2, 0);
        ExitOnFailure(hr, "Failed to append path2 to output.");
    }

    *psczCombined = sczCombined;
    sczCombined = NULL;

LExit:
    ReleaseStr(sczCombined);

    return hr;
}
