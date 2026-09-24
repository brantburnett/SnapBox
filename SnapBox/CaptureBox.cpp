#include "stdafx.h"
#include "CaptureBox.h"
#include "SnapBox.h"
#include "SnapBoxBase.h"
#include "Options.h"
#include "Email.h"
#include "SizeMarks.h"

using namespace Gdiplus;

#define MAX_LOADSTRING		100
#define MAX_FILTERSTRING	512
#define MIN_CAPTURE_SIZE	15
#define CLOSE_HEIGHT		19
#define CLOSE_WIDTH			24
#define CROP_SIZE			5
#define FRAME_OPACITY		(float)0.4

#define GWLP_INFO			GWLP_USERDATA

TCHAR szCaptureBoxWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szSaveFilter[MAX_FILTERSTRING];
PCAPTUREBOXCLOSEINFO prevCaptureBox[MAX_CAPTURE_HISTORY];
int savedCaptureBoxes = 0;
PCAPTUREBOXWINDOW openCaptureBoxes = NULL;

const SolidBrush* pTransparentBrush;
const SolidBrush* pCropBrush;
const Pen* pBorderPen;

LRESULT CALLBACK	CaptureBoxWndProc(HWND, UINT, WPARAM, LPARAM);
void				SetTracking(HWND hWnd, PCAPTUREBOXINFO info);
void				AnimateFrameFade(HWND hWnd, PCAPTUREBOXINFO info, float targetOpacity);

ATOM RegisterCaptureBoxClass(HINSTANCE hInstance)
{
    LoadString(hInstance, IDC_CAPTUREBOX, szCaptureBoxWindowClass, MAX_LOADSTRING);
    LoadString(hInstance, IDS_FILE_FILTER, szSaveFilter, MAX_FILTERSTRING);

    for (int i=0; i<MAX_FILTERSTRING; i++)
    {
        if (szSaveFilter[i] == _T('\0'))
            break;
        else if (szSaveFilter[i] == _T('\t'))
            szSaveFilter[i] = _T('\0');
    }

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= CaptureBoxWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= sizeof(PCAPTUREBOXINFO);
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNAPBOX));
    wcex.hCursor		= hCursorMove;
    wcex.lpszClassName	= szCaptureBoxWindowClass;

    pTransparentBrush = new SolidBrush(Color::Transparent);
    pCropBrush = new SolidBrush(Color(0x44, 0, 0, 0));
    pBorderPen = new Pen(Color(0, 0, 0), 1.0);

    return RegisterClassEx(&wcex);
}

PCAPTUREBOXWINDOW AddCaptureBoxWindow(HWND hWnd)
{
    PCAPTUREBOXWINDOW w = new CAPTUREBOXWINDOW();
    w->hWnd = hWnd;
    w->next = openCaptureBoxes;
    openCaptureBoxes = w;
    return w;
}

HWND CreateCaptureBox(Bitmap *bitmap, RECT r)
{
    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, szCaptureBoxWindowClass, _T("SNAP"), WS_POPUP,
        r.left-1, r.top-1, r.right-r.left+2, r.bottom-r.top+2, 0, 0, hInst, NULL);

    PCAPTUREBOXINFO info = new CAPTUREBOXINFO();
    memset(info, 0, sizeof(CAPTUREBOXINFO));
    info->bitmap = bitmap;
    info->size.x = r.right-r.left;
    info->size.y = r.bottom-r.top;
    //info->cropRect.X = 0;
    //info->cropRect.Y = 0;
    info->cropRect.Width = (REAL)info->size.x;
    info->cropRect.Height = (REAL)info->size.y;
    info->centerMoved = true;
    //info->scaleIndex = 0;
    info->scale = 1.0;
    info->curScale = 1.0;
    //info->frameOpacity = 0.0;
    info->moving = false;
    info->captured = false;
    info->moveType = MOVETYPE_MOVE;
    SetWindowLongPtr(hWnd, GWLP_INFO, (LONG_PTR)info);

    AddCaptureBoxWindow(hWnd);
    ShowWindow(hWnd, SW_NORMAL);

    return hWnd;
}

HWND ReopenPrevCaptureBox() {
    if (!savedCaptureBoxes) return 0;
    PCAPTUREBOXCLOSEINFO p = prevCaptureBox[savedCaptureBoxes-1];

    RECT r = p->rLocation;
    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, szCaptureBoxWindowClass, _T("SNAP"), WS_POPUP,
        r.left, r.top, r.right-r.left, r.bottom-r.top, 0, 0, hInst, NULL);

    SetWindowLongPtr(hWnd, GWLP_INFO, (LONG_PTR)p->info);
    p->info->moveType = MOVETYPE_MOVE;
    p->info->moving = false;
    p->info->captured = false;
    p->info->isInClose = false;
    p->info->trackingMouse = false;
    p->info->frameOpacity = 0.0;

    AddCaptureBoxWindow(hWnd);
    ShowWindow(hWnd, SW_NORMAL);

    POINT pt;
    GetCursorPos(&pt);

    if (SendMessage(hWnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y)) != HTNOWHERE)
    {
        SetTracking(hWnd, p->info);
        if (options.showHoverInfo)
            AnimateFrameFade(hWnd, p->info, FRAME_OPACITY);
    }

    delete p;
    savedCaptureBoxes--;

    return hWnd;
}

void CloseAllCaptureBoxes()
{
    while (openCaptureBoxes)
    {
        PCAPTUREBOXWINDOW w = openCaptureBoxes;
        openCaptureBoxes = w->next;

        DestroyWindow(w->hWnd);
        delete w;
    }
}

void ShowAllCaptureBoxes()
{
    stopCaptures = false;

    PCAPTUREBOXWINDOW w = openCaptureBoxes;
    while (w)
    {
        ShowWindow(w->hWnd, SW_SHOW);
        w = w->next;
    }
}

void HideAllCaptureBoxes(bool forDialog)
{
    if (forDialog)
        stopCaptures = true;

    PCAPTUREBOXWINDOW w = openCaptureBoxes;
    while (w)
    {
        ShowWindow(w->hWnd, SW_HIDE);
        w = w->next;
    }
}

void SaveCaptureBox(PCAPTUREBOXCLOSEINFO info)
{
    if (savedCaptureBoxes == options.maxHistory)
    {
        PCAPTUREBOXCLOSEINFO p = prevCaptureBox[0];
        if (p)
        {
            delete p->info->bitmap;
            delete p->info;
            delete p;
        }

        for (int i=0; i<options.maxHistory-1; i++)
            prevCaptureBox[i] = prevCaptureBox[i+1];
        savedCaptureBoxes--;
    }

    prevCaptureBox[savedCaptureBoxes] = info;
    savedCaptureBoxes++;
}

void ClearCaptureHistory()
{
    for (int i=0; i<savedCaptureBoxes; i++)
    {
        PCAPTUREBOXCLOSEINFO p = prevCaptureBox[i];
        if (p)
        {
            delete p->info->bitmap;
            delete p->info;
            delete p;
        }
    }

    savedCaptureBoxes = 0;
}

void TrimCaptureHistory(int maxHistory)
{
    if (maxHistory == 0)
        ClearCaptureHistory();
    else if (maxHistory < savedCaptureBoxes)
    {
        int diff = savedCaptureBoxes - maxHistory;

        for (int i=0; i<diff; i++)
        {
            PCAPTUREBOXCLOSEINFO p = prevCaptureBox[i];
            if (p)
            {
                delete p->info->bitmap;
                delete p->info;
                delete p;
            }
        }

        for (int i=0; i<savedCaptureBoxes-diff; i++)
            prevCaptureBox[i] = prevCaptureBox[i + diff];

        savedCaptureBoxes = maxHistory;
    }
}

void LockClipboardData(HWND hWnd)
{
    OpenClipboard(hWnd);
    int i = 0;
    HANDLE hData;
    do
    {
        i = EnumClipboardFormats(i);
        if (i)
            hData = GetClipboardData(i);
    }
    while (i);
    CloseClipboard();
}

void CopyToClipboard(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    Graphics *g = new Graphics(hWnd);
    Bitmap *bitmap = new Bitmap((int)ceilf(info->cropRect.Width), (int)ceilf(info->cropRect.Height), g);
    delete g;

    g = new Graphics(bitmap);
    g->DrawImage(info->bitmap, 0.0, 0.0, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);
    delete g;

    HBITMAP hBitmap;
    if (bitmap->GetHBITMAP(Color(0xff, 0xff, 0xff), &hBitmap) == Gdiplus::Ok)
    {
        if (!OpenClipboard(hWnd)) return;
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
        LockClipboardData(hWnd);
        DeleteObject(hBitmap);
    }

    delete bitmap;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;

   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}

void SaveFile(HWND hWnd, const LPTSTR szPath, int fileType)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    Graphics *g = new Graphics(hWnd);
    Bitmap *bitmap = new Bitmap((int)ceilf(info->cropRect.Width), (int)ceilf(info->cropRect.Height), g);
    delete g;

    g = new Graphics(bitmap);
    g->DrawImage(info->bitmap, 0.0, 0.0, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);
    delete g;

    CLSID clsidEncoder;
    switch (fileType)
    {
    case SAVETYPE_PNG:
        GetEncoderClsid(_T("image/png"), &clsidEncoder);
        break;
    case SAVETYPE_BMP:
        GetEncoderClsid(_T("image/bmp"), &clsidEncoder);
        break;
    case SAVETYPE_GIF:
        GetEncoderClsid(_T("image/gif"), &clsidEncoder);
        break;
    case SAVETYPE_JPEG:
        GetEncoderClsid(_T("image/jpeg"), &clsidEncoder);
        break;
    }

    bitmap->Save(szPath, &clsidEncoder);

    delete bitmap;
}

UINT_PTR CALLBACK SaveHookProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    if (uMsg == WM_INITDIALOG)
    {
        hForeWindow = hDlg;
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
        SetWindowLongPtr(hDlg, GWL_EXSTYLE, GetWindowLongPtr(hDlg, GWL_EXSTYLE) | WS_EX_APPWINDOW);
    }
    else if (uMsg == WM_DESTROY)
    {
        hForeWindow = NULL;
    }

    return 0;
}

void SaveCaptureBox(HWND hWnd)
{
    TCHAR filename[MAX_PATH];
    filename[0] = '\0';

    OPENFILENAME file;
    memset(&file, 0, sizeof(OPENFILENAME));
    file.lStructSize = sizeof(OPENFILENAME);
    file.lpstrFilter = szSaveFilter;
    file.nFilterIndex = options.defaultSaveType;
    file.lpstrFile = filename;
    file.nMaxFile = MAX_PATH;
    file.lpfnHook = (LPOFNHOOKPROC)&SaveHookProc;
    file.Flags = OFN_DONTADDTORECENT | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLEHOOK;

    HideAllCaptureBoxes(true);
    if (!GetSaveFileName(&file))
    {
        ShowAllCaptureBoxes();
        return;
    }
    ShowAllCaptureBoxes();

    if (!file.nFileExtension)
    {
        switch (file.nFilterIndex)
        {
        case SAVETYPE_PNG:
            _tcscat_s(filename, MAX_PATH, _T(".png"));
            break;
        case SAVETYPE_BMP:
            _tcscat_s(filename, MAX_PATH, _T(".bmp"));
            break;
        case SAVETYPE_GIF:
            _tcscat_s(filename, MAX_PATH, _T(".gif"));
            break;
        case SAVETYPE_JPEG:
            _tcscat_s(filename, MAX_PATH, _T(".jpeg"));
            break;
        }
    }

    if (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES)
    {
        TCHAR szBuffer[255], fname[_MAX_FNAME], ext[_MAX_EXT];
        _tsplitpath_s(filename, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
        _tcscpy_s(szBuffer, 255, _T("The file "));
        _tcscat_s(szBuffer, 255, fname);
        _tcscat_s(szBuffer, 255, ext);
        _tcscat_s(szBuffer, 255, _T(" already exists.  Overwrite?"));
        if (MessageBox(hWnd, szBuffer, _T("Confirm Overwrite"), MB_YESNO | MB_ICONQUESTION) == IDNO)
            return;
    }

    SaveFile(hWnd, filename, file.nFilterIndex);
}

void GetFileName(LPTSTR szPath, int fileType)
{
    SYSTEMTIME time;
    GetSystemTime(&time);
    TCHAR szFile[50];
    _stprintf_s(szFile, 50, _T("\\SnapBox_%04d%02d%02d_%02d%02d%02d"), time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    _tcscat_s(szPath, MAX_PATH, szFile);

    switch (fileType)
    {
    case SAVETYPE_PNG:
        _tcscat_s(szPath, MAX_PATH, _T(".png"));
        break;
    case SAVETYPE_BMP:
        _tcscat_s(szPath, MAX_PATH, _T(".bmp"));
        break;
    case SAVETYPE_GIF:
        _tcscat_s(szPath, MAX_PATH, _T(".gif"));
        break;
    case SAVETYPE_JPEG:
        _tcscat_s(szPath, MAX_PATH, _T(".jpeg"));
        break;
    }
}

void QuickSaveCaptureBox(HWND hWnd)
{
    bool getDefault = true;
    TCHAR szPath[MAX_PATH];
    if (_tcslen(options.quickSavePath) > 0)
    {
        _tcscpy_s(szPath, MAX_PATH, options.quickSavePath);
        if (!_taccess_s(szPath, 0))
            getDefault = false;
    }
    if (getDefault)
        if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, szPath)))
            return;

    size_t i = _tcslen(szPath)-1;
    if (szPath[i] == _T('\\'))
        szPath[i] = _T('\0');

    GetFileName(szPath, options.defaultSaveType);

    SaveFile(hWnd, szPath, options.defaultSaveType);
}

void EmailCaptureBox(HWND hWnd)
{
    TCHAR szPath[MAX_PATH];
    if (!GetTempPath(MAX_PATH, szPath))
        return;
    size_t i = _tcslen(szPath)-1;
    if (szPath[i] == _T('\\'))
        szPath[i] = _T('\0');

    GetFileName(szPath, options.defaultSaveType);
    SaveFile(hWnd, szPath, options.defaultSaveType);

    EmailFile(szPath);
}

void AdjustRect(const PCAPTUREBOXINFO info, RECT &rect)
{
    if (info->moving && (info->moveType & MOVETYPE_RESIZE))
    {
        RECT newRect;
        memcpy(&newRect, &rect, sizeof(RECT));
        AdjustForSizeMarks(newRect, info->sizeMarks);

        rect.left += max(newRect.left-rect.left, (int)floorf(info->cropRect.X*info->scale));
        rect.top += max(newRect.top-rect.top, (int)floorf(info->cropRect.Y*info->scale));
        rect.right -= max(rect.right-newRect.right, (int)floorf((info->size.x - info->cropRect.GetRight())*info->scale));
        rect.bottom -= max(rect.bottom-newRect.bottom, (int)floorf((info->size.y - info->cropRect.GetBottom())*info->scale));
    }
    else
        AdjustForSizeMarks(rect, info->sizeMarks);
}

void AdjustPoint(const PCAPTUREBOXINFO info, POINT &p)
{
    if (info->moving && (info->moveType & MOVETYPE_RESIZE))
    {
        POINT newP;
        memcpy(&newP, &p, sizeof(POINT));
        AdjustPointForSizeMarks(newP, info->sizeMarks);

        p.x -= max(p.x-newP.x, (int)floorf(info->cropRect.X*info->scale));
        p.y -= max(p.y-newP.y, (int)floorf(info->cropRect.Y*info->scale));
    }
    else
        AdjustPointForSizeMarks(p, info->sizeMarks);
}

void DrawCaptureBox(HWND hWnd, PCAPTUREBOXINFO info, const RECT* rect)
{
    HDC hdcScreen = GetDC(NULL);

    bool drawShade = info->moving && (info->moveType & MOVETYPE_RESIZE);

    RECT frameRect, windowRect, shadeRect;
    ExpandForSizeMarks(rect, &frameRect, info->sizeMarks);
    memcpy(&shadeRect, rect, sizeof(RECT));

    if (drawShade)
    {
        shadeRect.left -= (int)floorf(info->cropRect.X*info->scale);
        shadeRect.top -= (int)floorf(info->cropRect.Y*info->scale);
        shadeRect.right += (int)floorf((info->size.x - info->cropRect.GetRight())*info->scale);
        shadeRect.bottom += (int)floorf((info->size.y - info->cropRect.GetBottom())*info->scale);

        windowRect.left = min(shadeRect.left, frameRect.left);
        windowRect.top = min(shadeRect.top, frameRect.top);
        windowRect.right = max(shadeRect.right, frameRect.right);
        windowRect.bottom = max(shadeRect.bottom, frameRect.bottom);
    }
    else
        memcpy(&windowRect, &frameRect, sizeof(RECT));

    int width = windowRect.right-windowRect.left,
        height = windowRect.bottom-windowRect.top;

    RectF clipRect;
    clipRect.X = (float)(rect->left - windowRect.left + 1);
    clipRect.Y = (float)(rect->top - windowRect.top + 1);
    clipRect.Width = (REAL)(rect->right-rect->left-2);
    clipRect.Height = (REAL)(rect->bottom-rect->top-2);

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ hBitmapOld = SelectObject(hdc, hBitmap);
    Graphics* g = new Graphics(hdc);
    g->SetCompositingMode(CompositingModeSourceOver);
    g->SetSmoothingMode(SmoothingModeAntiAlias);

    g->FillRectangle(pTransparentBrush, 0, 0, width, height);

    if (drawShade)
        g->FillRectangle(pCropBrush, shadeRect.left-windowRect.left, shadeRect.top-windowRect.top, shadeRect.right-shadeRect.left, shadeRect.bottom-shadeRect.top);

    g->DrawImage(info->bitmap, clipRect, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);

    clipRect.X -= 1;
    clipRect.Y -= 1;
    clipRect.Width += 1;
    clipRect.Height += 1;
    g->DrawRectangle(pBorderPen, clipRect);

    GraphicsState gState = g->Save();
    g->TranslateTransform((float)(frameRect.left-windowRect.left), (float)(frameRect.top-windowRect.top));
    SIZEMARKOPTIONS smOptions;
    memset(&smOptions, 0, sizeof(smOptions));
    smOptions.lpRect = &frameRect;
    smOptions.lpCropRect = &info->cropRect;
    smOptions.lpCloseRect = &info->closeRect;
    smOptions.dwLocation = info->sizeMarks;
    smOptions.dwOptions = options.showHoverInfo ? SIZEMARKOPTION_SHOWCLOSE : 0;
    if (info->isInClose)
    {
        smOptions.dwOptions |= SIZEMARKOPTION_HOVERCLOSE;
        if (info->captured)
            smOptions.dwOptions |= SIZEMARKOPTION_CLOSEDOWN;
    }
    smOptions.fOpacity = info->frameOpacity;
    DrawSizeMarks(g, &smOptions);
    g->Restore(gState);

    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.AlphaFormat = AC_SRC_ALPHA;
    blend.SourceConstantAlpha = 255;
    blend.BlendFlags = 0;

    POINT ptPos = {windowRect.left, windowRect.top};
    SIZE sizeWnd = {width, height};
    POINT ptSrc = {0, 0};

    UpdateLayeredWindow(hWnd, hdcScreen, &ptPos, &sizeWnd, hdc, &ptSrc, 0, &blend, ULW_ALPHA);

    delete g;
    SelectObject(hdc, hBitmapOld);
    DeleteObject(hBitmap);
    DeleteDC(hdc);

    ReleaseDC(NULL, hdcScreen);
}

void CALLBACK AnimateTimer(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(idEvent);
    UNREFERENCED_PARAMETER(dwTime);

    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    ULONG diff = (GetTickCount() - info->animateStart) / 30;

    RECT rect;
    GetWindowRect(hWnd, &rect);
    AdjustRect(info, rect);

    if (info->animateType & ANIMATETYPE_FRAME)
    {
        if (info->targetFrameOpacity > info->frameOpacity)
            info->frameOpacity = min(info->startFrameOpacity + 0.1f * static_cast<float>(diff), info->targetFrameOpacity);
        else
            info->frameOpacity = max(info->startFrameOpacity - 0.1f * static_cast<float>(diff), info->targetFrameOpacity);

        if (info->frameOpacity == info->targetFrameOpacity)
            info->animateType &= ~ANIMATETYPE_FRAME;
    }

    if (info->animateType & ANIMATETYPE_SCALE)
    {
        if (info->curScale < info->scale)
        {
            float temp = info->curScale * powf(1.05f, static_cast<float>(diff));
            info->curScale = min(temp, info->scale);
        }
        else
        {
            float temp = info->curScale * powf(0.95f, static_cast<float>(diff));
            info->curScale = max(temp, info->scale);
        }

        PointF center;
        if (info->centerMoved)
        {
            center.X = (float)(rect.right - rect.left - 1) / 2 + rect.left;
            center.Y = (float)(rect.bottom - rect.top - 1) / 2 + rect.top;
            info->center = center;
            info->centerMoved = false;
        }
        else
            center = info->center;

        int width = (int)floorf(info->cropRect.Width * info->curScale) + 2,
            height = (int)floorf(info->cropRect.Height * info->curScale) + 2;

        PointF newCenter;
        newCenter.X = ((float)width / 2) + rect.left;
        newCenter.Y = ((float)height / 2) + rect.top;

        rect.left -= (int)floorf(newCenter.X - center.X);
        rect.top -= (int)floorf(newCenter.Y - center.Y);
        rect.right = rect.left + width;
        rect.bottom = rect.top + height;
        DrawCaptureBox(hWnd, info, &rect);

        if (info->curScale == info->scale)
            info->animateType &= ~ANIMATETYPE_SCALE;
    }
    else
        DrawCaptureBox(hWnd, info, &rect);

    if (!info->animateType)
    {
        KillTimer(hWnd, info->animateTimer);
        info->animateTimer = (UINT_PTR)0;
    }
}

void AnimateZoom(HWND hWnd, int newScaleIndex, float newScale)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moving) return;

    info->scaleIndex = newScaleIndex;
    info->startScale = info->curScale;
    info->startFrameOpacity = info->frameOpacity;
    info->scale = newScale;
    info->animateStart = GetTickCount();
    info->animateType |= ANIMATETYPE_SCALE;

    if (!info->animateTimer)
        info->animateTimer = SetTimer(hWnd, 1, 30, (TIMERPROC)&AnimateTimer);
}

void AnimateFrameFade(HWND hWnd, PCAPTUREBOXINFO info, float targetOpacity)
{
    if (targetOpacity == info->frameOpacity) return;
    if ((info->animateType & ANIMATETYPE_FRAME) && targetOpacity == info->targetFrameOpacity) return;

    info->startScale = info->curScale;
    info->startFrameOpacity = info->frameOpacity;
    info->targetFrameOpacity = targetOpacity;
    info->animateStart = GetTickCount();
    info->animateType |= ANIMATETYPE_FRAME;

    if (!info->animateTimer)
        info->animateTimer = SetTimer(hWnd, 1, 30, (TIMERPROC)&AnimateTimer);
}

void SetTracking(HWND hWnd, PCAPTUREBOXINFO info)
{
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hWnd;
    TrackMouseEvent(&tme);

    info->trackingMouse = true;
}

void CaptureMouseDown(HWND hWnd, LPARAM lParam)
{
    SetCapture(hWnd);

    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    info->captured = true;
    if (!info->isInClose)
    {
        if (info->moveType == MOVETYPE_FRAME)
            info->moveType = MOVETYPE_MOVE;

        info->moved = false;
        info->movePoint.x = GET_X_LPARAM(lParam);
        info->movePoint.y = GET_Y_LPARAM(lParam);
        AdjustPoint(info, info->movePoint);
        info->screenMovePoint = info->movePoint;
        ClientToScreen(hWnd, &info->screenMovePoint);

        if (info->moveType & MOVETYPE_RESIZE)
            AnimateFrameFade(hWnd, info, 1.0);

        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);

        if (info->moveType & MOVETYPE_LEFT)
            info->sizeMarks = SIZEMARKLOCATION_RIGHT;
        else
            info->sizeMarks = SIZEMARKLOCATION_LEFT;
        if (info->moveType & MOVETYPE_TOP)
            info->sizeMarks |= SIZEMARKLOCATION_BOTTOM;
        else
            info->sizeMarks |= SIZEMARKLOCATION_TOP;
        info->moving = true;

        DrawCaptureBox(hWnd, info, &rect);
    }
    else
    {
        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);
        DrawCaptureBox(hWnd, info, &rect);
    }
}

void DoMoveWindow(HWND hWnd, PCAPTUREBOXINFO info, RECT &rect, int x, int y, int width, int height)
{
    rect.left += x;
    rect.top += y;
    rect.right += width + x;
    rect.bottom  += height + y;

    DrawCaptureBox(hWnd, info, &rect);
    info->centerMoved = true;
}

void CaptureMouseMove(HWND hWnd, LPARAM lParam)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    RECT rect;
    POINT p;
    bool alreadySized = false;

    p.x = GET_X_LPARAM(lParam);
    p.y = GET_Y_LPARAM(lParam);
    GetWindowRect(hWnd, &rect);

    if (!info->moving)
    {
        bool newInClose = !!PtInRect(&info->closeRect, p);
        if (newInClose != info->isInClose)
        {
            info->isInClose = newInClose;
            AdjustRect(info, rect);
            DrawCaptureBox(hWnd, info, &rect);
            alreadySized = true;
        }
    }
    else
        info->isInClose = false;

    if (!alreadySized)
        AdjustRect(info, rect);
    AdjustPoint(info, p);

    if (info->moving)
    {
        if ((info->movePoint.x != p.x) || (info->movePoint.y != p.y))
        {
            if (!info->moved)
            {
                POINT pScreen = p;
                ClientToScreen(hWnd, &pScreen);

                if ((abs(pScreen.x-info->screenMovePoint.x) >= dragMin.x) || (abs(pScreen.y-info->screenMovePoint.y) >= dragMin.y))
                    info->moved = true;
            }

            int offsetX = p.x - info->movePoint.x,
                offsetY = p.y - info->movePoint.y;
            RectF cropRect = info->cropRect;
            float scale = info->scale;

            if (info->moveType & MOVETYPE_LEFT)
            {
                int temp = (int)ceilf(cropRect.X*scale),
                    width = rect.right-rect.left;
                if (offsetX + temp < 0)
                    offsetX = -temp;
                else if (width - offsetX < MIN_CAPTURE_SIZE)
                    offsetX = width - MIN_CAPTURE_SIZE;
            }
            if (info->moveType & MOVETYPE_RIGHT)
            {
                int temp = (int)ceilf((info->size.x - cropRect.X)*scale)+2,
                    width = rect.right-rect.left;
                if (width + offsetX > temp)
                    offsetX = temp - width;
                else if (width + offsetX < MIN_CAPTURE_SIZE)
                    offsetX = MIN_CAPTURE_SIZE - width;
            }
            if (info->moveType & MOVETYPE_TOP)
            {
                int temp = (int)ceilf(cropRect.Y*scale),
                    height = rect.bottom-rect.top;
                if (offsetY + temp < 0)
                    offsetY = -temp;
                else if (height - offsetY < MIN_CAPTURE_SIZE)
                    offsetY = height - MIN_CAPTURE_SIZE;
            }
            if (info->moveType & MOVETYPE_BOTTOM)
            {
                int temp = (int)ceilf((info->size.y - cropRect.Y)*scale)+2,
                    height = rect.bottom-rect.top;
                if (height + offsetY > temp)
                    offsetY = temp - height;
                else if (height + offsetY < MIN_CAPTURE_SIZE)
                    offsetY = MIN_CAPTURE_SIZE - height;
            }

            switch (info->moveType)
            {
            case MOVETYPE_MOVE:
                DoMoveWindow(hWnd, info, rect, offsetX, offsetY, 0, 0);
                break;
            case MOVETYPE_LEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                DoMoveWindow(hWnd, info, rect, offsetX, 0, -offsetX, 0);
                break;
            case MOVETYPE_RIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                DoMoveWindow(hWnd, info, rect, 0, 0, offsetX, 0);
                break;
            case MOVETYPE_TOP:
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, 0, offsetY, 0, -offsetY);
                break;
            case MOVETYPE_BOTTOM:
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, 0, 0, 0, offsetY);
                break;
            case MOVETYPE_TOPLEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, offsetX, offsetY, -offsetX, -offsetY);
                break;
            case MOVETYPE_TOPRIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, 0, offsetY, offsetX, -offsetY);
                break;
            case MOVETYPE_BOTTOMLEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, offsetX, 0, -offsetX, offsetY);
                break;
            case MOVETYPE_BOTTOMRIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, 0, 0, offsetX, offsetY);
                break;
            }
        }
    } else {
        int moveType;
        int width = rect.right-rect.left,
            height = rect.bottom-rect.top;

        if (p.x < 0 || p.x > width || p.y < 0 || p.y > height)
            moveType = MOVETYPE_FRAME;
        else
        {
            moveType = MOVETYPE_NONE;
            if (info->size.x >= MIN_CAPTURE_SIZE)
            {
                if (p.x < CROP_SIZE)
                    moveType |= MOVETYPE_LEFT;
                else if (p.x > width-CROP_SIZE)
                    moveType |= MOVETYPE_RIGHT;
            }
            if (info->size.y >= MIN_CAPTURE_SIZE)
            {
                if (p.y < CROP_SIZE)
                    moveType |= MOVETYPE_TOP;
                else if (p.y > height-CROP_SIZE)
                    moveType |= MOVETYPE_BOTTOM;
            }
            if (moveType == MOVETYPE_NONE)
                moveType = MOVETYPE_MOVE;
        }

        if (!info->captured) {
            switch (moveType)
            {
            case MOVETYPE_FRAME:
                SetCursor(hCursorArrow);
                break;
            case MOVETYPE_MOVE:
                SetCursor(hCursorMove);
                break;
            case MOVETYPE_LEFT:
            case MOVETYPE_RIGHT:
                SetCursor(hCursorEW);
                break;
            case MOVETYPE_TOP:
            case MOVETYPE_BOTTOM:
                SetCursor(hCursorNS);
                break;
            case MOVETYPE_TOPLEFT:
            case MOVETYPE_BOTTOMRIGHT:
                SetCursor(hCursorNWSE);
                break;
            case MOVETYPE_TOPRIGHT:
            case MOVETYPE_BOTTOMLEFT:
                SetCursor(hCursorNESW);
                break;
            }
        }

        info->moveType = moveType;

        if (!info->captured && options.showHoverInfo)
        {
            if (moveType == MOVETYPE_FRAME)
                AnimateFrameFade(hWnd, info, 1.0);
            else
                AnimateFrameFade(hWnd, info, FRAME_OPACITY);
        }
    }

    if (!info->trackingMouse)
        SetTracking(hWnd, info);
}

void CaptureMouseUp(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->captured)
    {
        ReleaseCapture();

        if (info->moving)
        {
            if (info->moveType == MOVETYPE_MOVE && !info->moved)
            {
                info->captured = false;
                info->moving = false;
                DestroyWindow(hWnd);
                return;
            }
        }
        else
        {
            if ((info->moveType == MOVETYPE_FRAME) && info->isInClose)
            {
                info->captured = false;
                info->moving = false;
                DestroyWindow(hWnd);
                return;
            }
        }

        if (info->moveType & MOVETYPE_RESIZE)
            AnimateFrameFade(hWnd, info, options.showHoverInfo ? FRAME_OPACITY : (float)0.0);

        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);
        info->sizeMarks = SIZEMARKLOCATION_TOPLEFT;
        info->captured = false;
        info->moving = false;
        DrawCaptureBox(hWnd, info, &rect);
    }
}

void CaptureMouseLeave(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moveType == MOVETYPE_FRAME)
        info->moveType = MOVETYPE_NONE;
    AnimateFrameFade(hWnd, info, 0.0);

    info->trackingMouse = false;
}

void CaptureMouseWheel(HWND hWnd, short wheelDelta)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moving) return;

    int scaleIndex = info->scaleIndex + (wheelDelta/120);
    float scale = 1.0f;
    if (scaleIndex > 0)
        scale = powf(1.05f, static_cast<float>(scaleIndex));
    else if (scaleIndex < 0)
        scale = powf(0.95f, static_cast<float>(-scaleIndex));

    int width = (int)(floorf(info->cropRect.Width * scale) + 2),
        height = (int)(floorf(info->cropRect.Height * scale) + 2);

    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN),
        screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    if ((height < MIN_CAPTURE_SIZE || width < MIN_CAPTURE_SIZE) ||
        (height > screenHeight || width > screenWidth))
    {
        if (wheelDelta > 0)
            wheelDelta = ((wheelDelta / 120) - 1) * 120;
        else
            wheelDelta = ((wheelDelta / 120) + 1) * 120;

        if (wheelDelta)
            CaptureMouseWheel(hWnd, wheelDelta);
        return;
    }

    AnimateZoom(hWnd, scaleIndex, scale);
}

bool ProcessMouseWheel(POINT pt, short wheelDelta)
{
    HWND hWnd = WindowFromPoint(pt);

    if (hWnd)
    {
        PCAPTUREBOXWINDOW w = openCaptureBoxes;
        while (w)
        {
            if (hWnd == w->hWnd)
            {
                CaptureMouseWheel(hWnd, wheelDelta);
                return true;
            }

            w = w->next;
        }
    }

    return false;
}

LRESULT CALLBACK CaptureBoxWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    RECT rect;
    RectF clipRect, srcRect;
    PCAPTUREBOXINFO info;
    POINT p;

    switch (message)
    {
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_COPY:
            CopyToClipboard(hWnd);
            break;
        case IDM_SAVE:
            SaveCaptureBox(hWnd);
            break;
        case IDM_QUICKSAVE:
            QuickSaveCaptureBox(hWnd);
            break;
        case IDM_EMAIL:
            EmailCaptureBox(hWnd);
            break;
        case IDM_CLOSE:
            DestroyWindow(hWnd);
            break;
        case IDM_CLOSEALL:
            CloseAllCaptureBoxes();
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_RBUTTONUP:
        HMENU hPopup;
        hPopup = GetSubMenu(hCaptureMenu, 0);

        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        ClientToScreen(hWnd, &p);
        TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        break;
    case WM_LBUTTONDOWN:
        CaptureMouseDown(hWnd, lParam);
        break;
    case WM_MOUSEMOVE:
        CaptureMouseMove(hWnd, lParam);
        break;
    case WM_MOUSELEAVE:
        CaptureMouseLeave(hWnd);
        break;
    case WM_LBUTTONUP:
        CaptureMouseUp(hWnd);
        break;
    case WM_MBUTTONUP:
        AnimateZoom(hWnd, 0, 1.0);
        break;
    case WM_SHOWWINDOW:
        if (wParam)
        {
            info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &rect);
            AdjustRect(info, rect);
            info->sizeMarks = SIZEMARKLOCATION_TOPLEFT;
            DrawCaptureBox(hWnd, info, &rect);
        }
        break;
    case WM_DPICHANGED:
        {
            RECT* suggestedRect = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, suggestedRect->left, suggestedRect->top,
                suggestedRect->right - suggestedRect->left,
                suggestedRect->bottom - suggestedRect->top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

            info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &rect);
            AdjustRect(info, rect);
            DrawCaptureBox(hWnd, info, &rect);
        }
        return 0;
    case WM_DESTROY:
        {
            PCAPTUREBOXCLOSEINFO closeInfo = new CAPTUREBOXCLOSEINFO();
            closeInfo->info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &closeInfo->rLocation);
            SaveCaptureBox(closeInfo);

            PCAPTUREBOXWINDOW w = openCaptureBoxes;
            PCAPTUREBOXWINDOW *captureBoxLink = &openCaptureBoxes;
            while (w)
            {
                if (w->hWnd == hWnd)
                {
                    *captureBoxLink = w->next;
                    delete w;
                    break;
                }

                captureBoxLink = &w->next;
                w = w->next;
            }
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
