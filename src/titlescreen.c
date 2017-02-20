#include "gamemode.h"

#include <genesis.h>
#include "audio.h"
#include "input.h"
#include "system.h"
#include "common.h"
#include "resources.h"
#include "sprite.h"
#include "sheet.h"

#define OPTIONS		3
#define SAVES		22
#define ANIM_SPEED	7
#define ANIM_FRAMES	4

static const u16 cheat[] = {
	BUTTON_UP, BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT, NULL
};

u8 titlescreen_main() {
	u8 cheatEntry = 0, levelSelect = FALSE;
	u8 cursor = 0;
	u32 besttime = 0xFFFFFFFF;
	u8 tsong = SONG_TITLE;
	const SpriteDefinition *tsprite = &SPR_Quote;
	
	SYS_disableInts();
	VDP_setEnable(FALSE);
	VDP_resetScreen(); // This brings back the default SGDK font with transparency
	// No special scrolling for title screen
	VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
	VDP_setHorizontalScroll(PLAN_A, 0);
	VDP_setVerticalScroll(PLAN_A, 0);
	// Main palette
	VDP_setPalette(PAL0, PAL_Main.data);
	VDP_setPalette(PAL1, PAL_Main.data);
	VDP_setPaletteColor(PAL0, 0x444); // Gray background
	VDP_setPaletteColor(PAL0 + 8, 0x8CE); // Yellow text
	// PAL_Regu, for King and Toroko
	VDP_setPalette(PAL3, PAL_Regu.data);
	// Check save data, only enable continue if save data exists
	u8 sram_state = system_checkdata();
	if(sram_state == SRAM_VALID_SAVE) {
		VDP_drawText("Continue", 15, 14);
		cursor = 1;
		besttime = system_load_counter(); // 290.rec data
	}
	// Change character & song based on 290.rec value
	if(besttime <= 3*3000) {
		tsprite = &SPR_Sue;
		tsong = 2; // Safety
	} else if(besttime <= 4*3000) {
		tsprite = &SPR_King;
		tsong = 41; // White Stone Wall
	} else if(besttime <= 5*3000) {
		tsprite = &SPR_Toroko;
		tsong = 40; // Toroko's Theme
	} else if(besttime <= 6*3000) {
		tsprite = &SPR_Curly;
		tsong = 36; // Running Hell
	}
	// Load quote sprite
	SHEET_LOAD(tsprite, 4, 4, TILE_SHEETINDEX, 1, 0,1, 0,0, 0,2, 0,0);
	VDPSprite sprCursor = { 
		.attribut = TILE_ATTR_FULL(PAL1,0,0,1,TILE_SHEETINDEX),
		.size = SPRITE_SIZE(2,2)
	};
	u8 sprFrame = 0, sprTime = ANIM_SPEED;
	// Menu and version text
	VDP_drawText("New Game", 15, 12);
	VDP_drawText("Sound Test", 15, 16);
	// Debug
	{
		char vstr[40];
		sprintf(vstr, "Test Build - %s", __DATE__);
		VDP_drawText(vstr, 4, 26);
	}
	// Release
	//VDP_drawText("Mega Drive Version 0.2.1 2016.10", 4, 26);
	VDP_loadTileSet(&TS_Title, TILE_USERINDEX, TRUE);
	VDP_fillTileMapRectInc(PLAN_B,
			TILE_ATTR_FULL(PAL0,0,0,0,TILE_USERINDEX), 11, 3, 18, 4);
	VDP_fillTileMapRectInc(PLAN_B,
			TILE_ATTR_FULL(PAL0,0,0,0,TILE_USERINDEX + 18*4), 11, 23, 18, 2);
	VDP_setEnable(TRUE);
	SYS_enableInts();
	song_play(tsong);
	while(!joy_pressed(BUTTON_C) && !joy_pressed(BUTTON_START)) {
		input_update();
		if(!levelSelect) {
			if(joy_pressed(cheat[cheatEntry])) {
				cheatEntry++;
				if(cheat[cheatEntry] == NULL) {
					levelSelect = TRUE;
					sound_play(SND_COMPUTER_BEEP, 5);
				}
			} else {
				u16 mask = BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN | 
						BUTTON_A | BUTTON_B;
				//if(joy_pressed(mask)) cheatEntry = 0;
			}
		}
		if(joy_pressed(BUTTON_UP)) {
			if(cursor == 0) cursor = OPTIONS - 1;
			else cursor--;
			// Skip over "continue" if no save data
			if(sram_state != SRAM_VALID_SAVE && cursor == 1) cursor--;
			sound_play(SND_MENU_MOVE, 0);
		} else if(joy_pressed(BUTTON_DOWN)) {
			if(cursor == OPTIONS - 1) cursor = 0;
			else cursor++;
			// Skip over "continue" if no save data
			if(sram_state != SRAM_VALID_SAVE && cursor == 1) cursor++;
			sound_play(SND_MENU_MOVE, 0);
		}
		// Animate quote sprite
		if(--sprTime == 0) {
			sprTime = ANIM_SPEED;
			if(++sprFrame >= ANIM_FRAMES) sprFrame = 0;
			sprite_index(sprCursor, TILE_SHEETINDEX+sprFrame*4);
		}
		// Draw quote sprite at cursor position
		sprite_pos(sprCursor, 13*8-4, (12*8+cursor*16)-4);
		SYS_disableInts();
		DMA_doDma(DMA_VRAM, (u32)(&sprCursor), VDP_SPRITE_TABLE, 4, 2);
		SYS_enableInts();
		VDP_waitVSync();
	}
	if(levelSelect && (joystate&BUTTON_A) && joy_pressed(BUTTON_START)) {
		cursor = 0;
		input_update();
		SYS_disableInts();
		VDP_clearPlan(PLAN_A, TRUE);
		VDP_setPaletteColor(0, 0x222); // Darken background colors
		VDP_setPaletteColor(8, 0x468);
		VDP_setPaletteColor(12, 0x666);
		VDP_setPaletteColor(16 + 15, 0xAAA);
		static const char *levelStr[] = {
			" Mimiga Village",
			" Egg Corridor",
			" Grasstown",
			" Balfrog",
			" Sand Zone",
			" Toroko",
			" Labyrinth",
			" Booster",
			"=NORMAL END ROUTE=",
			" Core",
			" Waterway",
			" Empty Village",
			" Outer Wall",
			" Plantation",
			" Last Cave",
			" Balcony",
			"= BEST END ROUTE =",
			" Waterway 2",
			" Ma Pignon",
			" Last Cave 2",
			" Sacred Ground",
			" Ballos",
		};
		static const u16 tx = 11, ty = 2;
		for(u8 i = 0; i < SAVES; i++) {
			if(cursor == i) VDP_setTextPalette(PAL0);
			else VDP_setTextPalette(PAL1);
			VDP_drawText(levelStr[i], tx, 2+i);
		}
		SYS_enableInts();
		while(!joy_pressed(BUTTON_C) && !joy_pressed(BUTTON_START)) {
			input_update();
			if(joy_pressed(BUTTON_UP)) {
				VDP_setTextPalette(PAL1);
				VDP_drawText(levelStr[cursor], tx, ty + cursor);
				if(cursor == 0) cursor = SAVES - 1;
				else cursor--;
				if(cursor == 8 || cursor == 16) cursor--;
				VDP_setTextPalette(PAL0);
				VDP_drawText(levelStr[cursor], tx, ty + cursor);
				sound_play(SND_MENU_MOVE, 0);
			} else if(joy_pressed(BUTTON_DOWN)) {
				VDP_setTextPalette(PAL1);
				VDP_drawText(levelStr[cursor], tx, ty + cursor);
				if(cursor == SAVES - 1) cursor = 0;
				else cursor++;
				if(cursor == 8 || cursor == 16) cursor++;
				VDP_setTextPalette(PAL0);
				VDP_drawText(levelStr[cursor], tx, ty + cursor);
				sound_play(SND_MENU_MOVE, 0);
			}
			// Animate quote sprite
			if(--sprTime == 0) {
				sprTime = ANIM_SPEED;
				if(++sprFrame >= ANIM_FRAMES) sprFrame = 0;
				sprite_index(sprCursor, TILE_SHEETINDEX+sprFrame*4);
			}
			// Draw quote sprite at cursor position
			sprite_pos(sprCursor, (tx-2)*8-4, (ty*8+cursor*8)-5);
			SYS_disableInts();
			DMA_doDma(DMA_VRAM, (u32)(&sprCursor), VDP_SPRITE_TABLE, 4, 2);
			SYS_enableInts();
			VDP_waitVSync();
		}
		VDP_setTextPalette(PAL0);
		cursor += 4;
	}
	song_stop();
	sound_play(SND_MENU_SELECT, 0);
	//VDP_waitVSync();
	return cursor;
}
