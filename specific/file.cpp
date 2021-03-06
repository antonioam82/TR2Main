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
#include "specific/file.h"
#include "game/invfunc.h"
#include "game/items.h"
#include "game/setup.h"
#include "specific/frontend.h"
#include "specific/hwr.h"
#include "specific/init.h"
#include "specific/init_sound.h"
#include "specific/texture.h"
#include "specific/winvid.h"
#include "global/vars.h"

#define REQ_SCRIPT_VERSION	(3)
#define DESCRIPTION_LENGTH	(256)
#define REQ_GAME_STR_COUNT	(89)
#define SPECIFIC_STR_COUNT	(41)

#define READ_STRINGS(count, lpTable, lpBuffer, lpRead, hFile, failLabel) { \
	if( NULL == ((lpTable)=(char **)GlobalAlloc(GMEM_FIXED, sizeof(char*) * (count))) || \
		!Read_Strings((count), (lpTable), (lpBuffer), (lpRead), (hFile)) ) goto failLabel; \
}

BOOL __cdecl ReadFileSync(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, LPDWORD lpnBytesRead, LPOVERLAPPED lpOverlapped) {
	ReadFileBytesCounter += nBytesToRead;

	if( ReadFileBytesCounter > 0x4000 ) {
		ReadFileBytesCounter = 0;
		WinVidSpinMessageLoop(0);
	}
	return ReadFile(hFile, lpBuffer, nBytesToRead, lpnBytesRead, lpOverlapped);
}

BOOL __cdecl LoadTexturePages(HANDLE hFile) {
	int i, pageCount;
	DWORD bytesRead;
	DWORD pageSize;
	LPVOID texPageBuffer;
	BYTE *texPagePtr;

	ReadFileSync(hFile, &pageCount, sizeof(pageCount), &bytesRead, NULL);

	// for software renderer read 8bit texture pages to GAME allocated buffer and skip 16bit pages
	if( SavedAppSettings.RenderMode == RM_Software ) {
		for( i=0; i<pageCount; ++i ) {
			if( TexturePageBuffer8[i] == NULL ) {
				TexturePageBuffer8[i] = game_malloc(256*256*1, 1); // Texture Pages
			}
			ReadFileSync(hFile, TexturePageBuffer8[i], 256*256*1, &bytesRead, NULL);
		}
		SetFilePointer(hFile, pageCount*(256*256*2), NULL, FILE_CURRENT);
		return TRUE;
	}

	// for hardware renderer do BPP check and load 8 bit or 16 bit texture pages to GLOBAL allocated memory and skip others
	pageSize = ( TextureFormat.bpp < 16 ) ? 256*256*1 : 256*256*2;
	texPageBuffer = GlobalAlloc(GMEM_FIXED, pageCount*pageSize);

	if( texPageBuffer == NULL )
		return FALSE;

	texPagePtr = (BYTE *)texPageBuffer;

	if( TextureFormat.bpp < 16 ) {
		// load 8 bit texture pages and skip 16 bit texture pages
		for( i=0; i<pageCount; ++i ) {
			ReadFileSync(hFile, texPagePtr, pageSize, &bytesRead, NULL);
			texPagePtr += pageSize;
		}
		SetFilePointer(hFile, pageCount*(256*256*2), NULL, FILE_CURRENT);
		HWR_LoadTexturePages(pageCount, texPageBuffer, GamePalette8);
	} else {
		// skip 8 bit texture pages and load 16 bit texture pages
		SetFilePointer(hFile, pageCount*(256*256*1), NULL, FILE_CURRENT);
		for( i=0; i<pageCount; ++i ) {
			ReadFileSync(hFile, texPagePtr, pageSize, &bytesRead, NULL);
			texPagePtr += pageSize;
		}
		HWR_LoadTexturePages(pageCount, texPageBuffer, NULL);
	}

	// for hardware renderer textures stored in videomemory so we may clean up system memory
	GlobalFree(texPageBuffer);
	HwrTexturePagesCount = pageCount;

	return TRUE;
}

BOOL __cdecl LoadRooms(HANDLE hFile) {
	DWORD bytesRead;
	DWORD dwCount;
	__int16 wCount;

	// Get number of rooms
	ReadFileSync(hFile, &RoomCount, sizeof(__int16), &bytesRead, NULL);
	if( RoomCount < 0 || RoomCount > 0x400 ) {
		lstrcpy(StringToShow, "LoadRoom(): Too many rooms");
		return FALSE;
	}

	// Allocate memory for room info
	RoomInfo = (ROOM_INFO *)game_malloc(sizeof(ROOM_INFO)*RoomCount, 0xB); // Room Infos
	if( RoomInfo == NULL ) {
		lstrcpy(StringToShow, "LoadRoom(): Could not allocate memory for rooms");
		return FALSE;
	}

	// For every room read info
	for( int i = 0; i < RoomCount; ++i ) {

		// Room position
		ReadFileSync(hFile, &RoomInfo[i].x, sizeof(int), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].z, sizeof(int), &bytesRead, NULL);
		RoomInfo[i].y = 0;

		// Room floor/ceiling
		ReadFileSync(hFile, &RoomInfo[i].minFloor, sizeof(int), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].maxCeiling, sizeof(int), &bytesRead, NULL);

		// Room mesh
		ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
		RoomInfo[i].data = (__int16 *)game_malloc(sizeof(__int16)*dwCount, 0xC); // Room Mesh
		ReadFileSync(hFile, RoomInfo[i].data, sizeof(__int16)*dwCount, &bytesRead, NULL);

		// Doors
		ReadFileSync(hFile, &wCount, sizeof(__int16), &bytesRead, NULL);
		if( wCount == 0 ) {
			RoomInfo[i].doors = NULL;
		} else {
			RoomInfo[i].doors = (DOOR_INFOS *)game_malloc(sizeof(__int16) + sizeof(DOOR_INFO)*wCount, 0xD); // Room Door
			RoomInfo[i].doors->wCount = wCount;
			ReadFileSync(hFile, &RoomInfo[i].doors->door, sizeof(DOOR_INFO)*wCount, &bytesRead, NULL);
		}

		// Room floor
		ReadFileSync(hFile, &RoomInfo[i].xSize, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].ySize, sizeof(__int16), &bytesRead, NULL);
		dwCount = RoomInfo[i].xSize * RoomInfo[i].ySize;
		RoomInfo[i].floor = (FLOOR_INFO *)game_malloc(sizeof(FLOOR_INFO)*dwCount, 0xE);// Room Floor
		ReadFileSync(hFile, RoomInfo[i].floor, sizeof(FLOOR_INFO)*dwCount, &bytesRead, NULL);

		// Room lights
		ReadFileSync(hFile, &RoomInfo[i].ambient1, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].ambient2, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].lightMode, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &RoomInfo[i].numLights, sizeof(__int16), &bytesRead, NULL);
		if( RoomInfo[i].numLights == 0 ) {
			RoomInfo[i].light = NULL;
		} else {
			RoomInfo[i].light = (LIGHT_INFO *)game_malloc(sizeof(LIGHT_INFO)*RoomInfo[i].numLights, 0xF); // Room Lights
			ReadFileSync(hFile, RoomInfo[i].light, sizeof(LIGHT_INFO)*RoomInfo[i].numLights, &bytesRead, NULL);
		}

		// Static mesh infos
		ReadFileSync(hFile, &RoomInfo[i].numMeshes, sizeof(__int16), &bytesRead, NULL);
		if( RoomInfo[i].numMeshes == 0 ) {
			RoomInfo[i].mesh = NULL;
		} else {
			RoomInfo[i].mesh = (MESH_INFO *)game_malloc(sizeof(MESH_INFO)*RoomInfo[i].numMeshes, 0x10); // Room Static Mesh Infos
			ReadFileSync(hFile, RoomInfo[i].mesh, sizeof(MESH_INFO)*RoomInfo[i].numMeshes, &bytesRead, NULL);
		}

		// Flipped (alternative) room
		ReadFileSync(hFile, &RoomInfo[i].flippedRoom, sizeof(__int16), &bytesRead, NULL);

		// Room flags
		ReadFileSync(hFile, &RoomInfo[i].flags, sizeof(__int16), &bytesRead, NULL);

		// Initialise some variables
		RoomInfo[i].boundActive = 0;
		RoomInfo[i].boundLeft = PhdWinMaxX;
		RoomInfo[i].boundTop = PhdWinMaxY;
		RoomInfo[i].boundRight = 0;
		RoomInfo[i].boundBotom = 0;
		RoomInfo[i].itemNumber = -1;
		RoomInfo[i].fxNumber = -1;
	}

	// Read floor data
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	FloorData = (__int16 *)game_malloc(sizeof(__int16)*dwCount, 0x11); // Floor Data
	ReadFileSync(hFile, FloorData, sizeof(__int16)*dwCount, &bytesRead, NULL);
	return TRUE;
}

void __cdecl AdjustTextureUVs(bool resetUvAdd) {
	DWORD i, j;
	int adjustment;
	int offset;
	BYTE uvFlags;
	PHD_UV *pUV;

	if( (SavedAppSettings.TexelAdjustMode == TAM_Always && SavedAppSettings.RenderMode == RM_Hardware ) ||
		(SavedAppSettings.TexelAdjustMode == TAM_BilinearOnly && SavedAppSettings.BilinearFiltering) )
	{
		adjustment = SavedAppSettings.LinearAdjustment; // LinearAdjustment default is 128. It can be changed in the registry only
	} else {
		adjustment = SavedAppSettings.NearestAdjustment; // NearestAdjustment default is 16. It can be changed in the registry only
	}

	offset = adjustment;
	if( !resetUvAdd )
		offset -= UvAdd;

	UvAdd = adjustment;

	for( i=0; i<TextureInfoCount; ++i ) {

		uvFlags = LabTextureUVFlags[i];
		pUV = PhdTextureInfo[i].uv;

		for( j=0; j<4; ++j ) {
			pUV[j].u += ((uvFlags & 1) ? -offset : offset);
			pUV[j].v += ((uvFlags & 2) ? -offset : offset);
			uvFlags >>= 2;
		}
	}
}

BOOL __cdecl LoadObjects(HANDLE hFile) {
	DWORD i, j;
	DWORD bytesRead;
	DWORD dwCount;
	DWORD animCount;
	DWORD animOffset;
	DWORD objNumber;
	UINT16 *uv;

	// Load mesh base data
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	Meshes = (__int16 *)game_malloc(sizeof(__int16)*dwCount, 3); // Meshes
	ReadFileSync(hFile, Meshes, sizeof(__int16)*dwCount, &bytesRead, NULL);

	// Load mesh pointers
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	MeshPtr = (__int16 **)game_malloc(sizeof(__int16 *)*dwCount, 2); // Mesh Pointers
	ReadFileSync(hFile, MeshPtr, sizeof(__int16 *)*dwCount, &bytesRead, NULL);

	// Remap mesh pointers
	for( i = 0; i < dwCount; ++i )
		MeshPtr[i] = (__int16 *)((DWORD)Meshes + (DWORD)MeshPtr[i]);

	// Load anims
	ReadFileSync(hFile, &animCount, sizeof(DWORD), &bytesRead, NULL);
	Anims = (ANIM_STRUCT *)game_malloc(sizeof(ANIM_STRUCT)*animCount, 4); // Anims
	ReadFileSync(hFile, Anims, sizeof(ANIM_STRUCT)*animCount, &bytesRead, NULL);

	// Load changes
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	AnimChanges = (CHANGE_STRUCT *)game_malloc(sizeof(CHANGE_STRUCT)*dwCount, 5); // Structs
	ReadFileSync(hFile, AnimChanges, sizeof(CHANGE_STRUCT)*dwCount, &bytesRead, NULL);

	// Load ranges
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	AnimRanges = (RANGE_STRUCT *)game_malloc(sizeof(RANGE_STRUCT)*dwCount, 6); // Ranges
	ReadFileSync(hFile, AnimRanges, sizeof(RANGE_STRUCT)*dwCount, &bytesRead, NULL);

	// Load commands
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	AnimCommands = (__int16 *)game_malloc(sizeof(__int16)*dwCount, 7); // Commands
	ReadFileSync(hFile, AnimCommands, sizeof(__int16)*dwCount, &bytesRead, NULL);

	// Load bones
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	AnimBones = (int *)game_malloc(sizeof(int)*dwCount, 8); // Bones
	ReadFileSync(hFile, AnimBones, sizeof(int)*dwCount, &bytesRead, NULL);

	// Load frames
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	AnimFrames = (__int16 *)game_malloc(sizeof(__int16)*dwCount, 9); // Frames
	ReadFileSync(hFile, AnimFrames, sizeof(__int16)*dwCount, &bytesRead, NULL);

	// Remap anim pointers
	for( i = 0; i < animCount; ++i )
		Anims[i].framePtr = (__int16 *)((DWORD)AnimFrames + (DWORD)Anims[i].framePtr);

	// Load animated objects
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	for( i = 0; i < dwCount; ++i ) {
		ReadFileSync(hFile, &objNumber, sizeof(DWORD), &bytesRead, NULL);
		ReadFileSync(hFile, &Objects[objNumber].nMeshes, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &Objects[objNumber].meshIndex, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &Objects[objNumber].boneIndex, sizeof(int), &bytesRead, NULL);
		ReadFileSync(hFile, &animOffset, sizeof(DWORD), &bytesRead, NULL);
		ReadFileSync(hFile, &Objects[objNumber].animIndex, sizeof(__int16), &bytesRead, NULL);
		Objects[objNumber].frameBase = (__int16 *)((DWORD)AnimFrames + animOffset);
		Objects[objNumber].flags |= 1;
	}

	// Initialise animated objects
	InitialiseObjects();

	// Load static objects
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	for( i = 0; i < dwCount; ++i ) {
		ReadFileSync(hFile, &objNumber, sizeof(DWORD), &bytesRead, NULL);
		ReadFileSync(hFile, &StaticObjects[objNumber].meshIndex, sizeof(__int16), &bytesRead, NULL);
		ReadFileSync(hFile, &StaticObjects[objNumber].drawBounds, sizeof(STATIC_BOUNDS), &bytesRead, NULL);
		ReadFileSync(hFile, &StaticObjects[objNumber].collisionBounds, sizeof(STATIC_BOUNDS), &bytesRead, NULL);
		ReadFileSync(hFile, &StaticObjects[objNumber].flags, sizeof(UINT16), &bytesRead, NULL);
	}

	// Load textures info
	ReadFileSync(hFile, &TextureInfoCount, sizeof(DWORD), &bytesRead, NULL);
	if( TextureInfoCount > 0x800 ) {
		lstrcpy(StringToShow, "Too many Textures in level");
		return FALSE;
	}
	ReadFileSync(hFile, PhdTextureInfo, sizeof(PHD_TEXTURE)*TextureInfoCount, &bytesRead, NULL);
	for( i = 0; i < TextureInfoCount; ++i ) {
		LabTextureUVFlags[i] = 0;
		uv = &PhdTextureInfo[i].uv[0].u;
		for( j = 0; j < 8; ++j ) {
			if( (uv[j] & 0x0080) != 0 ) {
				uv[j] |= 0x00FF;
				LabTextureUVFlags[i] |= (1 << j);
			} else {
				uv[j] &= 0xFF00;
			}
		}
	}
	AdjustTextureUVs(true);

	return TRUE;
}

BOOL __cdecl LoadSprites(HANDLE hFile) {
	DWORD bytesRead;
	DWORD objNumber;
	DWORD dwCount;

	// Load sprite infos
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	ReadFileSync(hFile, PhdSpriteInfo, sizeof(PHD_SPRITE)*dwCount, &bytesRead, NULL);

	// Assign sprites to objects
	ReadFileSync(hFile, &dwCount, sizeof(DWORD), &bytesRead, NULL);
	for( DWORD i = 0; i < dwCount; ++i ) {
		ReadFileSync(hFile, &objNumber, sizeof(DWORD), &bytesRead, NULL);
		if ( objNumber < ID_NUMBER_OBJECTS ) {
			ReadFileSync(hFile, &Objects[objNumber].nMeshes, sizeof(__int16), &bytesRead, NULL);
			ReadFileSync(hFile, &Objects[objNumber].meshIndex,  sizeof(__int16), &bytesRead, NULL);
			Objects[objNumber].flags |= 1;
		} else {
			objNumber -= ID_NUMBER_OBJECTS;
			SetFilePointer(hFile, sizeof(__int16), NULL, FILE_CURRENT); // StaticObjects don't have nMeshes (just one mesh)
			ReadFileSync(hFile, &StaticObjects[objNumber].meshIndex, sizeof(__int16), &bytesRead, NULL);
		}
	}
	return TRUE;
}

BOOL __cdecl LoadItems(HANDLE hFile) {
	DWORD itemsCount, bytesRead;

	ReadFileSync(hFile, &itemsCount, sizeof(DWORD), &bytesRead, NULL);
	if( itemsCount == 0 )
		return TRUE;

	if( itemsCount > 256 ) {
		lstrcpy(StringToShow, "LoadItems(): Too Many Items being Loaded!!");
		return FALSE;
	}

	Items = (ITEM_INFO *)game_malloc(sizeof(ITEM_INFO)*256, 0x12); // ITEMS!!
	if( Items == NULL ) {
		lstrcpy(StringToShow, "LoadItems(): Unable to allocate memory for 'items'");
		return FALSE;
	}
	LevelItemCount = itemsCount;
	InitialiseItemArray(256);

	for( DWORD i = 0; i < itemsCount; ++i ) {
		ReadFileSync(hFile, &Items[i].objectID,		sizeof(__int16),	&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].roomNumber,	sizeof(__int16),	&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].pos.x,		sizeof(int),		&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].pos.y,		sizeof(int),		&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].pos.z,		sizeof(int),		&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].pos.rotY,		sizeof(__int16),	&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].shade1,		sizeof(__int16),	&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].shade2,		sizeof(__int16),	&bytesRead, NULL);
		ReadFileSync(hFile, &Items[i].flags,		sizeof(UINT16),		&bytesRead, NULL);

		if( Items[i].objectID < 0 || Items[i].objectID >= ID_NUMBER_OBJECTS ) {
			wsprintf(StringToShow, "LoadItems(): Bad Object number (%d) on Item %d", Items[i].objectID, i);
			return FALSE;
		}
		InitialiseItem(i);
	}
	return TRUE;
}

BOOL __cdecl LoadDepthQ(HANDLE hFile) {
	int i, j;
	DWORD bytesRead;
	RGB paletteBuffer[256];

	ReadFileSync(hFile, DepthQTable, 32*sizeof(DEPTHQ_ENTRY), &bytesRead, NULL);

	for( i=0; i<32; ++i )
		DepthQTable[i].index[0] = 0;

	if( GameVid_IsWindowedVga ) {
		CopyBitmapPalette(GamePalette8, DepthQTable[0].index, 32*sizeof(DEPTHQ_ENTRY), paletteBuffer);
		SyncSurfacePalettes(DepthQTable, 256, 32, 256, GamePalette8, DepthQTable, 256, paletteBuffer, true);
		memcpy(GamePalette8, paletteBuffer, sizeof(RGB)*256);
		for( i=0; i<256; ++i ) {
			DepthQIndex[i] = S_COLOUR(GamePalette8[i].red, GamePalette8[i].green, GamePalette8[i].blue);
		}
	} else {
		memcpy(DepthQIndex, &DepthQTable[24], 256);
	}

	for( i=0; i<32; ++i ) {
		for( j=0; j<256; ++j ) {
			GouraudTable[j].index[i] = DepthQTable[i].index[j];
		}
	}

	IsWet = 0;
	for( i=0; i<256; ++i ) {
		WaterPalette[i].red   = GamePalette8[i].red   * 2 / 3;
		WaterPalette[i].green = GamePalette8[i].green * 2 / 3;
		WaterPalette[i].blue  = GamePalette8[i].blue;
	}

	return TRUE;
}

BOOL __cdecl LoadPalettes(HANDLE hFile) {
	DWORD bytesRead;

	ReadFileSync(hFile, GamePalette8, 256*sizeof(RGB), &bytesRead, NULL);

	GamePalette8[0].red = 0;
	GamePalette8[0].green = 0;
	GamePalette8[0].blue = 0;

	for( int i=1; i<256; ++i ) {
		GamePalette8[i].red   *= 4;
		GamePalette8[i].green *= 4;
		GamePalette8[i].blue  *= 4;
	}

	ReadFileSync(hFile, GamePalette16, 256*sizeof(PALETTEENTRY), &bytesRead, NULL);
	return TRUE;
}

BOOL __cdecl LoadCameras(HANDLE hFile) {
	DWORD bytesRead;

	ReadFileSync(hFile, &CameraCount, sizeof(DWORD), &bytesRead, NULL);
	if( CameraCount != 0 ) {
		Camera.fixed = (OBJECT_VECTOR *)game_malloc(sizeof(OBJECT_VECTOR)*CameraCount, 0x13); // Cameras
		if ( Camera.fixed == NULL ) {
			return FALSE;
		}
		ReadFileSync(hFile, Camera.fixed, sizeof(OBJECT_VECTOR)*CameraCount, &bytesRead, NULL);
	}
	return TRUE;
}

BOOL __cdecl LoadSoundEffects(HANDLE hFile) {
	DWORD bytesRead;

	ReadFileSync(hFile, &SoundEffectCount, sizeof(DWORD), &bytesRead, NULL);
	if( SoundEffectCount != 0 ) {
		SoundEffects = (OBJECT_VECTOR *)game_malloc(sizeof(OBJECT_VECTOR)*SoundEffectCount, 0x14);// Sound FX
		if( SoundEffects == NULL ) {
			return FALSE;
		}
		ReadFileSync(hFile, SoundEffects, sizeof(OBJECT_VECTOR)*SoundEffectCount, &bytesRead, NULL);
	}
	return TRUE;
}

BOOL __cdecl LoadBoxes(HANDLE hFile) {
	DWORD overlapsCount, bytesRead;

	// Load Boxes
	ReadFileSync(hFile, &BoxesCount, sizeof(DWORD), &bytesRead, NULL);
	Boxes = (BOX_INFO *)game_malloc(sizeof(BOX_INFO)*BoxesCount, 0x15); // Boxes
	ReadFileSync(hFile, Boxes, sizeof(BOX_INFO)*BoxesCount, &bytesRead, NULL);
	if( bytesRead != sizeof(BOX_INFO)*BoxesCount ) {
		lstrcpy(StringToShow, "LoadBoxes(): Unable to load boxes");
		return FALSE;
	}

	// Load Overlaps
	ReadFileSync(hFile, &overlapsCount, sizeof(DWORD), &bytesRead, NULL);
	Overlaps = (UINT16 *)game_malloc(sizeof(UINT16)*overlapsCount, 0x16); // Overlaps
	ReadFileSync(hFile, Overlaps, sizeof(UINT16)*overlapsCount, &bytesRead, NULL);
	if( bytesRead != sizeof(UINT16)*overlapsCount ) {
		lstrcpy(StringToShow, "LoadBoxes(): Unable to load box overlaps");
		return FALSE;
	}

	// Load GroundZones and FlyZones
	for( int i=0; i<2; ++i ) {
		for( int j=0; j<4; ++j ) {
			if( (j == 2) ||
				(j == 1 && (Objects[ID_SPIDER].flags & 1) == 0 && (Objects[ID_SKIDOO_ARMED].flags & 1) == 0) ||
				(j == 3 && (Objects[ID_YETI].flags & 1) == 0 && (Objects[ID_WORKER3].flags & 1) == 0) )
			{
				SetFilePointer(hFile, sizeof(__int16)*BoxesCount, NULL, FILE_CURRENT); // skip some GroundZones
				continue;
			}

			GroundZones[j*2+i] = (__int16 *)game_malloc(sizeof(__int16)*BoxesCount, 0x17); // GroundZone
			ReadFileSync(hFile, GroundZones[j*2+i], sizeof(__int16)*BoxesCount, &bytesRead, NULL);
			if( bytesRead != sizeof(__int16)*BoxesCount ) {
				lstrcpy(StringToShow, "LoadBoxes(): Unable to load 'ground_zone'");
				return FALSE;
			}
		}
		FlyZones[i] = (__int16 *)game_malloc(sizeof(__int16)*BoxesCount, 0x18); // FlyZone
		ReadFileSync(hFile, FlyZones[i], sizeof(__int16)*BoxesCount, &bytesRead, NULL);
		if( bytesRead != sizeof(__int16)*BoxesCount ) {
			lstrcpy(StringToShow, "LoadBoxes(): Unable to load 'fly_zone'");
			return FALSE;
		}
	}
	return TRUE;
}

BOOL __cdecl LoadAnimatedTextures(HANDLE hFile) {
	DWORD animTexCount, bytesRead;

	ReadFileSync(hFile, &animTexCount, sizeof(DWORD), &bytesRead, NULL);
	AnimatedTextureRanges = (__int16 *)game_malloc(sizeof(__int16)*animTexCount, 0x19); // Animating Texture Ranges
	ReadFileSync(hFile, AnimatedTextureRanges, sizeof(__int16)*animTexCount, &bytesRead, NULL);
	return TRUE;
}

BOOL __cdecl LoadCinematic(HANDLE hFile) {
	DWORD bytesRead;

	ReadFileSync(hFile, &CineFramesCount, sizeof(__int16), &bytesRead, NULL);
	if( CineFramesCount != 0 ) {
		CineFrames = (CINE_FRAME_INFO *)game_malloc(sizeof(CINE_FRAME_INFO)*CineFramesCount, 0x1A); // Cinematic Frames
		ReadFileSync(hFile, CineFrames, sizeof(CINE_FRAME_INFO)*CineFramesCount, &bytesRead, NULL);
		IsCinematicLoaded = TRUE;
	} else {
		IsCinematicLoaded = FALSE;
	}
	return TRUE;
}

BOOL __cdecl LoadDemo(HANDLE hFile) {
	DWORD bytesRead;
	__int16 demoSize;

	DemoCount = 0;
	DemoPtr = game_malloc(36000, 0x1B); // LoadDemo Buffer
	ReadFileSync(hFile, &demoSize, sizeof(__int16), &bytesRead, NULL);
	if( demoSize != 0 ) {
		ReadFileSync(hFile, DemoPtr, demoSize, &bytesRead, NULL);
		IsDemoLoaded = TRUE;
	} else {
		IsDemoLoaded = FALSE;
	}
	return TRUE;
}

void __cdecl LoadDemoExternal(LPCTSTR levelName) {
	HANDLE hFile;
	DWORD bytesRead = 0;
	char fileName[80] = {0};

	strcpy(fileName, levelName);
	ChangeFileNameExtension(fileName, "DEM");
	hFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE ) {
		ReadFileSync(hFile, DemoPtr, 36000, &bytesRead, NULL);
		IsDemoLoaded = ( bytesRead > 0 );
		CloseHandle(hFile);
	}
}

BOOL __cdecl LoadSamples(HANDLE hFile) {
	int i, j;
	DWORD bytesRead;
	HANDLE hSfxFile;
	LPCTSTR sfxFileName;
	DWORD dataSize;
	LPVOID waveData;
	int sampleCount;
	WAVEPCM_HEADER waveHeader;
	LPWAVEFORMATEX waveFormat;
	int sampleIndexes[500];

	SoundIsActive = FALSE;
	if( !WinSndIsSoundEnabled() ) {
		return TRUE;
	}
	WinSndFreeAllSamples();

	// Load Sample Lut
	ReadFileSync(hFile, SampleLut, sizeof(__int16)*370, &bytesRead, NULL);

	// Load Sample Infos
	ReadFileSync(hFile, &SampleInfoCount, sizeof(DWORD), &bytesRead, NULL);
	if( SampleInfoCount == 0 ) {
		return FALSE;
	}
	SampleInfos = (SAMPLE_INFO *)game_malloc(sizeof(SAMPLE_INFO)*SampleInfoCount, 0x23); // Sample Infos
	ReadFileSync(hFile, SampleInfos, sizeof(SAMPLE_INFO)*SampleInfoCount, &bytesRead, NULL);

	// Load Samples Count
	ReadFileSync(hFile, &sampleCount, sizeof(int), &bytesRead, NULL);
	if( sampleCount == 0 ) {
		return FALSE;
	}

	// Load Samples Indexes
	ReadFileSync(hFile, sampleIndexes, sizeof(DWORD)*sampleCount, &bytesRead, NULL);

	// Open SFX file
	sfxFileName = GetFullPath("data\\main.sfx");
	hSfxFile = CreateFile(sfxFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hSfxFile == INVALID_HANDLE_VALUE ) {
		wsprintf(StringToShow, "Could not open MAIN.SFX file");
		return FALSE;
	}

	for( i=0, j=0; i < sampleCount; ++j ) {
		ReadFileSync(hSfxFile, &waveHeader, sizeof(WAVEPCM_HEADER), &bytesRead, NULL);

		if( waveHeader.dwRiffChunkID != 0x46464952 || // "RIFF"
			waveHeader.dwFormat != 0x45564157 || // "WAVE"
			waveHeader.dwDataSubchunkID != 0x61746164 ) // "data"
		{
			CloseHandle(hSfxFile);
			return FALSE;
		}

		dataSize = (waveHeader.dwDataSubchunkSize + 1) & ~1; // aligned data size
		waveFormat = (LPWAVEFORMATEX)&waveHeader.wFormatTag;
		waveFormat->cbSize = 0;

		if( sampleIndexes[i] == j ) {
			waveData = game_malloc(dataSize, 0x24); // Samples
			ReadFileSync(hSfxFile, waveData, dataSize, &bytesRead, NULL);
			if( !WinSndMakeSample(i, waveFormat, waveData, dataSize) ) {
				CloseHandle(hSfxFile);
				return FALSE;
			}
			game_free(dataSize);
			++i;
		} else {
			SetFilePointer(hSfxFile, dataSize, NULL, FILE_CURRENT);
		}
	}
	CloseHandle(hSfxFile);
	SoundIsActive = TRUE;
	return TRUE;
}

void __cdecl ChangeFileNameExtension(char *fileName, const char *fileExt) {
	char *fileNamePtr = fileName;

	for( ; *fileNamePtr; ++fileNamePtr ) {
		if( *fileNamePtr == '.' )
			break;
	}

	fileNamePtr[0] = '.';
	fileNamePtr[1] = fileExt[0];
	fileNamePtr[2] = fileExt[1];
	fileNamePtr[3] = fileExt[2];
	fileNamePtr[4] = 0;
}

LPCTSTR __cdecl GetFullPath(LPCTSTR fileName) {
	static char fullPathBuffer[136];
#if defined FEATURE_NOCD_DATA
	wsprintf(fullPathBuffer, ".\\%s", fileName);
#else // !FEATURE_NOCD_DATA
	wsprintf(fullPathBuffer, "%c:\\%s", DriveLetter, fileName);
#endif // FEATURE_NOCD_DATA
	return fullPathBuffer;
}

BOOL __cdecl SelectDrive() {
	HANDLE hFile;
	DWORD driveBitMask;
	char fileName[] = "D:\\data\\legal.pcx";
	char driveName[] = "A:\\";

	DriveLetter = 'A';
	for( driveBitMask = GetLogicalDrives(); driveBitMask; driveBitMask >>= 1 ) {
		if( (driveBitMask & 1) != 0 ) {
			driveName[0] = DriveLetter;
			if( GetDriveType(driveName) == DRIVE_CDROM ) {
				fileName[0] = DriveLetter;
				hFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if( hFile != INVALID_HANDLE_VALUE ) {
					CloseHandle(hFile);
					return TRUE;
				}
			}
		}
		++DriveLetter;
	}
	return FALSE;
}

BOOL __cdecl LoadLevel(LPCTSTR fileName, int levelID) {
	BOOL result = FALSE;
	LPCTSTR fullPath;
	HANDLE hFile;
	DWORD reserved;
	DWORD bytesRead;
	int levelVersion;

	fullPath = GetFullPath(fileName);
	strcpy(LevelFileName, fullPath);
	init_game_malloc();

	hFile = CreateFile(fullPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN|FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) {
		wsprintf(StringToShow, "LoadLevel(): Could not open %s (level %d)", fullPath, levelID);
		return FALSE;
	}

	ReadFileSync(hFile, &levelVersion, sizeof(levelVersion), &bytesRead, NULL);
	if( levelVersion != REQ_LEVEL_VERSION ) {
		if( levelVersion < REQ_LEVEL_VERSION )
			wsprintf(StringToShow, "FATAL: Level %d (%s) is OUT OF DATE (version %d). COPY NEW EDITOR", levelID, fullPath, fileName);
		else
			wsprintf(StringToShow, "FATAL: Level %d (%s) requires a new TOMB2.EXE (version %d) to run", levelID, fullPath, fileName);
		goto EXIT;
	}

	LevelFilePalettesOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
	if( !LoadPalettes(hFile) ) {
		goto EXIT;
	}

	LevelFileTexPagesOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
	if( !LoadTexturePages(hFile) ) {
		goto EXIT;
	}

	ReadFileSync(hFile, &reserved, sizeof(reserved), &bytesRead, NULL);
	if( !LoadRooms(hFile) ||
		!LoadObjects(hFile) ||
		!LoadSprites(hFile) ||
		!LoadCameras(hFile) ||
		!LoadSoundEffects(hFile) ||
		!LoadBoxes(hFile) ||
		!LoadAnimatedTextures(hFile) ||
		!LoadItems(hFile) )
	{
		goto EXIT;
	}

	LevelFileDepthQOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
	if( !LoadDepthQ(hFile) ||
		!LoadCinematic(hFile) ||
		!LoadDemo(hFile) ||
		!LoadSamples(hFile) )
	{
		goto EXIT;
	}

	LoadDemoExternal(fullPath);
	result = TRUE;

EXIT :
	CloseHandle(hFile);
	return result;
}

BOOL __cdecl S_LoadLevelFile(LPCTSTR fileName, int levelID) {
	S_UnloadLevelFile();
	return LoadLevel(fileName, levelID);
}

void __cdecl S_UnloadLevelFile() {
	if( SavedAppSettings.RenderMode == RM_Hardware ) {
		HWR_FreeTexturePages();
	}
	memset(TexturePageBuffer8, 0, sizeof(LPVOID)*32);
	*LevelFileName = 0;
	TextureInfoCount = 0;
}

void __cdecl S_AdjustTexelCoordinates() {
	if( TextureInfoCount ) {
		AdjustTextureUVs(false);
	}
}

BOOL __cdecl S_ReloadLevelGraphics(BOOL reloadPalettes, BOOL reloadTexPages) {
	HANDLE hFile;

	if( *LevelFileName ) {
		hFile = CreateFile(LevelFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if( hFile == INVALID_HANDLE_VALUE )
			return FALSE;

		if( reloadPalettes && SavedAppSettings.RenderMode == RM_Software ) {
			SetFilePointer(hFile, LevelFilePalettesOffset, NULL, FILE_BEGIN);
			LoadPalettes(hFile);
			SetFilePointer(hFile, LevelFileDepthQOffset, NULL, FILE_BEGIN);
			LoadDepthQ(hFile);
		}

		if( reloadTexPages ) {
			if( SavedAppSettings.RenderMode == RM_Hardware )
				HWR_FreeTexturePages();
			SetFilePointer(hFile, LevelFileTexPagesOffset, NULL, FILE_BEGIN);
			LoadTexturePages(hFile);
		}
		CloseHandle(hFile);
	}

	if( reloadPalettes )
		InitColours();

	return TRUE;
}

BOOL __cdecl Read_Strings(DWORD dwCount, char **stringTable, char **stringBuffer, LPDWORD lpBufferSize, HANDLE hFile) {
	DWORD i, bytesRead;
	UINT16 bufferSize;
	UINT16 offsets[200]; // buffer for offsets

	ReadFileSync(hFile, offsets, sizeof(UINT16) * dwCount, &bytesRead, NULL);
	ReadFileSync(hFile, &bufferSize, sizeof(bufferSize), &bytesRead, NULL);

	*lpBufferSize = bufferSize;
	*stringBuffer = (char *)GlobalAlloc(GMEM_FIXED, bufferSize);

	if( *stringBuffer == NULL )
		return FALSE;

	ReadFileSync(hFile, *stringBuffer, bufferSize, &bytesRead, NULL);
	if( (GF_GameFlow.flags & GFF_UseSecurityTag) != 0 ) {
		for( i = 0; i < bufferSize; ++i )
			(*stringBuffer)[i] ^= GF_GameFlow.cypherCode;
	}

	for( i=0; i < dwCount; ++i ) {
		stringTable[i] = &(*stringBuffer)[offsets[i]];
	}
	return TRUE;
}

BOOL __cdecl S_LoadGameFlow(LPCTSTR fileName) {
	DWORD scriptVersion;
	char scriptDescription[DESCRIPTION_LENGTH];
	UINT16 offsets[200]; // buffer for offsets
	DWORD bytesRead;
	UINT16 flowSize, scriptSize, gameStringsCount;
	BOOL result = FALSE;
	LPCTSTR filePath = GetFullPath(fileName);
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if( hFile == INVALID_HANDLE_VALUE )
		return FALSE;

	ReadFileSync(hFile, &scriptVersion, sizeof(DWORD), &bytesRead, NULL);
	if( scriptVersion != REQ_SCRIPT_VERSION )
		goto CLEANUP;

	ReadFileSync(hFile, &scriptDescription, DESCRIPTION_LENGTH, &bytesRead, NULL);
	ReadFileSync(hFile, &flowSize, sizeof(flowSize), &bytesRead, NULL);
	if( flowSize != sizeof(GAME_FLOW) )
		goto CLEANUP;

	ReadFileSync(hFile, &GF_GameFlow, flowSize, &bytesRead, NULL);

	READ_STRINGS(GF_GameFlow.num_Levels,	GF_LevelNamesStringTable,	&GF_LevelNamesStringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Pictures,	GF_PictureFilesStringTable,	&GF_PictureFilesStringBuffer,	&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Titles,	GF_TitleFilesStringTable,	&GF_TitleFilesStringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Fmvs,		GF_FmvFilesStringTable,		&GF_FmvFilesStringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_LevelFilesStringTable,	&GF_LevelFilesStringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Cutscenes,	GF_CutsFilesStringTable,	&GF_CutsFilesStringBuffer,		&bytesRead, hFile, CLEANUP);

	ReadFileSync(hFile, offsets, sizeof(UINT16) * (GF_GameFlow.num_Levels + 1), &bytesRead, NULL);
	ReadFileSync(hFile, &scriptSize, sizeof(scriptSize), &bytesRead, NULL);

	GF_ScriptBuffer = (__int16 *)GlobalAlloc(GMEM_FIXED, scriptSize);
	if( GF_ScriptBuffer == NULL )
		goto CLEANUP;

	ReadFileSync(hFile, GF_ScriptBuffer, scriptSize, &bytesRead, NULL);
	for( int i=0; i < (GF_GameFlow.num_Levels + 1); ++i ) {
		GF_ScriptTable[i] = &GF_ScriptBuffer[offsets[i+1]/2];
	}

	if( GF_GameFlow.num_Demos > 0 )
		ReadFileSync(hFile, GF_DemoLevels, sizeof(UINT16) * GF_GameFlow.num_Demos, &bytesRead, NULL);

	ReadFileSync(hFile, &gameStringsCount, sizeof(gameStringsCount), &bytesRead, NULL);
	if( gameStringsCount != REQ_GAME_STR_COUNT )
		goto CLEANUP;

	// game and platform specific strings
	READ_STRINGS(gameStringsCount,			GF_GameStringTable,			&GF_GameStringBuffer,			&bytesRead, hFile, CLEANUP);
	READ_STRINGS(SPECIFIC_STR_COUNT,		GF_SpecificStringTable,		&GF_SpecificStringBuffer,		&bytesRead, hFile, CLEANUP);
	// puzzle strings
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Puzzle1StringTable,		&GF_Puzzle1StringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Puzzle2StringTable,		&GF_Puzzle2StringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Puzzle3StringTable,		&GF_Puzzle3StringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Puzzle4StringTable,		&GF_Puzzle4StringBuffer,		&bytesRead, hFile, CLEANUP);
	// pickup strings
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Pickup1StringTable,		&GF_Pickup1StringBuffer,		&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Pickup2StringTable,		&GF_Pickup2StringBuffer,		&bytesRead, hFile, CLEANUP);
	// key strings
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Key1StringTable,			&GF_Key1StringBuffer,			&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Key2StringTable,			&GF_Key2StringBuffer,			&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Key3StringTable,			&GF_Key3StringBuffer,			&bytesRead, hFile, CLEANUP);
	READ_STRINGS(GF_GameFlow.num_Levels,	GF_Key4StringTable,			&GF_Key4StringBuffer,			&bytesRead, hFile, CLEANUP);

	result = TRUE;

CLEANUP:
	CloseHandle(hFile);
	return result;
}

/*
 * Inject function
 */
void Inject_File() {
	INJECT(0x00449980, ReadFileSync);
	INJECT(0x004499D0, LoadTexturePages);
	INJECT(0x00449B60, LoadRooms);
	INJECT(0x00449F00, AdjustTextureUVs);
	INJECT(0x00449FA0, LoadObjects);
	INJECT(0x0044A520, LoadSprites);
	INJECT(0x0044A660, LoadItems);
	INJECT(0x0044A840, LoadDepthQ);
	INJECT(0x0044A9D0, LoadPalettes);
	INJECT(0x0044AA50, LoadCameras);
	INJECT(0x0044AAB0, LoadSoundEffects);
	INJECT(0x0044AB10, LoadBoxes);
	INJECT(0x0044AD40, LoadAnimatedTextures);
	INJECT(0x0044ADA0, LoadCinematic);
	INJECT(0x0044AE20, LoadDemo);
	INJECT(0x0044AEB0, LoadDemoExternal);
	INJECT(0x0044AF50, LoadSamples);
	INJECT(0x0044B1C0, ChangeFileNameExtension);
	INJECT(0x0044B200, GetFullPath);
	INJECT(0x0044B230, SelectDrive);
	INJECT(0x0044B310, LoadLevel);
	INJECT(0x0044B560, S_LoadLevelFile);
	INJECT(0x0044B580, S_UnloadLevelFile);
	INJECT(0x0044B5B0, S_AdjustTexelCoordinates);
	INJECT(0x0044B5D0, S_ReloadLevelGraphics);
	INJECT(0x0044B6A0, Read_Strings);
	INJECT(0x0044B770, S_LoadGameFlow);
}
