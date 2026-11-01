#pragma once

#include "stdafx.h"

// Menu icons are Segoe Fluent Icons glyphs (Segoe MDL2 Assets on Windows 10)
// rendered in the SnapView blue. Call ApplyMenuIcons with the point a menu
// opens at before showing it; the icons are re-rendered when that monitor's
// DPI differs from the last one. Both menus share the bitmaps, so each must
// be applied again before it is shown. GDI+ must be started before
// ApplyMenuIcons, and the menus must be destroyed before CleanupMenuIcons.

void ApplyMenuIcons(HMENU hMenu, POINT pt);
void CleanupMenuIcons();
