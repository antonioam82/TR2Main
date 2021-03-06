/*
 * Copyright (c) 2017 Michael Chaban. All rights reserved.
 * Original game is written by Core Design Ltd. in 1997.
 * Lara Croft and Tomb Raider are trademarks of Square Enix Ltd.
 *
 * This file is part of TR2Main.
 *
 * TR2Main is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TR2Main is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TR2Main.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TEXTURE_H_INCLUDED
#define TEXTURE_H_INCLUDED

#include "global/types.h"

/*
 * Function list
 */
void __cdecl CopyBitmapPalette(RGB *srcPal, BYTE *srcBitmap, int bitmapSize, RGB *destPal); // 0x00455990
BYTE __cdecl FindNearestPaletteEntry(RGB *palette, int red, int green, int blue, bool ignoreSysPalette); // 0x00455AD0
void __cdecl SyncSurfacePalettes(void *srcData, int width, int height, int srcPitch, RGB *srcPalette, void *dstData, int dstPitch, RGB *dstPalette, bool preserveSysPalette); // 0x00455BA0
int __cdecl CreateTexturePalette(RGB *pal); // 0x00455C50
int __cdecl GetFreePaletteIndex(); // 0x00455CE0
void __cdecl FreePalette(int paletteIndex); // 0x00455D00
void __cdecl SafeFreePalette(int paletteIndex); // 0x00455D30
int __cdecl CreateTexturePage(int width, int height, LPDIRECTDRAWPALETTE palette); // 0x00455D80
int __cdecl GetFreeTexturePageIndex(); // 0x00455DF0
bool __cdecl CreateTexturePageSurface(TEXPAGE_DESC *desc); // 0x00455E10
bool __cdecl TexturePageInit(TEXPAGE_DESC *page); // 0x00455EB0
LPDIRECT3DTEXTURE2 __cdecl Create3DTexture(LPDIRECTDRAWSURFACE3 surface); // 0x00456030
void __cdecl SafeFreeTexturePage(int pageIndex); // 0x00456060
void __cdecl FreeTexturePage(int pageIndex); // 0x00456080
void __cdecl TexturePageReleaseVidMemSurface(TEXPAGE_DESC *page); // 0x004560C0
void __cdecl FreeTexturePages(); // 0x00456100
bool __cdecl LoadTexturePage(int pageIndex, bool reset); // 0x00456130
bool __cdecl ReloadTextures(bool reset); // 0x004561E0
D3DTEXTUREHANDLE __cdecl GetTexturePageHandle(int pageIndex); // 0x00456220
int __cdecl AddTexturePage8(int width, int height, BYTE *pageBuffer, int palIndex); // 0x00456260
int __cdecl AddTexturePage16(int width, int height, BYTE *pageBuffer); // 0x00456360
HRESULT CALLBACK EnumTextureFormatsCallback(LPDDSURFACEDESC lpDdsd, LPVOID lpContext); // 0x00456500
HRESULT __cdecl EnumerateTextureFormats(); // 0x00456620
void __cdecl CleanupTextures(); // 0x00456650
bool __cdecl InitTextures(); // 0x00456660

#endif // TEXTURE_H_INCLUDED
