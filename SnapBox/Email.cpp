#include "stdafx.h"
#include "Email.h"

DWORD EmailThreadProc(LPTSTR szFilename);

void DisplayError(HWND hWnd, LPTSTR message)
{
    MessageBox(hWnd, message, _T("Error"), MB_ICONERROR | MB_OK);
}

void DisplayErrorFromCode(HWND hWnd, const LPTSTR message, HRESULT res)
{
    TCHAR text[20];
    _stprintf_s(text, 20, message, res);
    DisplayError(hWnd, text);
}

void EmailFile(LPTSTR szFilename)
{
    size_t len = _tcslen(szFilename)+1;
    LPTSTR szLocalName = new TCHAR[len];
    _tcscpy_s(szLocalName, len, szFilename);

    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&EmailThreadProc, szLocalName, 0, NULL);
}

DWORD EmailThreadProc(LPTSTR szFilename)
{
    MapiMessage* msg = new MapiMessage();
    memset(msg, 0, sizeof(msg));
    msg->nFileCount = 1;
    msg->lpFiles = new MapiFileDesc[1];
    memset(msg->lpFiles, 0, sizeof(MapiFileDesc));
    msg->lpFiles[0].nPosition = -1;

#ifdef _UNICODE
    CHAR szCharPath[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, szFilename, -1, szCharPath, MAX_PATH, NULL, NULL);
    msg->lpFiles[0].lpszPathName = szCharPath;
#else
    msg->lpFiles[0].lpszPathName = szFilename;
#endif

    HMODULE hModule = LoadLibrary(_T("mapi32.dll"));
    if (hModule)
    {
        LPMAPISENDMAIL sendMail = (LPMAPISENDMAIL)GetProcAddress(hModule, "MAPISendMail");
        if (sendMail)
        {
            HRESULT res = (sendMail)(0, 0, msg, MAPI_DIALOG, 0);
            if (res != SUCCESS_SUCCESS && res != MAPI_E_USER_ABORT)
                DisplayErrorFromCode(0, _T("MAPI Error: %d"), res);
        }
        else
            DisplayError(0, _T("Error Loading MAPI32"));
    }
    else
        DisplayError(0, _T("Error Loading MAPI32"));
    FreeLibrary(hModule);

    delete msg->lpFiles;
    delete msg;

    DeleteFile(szFilename);
    delete szFilename;

    return 0;
}
