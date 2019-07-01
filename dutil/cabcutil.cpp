//-------------------------------------------------------------------------------------------------
// <copyright file="cabcutil.cpp" company="Microsoft">
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
//    Cabinet creation helper functions.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

static const DWORD MAX_CABINET_HEADER_SIZE = 16 * 1024 * 1024;


// structs
struct MS_CABINET_HEADER
{
    DWORD sig;
    DWORD csumHeader;
    DWORD cbCabinet;
    DWORD csumFolders;
    DWORD coffFiles;
    DWORD csumFiles;
    WORD version;
    WORD cFolders;
    WORD cFiles;
    WORD flags;
    WORD setID;
    WORD iCabinet;
};


struct MS_CABINET_ITEM
{
    DWORD cbFile;
    DWORD uoffFolderStart;
    WORD iFolder;
    WORD date;
    WORD time;
    WORD attribs;
};


struct CABC_DUPLICATEFILE
{
    DWORD dwFileIndex;
    DWORD dwDuplicateFileIndex;
};


struct CABC_FILE
{
    DWORD dwIndex;
    LPWSTR pwzSourcePath;
};


struct CABC_DATA
{
    HFCI hfci;
    ERF erf;
    CCAB ccab;
    TCOMP tc;

    WCHAR wzCabinetPath[MAX_PATH];
    CHAR szEmptyFile[MAX_PATH];
    HANDLE hEmptyFile;
    DWORD dwLastFileIndex;

    DWORD cFilePaths;
    DWORD cMaxFilePaths;
    CABC_FILE *prgFiles;

    DWORD cDuplicates;
    DWORD cMaxDuplicates;
    CABC_DUPLICATEFILE *prgDuplicates;

    HRESULT hrLastError;
    BOOL fGoodCab;
};

//
// prototypes
//
static void FreeCabCData(
    __in CABC_DATA* pcd
    );
static CABC_FILE* CheckForDuplicateFile(
    __in CABC_DATA *pcd,
    __in LPCWSTR wzFileName
    );
static HRESULT AddDuplicateFile(
    __in CABC_DATA *pcd,
    __in const CABC_FILE *pcf,
    __in DWORD dwDuplicateFileIndex
    );
static HRESULT AddNonDuplicateFile(
    __in CABC_DATA *pcd,
    __in LPCWSTR wzFile,
    __in DWORD dwFileIndex
    );
static HRESULT UpdateDuplicateFiles(
    __in CABC_DATA *pcd
    );
static HRESULT DuplicateFile(
    __in MS_CABINET_HEADER *pHeader,
    __in const CABC_DUPLICATEFILE *pDuplicate
    );

static __callback int DIAMONDAPI CabCFilePlaced(__in PCCAB pccab, __in_z PSTR szFile, __in long cbFile, __in BOOL fContinuation, __out void *pv);
static __callback void * DIAMONDAPI CabCAlloc(__in ULONG cb);
static __callback void DIAMONDAPI CabCFree(__out void *pv);
static __callback INT_PTR DIAMONDAPI CabCOpen(__in_z PSTR pszFile, __in int oflag, __in int pmode, __out int *err, __out void *pv);
static __callback UINT FAR DIAMONDAPI CabCRead(__in INT_PTR hf, __out_bcount(cb) void FAR *memory, __in UINT cb, __out int *err, __out void *pv);
static __callback UINT FAR DIAMONDAPI CabCWrite(__in INT_PTR hf, __in_bcount(cb) void FAR *memory, __in UINT cb, __out int *err, __out void *pv);
static __callback long FAR DIAMONDAPI CabCSeek(__in INT_PTR hf, __in long dist, __in int seektype, __out int *err, __out void *pv);
static __callback int FAR DIAMONDAPI CabCClose(__in INT_PTR hf, __out int *err, __out void *pv);
static __callback int DIAMONDAPI CabCDelete(__in_z PSTR szFile, __out int *err, __out void *pv);
__success(return != FALSE) static __callback BOOL DIAMONDAPI CabCGetTempFile(__out_ecount_z(cchFile) char *szFile, __in int cchFile, __out void *pv);
__success(return != FALSE) static __callback BOOL DIAMONDAPI CabCGetNextCabinet(__in PCCAB pccab, __in ULONG ul, __out void *pv);
static __callback INT_PTR DIAMONDAPI CabCGetOpenInfo(__in_z PSTR pszName, __out USHORT *pdate, __out USHORT *ptime, __out USHORT *pattribs, __out int *err, __out void *pv);
static __callback long DIAMONDAPI CabCStatus(__in UINT ui, __in ULONG cb1, __in ULONG cb2, __out void *pv);


/********************************************************************
CabcBegin - begins creating a cabinet

NOTE: phContext must be the same handle used in AddFile and Finish.
      wzCabDir can be L"", but not NULL.
      dwMaxSize and dwMaxThresh can be 0.  A large default value will be used in that case.

********************************************************************/
extern "C" HRESULT DAPI CabCBegin(
    __in LPCWSTR wzCab,
    __in LPCWSTR wzCabDir,
    __in DWORD dwMaxSize,
    __in DWORD dwMaxThresh,
    __in COMPRESSION_TYPE ct,
    __out HANDLE *phContext
    )
{
    Assert(wzCab && *wzCab && phContext);

    HRESULT hr = S_OK;
    CABC_DATA *pcd = NULL;
    CHAR szTempPath[MAX_PATH] = { 0 };

    WCHAR wzPathBuffer [MAX_PATH] = L"";
    size_t cchPathBuffer;
    if (wzCabDir)
    {
        hr = StringCchLengthW(wzCabDir, MAX_PATH, &cchPathBuffer);
        ExitOnFailure(hr, "Failed to get length of cab directory");

        // Need room to terminate with L'\\' and L'\0'
        Assert(cchPathBuffer < (MAX_PATH - 1));

        hr = StringCchCopyW(wzPathBuffer, countof(wzPathBuffer), wzCabDir);
        ExitOnFailure(hr, "Failed to copy cab directory to buffer");

        if (L'\\' != wzPathBuffer[cchPathBuffer - 1])
        {
            hr = StringCchCatW(wzPathBuffer, countof(wzPathBuffer), L"\\");
            ExitOnFailure(hr, "Failed to cat \\ to end of buffer");
            cchPathBuffer++;
        }
    }

    pcd = static_cast<CABC_DATA*>(MemAlloc(sizeof(CABC_DATA), TRUE));
    ExitOnNull(pcd, hr, E_OUTOFMEMORY, "failed to allocate cab creation data structure");

    pcd->hrLastError = S_OK;
    pcd->fGoodCab = TRUE;

    pcd->hEmptyFile = INVALID_HANDLE_VALUE;

    if (NULL == dwMaxSize)
    {
        pcd->ccab.cb = CAB_MAX_SIZE;
    }
    else
    {
        pcd->ccab.cb = dwMaxSize;
    }

    if (0 == dwMaxThresh)
    {
        // Subtract 16 to magically make cabbing of uncompressed data larger than 2GB work.
        pcd->ccab.cbFolderThresh = CAB_MAX_SIZE - 16; 
    }
    else
    {
        pcd->ccab.cbFolderThresh = dwMaxThresh;
    }

    // Translate the compression type
    if (COMPRESSION_TYPE_NONE == ct)
    {
        pcd->tc = tcompTYPE_NONE;
    }
    else if (COMPRESSION_TYPE_LOW == ct)
    {
        pcd->tc = tcompTYPE_LZX | tcompLZX_WINDOW_LO;
    }
    else if (COMPRESSION_TYPE_MEDIUM == ct)
    {
        pcd->tc = TCOMPfromLZXWindow(18);
    }
    else if (COMPRESSION_TYPE_HIGH == ct)
    {
        pcd->tc = tcompTYPE_LZX | tcompLZX_WINDOW_HI;
    }
    else if (COMPRESSION_TYPE_MSZIP == ct)
    {
        pcd->tc = tcompTYPE_MSZIP;
    }
    else
    {
        ExitOnFailure(hr = E_INVALIDARG, "Invalid compression type specified.");
    }

    if (0 == ::WideCharToMultiByte(CP_ACP, 0, wzCab, -1, pcd->ccab.szCab, sizeof(pcd->ccab.szCab), NULL, NULL))
    {
        ExitWithLastError(hr, "failed to convert cab name to multi-byte");
    }

    if (0 ==  ::WideCharToMultiByte(CP_ACP, 0, wzPathBuffer, -1, pcd->ccab.szCabPath, sizeof(pcd->ccab.szCab), NULL, NULL))
    {
        ExitWithLastError(hr, "failed to convert cab dir to multi-byte");
    }

    // Remember the path to the cabinet.
    hr= ::StringCchCopyW(pcd->wzCabinetPath, countof(pcd->wzCabinetPath), wzPathBuffer);
    ExitOnFailure1(hr, "Failed to copy cabinet path from path: %S", wzPathBuffer);

    hr = ::StringCchCatW(pcd->wzCabinetPath, countof(pcd->wzCabinetPath), wzCab);
    ExitOnFailure1(hr, "Failed to concat to cabinet path cabinet name: %S", wzCab);

    // Get the empty file to use as the blank marker for duplicates.
    if (!::GetTempPathA(countof(szTempPath), szTempPath))
    {
        ExitWithLastError(hr, "Failed to get temp path.");
    }

    if (!::GetTempFileNameA(szTempPath, "WSC", 0, pcd->szEmptyFile))
    {
        ExitWithLastError(hr, "Failed to create a temp file name.");
    }

    // Try to open the newly created empty file (remember, GetTempFileName() is kind enough to create a file for us)
    // with a handle to automatically delete the file on close. Ignore any failure that might happen, since the worst
    // case is we'll leave a zero byte file behind in the temp folder.
    pcd->hEmptyFile = ::CreateFileA(pcd->szEmptyFile, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    // Tell cabinet API about our configuration.
    pcd->hfci = ::FCICreate(&(pcd->erf), CabCFilePlaced, CabCAlloc, CabCFree, CabCOpen, CabCRead, CabCWrite, CabCClose, CabCSeek, CabCDelete, CabCGetTempFile, &(pcd->ccab), pcd);
    if (NULL == pcd->hfci || pcd->erf.fError)
    {
        // Prefer our recorded last error, then ::GetLastError(), finally fallback to the useless "E_FAIL" error
        if (FAILED(pcd->hrLastError))
        {
            hr = pcd->hrLastError;
        }
        else
        {
            ExitWithLastError2(hr, "failed to create FCI object Oper: 0x%x Type: 0x%x", pcd->erf.erfOper, pcd->erf.erfType);
        }

        pcd->fGoodCab = FALSE;

        ExitOnFailure2(hr, "failed to create FCI object Oper: 0x%x Type: 0x%x", pcd->erf.erfOper, pcd->erf.erfType);  // TODO: can these be converted to HRESULTS?
    }

    *phContext = pcd;

LExit:
    if (FAILED(hr) && pcd && pcd->hfci)
    {
        ::FCIDestroy(pcd->hfci);
    }

    return hr;
}


/********************************************************************
CabCNextCab - This will be useful when creating multiple cabs.
Haven't needed it yet.
********************************************************************/
extern "C" HRESULT DAPI CabCNextCab(
    __in HANDLE hContext
    )
{
    UNREFERENCED_PARAMETER(hContext);
    // TODO: Make the appropriate FCIFlushCabinet and FCIFlushFolder calls
    return E_NOTIMPL;
}


/********************************************************************
CabcAddFile - adds a file to a cabinet

NOTE: hContext must be the same used in Begin and Finish
if wzToken is null, the file's original name is used within the cab
********************************************************************/
extern "C" HRESULT DAPI CabCAddFile(
    __in LPCWSTR wzFile,
    __in_opt LPCWSTR wzToken,
    __in HANDLE hContext
    )
{
    Assert(wzFile && *wzFile && hContext);

    HRESULT hr = S_OK;
    LPCWSTR wzFileName = NULL;
    char szFile[MAX_PATH * sizeof(WCHAR)] = {0};
    LPSTR pszFileName = NULL;
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(hContext);
    const CABC_FILE *pcfDuplicate = NULL;

    // use the token if given
    if (wzToken && *wzToken)
    {
        wzFileName = wzToken;
    }
    else
    {
        wzFileName = FileFromPath(wzFile);
    }

    pcfDuplicate = CheckForDuplicateFile(pcd, wzFile);
    if (pcfDuplicate)
    {
        // For duplicate files, we point them at our empty (zero-byte) file so it takes up no space
        // in the resultant cabinet.  Later on (CabCFinish) we'll go through and change all the zero
        // byte files to point at their duplicated file index.
        //
        // Notice that duplicate files are not added to the list of file paths because all duplicate
        // files point at the same path (the empty file) so there is no point in tracking them with
        // their path.
        hr = ::StringCchCopyA(szFile, MAX_PATH, pcd->szEmptyFile);
        ExitOnFailure1(hr, "Failed to copy empty file name: %s", pcd->szEmptyFile);

        hr = AddDuplicateFile(pcd, pcfDuplicate, pcd->dwLastFileIndex);
        ExitOnFailure1(hr, "Failed to add duplicate of file name: %S", pcfDuplicate->pwzSourcePath);
    }
    else
    {
        // Just a normal, non-duplicated file.  We'll add it to the list for later checking of
        // duplicates.
        szFile[0] = '?'; // signal that the following bytes are actually Unicode (wide) characters
        memcpy_s(szFile + 1, countof(szFile) - 1, wzFile, lstrlenW(wzFile) * sizeof(WCHAR));

        hr = AddNonDuplicateFile(pcd, wzFile, pcd->dwLastFileIndex);
        ExitOnFailure1(hr, "Failed to add non-duplicated file: %S", wzFile);
    }
    hr = StrAnsiAllocString(&pszFileName, wzFileName, 0, CP_ACP);
    ExitOnFailure1(hr, "failed to convert to ANSI: %S", wzFileName);

    ++pcd->dwLastFileIndex;

    // add the file to the cab
    if (!::FCIAddFile(pcd->hfci, szFile, pszFileName, FALSE, CabCGetNextCabinet, CabCStatus, CabCGetOpenInfo, pcd->tc))
    {
        // Prefer our recorded last error, then ::GetLastError(), finally fallback to the useless "E_FAIL" error
        if (FAILED(pcd->hrLastError))
        {
            hr = pcd->hrLastError;
        }
        else
        {
            ExitWithLastError3(hr, "failed to add file to FCI object Oper: 0x%x Type: 0x%x File: %s", pcd->erf.erfOper, pcd->erf.erfType, szFile);
        }

        pcd->fGoodCab = FALSE;

        ExitOnFailure3(hr, "failed to add file to FCI object Oper: 0x%x Type: 0x%x File: %s", pcd->erf.erfOper, pcd->erf.erfType, szFile);  // TODO: can these be converted to HRESULTS?
    }

LExit:
    ReleaseNullStr(pszFileName);

    return hr;
}


/********************************************************************
CabcFinish - finishes making a cabinet

NOTE: hContext must be the same used in Begin and AddFile
*********************************************************************/
extern "C" HRESULT DAPI CabCFinish(
    __in HANDLE hContext
    )
{
    Assert(hContext);

    HRESULT hr = S_OK;
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(hContext);

    // Only flush the cabinet if we actually succeeded in previous calls - otherwise we just waste time (a lot on big cabs)
    if (pcd->fGoodCab && !::FCIFlushCabinet(pcd->hfci, FALSE, CabCGetNextCabinet, CabCStatus))
    {
        // If we have a last error, use that, otherwise return the useless error
        hr = FAILED(pcd->hrLastError) ? pcd->hrLastError : E_FAIL;
        ExitOnFailure2(hr, "failed to flush FCI object Oper: 0x%x Type: 0x%x", pcd->erf.erfOper, pcd->erf.erfType);  // TODO: can these be converted to HRESULTS?
    }

    ::FCIDestroy(pcd->hfci);

    if (pcd->fGoodCab && pcd->cDuplicates)
    {
        hr = UpdateDuplicateFiles(pcd);
        ExitOnFailure1(hr, "Failed to update duplicates in cabinet: %S", pcd->wzCabinetPath);
    }

    FreeCabCData(pcd);

LExit:
    return hr;
}


/********************************************************************
CabCCancel - cancels making a cabinet

NOTE: hContext must be the same used in Begin and AddFile
*********************************************************************/
extern "C" void DAPI CabCCancel(
    __in HANDLE hContext
    )
{
    Assert(hContext);

    CABC_DATA* pcd = reinterpret_cast<CABC_DATA*>(hContext);
    ::FCIDestroy(pcd->hfci);
    FreeCabCData(pcd);
}


//
// private
//

static void FreeCabCData(
    __in CABC_DATA* pcd
    )
{
    if (pcd)
    {
        if (INVALID_HANDLE_VALUE != pcd->hEmptyFile)
        {
            ::CloseHandle(pcd->hEmptyFile);
        }

        for (DWORD i = 0; i < pcd->cFilePaths; ++i)
        {
            ReleaseStr(pcd->prgFiles[i].pwzSourcePath);
        }
        ReleaseMem(pcd->prgFiles);
        ReleaseMem(pcd->prgDuplicates);

        ReleaseMem(pcd);
    }
}

/********************************************************************
 SmartCab functions

********************************************************************/

static CABC_FILE* CheckForDuplicateFile(
    __in CABC_DATA *pcd,
    __in LPCWSTR wzFileName
    )
{
    CABC_FILE *pcf = NULL;

    // TODO: turn this into a binary search to improve perf
    for (DWORD i = 0; i < pcd->cFilePaths; ++i)
    {
        if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, pcd->prgFiles[i].pwzSourcePath, -1, wzFileName, -1))
        {
            pcf = pcd->prgFiles + i;
            break;
        }
    }

    return pcf;
}


static HRESULT AddDuplicateFile(
    __in CABC_DATA *pcd,
    __in const CABC_FILE *pcf,
    __in DWORD dwDuplicateFileIndex
    )
{
    HRESULT hr = S_OK;
    LPVOID pv = NULL;

    // Ensure there is enough memory to store this duplicate file index.
    if (pcd->cDuplicates == pcd->cMaxDuplicates)
    {
        pcd->cMaxDuplicates += 20; // grow by a reasonable number (20 is reasonable, right?)
        size_t cbDuplicates = 0;

        hr = ::SizeTMult(pcd->cMaxDuplicates, sizeof(CABC_DUPLICATEFILE), &cbDuplicates);
        ExitOnFailure(hr, "Maximum allocation exceeded.");

        if (pcd->cDuplicates)
        {
            pv = MemReAlloc(pcd->prgDuplicates, cbDuplicates, FALSE);
            ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to reallocate memory for duplicate file.");
        }
        else
        {
            pv = MemAlloc(cbDuplicates, FALSE);
            ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to allocate memory for duplicate file.");
        }

        ZeroMemory(reinterpret_cast<BYTE*>(pv) + (pcd->cDuplicates * sizeof(CABC_DUPLICATEFILE)), (pcd->cMaxDuplicates - pcd->cDuplicates) * sizeof(CABC_DUPLICATEFILE));

        pcd->prgDuplicates = static_cast<CABC_DUPLICATEFILE*>(pv);
        pv = NULL;
    }

    // Store the duplicate file index.
    pcd->prgDuplicates[pcd->cDuplicates].dwFileIndex = pcf->dwIndex;
    pcd->prgDuplicates[pcd->cDuplicates].dwDuplicateFileIndex = dwDuplicateFileIndex;
    ++pcd->cDuplicates;

LExit:
    ReleaseMem(pv);
    return hr;
}


static HRESULT AddNonDuplicateFile(
    __in CABC_DATA *pcd,
    __in LPCWSTR wzFile,
    __in DWORD dwFileIndex
    )
{
    HRESULT hr = S_OK;
    LPVOID pv = NULL;

    // Ensure there is enough memory to store this file index.
    if (pcd->cFilePaths == pcd->cMaxFilePaths)
    {
        pcd->cMaxFilePaths += 100; // grow by a reasonable number (100 is reasonable, right?)
        size_t cbFilePaths = 0;

        hr = ::SizeTMult(pcd->cMaxFilePaths, sizeof(CABC_FILE), &cbFilePaths);
        ExitOnFailure(hr, "Maximum allocation exceeded.");

        if (pcd->cFilePaths)
        {
            pv = MemReAlloc(pcd->prgFiles, cbFilePaths, FALSE);
            ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to reallocate memory for file.");
        }
        else
        {
            pv = MemAlloc(cbFilePaths, FALSE);
            ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to allocate memory for file.");
        }

        ZeroMemory(reinterpret_cast<BYTE*>(pv) + (pcd->cFilePaths * sizeof(CABC_FILE)), (pcd->cMaxFilePaths - pcd->cFilePaths) * sizeof(CABC_FILE));

        pcd->prgFiles = static_cast<CABC_FILE*>(pv);
        pv = NULL;
    }

    // Store the file index information.
    // TODO: add this to a sorted list so we can do a binary search later.
    CABC_FILE *pcf = pcd->prgFiles + pcd->cFilePaths;
    pcf->dwIndex = dwFileIndex;
    hr = StrAllocString(&pcf->pwzSourcePath, wzFile, 0);
    ExitOnFailure1(hr, "Failed to copy file path: %S", wzFile);
    ++pcd->cFilePaths;

LExit:
    ReleaseMem(pv);
    return hr;
}


static HRESULT UpdateDuplicateFiles(
    __in CABC_DATA *pcd
    )
{
    HRESULT hr = S_OK;
    DWORD cbCabinet = 0;
    LARGE_INTEGER liCabinetSize = { 0 };
    HANDLE hCabinet = INVALID_HANDLE_VALUE;
    HANDLE hCabinetMapping = NULL;
    LPVOID pv = NULL;
    MS_CABINET_HEADER *pCabinetHeader = NULL;

    hCabinet = ::CreateFileW(pcd->wzCabinetPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hCabinet)
    {
        ExitWithLastError1(hr, "Failed to open cabinet: %S", pcd->wzCabinetPath);
    }

    // Shouldn't need more than 16 MB to get the whole cabinet header into memory so use that as
    // the upper bound for the memory map.
    if (!::GetFileSizeEx(hCabinet, &liCabinetSize))
    {
        ExitWithLastError1(hr, "Failed to get size of cabinet: %S", pcd->wzCabinetPath);
    }

    if (0 == liCabinetSize.HighPart && liCabinetSize.LowPart < MAX_CABINET_HEADER_SIZE)
    {
        cbCabinet = liCabinetSize.LowPart;
    }
    else
    {
        cbCabinet = MAX_CABINET_HEADER_SIZE;
    }

    hCabinetMapping = ::CreateFileMappingW(hCabinet, NULL, PAGE_READWRITE | SEC_COMMIT, 0, cbCabinet, NULL);
    if (NULL == hCabinetMapping || INVALID_HANDLE_VALUE == hCabinetMapping)
    {
        ExitWithLastError1(hr, "Failed to memory map cabinet file: %S", pcd->wzCabinetPath);
    }

    pv = ::MapViewOfFile(hCabinetMapping, FILE_MAP_WRITE, 0, 0, 0);
    ExitOnNullWithLastError1(pv, hr, "Failed to map view of cabinet file: %S", pcd->wzCabinetPath);

    pCabinetHeader = static_cast<MS_CABINET_HEADER*>(pv);

    for (DWORD i = 0; i < pcd->cDuplicates; ++i)
    {
        const CABC_DUPLICATEFILE *pDuplicateFile = pcd->prgDuplicates + i;

        hr = DuplicateFile(pCabinetHeader, pDuplicateFile);
        ExitOnFailure2(hr, "Failed to find cabinet file items at index: %d and %d", pDuplicateFile->dwFileIndex, pDuplicateFile->dwDuplicateFileIndex);
    }

LExit:
    if (pv)
    {
        ::UnmapViewOfFile(pv);
    }
    if (hCabinetMapping)
    {
        ::CloseHandle(hCabinetMapping);
    }
    if (INVALID_HANDLE_VALUE != hCabinet)
    {
        ::CloseHandle(hCabinet);
    }

    return hr;
}


static HRESULT DuplicateFile(
    __in MS_CABINET_HEADER *pHeader,
    __in const CABC_DUPLICATEFILE *pDuplicate
    )
{
    HRESULT hr = S_OK;
    BYTE *pbHeader = reinterpret_cast<BYTE*>(pHeader);
    BYTE* pbItem = pbHeader + pHeader->coffFiles;
    const MS_CABINET_ITEM *pOriginalItem = NULL;
    MS_CABINET_ITEM *pDuplicateItem = NULL;

    if (pHeader->cFiles <= pDuplicate->dwFileIndex ||
        pHeader->cFiles <= pDuplicate->dwDuplicateFileIndex ||
        pDuplicate->dwDuplicateFileIndex <= pDuplicate->dwFileIndex)
    {
        hr = E_UNEXPECTED;
        ExitOnFailure3(hr, "Unexpected duplicate file indicies, header cFiles: %d, file index: %d, duplicate index: %d", pHeader->cFiles, pDuplicate->dwFileIndex, pDuplicate->dwDuplicateFileIndex);
    }

    // Step through each cabinet items until we get to the original
    // file's index.  Notice that the name of the cabinet item is
    // appended to the end of the MS_CABINET_INFO, that's why we can't
    // index straight to the data we want.
    for (DWORD i = 0; i < pDuplicate->dwFileIndex; ++i)
    {
        LPCSTR szItemName = reinterpret_cast<LPCSTR>(pbItem + sizeof(MS_CABINET_ITEM));
        pbItem = pbItem + sizeof(MS_CABINET_ITEM) + lstrlenA(szItemName) + 1;
    }

    pOriginalItem = reinterpret_cast<const MS_CABINET_ITEM*>(pbItem);

    // Now pick up where we left off after the original file's index
    // was found and loop until we find the duplicate file's index.
    for (DWORD i = pDuplicate->dwFileIndex; i < pDuplicate->dwDuplicateFileIndex; ++i)
    {
        LPCSTR szItemName = reinterpret_cast<LPCSTR>(pbItem + sizeof(MS_CABINET_ITEM));
        pbItem = pbItem + sizeof(MS_CABINET_ITEM) + lstrlenA(szItemName) + 1;
    }

    pDuplicateItem = reinterpret_cast<MS_CABINET_ITEM*>(pbItem);

    if (0 != pDuplicateItem->cbFile)
    {
        hr = E_UNEXPECTED;
        ExitOnFailure1(hr, "Failed because duplicate file does not have a file size of zero: %d", pDuplicateItem->cbFile);
    }

    pDuplicateItem->cbFile = pOriginalItem->cbFile;
    pDuplicateItem->uoffFolderStart = pOriginalItem->uoffFolderStart;
    pDuplicateItem->iFolder = pOriginalItem->iFolder;
    pDuplicateItem->date = pOriginalItem->date;
    pDuplicateItem->time = pOriginalItem->time;
    pDuplicateItem->attribs = pOriginalItem->attribs;

LExit:
    return hr;
}


/********************************************************************
 FCI callback functions

*********************************************************************/
static __callback int DIAMONDAPI CabCFilePlaced(
    __in PCCAB pccab,
    __in_z PSTR szFile,
    __in long cbFile,
    __in BOOL fContinuation,
    __out void *pv
    )
{
    UNREFERENCED_PARAMETER(pccab);
    UNREFERENCED_PARAMETER(szFile);
    UNREFERENCED_PARAMETER(cbFile);
    UNREFERENCED_PARAMETER(fContinuation);
    UNREFERENCED_PARAMETER(pv);
    return 0;
}


static __callback void * DIAMONDAPI CabCAlloc(
    __in ULONG cb
    )
{
    return MemAlloc(cb, FALSE);
}


static __callback void DIAMONDAPI CabCFree(
    __out void *pv
    )
{
    MemFree(pv);
}

static __callback INT_PTR DIAMONDAPI CabCOpen(
    __in_z PSTR pszFile,
    __in int oflag,
    __in int pmode,
    __out int *err,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    HRESULT hr = S_OK;
    INT_PTR pFile = -1;
    DWORD dwAccess = 0;
    DWORD dwDisposition = 0;
    DWORD dwAttributes = 0;

    //
    // Translate flags for CreateFile
    //
    if (oflag & _O_CREAT)
    {
        if (pmode == _S_IREAD)
            dwAccess |= GENERIC_READ;
        else if (pmode == _S_IWRITE)
            dwAccess |= GENERIC_WRITE;
        else if (pmode == (_S_IWRITE | _S_IREAD))
            dwAccess |= GENERIC_READ | GENERIC_WRITE;

        if (oflag & _O_SHORT_LIVED)
            dwDisposition = FILE_ATTRIBUTE_TEMPORARY;
        else if (oflag & _O_TEMPORARY)
            dwAttributes |= FILE_FLAG_DELETE_ON_CLOSE;
        else if (oflag & _O_EXCL)
            dwDisposition = CREATE_NEW;
    }
    if (oflag & _O_TRUNC)
        dwDisposition = CREATE_ALWAYS;

    if (!dwAccess)
        dwAccess = GENERIC_READ;
    if (!dwDisposition)
        dwDisposition = OPEN_EXISTING;
    if (!dwAttributes)
        dwAttributes = FILE_ATTRIBUTE_NORMAL;

    if (pszFile && '?' == *pszFile)
    {
        pFile = reinterpret_cast<INT_PTR>(::CreateFileW(reinterpret_cast<LPCWSTR>(pszFile + 1), dwAccess, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, dwDisposition, dwAttributes, NULL));
    }
    else
    {
        pFile = reinterpret_cast<INT_PTR>(::CreateFileA(pszFile, dwAccess, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, dwDisposition, dwAttributes, NULL));
    }
    if (INVALID_HANDLE_VALUE == reinterpret_cast<HANDLE>(pFile))
    {
        ExitOnLastError1(hr, "failed to open file: %s", pszFile);
    }

LExit:
    if (FAILED(hr))
        pcd->hrLastError = *err = hr;

    return FAILED(hr) ? -1 : pFile;
}


static __callback UINT FAR DIAMONDAPI CabCRead(
    __in INT_PTR hf,
    __out_bcount(cb) void FAR *memory,
    __in UINT cb,
    __out int *err,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    HRESULT hr = S_OK;
    DWORD cbRead = 0;

    ExitOnNull(hf, *err, E_INVALIDARG, "Failed to read during cabinet extraction because no file handle was provided");
    if (!::ReadFile(reinterpret_cast<HANDLE>(hf), memory, cb, &cbRead, NULL))
    {
        *err = ::GetLastError();
        ExitOnLastError(hr, "failed to read during cabinet extraction");
    }

LExit:
    if (FAILED(hr))
        pcd->hrLastError = *err = hr;

    return FAILED(hr) ? -1 : cbRead;
}


static __callback UINT FAR DIAMONDAPI CabCWrite(
    __in INT_PTR hf,
    __in_bcount(cb) void FAR *memory,
    __in UINT cb,
    __out int *err,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    HRESULT hr = S_OK;
    DWORD cbWrite = 0;

    ExitOnNull(hf, *err, E_INVALIDARG, "Failed to write during cabinet extraction because no file handle was provided");
    if (!::WriteFile(reinterpret_cast<HANDLE>(hf), memory, cb, &cbWrite, NULL))
    {
        *err = ::GetLastError();
        ExitOnLastError(hr, "failed to write during cabinet extraction");
    }

LExit:
    if (FAILED(hr))
        pcd->hrLastError = *err = hr;

    return FAILED(hr) ? -1 : cbWrite;
}


static __callback long FAR DIAMONDAPI CabCSeek(
    __in INT_PTR hf,
    __in long dist,
    __in int seektype,
    __out int *err,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    HRESULT hr = S_OK;
    DWORD dwMoveMethod;
    LONG lMove = 0;

    switch (seektype)
    {
    case 0:   // SEEK_SET
        dwMoveMethod = FILE_BEGIN;
        break;
    case 1:   /// SEEK_CUR
        dwMoveMethod = FILE_CURRENT;
        break;
    case 2:   // SEEK_END
        dwMoveMethod = FILE_END;
        break;
    default :
        dwMoveMethod = 0;
        ExitOnFailure1(hr = E_UNEXPECTED, "unexpected seektype in FDISeek(): %d", seektype);
    }

    // SetFilePointer returns -1 if it fails (this will cause FDI to quit with an FDIERROR_USER_ABORT error.
    // (Unless this happens while working on a cabinet, in which case FDI returns FDIERROR_CORRUPT_CABINET)
    lMove = ::SetFilePointer(reinterpret_cast<HANDLE>(hf), dist, NULL, dwMoveMethod);
    if (0xFFFFFFFF == lMove)
    {
        *err = ::GetLastError();
        ExitOnLastError1(hr, "failed to move file pointer %d bytes", dist);
    }

LExit:
    if (FAILED(hr))
        pcd->hrLastError = *err = hr;

    return FAILED(hr) ? -1 : lMove;
}


static __callback int FAR DIAMONDAPI CabCClose(
    __in INT_PTR hf,
    __out int *err,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    HRESULT hr = S_OK;

    if (!::CloseHandle(reinterpret_cast<HANDLE>(hf)))
    {
        *err = ::GetLastError();
        ExitOnLastError(hr, "failed to close file during cabinet extraction");
    }

LExit:
    if (FAILED(hr))
        pcd->hrLastError = *err = hr;

    return FAILED(hr) ? -1 : 0;
}

static __callback int DIAMONDAPI CabCDelete(
    __in_z PSTR szFile,
    __out int *err,
    __out void *pv
    )
{
    UNREFERENCED_PARAMETER(err);
    UNREFERENCED_PARAMETER(pv);
    ::DeleteFileA(szFile);
    return 0;
}


__success(return != FALSE)
static __callback BOOL DIAMONDAPI CabCGetTempFile(
    __out_ecount_z(cchFile) char *szFile,
    __in int cchFile,
    __out void *pv
    )
{
    CABC_DATA *pcd = reinterpret_cast<CABC_DATA*>(pv);
    static DWORD dwIndex = 0;

    HRESULT hr;

    LPSTR pszTempPath = NULL;
    DWORD cchTempPath = MAX_PATH;

    LPSTR pszTempFile = NULL;

    DWORD dwProcessId = ::GetCurrentProcessId();
    HANDLE hTempFile = INVALID_HANDLE_VALUE;

    hr = StrAnsiAlloc(&pszTempPath, cchTempPath);
    ExitOnFailure(hr, "failed to allocate memory for the temp path");
    ::GetTempPathA(cchTempPath, pszTempPath);

    for (DWORD i = 0; i < 0xFFFFFFFF; ++i)
    {
        ::InterlockedIncrement(reinterpret_cast<LONG*>(&dwIndex));

        hr = StrAnsiAllocFormatted(&pszTempFile, "%s\\%08x.%03x", pszTempPath, dwIndex, dwProcessId);
        ExitOnFailure(hr, "failed to allocate memory for log file");

        hTempFile = ::CreateFileA(pszTempFile, 0, FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (INVALID_HANDLE_VALUE != hTempFile)
        {
            // we found one that doesn't exist
            hr = S_OK;
            break;
        }
        else
        {
            hr = E_FAIL; // this file was taken so be pessimistic and assume we're not going to find one.
        }
    }
    ExitOnFailure(hr, "failed to find temporary file.");

    // TODO: Remember temp files so that we can ensure they're cleaned up later (especially if there's a failure)

    hr = StringCchCopyA(szFile, cchFile, pszTempFile);
    ExitOnFailure1(hr, "failed to copy to out parameter filename: %s", pszTempFile);

LExit:
    ReleaseStr(pszTempFile);
    ReleaseStr(pszTempPath);

    if (INVALID_HANDLE_VALUE != hTempFile)
    {
        ::CloseHandle(hTempFile);
    }

    if (FAILED(hr))
    {
        pcd->hrLastError = hr;
    }

    return FAILED(hr)? FALSE : TRUE;
}


__success(return != FALSE)
static __callback BOOL DIAMONDAPI CabCGetNextCabinet(
    __in PCCAB pccab,
    __in ULONG ul,
    __out void *pv
    )
{
    UNREFERENCED_PARAMETER(pccab);
    UNREFERENCED_PARAMETER(ul);
    UNREFERENCED_PARAMETER(pv);
    return(FALSE);
}


static __callback INT_PTR DIAMONDAPI CabCGetOpenInfo(
    __in_z PSTR pszName,
    __out USHORT *pdate,
    __out USHORT *ptime,
    __out USHORT *pattribs,
    __out int *err,
    __out void *pv
    )
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    BOOL fSucceeded = FALSE;


    if (*pszName && '?' == *pszName)
    {
        fSucceeded = ::GetFileAttributesExW(reinterpret_cast<LPCWSTR>(pszName + 1), GetFileExInfoStandard, &fad);
    }
    else
    {
        fSucceeded = ::GetFileAttributesExA(pszName, GetFileExInfoStandard, &fad);
    }

    if (fSucceeded)
    {
        *pattribs = static_cast<USHORT>(fad.dwFileAttributes);
        FILETIME ftLocal;
        ::FileTimeToLocalFileTime(&fad.ftLastWriteTime, &ftLocal);
        ::FileTimeToDosDateTime(&ftLocal, pdate, ptime);
    }
    else
    {
        *err = ::GetLastError();
    }

    return CabCOpen(pszName, _O_BINARY|_O_RDONLY, 0, err, pv);
}


static __callback long DIAMONDAPI CabCStatus(
    __in UINT ui,
    __in ULONG cb1,
    __in ULONG cb2,
    __out void *pv
    )
{
    UNREFERENCED_PARAMETER(ui);
    UNREFERENCED_PARAMETER(cb1);
    UNREFERENCED_PARAMETER(cb2);
    UNREFERENCED_PARAMETER(pv);
    return 0;
}
