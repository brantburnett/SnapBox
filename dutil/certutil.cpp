//-------------------------------------------------------------------------------------------------
// <copyright file="certutil.cpp" company="Microsoft">
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
//    Certificate helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

/********************************************************************
CertReadProperty - reads a property from the certificate.

NOTE: call MemFree() on the returned pvValue.
********************************************************************/
extern "C" HRESULT DAPI CertReadProperty(
    __in PCCERT_CONTEXT pCertContext,
    __in DWORD dwProperty,
    __deref_out_bound LPVOID* ppvValue
    )
{
    HRESULT hr = S_OK;
    LPVOID pv = NULL;
    DWORD cb = 0;

    if (!::CertGetCertificateContextProperty(pCertContext, dwProperty, NULL, &cb))
    {
        ExitWithLastError(hr, "Failed to get size of certificate property.");
    }

    pv = MemAlloc(cb, TRUE);
    ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to allocate memory for certificate property.");

    if (!::CertGetCertificateContextProperty(pCertContext, dwProperty, pv, &cb))
    {
        ExitWithLastError(hr, "Failed to get certificate property.");
    }

    *ppvValue = pv;
    pv = NULL;

LExit:
    ReleaseMem(pv);
    return hr;
}

extern "C" HRESULT DAPI GetCryptProvFromCert(
      __in_opt HWND hwnd,
      __in PCCERT_CONTEXT pCert,
      __out HCRYPTPROV *phCryptProv,
      __out DWORD *pdwKeySpec,
      __in BOOL *pfDidCryptAcquire,
      __deref_opt_out LPWSTR *ppwszTmpContainer,
      __deref_opt_out LPWSTR *ppwszProviderName,
      __out DWORD *pdwProviderType
      )
{
    HRESULT hr = S_OK;
    HMODULE hMsSign32 = NULL;

    typedef BOOL (WINAPI *GETCRYPTPROVFROMCERTPTR)(HWND, PCCERT_CONTEXT, HCRYPTPROV*, DWORD*,BOOL*,LPWSTR*,LPWSTR*,DWORD*);
    GETCRYPTPROVFROMCERTPTR pGetCryptProvFromCert = NULL;

    hMsSign32 = ::LoadLibraryW(L"MsSign32.dll");
    ExitOnNullWithLastError(hMsSign32, hr, "Failed to get handle to MsSign32.dll");

    pGetCryptProvFromCert = (GETCRYPTPROVFROMCERTPTR)::GetProcAddress(hMsSign32, "GetCryptProvFromCert");
    ExitOnNullWithLastError(hMsSign32, hr, "Failed to get handle to MsSign32.dll");

    if (!pGetCryptProvFromCert(hwnd,
                               pCert,
                               phCryptProv, 
                               pdwKeySpec, 
                               pfDidCryptAcquire, 
                               ppwszTmpContainer,
                               ppwszProviderName,
                               pdwProviderType))
    {
        ExitWithLastError(hr, "Failed to get CSP from cert.");
    }
LExit:
    return hr;
}

extern "C" HRESULT DAPI FreeCryptProvFromCert(
    __in BOOL fAcquired,
    __in HCRYPTPROV hProv,
    __in_opt LPWSTR pwszCapiProvider,
    __in DWORD dwProviderType,
    __in_opt LPWSTR pwszTmpContainer
    )
{
    HRESULT hr = S_OK;
    HMODULE hMsSign32 = NULL;

    typedef void (WINAPI *FREECRYPTPROVFROMCERT)(BOOL, HCRYPTPROV, LPWSTR, DWORD, LPWSTR);
    FREECRYPTPROVFROMCERT pFreeCryptProvFromCert = NULL;

    hMsSign32 = ::LoadLibraryW(L"MsSign32.dll");
    ExitOnNullWithLastError(hMsSign32, hr, "Failed to get handle to MsSign32.dll");

    pFreeCryptProvFromCert = (FREECRYPTPROVFROMCERT)::GetProcAddress(hMsSign32, "FreeCryptProvFromCert");
    ExitOnNullWithLastError(hMsSign32, hr, "Failed to get handle to MsSign32.dll");

    pFreeCryptProvFromCert(fAcquired, hProv, pwszCapiProvider, dwProviderType, pwszTmpContainer);
LExit:
    return hr;
}

extern "C" HRESULT DAPI GetProvSecurityDesc(
    __in HCRYPTPROV hProv, 
    __deref_out SECURITY_DESCRIPTOR** ppSecurity) 
{
    HRESULT hr = S_OK;
    ULONG ulSize = 0;
    SECURITY_DESCRIPTOR* pSecurity = NULL;

    // Get the size of the security descriptor.
    if (!::CryptGetProvParam(
                             hProv,
                             PP_KEYSET_SEC_DESCR,
                             NULL,
                             &ulSize,
                             DACL_SECURITY_INFORMATION))
    {
        ExitWithLastError(hr, "Error getting security descriptor size for CSP.");
    }

    // Allocate the memory for the security descriptor.
    pSecurity = static_cast<SECURITY_DESCRIPTOR *>(MemAlloc(ulSize, TRUE));
    ExitOnNullWithLastError(pSecurity, hr, "Error allocating memory for CSP DACL");

    // Get the security descriptor.
    if (!::CryptGetProvParam(
                             hProv,
                             PP_KEYSET_SEC_DESCR,
                             (BYTE*)pSecurity,
                             &ulSize,
                             DACL_SECURITY_INFORMATION))
    {
        MemFree(pSecurity);
        ExitWithLastError(hr, "Error getting security descriptor for CSP.");
    }
    *ppSecurity = pSecurity;

LExit:
    return hr;
}


extern "C" HRESULT DAPI SetProvSecurityDesc(
    __in HCRYPTPROV hProv,
    __in SECURITY_DESCRIPTOR* pSecurity)
{
    HRESULT hr = S_OK;

    // Set the new security descriptor.
    if(!::CryptSetProvParam(
                            hProv,
                            PP_KEYSET_SEC_DESCR,
                            (BYTE*)pSecurity,
                            DACL_SECURITY_INFORMATION))
    {
        ExitWithLastError(hr, "Error setting security descriptor for CSP.");
    }
LExit:
    return hr;
}