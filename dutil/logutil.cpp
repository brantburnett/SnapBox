//-------------------------------------------------------------------------------------------------
// <copyright file="logutil.cpp" company="Microsoft">
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
//    Logging helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

// globals
static HMODULE LogUtil_hModule = NULL;
static HANDLE LogUtil_hLog = INVALID_HANDLE_VALUE;
static LPWSTR LogUtil_pwzLogPath = NULL;
static REPORT_LEVEL LogUtil_rlCurrent = REPORT_STANDARD;
static LPSTR LogUtil_szNewLine = "\r\n";

// prototypes
static HRESULT LogIdWork(
    IN HMODULE hModule,
    IN DWORD dwLogId,
    IN va_list args,
    IN BOOL fNewLine
    );
static HRESULT LogStringWork(
    IN LPCSTR szFormat,
    IN va_list args,
    IN BOOL fNewLine
    );

/********************************************************************
 LogInitialize - creates an application log file

 NOTE: uses ANSI functions internally so it is Win9x safe
       if szExt is null then szLog is path to desired log else szLog and szExt are used to generate log name
********************************************************************/
extern "C" HRESULT DAPI LogInitialize(
    IN HMODULE hModule,
    IN LPCWSTR wzLog,
    IN LPCWSTR wzExt,
    IN BOOL fAppend,
    IN BOOL fHeader
    )
{
    AssertSz(INVALID_HANDLE_VALUE == LogUtil_hLog && !LogUtil_pwzLogPath, "LogInitialize() - already called");
    HRESULT hr = S_OK;
    LPSTR pszLogPath = NULL;

    if (wzExt && *wzExt)
    {
        hr = FileCreateTemp(wzLog, wzExt, &LogUtil_pwzLogPath, &LogUtil_hLog);
        ExitOnFailure2(hr, "failed to create log for: %S.%S", wzLog, wzExt);
    }
    else
    {
        hr = StrAnsiAllocString(&pszLogPath, wzLog, 0, CP_UTF8);
        ExitOnFailure1(hr, "failed to allocate memory for ansi log: %S", wzLog);

        LogUtil_hLog = ::CreateFileA(pszLogPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, (fAppend) ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (INVALID_HANDLE_VALUE == LogUtil_hLog)
            ExitOnLastError1(hr, "failed to create log file: %s", pszLogPath);

        if (fAppend)
            ::SetFilePointer(LogUtil_hLog, 0, 0, FILE_END);

        hr = StrAllocString(&LogUtil_pwzLogPath, wzLog, 0);
        ExitOnFailure1(hr, "failed to allocate memory for log: %S", wzLog);
    }

    if (fHeader)
    {
        LogHeader();
    }

    LogUtil_hModule = hModule;
LExit:
    if (pszLogPath)
    {
        StrFree(pszLogPath);
    }
    return hr;
}


extern "C" void DAPI LogUninitialize(
    IN BOOL fFooter
    )
{
    AssertSz(INVALID_HANDLE_VALUE != LogUtil_hLog && LogUtil_pwzLogPath, "LogInitialize() has not been called");

    if (fFooter)
    {
        LogFooter();
    }

    LogUtil_hModule = NULL;

    if (INVALID_HANDLE_VALUE != LogUtil_hLog)
    {
        ::CloseHandle(LogUtil_hLog);
        LogUtil_hLog = INVALID_HANDLE_VALUE;
    }

    if (LogUtil_pwzLogPath)
    {
        StrFree(LogUtil_pwzLogPath);
        LogUtil_pwzLogPath = NULL;
    }
}


/********************************************************************
 LogIsOpen - returns whether log file is open or note

********************************************************************/
extern "C" BOOL DAPI LogIsOpen()
{
    return INVALID_HANDLE_VALUE != LogUtil_hLog;
}


/********************************************************************
 LogSetLevel - sets the logging level

 NOTE: returns previous logging level
********************************************************************/
extern "C" REPORT_LEVEL DAPI LogSetLevel(
    IN REPORT_LEVEL rl,
    IN BOOL fLogChange
    )
{
    AssertSz(REPORT_ERROR != rl, "REPORT_ERROR is not a valid logging level to set");

    REPORT_LEVEL rlPrev = LogUtil_rlCurrent;

    if (LogUtil_rlCurrent != rl)
    {
        LogUtil_rlCurrent = rl;

        if (fLogChange)
        {
            LPCSTR pszLevel = "unknown";
            switch (LogUtil_rlCurrent)
            {
            case REPORT_STANDARD:
                pszLevel = "standard";
                break;
            case REPORT_VERBOSE:
                pszLevel = "verbose";
                break;
            case REPORT_DEBUG:
                pszLevel = "debug";
                break;
            case REPORT_NONE:
                pszLevel = "none";
                break;
            }

            LogLine(REPORT_STANDARD, "--- logging level: %s ---", pszLevel);
        }
    }

    return rlPrev;
}


/********************************************************************
 LogGetLevel - gets the current logging level

********************************************************************/
extern "C" REPORT_LEVEL DAPI LogGetLevel()
{
    return LogUtil_rlCurrent;
}


/********************************************************************
 LogGetPath - gets the current log path

********************************************************************/
extern "C" HRESULT DAPI LogGetPath(
    __out_ecount(cchLogPath) LPWSTR pwzLogPath, 
    __in DWORD cchLogPath
    )
{
    Assert(pwzLogPath);

    HRESULT hr = S_OK;
    
    if (NULL == LogUtil_pwzLogPath)		// they can't have a path if there isn't one!
    {
        ExitFunction1(hr = E_UNEXPECTED);
    }

    hr = StringCchCopyW(pwzLogPath, cchLogPath, LogUtil_pwzLogPath);

LExit:
    return hr;
}


/********************************************************************
 LogGetHandle - gets the current log file handle

********************************************************************/
extern "C" HANDLE DAPI LogGetHandle()
{
    return LogUtil_hLog;
}


/********************************************************************
 LogString - write a string  to the log

 NOTE: use printf formatting ("%S", "%d", etc.)
********************************************************************/
extern "C" HRESULT DAPIV LogString(
    IN REPORT_LEVEL rl,
    IN LPCSTR szFormat,
    ...
    )
{
    HRESULT hr = S_OK;
    va_list args;
    
    va_start(args, szFormat);
    hr = LogStringArgs(rl, szFormat, args);
    va_end(args);
    
    return hr;
}

extern "C" HRESULT DAPI LogStringArgs(
    IN REPORT_LEVEL rl,
    IN LPCSTR szFormat,
    va_list args
    )
{
    AssertSz(REPORT_NONE != rl, "REPORT_NONE is not a valid logging level");
    HRESULT hr = S_OK;

    if ((REPORT_ERROR != rl && LogUtil_rlCurrent < rl) || INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    hr = LogStringWork(szFormat, args, FALSE);

LExit:
    return hr;
}

/********************************************************************
 LogStringLine - write a string plus newline  to the log

 NOTE: use printf formatting ("%S", "%d", etc.)
********************************************************************/
extern "C" HRESULT DAPIV LogStringLine(
    IN REPORT_LEVEL rl,
    IN LPCSTR szFormat,
    ...
    )
{
    HRESULT hr = S_OK;
    va_list args;
    
    va_start(args, szFormat);
    hr = LogStringLineArgs(rl, szFormat, args);
    va_end(args);

    return hr;
}

extern "C" HRESULT DAPI LogStringLineArgs(
    IN REPORT_LEVEL rl,
    IN LPCSTR szFormat,
    va_list args
    )
{
    AssertSz(REPORT_NONE != rl, "REPORT_NONE is not a valid logging level");
    HRESULT hr = S_OK;

    if ((REPORT_ERROR != rl && LogUtil_rlCurrent < rl) || INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    hr = LogStringWork(szFormat, args, TRUE);

LExit:
    return hr;
}

/********************************************************************
 LogIdModuleArgs - write a string embedded in a MESSAGETABLE  to the log

 NOTE: uses format string from MESSAGETABLE resource
********************************************************************/

extern "C" HRESULT DAPI LogIdModuleArgs(
    IN REPORT_LEVEL rl,
    IN DWORD dwLogId,
    IN HMODULE hModule,
    va_list args
    )
{
    AssertSz(REPORT_NONE != rl, "REPORT_NONE is not a valid logging level");
    HRESULT hr = S_OK;

    if ((REPORT_ERROR != rl && LogUtil_rlCurrent < rl) || INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    hr = LogIdWork((hModule) ? hModule : LogUtil_hModule, dwLogId, args, TRUE);

LExit:
    return hr;
}

extern "C" HRESULT DAPI LogIdModule(
    IN REPORT_LEVEL rl,
    IN DWORD dwLogId,
    IN HMODULE hModule,
    ...
    )
{
    AssertSz(REPORT_NONE != rl, "REPORT_NONE is not a valid logging level");
    HRESULT hr = S_OK;
    va_list args;

    if ((REPORT_ERROR != rl && LogUtil_rlCurrent < rl) || INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    va_start(args, hModule);
    hr = LogIdWork((hModule) ? hModule : LogUtil_hModule, dwLogId, args, TRUE);
    va_end(args);

LExit:
    return hr;
}




/********************************************************************
 LogError - write an error to the log

 NOTE: use printf formatting ("%S", "%d", etc.)
********************************************************************/
extern "C" HRESULT DAPIV LogErrorString(
    IN HRESULT hrError,
    IN LPCSTR szFormat,
    ...
    )
{
    HRESULT hr  = S_OK;

    va_list args;
    va_start(args, szFormat);
    hr = LogErrorStringArgs(hrError, szFormat, args);
    va_end(args);

    return hr;
}

extern "C" HRESULT DAPI LogErrorStringArgs(
    IN HRESULT hrError,
    IN LPCSTR szFormat,
    va_list args
    )
{
    HRESULT hr  = S_OK;
    LPSTR pszMessage = NULL;

    if (INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    hr = StrAnsiAllocFormattedArgs(&pszMessage, szFormat, args);
    ExitOnFailure1(hr, "failed to format error message: \"%s\"", szFormat);

    hr = LogLine(REPORT_ERROR, "Error 0x%x: %s", hrError, pszMessage);
    
LExit:
    ReleaseStr(pszMessage);
    return hr;
}


/********************************************************************
 LogErrorIdModule - write an error string embedded in a MESSAGETABLE  to the log

 NOTE:  uses format string from MESSAGETABLE resource
        can log no more than three strings in the error message
********************************************************************/
extern "C" HRESULT DAPI LogErrorIdModule(
    IN HRESULT hrError,
    IN DWORD dwLogId,
    IN HMODULE hModule,
    IN LPCWSTR wzString1,
    IN LPCWSTR wzString2,
    IN LPCWSTR wzString3
    )
{
    HRESULT hr = S_OK;
    WCHAR wzError[11];
    WORD cStrings = 1; // guaranteed wzError is in the list

    if (INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    hr = StringCchPrintfW(wzError, countof(wzError), L"0x%08x", hrError);
    cStrings += wzString1 ? 1 : 0;
    cStrings += wzString2 ? 1 : 0;
    cStrings += wzString3 ? 1 : 0;

    hr = LogIdModule(REPORT_ERROR, dwLogId, hModule, wzError, wzString1, wzString2, wzString3);

LExit:
    return hr;
    
}

/********************************************************************
 LogHeader - write a standard header to the log

********************************************************************/
extern "C" HRESULT DAPI LogHeader()
{
    HRESULT hr = S_OK;
    WCHAR wzComputerName[MAX_PATH];
    DWORD cchComputerName = countof(wzComputerName);
    WCHAR wzPath[MAX_PATH];
    DWORD dwMajorVersion = 0;
    DWORD dwMinorVersion = 0;
    LPCSTR pszLevel = "unknown";
    LPWSTR pwzCurrentDateTime = NULL;

    if (INVALID_HANDLE_VALUE == LogUtil_hLog || !LogUtil_pwzLogPath)
    {
        ExitFunction1(hr = S_FALSE);
    }

    //
    // get the interesting data
    //
    if (!::GetModuleFileNameW(NULL, wzPath, countof(wzPath)))
    {
        memset(wzPath, 0, sizeof(wzPath));
    }

    hr = FileVersion(wzPath, &dwMajorVersion, &dwMinorVersion);
    if (FAILED(hr))
    {
        dwMajorVersion = 0;
        dwMinorVersion = 0;
    }

    if (!::GetComputerNameW(wzComputerName, &cchComputerName))
    {
        ::SecureZeroMemory(wzComputerName, sizeof(wzComputerName));
    }

    StrCurrentDateTime(&pwzCurrentDateTime, FALSE);

    //
    // write data to the log
    //
    LogLine(REPORT_STANDARD, "=== Logging started: %S ===", pwzCurrentDateTime);
    LogLine(REPORT_STANDARD, "Executable: %S v%d.%d.%d.%d\r\nComputer  : %S", wzPath, dwMajorVersion >> 16, dwMajorVersion & 0xFFFF, dwMinorVersion >> 16, dwMinorVersion & 0xFFFF, wzComputerName);
    switch (LogUtil_rlCurrent)
    {
    case REPORT_STANDARD:
        pszLevel = "standard";
        break;
    case REPORT_VERBOSE:
        pszLevel = "verbose";
        break;
    case REPORT_DEBUG:
        pszLevel = "debug";
        break;
    case REPORT_NONE:
        pszLevel = "none";
        break;
    }
    LogLine(REPORT_STANDARD, "--- logging level: %s ---", pszLevel);

    hr = S_OK;
LExit:
    ReleaseStr(pwzCurrentDateTime);
    return hr;
}


/********************************************************************
 LogFooterWork - write a standard footer to the log

********************************************************************/

static HRESULT LogFooterWork(
    LPCSTR szFormat,
    ...
    )
{
    HRESULT hr;
    
    va_list args;
    va_start(args, szFormat);
    hr = LogStringWork(szFormat, args, TRUE);
    va_end(args);

    return hr;
}

extern "C" HRESULT DAPI LogFooter()
{
    HRESULT hr = S_OK;
    LPWSTR pwzCurrentDateTime = NULL;
    StrCurrentDateTime(&pwzCurrentDateTime, FALSE);
    hr = LogFooterWork("=== Logging stopped: %S ===", pwzCurrentDateTime);
    ReleaseStr(pwzCurrentDateTime);
    return hr;
}

//
// private worker functions
//
static HRESULT LogIdWork(
    IN HMODULE hModule,
    IN DWORD dwLogId,
    IN va_list args,
    IN BOOL fNewLine
    )
{
    HRESULT hr = S_OK;
    LPVOID pv = NULL;
    DWORD cch = 0;
    DWORD cbTotal = 0;
    DWORD cbWrote = 0;

    // get the string for the id
#pragma prefast(push)
#pragma prefast(disable:25028)
    cch = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE, 
                           static_cast<LPCVOID>(hModule), dwLogId, 0, reinterpret_cast<LPSTR>(&pv), 0, &args);
#pragma prefast(pop)

    if (0 == cch)
    {
        ExitOnLastError1(hr, "failed to log id: %d", dwLogId);
    }

    // write the string
    while (cbTotal < (sizeof(CHAR) * cch))
    {
        if (!::WriteFile(LogUtil_hLog, reinterpret_cast<BYTE*>(pv) + cbTotal, sizeof(CHAR) * cch - cbTotal, &cbWrote, NULL))
        {
            ExitOnLastError2(hr, "failed to write output to log: %S - %s", LogUtil_pwzLogPath, static_cast<LPCSTR>(pv));
        }

        cbTotal += cbWrote;
    }

    // write the newline
    if (fNewLine)
    {
        if (!::WriteFile(LogUtil_hLog, reinterpret_cast<BYTE*>(LogUtil_szNewLine), 2, &cbWrote, NULL))
        {
            ExitOnLastError1(hr, "failed to write newline to log: %S", LogUtil_pwzLogPath);
        }
    }

LExit:
    if (pv)
    {
        ::LocalFree(pv);
    }
    return hr;
}


static HRESULT LogStringWork(
    IN LPCSTR szFormat,
    IN va_list args,
    IN BOOL fNewLine
    )
{
    HRESULT hr = S_OK;
    LPSTR psz = NULL;
    DWORD cch = 0;
    DWORD cbTotal = 0;
    DWORD cbWrote = 0;

    ExitOnNull(szFormat, hr, E_INVALIDARG, "Invalid argument szFormat");
    ExitOnNull(*szFormat, hr, E_INVALIDARG, "Invalid argument *szFormat");

    // format the string
    hr = StrAnsiAllocFormattedArgs(&psz, szFormat, args);
    ExitOnFailure1(hr, "failed to format message: \"%s\"", szFormat);

    // write the string
    cch = lstrlenA(psz);
    while (cbTotal < (sizeof(*psz) * cch))
    {
        if (!::WriteFile(LogUtil_hLog, reinterpret_cast<BYTE*>(psz) + cbTotal, sizeof(*psz) * cch - cbTotal, &cbWrote, NULL))
        {
            ExitOnLastError2(hr, "failed to write output to log: %S - %s", LogUtil_pwzLogPath, psz);
        }

        cbTotal += cbWrote;
    }

    // write the newline
    if (fNewLine)
    {
        if (!::WriteFile(LogUtil_hLog, reinterpret_cast<BYTE*>(LogUtil_szNewLine), 2, &cbWrote, NULL))
        {
            ExitOnLastError1(hr, "failed to write newline to log: %S", LogUtil_pwzLogPath);
        }
    }

LExit:
    ReleaseStr(psz);
    return hr;
}
