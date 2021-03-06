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

#include "global/precompiled.h"
#include "game/text.h"
#include "game/health.h"
#include "specific/output.h"
#include "global/vars.h"

void __cdecl T_InitPrint() {
	DisplayModeInfo(NULL);

	for( int i=0; i<64; ++i )
		TextInfoTable[i].flags = 0;

	TextStringCount = 0;
}

TEXT_STR_INFO *__cdecl T_Print(int x, int y, __int16 z, const char *str) {
	if( str == NULL || TextStringCount >= 64 )
		return NULL;

	for( int i=0; i<64; ++i ) {
		if( (TextInfoTable[i].flags & TIF_Active) == 0 ) {
			int stringLen = T_GetStringLen(str);
			if( stringLen > 64 )
				stringLen = 64; // TODO: remove. Useless check

			TextInfoTable[i].scaleW = PHD_ONE;
			TextInfoTable[i].scaleH = PHD_ONE;
			TextInfoTable[i].xPos = x * GetTextScaleH(PHD_ONE) / PHD_ONE;
			TextInfoTable[i].yPos = y * GetTextScaleV(PHD_ONE) / PHD_ONE;
			TextInfoTable[i].zPos = z;
			TextInfoTable[i].letterSpacing = 1;
			TextInfoTable[i].wordSpacing   = 6;

			TextInfoTable[i].textFlags = 0;
			TextInfoTable[i].outFlags  = 0;
			TextInfoTable[i].bgndFlags = 0;
			TextInfoTable[i].bgndSizeX = 0;
			TextInfoTable[i].bgndSizeY = 0;
			TextInfoTable[i].bgndOffX  = 0;
			TextInfoTable[i].bgndOffY  = 0;
			TextInfoTable[i].bgndOffZ  = 0;

			TextInfoTable[i].flags = TIF_Active;
			TextInfoTable[i].pString = TheStrings[i].str;

			memcpy(TheStrings[i].str, str, stringLen);
			++TextStringCount;

			return &TextInfoTable[i];
		}
	}
	return NULL;
}

void __cdecl T_ChangeText(TEXT_STR_INFO *textInfo, char *newString) {
	if( newString == NULL || textInfo == NULL || (textInfo->flags & TIF_Active) == 0 )
		return;

	if( T_GetStringLen(newString) >= 64 )
		newString[63] = 0;
	memcpy(textInfo->pString, newString, 64); // TODO: Fix it! This is unsafe!
}

void __cdecl T_RightAlign(TEXT_STR_INFO *textInfo, bool state) {
	if( textInfo == NULL)
		return;

	if( state ) {
		textInfo->flags |= TIF_Right;
	} else {
		textInfo->flags &= ~TIF_Right;
	}
}

void __cdecl T_BottomAlign(TEXT_STR_INFO *textInfo, bool state) {
	if( textInfo == NULL)
		return;

	if( state ) {
		textInfo->flags |= TIF_Bottom;
	} else {
		textInfo->flags &= ~TIF_Bottom;
	}
}

BOOL __cdecl T_RemovePrint(TEXT_STR_INFO *textInfo) {
	if( textInfo == NULL || (textInfo->flags & TIF_Active) == 0 )
		return false;

	textInfo->flags &= ~TIF_Active;
	--TextStringCount;
	return true;
}

__int16 __cdecl T_GetStringLen(const char *str) {
	// Calculates string length up to 64 chars including null terminator
	for( int i=0; i<64; ++i ) {
		if( str[i] == 0 )
			return i+1;
	}
	return 64;
}

void __cdecl T_DrawText() {
	for( int i=0; i<64; ++i ) {
		if( (TextInfoTable[i].flags & TIF_Active) != 0 )
			T_DrawThisText(&TextInfoTable[i]);
	}
}

DWORD __cdecl GetTextScaleH(DWORD baseScale) {
	DWORD renderWidth;
	DWORD interValue;

	renderWidth = GetRenderWidth();
	if( renderWidth < 640 )
		renderWidth = 640;

	interValue = renderWidth * PHD_ONE / 640;
	return (baseScale / PHD_HALF) * (interValue / PHD_HALF);
}

DWORD __cdecl GetTextScaleV(DWORD baseScale) {
	DWORD renderHeight;
	DWORD interValue;

	renderHeight = GetRenderHeight();
	if( renderHeight < 480 )
		renderHeight = 480;

	interValue = renderHeight * PHD_ONE / 480;
	return (baseScale / PHD_HALF) * (interValue / PHD_HALF);
}

/*
 * Inject function
 */
void Inject_Text() {
	INJECT(0x00440500, T_InitPrint);
	INJECT(0x00440530, T_Print);
	INJECT(0x00440640, T_ChangeText);
	INJECT(0x004407F0, T_RightAlign);
	INJECT(0x00440810, T_BottomAlign);
	INJECT(0x00440940, T_RemovePrint);
	INJECT(0x00440970, T_GetStringLen);
	INJECT(0x004409A0, T_DrawText);
	INJECT(0x00440F40, GetTextScaleH);
	INJECT(0x00440F80, GetTextScaleV);
}
