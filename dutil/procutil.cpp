//-------------------------------------------------------------------------------------------------
// <copyright file="procutil.cpp" company="Microsoft">
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
//    Process helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


// private functions
static HRESULT CreatePipes(
    __out HANDLE *phOutRead,
    __out HANDLE *phOutWrite,
    __out HANDLE *phErrWrite,
    __out HANDLE *phInRead,
    __out HANDLE *phInWrite
    );

static BOOL CALLBACK CloseWindowEnumCallback(
    __in HWND hWnd,
    __in LPARAM lParam
    );


/********************************************************************
 ProcExecute() - executes a command-line.

*******************************************************************/
extern "C" HRESULT DAPI ProcExecute(
    __in LPWSTR wzCommand,
    __out HANDLE *phProcess,
    __out_opt HANDLE *phChildStdIn,
    __out_opt HANDLE *phChildStdOutErr
    )
{
    HRESULT hr = S_OK;

    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOW si = { 0 };

    HANDLE hOutRead = INVALID_HANDLE_VALUE;
    HANDLE hOutWrite = INVALID_HANDLE_VALUE;
    HANDLE hErrWrite = INVALID_HANDLE_VALUE;
    HANDLE hInRead = INVALID_HANDLE_VALUE;
    HANDLE hInWrite = INVALID_HANDLE_VALUE;

    // Create redirect pipes.
    hr = CreatePipes(&hOutRead, &hOutWrite, &hErrWrite, &hInRead, &hInWrite);
    ExitOnFailure(hr, "failed to create output pipes");

    // Set up startup structure.
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError = hErrWrite;

#pragma prefast(push)
#pragma prefast(disable:25028)
    if (::CreateProcessW(NULL,
        wzCommand, // command line
        NULL, // security info
        NULL, // thread info
        TRUE, // inherit handles
        ::GetPriorityClass(::GetCurrentProcess()) | CREATE_NO_WINDOW, // creation flags
        NULL, // environment
        NULL, // cur dir
        &si,
        &pi))
#pragma prefast(pop)
    {
        // Close child process output/input handles so child doesn't hang
        // while waiting for input from parent process.
        ::CloseHandle(hOutWrite);
        hOutWrite = INVALID_HANDLE_VALUE;

        ::CloseHandle(hErrWrite);
        hErrWrite = INVALID_HANDLE_VALUE;

        ::CloseHandle(hInRead);
        hInRead = INVALID_HANDLE_VALUE;
    }
    else
    {
        ExitWithLastError(hr, "Process failed to execute.");
    }

    *phProcess = pi.hProcess;
    pi.hProcess = 0;

    if (phChildStdIn)
    {
        *phChildStdIn = hInWrite;
        hInWrite = INVALID_HANDLE_VALUE;
    }

    if (phChildStdOutErr)
    {
        *phChildStdOutErr = hOutRead;
        hOutRead = INVALID_HANDLE_VALUE;
    }

LExit:
    if (pi.hThread)
    {
        ::CloseHandle(pi.hThread);
    }

    if (pi.hProcess)
    {
        ::CloseHandle(pi.hProcess);
    }

    if (INVALID_HANDLE_VALUE != hOutRead)
    {
        ::CloseHandle(hOutRead);
    }

    if (INVALID_HANDLE_VALUE != hOutWrite)
    {
        ::CloseHandle(hOutWrite);
    }

    if (INVALID_HANDLE_VALUE != hErrWrite)
    {
        ::CloseHandle(hErrWrite);
    }

    if (INVALID_HANDLE_VALUE != hInRead)
    {
        ::CloseHandle(hInRead);
    }

    if (INVALID_HANDLE_VALUE != hInWrite)
    {
        ::CloseHandle(hInWrite);
    }


    return hr;
}


/********************************************************************
 ProcWaitForCompletion() - waits for process to complete and gets return code.

*******************************************************************/
extern "C" HRESULT DAPI ProcWaitForCompletion(
    __in HANDLE hProcess,
    __in DWORD dwTimeout,
    __out DWORD *pReturnCode
    )
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;

    // Wait for everything to finish
    er = ::WaitForSingleObject(hProcess, dwTimeout);
    if (WAIT_FAILED == er)
    {
        ExitWithLastError(hr, "Failed to wait for process to complete.");
    }
    else if (WAIT_TIMEOUT == er)
    {
        hr = HRESULT_FROM_WIN32(er);
        ExitOnFailure(hr, "Timed out while waiting for process to complete.");
    }

    if (!::GetExitCodeProcess(hProcess, &er))
    {
        ExitWithLastError(hr, "Failed to get process return code.");
    }

    *pReturnCode = er;

LExit:
    return hr;
}

/********************************************************************
 ProcWaitForCompletion() - waits for multiple processes to complete.

*******************************************************************/
extern "C" HRESULT DAPI ProcWaitForIds(
    __in_ecount(cProcessIds) const DWORD rgdwProcessIds[],
    __in DWORD cProcessIds,
    __in DWORD dwMilliseconds
    )
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    HANDLE hProcess = NULL;
    HANDLE * rghProcesses = NULL;
    DWORD cProcesses = 0;

    rghProcesses =  static_cast<HANDLE*>(MemAlloc(sizeof(DWORD) * cProcessIds, TRUE));
    ExitOnNull(rgdwProcessIds, hr, E_OUTOFMEMORY, "Failed to allocate array for process ID Handles.");

    for (DWORD i = 0; i < cProcessIds; ++i)
    {
        hProcess = ::OpenProcess(SYNCHRONIZE, FALSE, rgdwProcessIds[i]);
        if (hProcess != NULL)
        {
            rghProcesses[cProcesses++] = hProcess;
        }
    }

    er = ::WaitForMultipleObjects(cProcesses, rghProcesses, TRUE, dwMilliseconds);
    if (WAIT_FAILED == er)
    {
        ExitWithLastError(hr, "Failed to wait for process to complete.");
    }
    else if (WAIT_TIMEOUT == er)
    {
        hr = HRESULT_FROM_WIN32(er);
        ExitOnFailure(hr, "Timed out while waiting for process to complete.");
    }
    
LExit:
    if (rghProcesses)
    {
        for (DWORD i = 0; i < cProcesses; ++i)
        {
            if (NULL != rghProcesses[i])
            {
                ::CloseHandle(rghProcesses[i]);
            }
        }
        
        MemFree(rghProcesses);
    }

    return hr;
}

/********************************************************************
 ProcCloseIds() - sends WM_CLOSE messages to all process ids.

*******************************************************************/
extern "C" HRESULT DAPI ProcCloseIds(
    __in_ecount(cProcessIds) const DWORD* pdwProcessIds,
    __in DWORD cProcessIds
    )
{
    HRESULT hr = S_OK;

    for (DWORD i = 0; i < cProcessIds; ++i)
    {
        if (!::EnumWindows(&CloseWindowEnumCallback, pdwProcessIds[i]))
        {
            ExitWithLastError(hr, "Failed to enumerate windows.");
        }
    }

LExit:
    return hr;
}


static HRESULT CreatePipes(
    __out HANDLE *phOutRead,
    __out HANDLE *phOutWrite,
    __out HANDLE *phErrWrite,
    __out HANDLE *phInRead,
    __out HANDLE *phInWrite
    )
{
    HRESULT hr = S_OK;
    SECURITY_ATTRIBUTES sa;
    HANDLE hOutTemp = INVALID_HANDLE_VALUE;
    HANDLE hInTemp = INVALID_HANDLE_VALUE;

    HANDLE hOutRead = INVALID_HANDLE_VALUE;
    HANDLE hOutWrite = INVALID_HANDLE_VALUE;
    HANDLE hErrWrite = INVALID_HANDLE_VALUE;
    HANDLE hInRead = INVALID_HANDLE_VALUE;
    HANDLE hInWrite = INVALID_HANDLE_VALUE;

    // Fill out security structure so we can inherit handles
    ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes
    if (!::CreatePipe(&hOutTemp, &hOutWrite, &sa, 0))
    {
        ExitWithLastError(hr, "failed to create output pipe");
    }

    if (!::CreatePipe(&hInRead, &hInTemp, &sa, 0))
    {
        ExitWithLastError(hr, "failed to create input pipe");
    }

    // Duplicate output pipe so standard error and standard output write to the same pipe.
    if (!::DuplicateHandle(::GetCurrentProcess(), hOutWrite, ::GetCurrentProcess(), &hErrWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        ExitWithLastError(hr, "failed to duplicate write handle");
    }

    // We need to create new "output read" and "input write" handles that are non inheritable.  Otherwise CreateProcess will creates handles in 
    // the child process that can't be closed.
    if (!::DuplicateHandle(::GetCurrentProcess(), hOutTemp, ::GetCurrentProcess(), &hOutRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        ExitWithLastError(hr, "failed to duplicate output pipe");
    }

    if (!::DuplicateHandle(::GetCurrentProcess(), hInTemp, ::GetCurrentProcess(), &hInWrite, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        ExitWithLastError(hr, "failed to duplicate input pipe");
    }

    // now that everything has succeeded, assign to the outputs
    *phOutRead = hOutRead;
    hOutRead = INVALID_HANDLE_VALUE;

    *phOutWrite = hOutWrite;
    hOutWrite = INVALID_HANDLE_VALUE;

    *phErrWrite = hErrWrite;
    hErrWrite = INVALID_HANDLE_VALUE;

    *phInRead = hInRead;
    hInRead = INVALID_HANDLE_VALUE;

    *phInWrite = hInWrite;
    hInWrite = INVALID_HANDLE_VALUE;

LExit:
    if (INVALID_HANDLE_VALUE != hOutRead)
    {
        ::CloseHandle(hOutRead);
    }

    if (INVALID_HANDLE_VALUE != hOutWrite)
    {
        ::CloseHandle(hOutWrite);
    }

    if (INVALID_HANDLE_VALUE != hErrWrite)
    {
        ::CloseHandle(hErrWrite);
    }

    if (INVALID_HANDLE_VALUE != hInRead)
    {
        ::CloseHandle(hInRead);
    }

    if (INVALID_HANDLE_VALUE != hInWrite)
    {
        ::CloseHandle(hInWrite);
    }

    if (INVALID_HANDLE_VALUE != hOutTemp)
    {
        ::CloseHandle(hOutTemp);
    }

    if (INVALID_HANDLE_VALUE != hInTemp)
    {
        ::CloseHandle(hInTemp);
    }

    return hr;
}


/********************************************************************
 CloseWindowCallback() - outputs trace and log info

*******************************************************************/
static BOOL CALLBACK CloseWindowEnumCallback(
    __in HWND hWnd,
    __in LPARAM lParam
    )
{
    DWORD dwPid = static_cast<DWORD>(lParam);
    DWORD dwProcessId = 0;

    ::GetWindowThreadProcessId(hWnd, &dwProcessId);
    if (dwPid == dwProcessId)
    {
        ::SendMessageW(hWnd, WM_CLOSE, 0, 0);
    }

    return TRUE;
}
