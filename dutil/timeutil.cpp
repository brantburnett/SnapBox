//-------------------------------------------------------------------------------------------------
// <copyright file="timeutil.cpp" company="Microsoft">
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
//    Time helper functions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

const LPCWSTR DAY_OF_WEEK[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
const LPCWSTR MONTH_OF_YEAR[] = { L"None", L"Jan", L"Feb", L"Mar", L"Apr", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
enum TIME_PARSER { DayOfWeek, DayOfMonth, MonthOfYear, Year, Hours, Minutes, Seconds, TimeZone };

// prototypes
static HRESULT DayFromString(
	__in LPCWSTR wzDay,
	__out WORD* pwDayOfWeek
	);
static HRESULT MonthFromString(
	__in LPCWSTR wzMonth,
	__out WORD* pwMonthOfYear
	);


/********************************************************************
 TimeFromString - converts string to FILETIME

*******************************************************************/
extern "C" HRESULT DAPI TimeFromString(
	__in LPCWSTR wzTime,
	__out FILETIME* pFileTime
	)
{
	Assert(wzTime && pFileTime);

	HRESULT hr = S_OK;
	LPWSTR pwzTime = NULL;

	SYSTEMTIME sysTime = { 0 };
	TIME_PARSER timeParser = DayOfWeek;

	LPCWSTR pwzStart = NULL;
	LPWSTR pwzEnd = NULL;

	hr = StrAllocString(&pwzTime, wzTime, 0);
	ExitOnFailure(hr, "Failed to copy time.");

	pwzStart = pwzEnd = pwzTime;
	while (pwzEnd && *pwzEnd)
	{
		if (L',' == *pwzEnd || L' ' == *pwzEnd || L':' == *pwzEnd)
		{
			*pwzEnd = L'\0'; // null terminate
			pwzEnd++;

			while (L' ' == *pwzEnd)
			{
				pwzEnd++; // and skip past the blank space
			}

			switch (timeParser)
			{
				case DayOfWeek:
					hr = DayFromString(pwzStart, &sysTime.wDayOfWeek);
					ExitOnFailure1(hr, "Failed to convert string to day: %S", pwzStart);
					break;

				case DayOfMonth:
					sysTime.wDay = (WORD)wcstoul(pwzStart, NULL, 10);
					break;

				case MonthOfYear:
					hr = MonthFromString(pwzStart, &sysTime.wMonth);
					ExitOnFailure1(hr, "Failed to convert to month: %S", pwzStart);
					break;

				case Year:
					sysTime.wYear = (WORD)wcstoul(pwzStart, NULL, 10);
					break;

				case Hours:
					sysTime.wHour = (WORD)wcstoul(pwzStart, NULL, 10);
					break;

				case Minutes:
					sysTime.wMinute = (WORD)wcstoul(pwzStart, NULL, 10);
					break;

				case Seconds:
					sysTime.wSecond = (WORD)wcstoul(pwzStart, NULL, 10);
					break;

				case TimeZone:
					// TODO: do something with this in the future, but this should only hit outside of the while loop.
					break;

				default:
					break;
			}

			pwzStart = pwzEnd;
			timeParser = (TIME_PARSER)((int)timeParser + 1);
		}

		++pwzEnd;
	}


	if (!::SystemTimeToFileTime(&sysTime, pFileTime))
	{
		ExitWithLastError(hr, "Failed to convert system time to file time.");
	}

LExit:
	ReleaseStr(pwzTime);
	return hr;
}


/********************************************************************
 DayFromString - converts string to day

*******************************************************************/
static HRESULT DayFromString(
	__in LPCWSTR wzDay,
	__out WORD* pwDayOfWeek
	)
{
	HRESULT hr = E_INVALIDARG; // assume we won't find a matching name

	for (WORD i = 0; i < countof(DAY_OF_WEEK); ++i)
	{
		if (0 == lstrcmpW(wzDay, DAY_OF_WEEK[i]))
		{
			*pwDayOfWeek = i;
			hr = S_OK;
			break;
		}
	}

	return hr;
}


/********************************************************************
 MonthFromString - converts string to month

*******************************************************************/
static HRESULT MonthFromString(
	__in LPCWSTR wzMonth,
	__out WORD* pwMonthOfYear
	)
{
	HRESULT hr = E_INVALIDARG; // assume we won't find a matching name

	for (WORD i = 0; i < countof(MONTH_OF_YEAR); ++i)
	{
		if (0 == lstrcmpW(wzMonth, MONTH_OF_YEAR[i]))
		{
			*pwMonthOfYear = i;
			hr = S_OK;
			break;
		}
	}

	return hr;
}

