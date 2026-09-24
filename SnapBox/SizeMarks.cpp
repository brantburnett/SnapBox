#include "stdafx.h"
#include "resource.h"
#include "SizeMarks.h"

using namespace Gdiplus;

#define SIZEMARK_PADDING		2
#define SIZEMARK_ROUNDING		3
#define SIZEMARK_TRAPEZOID		3
#define SIZEMARK_FONTSIZE		8
#define SIZEMARK_CLOSEPADDING	1
#define SIZEMARK_CLOSEBORDER	2
#define SIZEMARK_CLOSESPACING	4

Font* sizeFont = NULL;
StringFormat* sizeStringFormat = NULL;
Bitmap* biClose = NULL;

UINT sizeHeight = 0, closeHeight = 0, closeWidth = 0;
float fSizeHeight = 0;

const Color SizeMarkColor1(0xcf, 0xce, 0xce);
const Color SizeMarkColor2(0x99, 0x99, 0x99);
const Color SizeMarkBorderColor(0, 0, 0);
const Color CloseBoxHighColor(0xff, 0xff, 0xff);
const Color CloseBoxLowColor(0x99, 0x99, 0x99);
const ColorMatrix identityMatrix = {{{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}}};

Bitmap* LoadBitmapResource(HINSTANCE hInstance, const TCHAR* bitmapName, const TCHAR* resType)
{
    HRSRC hResource = FindResource(hInstance, bitmapName, resType);
    if (!hResource)
        return NULL;

    DWORD imageSize = SizeofResource(hInstance, hResource);
    if (!imageSize)
        return NULL;

    const void* pResourceData = LockResource(LoadResource(hInstance, hResource));
    if (!pResourceData)
        return NULL;

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
    if (!hBuffer)
        return NULL;

    Bitmap* pResult = NULL;
    void* pBuffer = GlobalLock(hBuffer);
    if (!pBuffer)
    {
        GlobalFree(hBuffer);
        return NULL;
    }

    CopyMemory(pBuffer, pResourceData, imageSize);

    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(hBuffer, FALSE, &pStream) == S_OK)
    {
        Bitmap* pBitmap = Bitmap::FromStream(pStream);
        if (pBitmap && pBitmap->GetLastStatus() == Gdiplus::Ok)
        {
            pResult = pBitmap->Clone(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight(), pBitmap->GetPixelFormat());
            if (pResult && pResult->GetLastStatus() != Gdiplus::Ok)
            {
                delete pResult;
                pResult = NULL;
            }
        }
        delete pBitmap;
        pStream->Release();
    }

    GlobalUnlock(hBuffer);
    GlobalFree(hBuffer);
    return pResult;
}

void ShutdownSizeMarks()
{
    delete sizeFont;
    sizeFont = NULL;
    delete sizeStringFormat;
    sizeStringFormat = NULL;
    delete biClose;
    biClose = NULL;

    sizeHeight = 0;
    closeHeight = 0;
    closeWidth = 0;
    fSizeHeight = 0;
}

void InitSizeMarks(HINSTANCE hInstance)
{
    ShutdownSizeMarks();

    biClose = LoadBitmapResource(hInstance, MAKEINTRESOURCE(IDB_CLOSEPNG), _T("PNG"));
    if (biClose)
    {
        closeHeight = biClose->GetHeight();
        closeWidth = biClose->GetWidth();
    }

    Graphics* g = new Graphics(GetDesktopWindow());

    sizeFont = new Font(_T("Verdana"), SIZEMARK_FONTSIZE);

    sizeStringFormat = new StringFormat();
    if (!g || g->GetLastStatus() != Ok || !sizeFont || sizeFont->GetLastStatus() != Ok || !sizeStringFormat || sizeStringFormat->GetLastStatus() != Ok)
    {
        delete g;
        ShutdownSizeMarks();
        return;
    }

    sizeStringFormat->SetTrimming(StringTrimmingNone);
    sizeStringFormat->SetAlignment(StringAlignmentCenter);
    sizeStringFormat->SetLineAlignment(StringAlignmentCenter);

    RectF boundingBox;
    g->MeasureString(_T("Mg"), -1, sizeFont, PointF(0, 0), &boundingBox);
    sizeHeight = (int)ceilf(boundingBox.Height) + SIZEMARK_PADDING*2;
    sizeHeight = max(sizeHeight, closeHeight + SIZEMARK_PADDING*2 + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSEBORDER*2);
    fSizeHeight = (float)sizeHeight;

    delete g;
}

void DrawSizeMarks(Graphics* g, PCSIZEMARKOPTIONS options)
{
    if (!g || !options || !options->lpRect || !options->lpCropRect || !sizeFont || !sizeStringFormat)
        return;

    g->SetTextRenderingHint(TextRenderingHintAntiAlias);
    if (options->lpCloseRect)
        memset(options->lpCloseRect, 0, sizeof(RECT));

    BYTE opacityByte = (DWORD)ceilf(options->fOpacity * 255) & 0xff;
    ARGB opacityArgb = (DWORD)opacityByte << 24;

    TCHAR sizeStr[20];
    RectF boundingBox;
    GraphicsPath* path;
    float width = (float)(options->lpRect->right - options->lpRect->left),
        height = (float)(options->lpRect->bottom - options->lpRect->top);

    if (options->dwLocation & (SIZEMARKLOCATION_TOP | SIZEMARKLOCATION_BOTTOM))
        height -= fSizeHeight;
    if (options->dwLocation & (SIZEMARKLOCATION_LEFT | SIZEMARKLOCATION_RIGHT))
        width -= fSizeHeight;

    if (options->dwLocation & (SIZEMARKLOCATION_TOP | SIZEMARKLOCATION_BOTTOM))
    {
        bool drawClose = biClose && (bool)(options->dwOptions & SIZEMARKOPTION_SHOWCLOSE);

        _itot_s((int)ceilf(options->lpCropRect->Width), sizeStr, 20, 10);
        _tcscat_s(sizeStr, 20, _T("px"));

        g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
        boundingBox.Width += SIZEMARK_PADDING*2;
        if (drawClose)
            boundingBox.Width += closeWidth + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSEBORDER*2;

        if (boundingBox.Width > width)
        {
            sizeStr[_tcslen(sizeStr)-2] = _T('\0');
            g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
            boundingBox.Width += SIZEMARK_PADDING*2 + SIZEMARK_CLOSESPACING;
            if (drawClose)
                boundingBox.Width += closeWidth + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSEBORDER*2;
        }
        if (drawClose && boundingBox.Width > width)
        {
            drawClose = false;
            width -= closeWidth + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSEBORDER*2;
        }

        if (boundingBox.Width <= width)
        {
            boundingBox.Height = (float)sizeHeight;
            boundingBox.X = width/2 - boundingBox.Width/2;
            boundingBox.Y = 0;

            if (options->dwLocation & SIZEMARKLOCATION_LEFT)
                boundingBox.X += (float)sizeHeight;

            path = new GraphicsPath();
            Brush* brush;
            GraphicsState gState = g->Save();
            if (options->dwLocation & SIZEMARKLOCATION_TOP)
            {
                brush = new LinearGradientBrush(PointF(0,0), PointF(0, (float)sizeHeight), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb));

                path->AddLine(boundingBox.X, boundingBox.Height, boundingBox.X, (float)SIZEMARK_ROUNDING);
                path->AddArc(boundingBox.X, 0.0, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 180, 90);
                path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, 0.0, boundingBox.GetRight() - SIZEMARK_ROUNDING, 0.0);
                path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, 0.0, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 270, 90);
                path->AddLine(boundingBox.GetRight(), (float)SIZEMARK_ROUNDING, boundingBox.GetRight(), boundingBox.Height);
                path->CloseFigure();
            }
            else
            {
                g->TranslateTransform(0, (float)(options->lpRect->bottom - options->lpRect->top - sizeHeight - 1));
                brush = new LinearGradientBrush(PointF(0,0), PointF(0, (float)sizeHeight), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb));

                path->AddLine(boundingBox.X, 0.0, boundingBox.X, boundingBox.Height-SIZEMARK_ROUNDING);
                path->AddArc(boundingBox.X, boundingBox.Height-SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 90, 90);
                path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, boundingBox.Height, boundingBox.GetRight() - SIZEMARK_ROUNDING, boundingBox.Height);
                path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, boundingBox.Height-SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 0, 90);
                path->AddLine(boundingBox.GetRight(), boundingBox.Height-SIZEMARK_ROUNDING, boundingBox.GetRight(), 0.0);
                path->CloseFigure();
            }

            g->FillPath(brush, path);
            delete brush;

            Pen* pen = new Pen(Color(SizeMarkBorderColor.GetValue() & 0xffffff | opacityArgb), 1.0);
            g->DrawPath(pen, path);
            delete pen;

            if (drawClose)
                boundingBox.Width -= closeWidth + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSEBORDER*2;

            brush = new SolidBrush(Color(opacityByte, 0, 0, 0));
            g->DrawString(sizeStr, -1, sizeFont, boundingBox, sizeStringFormat, brush);
            delete brush;

            delete path;

            if (drawClose)
            {
                boundingBox.X += boundingBox.Width + SIZEMARK_CLOSESPACING;
                boundingBox.Y = boundingBox.Height/2 - (float)closeHeight/2;
                boundingBox.Width = (float)closeWidth;
                boundingBox.Height = (float)closeHeight;

                ImageAttributes* attr = new ImageAttributes();
                ColorMatrix* matrix = new ColorMatrix();
                memcpy(matrix, &identityMatrix, sizeof(ColorMatrix));
                matrix->m[3][3] = options->fOpacity;
                attr->SetColorMatrix(matrix);

                g->DrawImage(biClose, boundingBox, 0.0, 0.0, (float)closeWidth, (float)closeHeight, UnitPixel, attr);

                delete attr;
                delete matrix;

                if (options->lpCloseRect)
                {
                    options->lpCloseRect->left = (int)floorf(boundingBox.X);
                    options->lpCloseRect->top = (int)floorf(boundingBox.Y);
                    options->lpCloseRect->right = (int)ceilf(boundingBox.GetRight());
                    options->lpCloseRect->bottom = (int)ceilf(boundingBox.GetBottom());
                }

                if (options->dwOptions & SIZEMARKOPTION_HOVERCLOSE)
                {
                    boundingBox.X -= SIZEMARK_CLOSEPADDING + SIZEMARK_CLOSEBORDER;
                    boundingBox.Y -= SIZEMARK_CLOSEPADDING + SIZEMARK_CLOSEBORDER;
                    boundingBox.Width += SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSEBORDER*2;
                    boundingBox.Height += SIZEMARK_CLOSEPADDING*2 + SIZEMARK_CLOSEBORDER*2;

                    pen = new Pen(Color((options->dwOptions & SIZEMARKOPTION_CLOSEDOWN ? CloseBoxLowColor : CloseBoxHighColor).GetValue() & 0xffffff | opacityArgb), (float)SIZEMARK_CLOSEBORDER);
                    g->DrawLine(pen, boundingBox.X, boundingBox.Y, boundingBox.X, boundingBox.Y + boundingBox.Height - 1);
                    g->DrawLine(pen, boundingBox.X, boundingBox.Y, boundingBox.X + boundingBox.Width - 1, boundingBox.Y);
                    delete pen;

                    pen = new Pen(Color((options->dwOptions & SIZEMARKOPTION_CLOSEDOWN ? CloseBoxHighColor : CloseBoxLowColor).GetValue() & 0xffffff | opacityArgb), (float)SIZEMARK_CLOSEBORDER);
                    g->DrawLine(pen, boundingBox.X, boundingBox.Y + boundingBox.Height - 1, boundingBox.X + boundingBox.Width - 1, boundingBox.Y + boundingBox.Height - 1);
                    g->DrawLine(pen, boundingBox.X + boundingBox.Width - 1, boundingBox.Y, boundingBox.X + boundingBox.Width - 1, boundingBox.Y + boundingBox.Height - 1);
                    delete pen;
                }
            }

            g->Restore(gState);
        }
    }

    if (options->dwLocation & (SIZEMARKLOCATION_LEFT | SIZEMARKLOCATION_RIGHT))
    {
        _itot_s((int)ceilf(options->lpCropRect->Height), sizeStr, 20, 10);
        _tcscat_s(sizeStr, 20, _T("px"));

        g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
        boundingBox.Width += SIZEMARK_PADDING*2;

        if (boundingBox.Width > height)
        {
            sizeStr[_tcslen(sizeStr)-2] = _T('\0');
            g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
            boundingBox.Width += SIZEMARK_PADDING*2;
        }

        if (boundingBox.Width <= height)
        {
            boundingBox.Height = (float)sizeHeight;
            boundingBox.X = -boundingBox.Width/2;
            boundingBox.Y = -(float)sizeHeight/2;

            path = new GraphicsPath();
            path->AddLine(boundingBox.X, boundingBox.GetBottom(), boundingBox.X, (float)boundingBox.GetTop()+SIZEMARK_ROUNDING);
            path->AddArc(boundingBox.X, boundingBox.GetTop(), (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 180, 90);
            path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, boundingBox.GetTop(), boundingBox.GetRight() - SIZEMARK_ROUNDING, boundingBox.GetTop());
            path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, boundingBox.GetTop(), (float)SIZEMARK_ROUNDING*2, boundingBox.GetTop()+SIZEMARK_ROUNDING*2, 270, 90);
            path->AddLine(boundingBox.GetRight(), boundingBox.GetTop()+SIZEMARK_ROUNDING, boundingBox.GetRight(), boundingBox.GetBottom());
            path->CloseFigure();

            GraphicsState gState = g->Save();
            if (options->dwLocation & SIZEMARKLOCATION_LEFT)
            {
                g->TranslateTransform(-boundingBox.Y, height/2);
                if (options->dwLocation & SIZEMARKLOCATION_TOP)
                    g->TranslateTransform(0, (float)sizeHeight);
                g->RotateTransform(270);
            }
            else
            {
                g->TranslateTransform(options->lpRect->right-options->lpRect->left+boundingBox.Y-1, height/2);
                if (options->dwLocation & SIZEMARKLOCATION_TOP)
                    g->TranslateTransform(0, (float)sizeHeight);
                g->RotateTransform(90);
            }

            Brush* brush = new LinearGradientBrush(PointF(0,-(float)sizeHeight/2), PointF(0, (float)sizeHeight/2), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb));
            g->FillPath(brush, path);
            delete brush;

            Pen* pen = new Pen(Color(SizeMarkBorderColor.GetValue() & 0xffffff | opacityArgb), 1.0);
            g->DrawPath(pen, path);
            delete pen;

            brush = new SolidBrush(Color(opacityByte, 0, 0, 0));
            g->DrawString(sizeStr, -1, sizeFont, boundingBox, sizeStringFormat, brush);
            delete brush;

            delete path;
            g->Restore(gState);
        }
    }
}

void ExpandForSizeMarks(const RECT* rect, RECT* newRect, int location)
{
    memcpy(newRect, rect, sizeof(RECT));

    if (location & SIZEMARKLOCATION_LEFT)
        newRect->left -= sizeHeight;
    else if (location & SIZEMARKLOCATION_RIGHT)
        newRect->right += sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        newRect->top -= sizeHeight;
    else if (location & SIZEMARKLOCATION_BOTTOM)
        newRect->bottom += sizeHeight;
}

void AdjustForSizeMarks(RECT &rect, int location)
{
    if (location & SIZEMARKLOCATION_LEFT)
        rect.left += sizeHeight;
    else if (location & SIZEMARKLOCATION_RIGHT)
        rect.right -= sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        rect.top += sizeHeight;
    else if (location & SIZEMARKLOCATION_BOTTOM)
        rect.bottom -= sizeHeight;
}

void AdjustPointForSizeMarks(POINT &p, int location)
{
    if (location & SIZEMARKLOCATION_LEFT)
        p.x -= sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        p.y -= sizeHeight;
}
