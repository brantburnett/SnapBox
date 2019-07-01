//-------------------------------------------------------------------------------------------------
// <copyright file="wiutil.cpp" company="Microsoft">
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
//    Windows Installer helper functions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

#define GOOD_ENOUGH_PROPERTY_LENGTH 64


extern "C" HRESULT DAPI WiuGetProductProperty(
    __in MSIHANDLE hProduct,
    __in LPCWSTR wzProperty,
    __out LPWSTR* ppwzValue
    )
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    DWORD cch = GOOD_ENOUGH_PROPERTY_LENGTH;

    hr = StrAlloc(ppwzValue, cch);
    ExitOnFailure(hr, "Failed to allocate string for product property.");

    er = ::MsiGetProductPropertyW(hProduct, wzProperty, *ppwzValue, &cch);
    if (ERROR_MORE_DATA == er)
    {
        ++cch;
        hr = StrAlloc(ppwzValue, cch);
        ExitOnFailure(hr, "Failed to reallocate string for product property.");

        er = ::MsiGetProductPropertyW(hProduct, wzProperty, *ppwzValue, &cch);
    }
    hr = HRESULT_FROM_WIN32(er);
    ExitOnFailure(hr, "Failed to get product property.");

LExit:
    return hr;
}


extern "C" HRESULT DAPI WiuGetProductInfo(
    __in LPCWSTR wzProductCode,
    __in LPCWSTR wzProperty,
    __out LPWSTR* ppwzValue
    )
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    DWORD cch = GOOD_ENOUGH_PROPERTY_LENGTH;

    hr = StrAlloc(ppwzValue, cch);
    ExitOnFailure(hr, "Failed to allocate string for product info.");

    er = ::MsiGetProductInfoW(wzProductCode, wzProperty, *ppwzValue, &cch);
    if (ERROR_MORE_DATA == er)
    {
        ++cch;
        hr = StrAlloc(ppwzValue, cch);
        ExitOnFailure(hr, "Failed to reallocate string for product info.");

        er = ::MsiGetProductInfoW(wzProductCode, wzProperty, *ppwzValue, &cch);
    }
    hr = HRESULT_FROM_WIN32(er);
    ExitOnFailure(hr, "Failed to get product info.");

LExit:
    return hr;
}


extern "C" HRESULT DAPI WiuGetComponentPath(
    __in LPCWSTR wzProductCode,
    __in LPCWSTR wzComponentId,
    __out LPWSTR* ppwzPath
    )
{
    HRESULT hr = S_OK;
    INSTALLSTATE installState = INSTALLSTATE_UNKNOWN;
    DWORD cch = GOOD_ENOUGH_PROPERTY_LENGTH;
    DWORD cchCompare;

    hr = StrAlloc(ppwzPath, cch);
    ExitOnFailure(hr, "Failed to allocate string for component path.");

    cchCompare = cch;
    installState = ::MsiGetComponentPathW(wzProductCode, wzComponentId, *ppwzPath, &cch);
    if (INSTALLSTATE_LOCAL == installState)
    {
        // If the actual path length is greater than or equal to the original buffer
        // allocate a larger buffer and get the path again, just in case we are 
        // missing any part of the path.
        if (cchCompare <= cch)
        {
            ++cch;
            hr = StrAlloc(ppwzPath, cch);
            ExitOnFailure(hr, "Failed to reallocate string for component path.");

            installState = ::MsiGetComponentPathW(wzProductCode, wzComponentId, *ppwzPath, &cch);
            if (INSTALLSTATE_LOCAL != installState)
            {
                hr = E_UNEXPECTED;
            }
        }
    }
    else if (INSTALLSTATE_MOREDATA == installState)
    {
        ++cch;
        hr = StrAlloc(ppwzPath, cch);
        ExitOnFailure(hr, "Failed to reallocate string for component path.");

        installState = ::MsiGetComponentPathW(wzProductCode, wzComponentId, *ppwzPath, &cch);
        if (INSTALLSTATE_LOCAL != installState)
        {
            hr = E_UNEXPECTED;
        }
    }
    else
    {
        hr = E_UNEXPECTED;
    }
    ExitOnFailure(hr, "Failed to get component path.");

LExit:
    return hr;
}
