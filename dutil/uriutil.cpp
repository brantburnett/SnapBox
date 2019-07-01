//-------------------------------------------------------------------------------------------------
// <copyright file="uriutil.cpp" company="Microsoft">
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
//    URI helper funtions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


/*******************************************************************
 UriFile -  returns a pointer to the file part of the uri

********************************************************************/
extern "C" LPWSTR DAPI UriFile(
    __in LPCWSTR wzUri
    )
{
    if (!wzUri)
        return NULL;

    LPWSTR wzFile = const_cast<LPWSTR>(wzUri);
    for (LPWSTR wz = wzFile; *wz; wz++)
    {
        // valid delineators 
        //     \ => Windows path
        //     / => unix and URL path
        if (L'\\' == *wz || L'/' == *wz)
        {
            wzFile = wz + 1;
        }
    }

    return wzFile;
}


/*******************************************************************
 UriProtocol - determines the protocol of a URI.

********************************************************************/
extern "C" HRESULT DAPI UriProtocol(
    __in LPCWSTR wzUri,
    __out URI_PROTOCOL* pProtocol
    )
{
    Assert(wzUri && *wzUri);
    Assert(pProtocol);

    HRESULT hr = S_OK;

    if (L'f' == wzUri[0] && L'i' == wzUri[1] && L'l' == wzUri[2] && L'e' == wzUri[3] && L':' == wzUri[4] && L'/' == wzUri[5] && L'/' == wzUri[6])
    {
        *pProtocol = URI_PROTOCOL_FILE;
    }
    else if (L'f' == wzUri[0] && L't' == wzUri[1] && L'p' == wzUri[2] && L':' == wzUri[3] && L'/' == wzUri[4] && L'/' == wzUri[5])
    {
        *pProtocol = URI_PROTOCOL_FILE;
    }
    else if (L'h' == wzUri[0] && L't' == wzUri[1] && L't' == wzUri[2] && L'p' == wzUri[3] && L':' == wzUri[4] && L'/' == wzUri[5] && L'/' == wzUri[6])
    {
        *pProtocol = URI_PROTOCOL_HTTP;
    }
    else
    {
        *pProtocol = URI_PROTOCOL_UNKNOWN;
    }

//LExit:
    return hr;
}


/*******************************************************************
 UriRoot - returns the root of the path specified in the URI.

 examples:
    file:///C:\path\path             -> C:\
    file://server/share/path/path    -> \\server\share
    http://www.example.com/path/path -> http://www.example.com/
    ftp://ftp.example.com/path/path  -> ftp://www.example.com/

 NOTE: This function should only be used on cannonicalized URIs.
       It does not cannonicalize itself.
********************************************************************/
extern "C" HRESULT DAPI UriRoot(
    __in LPCWSTR wzUri,
    __out LPWSTR* ppwzRoot,
    __out_opt URI_PROTOCOL* pProtocol
    )
{
    Assert(wzUri && *wzUri);
    Assert(ppwzRoot);

    HRESULT hr = S_OK;
    URI_PROTOCOL protocol = URI_PROTOCOL_UNKNOWN;
    LPCWSTR pwcSlash = NULL;

    hr = UriProtocol(wzUri, &protocol);
    ExitOnFailure(hr, "Invalid URI.");

    switch (protocol)
    {
    case URI_PROTOCOL_FILE:
        if (L'/' == wzUri[7]) // file path
        {
            if (((L'a' <= wzUri[8] && L'z' >= wzUri[8]) || (L'A' <= wzUri[8] && L'Z' >= wzUri[8])) && L':' == wzUri[9])
            {
                hr = StrAlloc(ppwzRoot, 4);
                ExitOnFailure(hr, "Failed to allocate string for root of URI.");
                *ppwzRoot[0] = wzUri[8];
                *ppwzRoot[1] = L':';
                *ppwzRoot[2] = L'\\';
                *ppwzRoot[3] = L'\0';
            }
            else
            {
                hr = E_INVALIDARG;
                ExitOnFailure(hr, "Invalid file path in URI.");
            }
        }
        else // UNC share
        {
            pwcSlash = wcschr(wzUri + 8, L'/');
            if (!pwcSlash)
            {
                hr = E_INVALIDARG;
                ExitOnFailure(hr, "Invalid server name in URI.");
            }
            else
            {
                hr = StrAllocString(ppwzRoot, L"\\\\", 64);
                ExitOnFailure(hr, "Failed to allocate string for root of URI.");

                pwcSlash = wcschr(pwcSlash + 1, L'/');
                if (pwcSlash)
                {
                    hr = StrAllocConcat(ppwzRoot, wzUri + 8, pwcSlash - wzUri - 8);
                    ExitOnFailure(hr, "Failed to add server/share to root of URI.");
                }
                else
                {
                    hr = StrAllocConcat(ppwzRoot, wzUri + 8, 0);
                    ExitOnFailure(hr, "Failed to add server/share to root of URI.");
                }

                // replace all slashes with backslashes to be truly UNC.
                for (LPWSTR pwc = *ppwzRoot; pwc && *pwc; ++pwc)
                {
                    if (L'/' == *pwc)
                    {
                        *pwc = L'\\';
                    }
                }
            }
        }
        break;
    case URI_PROTOCOL_FTP:
        pwcSlash = wcschr(wzUri + 6, L'/');
        if (pwcSlash)
        {
            hr = StrAllocString(ppwzRoot, wzUri, pwcSlash - wzUri);
            ExitOnFailure(hr, "Failed allocate root from URI.");
        }
        else
        {
            hr = StrAllocString(ppwzRoot, wzUri, 0);
            ExitOnFailure(hr, "Failed allocate root from URI.");
        }
        break;
    case URI_PROTOCOL_HTTP:
        pwcSlash = wcschr(wzUri + 7, L'/');
        if (pwcSlash)
        {
            hr = StrAllocString(ppwzRoot, wzUri, pwcSlash - wzUri);
            ExitOnFailure(hr, "Failed allocate root from URI.");
        }
        else
        {
            hr = StrAllocString(ppwzRoot, wzUri, 0);
            ExitOnFailure(hr, "Failed allocate root from URI.");
        }
        break;
    default:
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "Unknown URI protocol.");
    }

    if (pProtocol)
    {
        *pProtocol = protocol;
    }

LExit:
    return hr;
}


extern "C" HRESULT DAPI UriResolve(
    __in LPCWSTR wzUri,
    __in_opt LPCWSTR wzBaseUri,
    __out LPWSTR* ppwzResolvedUri,
    __out_opt URI_PROTOCOL* pResolvedProtocol
    )
{
    HRESULT hr = S_OK;
    URI_PROTOCOL protocol = URI_PROTOCOL_UNKNOWN;

    hr = UriProtocol(wzUri, &protocol);
    ExitOnFailure1(hr, "Failed to determine protocol for URL: %S", wzUri);

    ExitOnNull(ppwzResolvedUri, hr, E_INVALIDARG, "Failed to resolve URI, because no method of output was provided");

    if (URI_PROTOCOL_UNKNOWN == protocol)
    {
        ExitOnNull(wzBaseUri, hr, E_INVALIDARG, "Failed to resolve URI - base URI provided was NULL");

        if (L'/' == *wzUri || L'\\' == *wzUri)
        {
            hr = UriRoot(wzBaseUri, ppwzResolvedUri, &protocol);
            ExitOnFailure1(hr, "Failed to get root from URI: %S", wzBaseUri);

            hr = StrAllocConcat(ppwzResolvedUri, wzUri, 0);
            ExitOnFailure(hr, "Failed to concat file to base URI.");
        }
        else
        {
            hr = UriProtocol(wzBaseUri, &protocol);
            ExitOnFailure1(hr, "Failed to get protocol of base URI: %S", wzBaseUri);

            LPCWSTR pwcFile = const_cast<LPCWSTR> (UriFile(wzBaseUri));
            if (!pwcFile)
            {
                hr = E_INVALIDARG;
                ExitOnFailure1(hr, "Failed to get file from base URI: %S", wzBaseUri);
            }

            hr = StrAllocString(ppwzResolvedUri, wzBaseUri, pwcFile - wzBaseUri);
            ExitOnFailure(hr, "Failed to allocate string for resolved URI.");

            hr = StrAllocConcat(ppwzResolvedUri, wzUri, 0);
            ExitOnFailure(hr, "Failed to concat file to resolved URI.");
        }
    }
    else
    {
        hr = StrAllocString(ppwzResolvedUri, wzUri, 0);
        ExitOnFailure(hr, "Failed to copy resolved URI.");
    }

    if (pResolvedProtocol)
    {
        *pResolvedProtocol = protocol;
    }

LExit:
    return hr;
}
