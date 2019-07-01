#include "stdafx.h"
#include "Options.h"
#include "SnapBox.h"
#include "CaptureBox.h"
#include "SnapBoxBase.h"

using namespace xercesc;

#define SETTINGS_FILENAME	_T("settings")
#define SETTINGS_EXTENSION	_T(".xml")

#define SETTINGS_NAMESPACE			TEXT("http://www.snapbox.com/settings/2009")
#define SETTINGS_ROOT				TEXT("SnapBox")
#define SETTINGS_MAXHISTORY			TEXT("MaxHistory")
#define SETTINGS_QUICKSAVEPATH		TEXT("QuickSavePath")
#define SETTINGS_DEFAULTSAVETYPE	TEXT("DefaultSaveType")
#define SETTINGS_HIDEONNEWSNAP		TEXT("HideOnNewSnap")
#define SETTINGS_SHOWHOVERINFO		TEXT("ShowHoverInfo")

#define MAXHISTORYWNDPROC_SETTING	_T("MaxHistoryWndProc")

OPTIONS options;

INT_PTR OptionsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool GetSettingsFileName(LPTSTR szPath, bool createFolder)
{
    GetModuleFileName(NULL, szPath, MAX_PATH);

    TCHAR szDrive[_MAX_DRIVE], szDir[_MAX_DIR];
    _tsplitpath_s(szPath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, NULL, 0, NULL, 0);

    _tmakepath_s(szPath, MAX_PATH, szDrive, szDir, SETTINGS_FILENAME, SETTINGS_EXTENSION);

    if (_taccess(szPath, 0))
    {
        if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath)))
            return false;

        size_t i = _tcslen(szPath)-1;
        if (szPath[i] == _T('\\'))
            szPath[i] = _T('\0');

        _tcscat_s(szPath, MAX_PATH, _T("\\BurnettSoft"));
        if (createFolder && _taccess(szPath, 0))
            if (_tmkdir(szPath)) return false;

        _tcscat_s(szPath, MAX_PATH, _T("\\SnapBox"));
        if (createFolder && _taccess(szPath, 0))
            if (_tmkdir(szPath)) return false;

        _tcscat_s(szPath, MAX_PATH, _T("\\settings.xml"));
    }

    return true;
}

void SetDefaultOptions(POPTIONS options)
{
    memset(options, 0, sizeof(OPTIONS));

    options->maxHistory = 5;
    options->quickSavePath[0] = _T('\0');
    options->defaultSaveType = SAVETYPE_PNG;
    options->hideOnNewSnap = true;
    options->showHoverInfo = true;
}

void LoadOptions()
{
    SetDefaultOptions(&options);

    TCHAR szPath[MAX_PATH];
    if (!GetSettingsFileName(szPath, false)) return;

    XercesDOMParser* parser = new XercesDOMParser();
    parser->setDoNamespaces(true);

    ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
    parser->setErrorHandler(errHandler);

    try {
        parser->parse(szPath);

        const XMLCh *tempName, *namespaceUri;

        xercesc_3_0::DOMDocument* doc = parser->getDocument();
        DOMElement* root = doc->getDocumentElement();

        namespaceUri = root->getNamespaceURI();
        if (wcscmp(namespaceUri, SETTINGS_NAMESPACE))
            return;

        tempName = root->getLocalName();
        if (wcscmp(tempName, SETTINGS_ROOT))
            return;

        DOMNode* child = root->getFirstChild();
        while (child)
        {
            if (child->getNodeType() == DOMNode::ELEMENT_NODE)
            {
                DOMElement *element = (DOMElement*)child;
                namespaceUri = element->getNamespaceURI();
                if (!wcscmp(namespaceUri, SETTINGS_NAMESPACE))
                {
                    tempName = element->getLocalName();
#ifdef _UNICODE
                    const XMLCh* value = element->getTextContent();
#else
                    const XMLCh* temp = element->getTextContent();
                    char* value = XMLString::transcode(temp);
#endif

                    if (!wcscmp(tempName, SETTINGS_MAXHISTORY))
                    {
                        try
                        {
                            options.maxHistory = _ttoi(value);
                        }
                        catch (...)
                        {
                        }

                        if (options.maxHistory > MAX_CAPTURE_HISTORY)
                            options.maxHistory = MAX_CAPTURE_HISTORY;
                        else if (options.maxHistory < 0)
                            options.maxHistory = 0;
                    }
                    else if (!wcscmp(tempName, SETTINGS_QUICKSAVEPATH))
                    {
                        value = child->getTextContent();
                        _tcscpy_s(options.quickSavePath, MAX_PATH, value);
                    }
                    else if (!wcscmp(tempName, SETTINGS_DEFAULTSAVETYPE))
                    {
                        try
                        {
                            options.defaultSaveType = _ttoi(value);
                        }
                        catch (...)
                        {
                        }

                        if (options.defaultSaveType > 4)
                            options.defaultSaveType = 1;
                        else if (options.maxHistory < 1)
                            options.defaultSaveType = 1;
                    }
                    else if (!wcscmp(tempName, SETTINGS_HIDEONNEWSNAP))
                    {
                        try
                        {
                            options.hideOnNewSnap = _ttoi(value) != 0;
                        }
                        catch (...)
                        {
                        }
                    }
                    else if (!wcscmp(tempName, SETTINGS_SHOWHOVERINFO))
                    {
                        try
                        {
                            options.showHoverInfo = _ttoi(value) != 0;
                        }
                        catch (...)
                        {
                        }
                    }

#ifndef _UNICODE
                    XMLString::release(value);
#endif
                }
            }

            child = child->getNextSibling();
        }
    }
    catch (...) {
    }

    delete parser;
    delete errHandler;
}

 int serializeDOM(DOMImplementation* impl, DOMNode* node, LPTSTR szFilename)
 {
    DOMLSSerializer* theSerializer = ((DOMImplementationLS*)impl)->createLSSerializer();

    if (theSerializer->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
         theSerializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);

    XMLFormatTarget *myFormTarget = new LocalFileFormatTarget(szFilename);
    DOMLSOutput* theOutput = ((DOMImplementationLS*)impl)->createLSOutput();
    theOutput->setByteStream(myFormTarget);

    try {
        // do the serialization through DOMLSSerializer::write();
        theSerializer->write(node, theOutput);
    }
    catch (...) {
        theOutput->release();
        theSerializer->release();
        delete myFormTarget;
        return -1;
    }

    theOutput->release();
    theSerializer->release();
    delete myFormTarget;
    return 0;
}

bool SaveOptions(const POPTIONS newOptions)
{
    TCHAR szPath[MAX_PATH];
    if (!GetSettingsFileName(szPath, true)) return false;

    XMLCh tempStr[100];
    XMLString::transcode("LS", tempStr, 99);
    DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(tempStr);

#ifndef _UNICODE
    char tempStrCh[100];
#endif

    xercesc_3_0::DOMDocument* doc = impl->createDocument(SETTINGS_NAMESPACE, SETTINGS_ROOT, NULL);

    try
    {
        doc->setXmlVersion(TEXT("1.0"));

        DOMElement* rootNode = doc->getDocumentElement();

#ifdef _UNICODE
        _stprintf_s(tempStr, 100, _T("%d"), newOptions->maxHistory);
#else
        _sprintf_s(tempStrCh, 100, "%d", newOptions-maxHistory);
        XMLString::transcode(tempStrCh, tempStr, 99);
#endif
        DOMElement* element = doc->createElementNS(SETTINGS_NAMESPACE, SETTINGS_MAXHISTORY);
        element->setTextContent(tempStr);
        rootNode->appendChild(element);

        element = doc->createElementNS(SETTINGS_NAMESPACE, SETTINGS_QUICKSAVEPATH);
        element->setTextContent(newOptions->quickSavePath);
        rootNode->appendChild(element);

#ifdef _UNICODE
        _stprintf_s(tempStr, 100, _T("%d"), newOptions->defaultSaveType);
#else
        _sprintf_s(tempStrCh, 100, "%d", newOptions-maxHistory);
        XMLString::transcode(tempStrCh, tempStr, 99);
#endif
        element = doc->createElementNS(SETTINGS_NAMESPACE, SETTINGS_DEFAULTSAVETYPE);
        element->setTextContent(tempStr);
        rootNode->appendChild(element);

        element = doc->createElementNS(SETTINGS_NAMESPACE, SETTINGS_HIDEONNEWSNAP);
        element->setTextContent(newOptions->hideOnNewSnap ? TEXT("1") : TEXT("0"));
        rootNode->appendChild(element);

        element = doc->createElementNS(SETTINGS_NAMESPACE, SETTINGS_SHOWHOVERINFO);
        element->setTextContent(newOptions->showHoverInfo ? TEXT("1") : TEXT("0"));
        rootNode->appendChild(element);

        if (serializeDOM(impl, doc, szPath))
        {
            doc->release();
            //delete impl;
            return false;
        }
    }
    catch (...)
    {
        doc->release();
        //delete impl;
        return false;
    }

    doc->release();
    //delete impl;

    options = *newOptions;
    TrimCaptureHistory(options.maxHistory);
    return true;
}

void ShowOptionError(HWND hWnd, const LPTSTR message)
{
    MessageBox(hWnd, message, _T("Error"), MB_OK | MB_ICONERROR);
}

bool SaveOptionsFromDialog(HWND hDlg)
{
    OPTIONS newOptions;
    memset(&newOptions, 0, sizeof(newOptions));

    HWND hWnd = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
    BOOL bError;
    newOptions.maxHistory = (int)SendMessage(hWnd, UDM_GETPOS32, 0, (LPARAM)&bError);
    if (bError || newOptions.maxHistory > MAX_CAPTURE_HISTORY)
    {
        ShowOptionError(hDlg, _T("Invalid Number Of Snaps To Keep In History"));
        SetFocus(GetDlgItem(hDlg, IDC_MAXHISTORY));
        return false;
    }

    newOptions.defaultSaveType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DEFAULTSAVETYPE))+1;

    hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
    Edit_GetText(hWnd, newOptions.quickSavePath, MAX_PATH);
    if (_taccess(newOptions.quickSavePath, 0))
    {
        ShowOptionError(hDlg, _T("Invalid Quick Save Folder"));
        SetFocus(hWnd);
        return false;
    }

    newOptions.hideOnNewSnap = IsDlgButtonChecked(hDlg, IDC_HIDEONNEWSNAP) == BST_CHECKED;
    newOptions.showHoverInfo = IsDlgButtonChecked(hDlg, IDC_SHOWHOVERINFO) == BST_CHECKED;

    return SaveOptions(&newOptions);
}

int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case BFFM_INITIALIZED:
        SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
        break;
    }

    return 0;
}

BOOL BrowseForFolder(HWND hWnd, LPTSTR szFolderName)
{
    BROWSEINFO bi;
    memset(&bi, 0, sizeof(bi));

    bi.hwndOwner = hWnd;
    bi.lpszTitle = _T("Select the folder where you would like your quick saves to be placed");
    bi.lpfn = (BFFCALLBACK)&BrowseCallbackProc;
    bi.lParam = (LPARAM)szFolderName;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    PIDLIST_ABSOLUTE idList = SHBrowseForFolder(&bi);
    if (!idList) return false;

    BOOL res = SHGetPathFromIDList(idList, szFolderName);
    CoTaskMemFree(idList);
    return res;
}

LRESULT CALLBACK MaxHistoryWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = GetAncestor(hWnd, GA_ROOT);
    PMAXHISTORYDATA maxHistoryData = (PMAXHISTORYDATA)GetProp(hDlg, MAXHISTORYWNDPROC_SETTING);

    switch (uMsg)
    {
    case WM_KILLFOCUS:
        HWND hUpDwn = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
        BOOL bError;
        int maxHistory = (int)SendMessage(hUpDwn, UDM_GETPOS32, 0, (LPARAM)&bError);
        if (bError)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, maxHistoryData->prevValue);
        else if (maxHistory < 0)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, 0);
        else if (maxHistory > MAX_CAPTURE_HISTORY)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, MAX_CAPTURE_HISTORY);
        else
            maxHistoryData->prevValue = maxHistory;
    }

    return CallWindowProc(maxHistoryData->lpfnWndProc, hWnd, uMsg, wParam, lParam);
}

void InitOptionsDialog(HWND hDlg)
{
    TCHAR szTemp[MAX_PATH];

    HWND hWnd = GetDlgItem(hDlg, IDC_MAXHISTORY);
    Edit_LimitText(hWnd, 2);

    PMAXHISTORYDATA maxHistoryData = new MAXHISTORYDATA();
    maxHistoryData->lpfnWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&MaxHistoryWndProc);
    maxHistoryData->prevValue = options.maxHistory;
    SetProp(hDlg, MAXHISTORYWNDPROC_SETTING, (HANDLE)maxHistoryData);

    hWnd = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
    SendMessage(hWnd, UDM_SETRANGE32, 0, MAX_CAPTURE_HISTORY);
    SendMessage(hWnd, UDM_SETPOS32, 0, options.maxHistory);
    //_stprintf_s(szTemp, MAX_PATH, _T("%d"), options.maxHistory);
    //Edit_SetText(hWnd, szTemp);

    hWnd = GetDlgItem(hDlg, IDC_DEFAULTSAVETYPE);
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Portable Network Graphics (PNG)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Windows Bitmap (BMP)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Graphics Interchange Format (GIF)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("JPEG"));
    ComboBox_SetCurSel(hWnd, options.defaultSaveType-1);

    hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
    Edit_LimitText(hWnd, MAX_PATH);
    if (options.quickSavePath[0] != _T('\0'))
        Edit_SetText(hWnd, options.quickSavePath);
    else
    {
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, szTemp)))
            Edit_SetText(hWnd, szTemp);
    }

    CheckDlgButton(hDlg, IDC_HIDEONNEWSNAP, options.hideOnNewSnap ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOWHOVERINFO, options.showHoverInfo ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR ShowOptionsDialog(HWND hWnd)
{
    return DialogBox(hInst, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, (DLGPROC)&OptionsDialogProc);
}

INT_PTR OptionsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    int cx, cy;

    switch (message)
    {
    case WM_INITDIALOG:
        cx = GetSystemMetrics(SM_CXSCREEN);
        cy = GetSystemMetrics(SM_CYSCREEN);

        RECT rect;
        GetWindowRect(hDlg, &rect);

        cx = (cx - (rect.right - rect.left)) / 2;
        cy = (cy - (rect.bottom - rect.top)) / 2;
        SetWindowPos(hDlg, NULL, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);

        InitOptionsDialog(hDlg);

        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);

        hForeWindow = hDlg;

        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (LOWORD(wParam) == IDOK)
                if (!SaveOptionsFromDialog(hDlg)) return (INT_PTR)TRUE;

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_CLEARHISTORY)
        {
            ClearCaptureHistory();
            MessageBox(hDlg, _T("History Cleared"), _T("Information"), MB_OK | MB_ICONINFORMATION);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_QUICKSAVEPATHBROWSE)
        {
            TCHAR szFolder[MAX_PATH];
            HWND hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
            Edit_GetText(hWnd, szFolder, MAX_PATH);
            if (BrowseForFolder(hDlg, szFolder))
                Edit_SetText(hWnd, szFolder);
            return (INT_PTR)TRUE;
        }
        break;

    case WM_NCDESTROY:
        hForeWindow = NULL;

        PMAXHISTORYDATA maxHistoryData = (PMAXHISTORYDATA)GetProp(hDlg, MAXHISTORYWNDPROC_SETTING);
        delete maxHistoryData;
        RemoveProp(hDlg, MAXHISTORYWNDPROC_SETTING);
        break;
    }

    return (INT_PTR)FALSE;
}
