#include "common.h"

#include "audio.h"
#include "dma.h"
#include "entity.h"
#include "gamemode.h"
#include "hud.h"
#include "joy.h"
#include "kanji.h"
#include "memory.h"
#include "player.h"
#include "resources.h"
#include "sheet.h"
#include "system.h"
#include "tables.h"
#include "tsc.h"
#include "vdp.h"

#include "window.h"

// Window location
#define WINDOW_X1 1
#define WINDOW_X2 38
#define WINDOW_Y1 (pal_mode ? 21 : 20)
#define WINDOW_Y2 (pal_mode ? 28 : 27)
// Text area location within window
#define TEXT_X1 (WINDOW_X1 + 1)
#define TEXT_X2 (WINDOW_X2 - 1)
#define TEXT_Y1 (WINDOW_Y1 + 1)
#define TEXT_Y2 (WINDOW_Y2 - 1)
#define TEXT_X1_FACE (WINDOW_X1 + 8)
// On top
#define WINDOW_Y1_TOP 0
#define WINDOW_Y2_TOP 7
#define TEXT_Y1_TOP (WINDOW_Y1_TOP + 1)
#define TEXT_Y2_TOP (WINDOW_Y2_TOP - 1)
// Prompt window location
#define PROMPT_X 27
#define PROMPT_Y 17

#define ITEM_Y_START	(SCREEN_HALF_H + 128)
#define ITEM_Y_END		(SCREEN_HALF_H + 12 + 128)

const uint8_t ITEM_PAL[40] = {
	0, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 1, 1, 0, 1, 1, 1,
	0, 1, 0, 1, 0, 0, 0, 0,
	1, 0, 0, 1, 0, 1, 1, 1,
};

uint8_t windowOpen = FALSE;
uint16_t showingFace = 0;

uint8_t textMode = TM_NORMAL;

uint8_t windowText[3][36];
uint16_t jwindowText[2][18];
uint8_t textRow, textColumn;
uint8_t windowTextTick = 0;
uint8_t spaceCounter = 0, spaceOffset = 0;

uint8_t promptShowing = FALSE;
uint8_t promptAnswer = TRUE;
VDPSprite promptSpr[2], handSpr;

uint16_t showingItem = 0;

uint8_t blinkTime = 0;

void window_clear_text();
void window_draw_face();

void window_open(uint8_t mode) {
	mapNameTTL = 0; // Hide map name to avoid tile conflict
	window_clear_text();
	if(cfg_language == LANG_JA) {
		const uint32_t *data = &TS_MsgFont.tiles[('_' - 0x20) << 3];
		vdp_tiles_load_from_rom(data, (0xB000 >> 5) + 3 + (29 << 2), 1);
	}
	textRow = textColumn = 0;
	
	windowOnTop = mode;
	if(mode) hud_hide();
	uint16_t wy1 = mode ? WINDOW_Y1_TOP : WINDOW_Y1,
		wy2 = mode ? WINDOW_Y2_TOP : WINDOW_Y2,
		ty1 = mode ? TEXT_Y1_TOP : TEXT_Y1,
		ty2 = mode ? TEXT_Y2_TOP : TEXT_Y2;
	
	vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(0), WINDOW_X1, wy1);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(1), TEXT_X1, wy1, 36, 1, 0);
	vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(2), WINDOW_X2, wy1);
	for(uint8_t y = ty1; y <= ty2; y++) {
        vdp_map_xy(VDP_PLAN_W, 0, 0, y);
		vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(3), WINDOW_X1, y);
		vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), TEXT_X1, y, 36, 1, 0);
		vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(5), WINDOW_X2, y);
        vdp_map_xy(VDP_PLAN_W, 0, 39, y);
	}
	vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(6), WINDOW_X1, wy2);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(7), TEXT_X1, wy2, 36, 1, 0);
	vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(8), WINDOW_X2, wy2);
	
	if(!paused) {
		if(showingFace > 0) window_draw_face(showingFace);
		vdp_set_window(0, mode ? 8 : (pal_mode ? 245 : 244));
	} else showingFace = 0;

    linesSinceLastNOD = 0;
	windowOpen = TRUE;
}

uint8_t window_is_open() {
	return windowOpen;
}

void window_clear() {
	uint8_t x = showingFace ? TEXT_X1_FACE : TEXT_X1;
	uint8_t y = windowOnTop ? TEXT_Y1_TOP : TEXT_Y1;
	uint8_t w = showingFace ? 29 : 36;
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), x, y,   w, 1, 0);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), x, y+1, w, 1, 0);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), x, y+2, w, 1, 0);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), x, y+3, w, 1, 0);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), x, y+4, w, 1, 0);
	window_clear_text();

    linesSinceLastNOD = 0;
    if(textMode == TM_MSG) textMode = TM_NORMAL;
}

void window_clear_text() {
	textRow = textColumn = spaceCounter = spaceOffset = 0;
	memset(windowText, ' ', 36*3);
	memset(jwindowText, '\0', 36*2);
}

void window_close() {
	if(!paused) {
	    vdp_set_window(0, 0);
	}
	showingItem = 0;
	windowOpen = FALSE;
	textMode = TM_NORMAL;
}

void window_set_face(uint16_t face, uint8_t open) {
	if(paused) return;
	if(open && !windowOpen) window_open(windowOnTop);
	showingFace = face;
	if(face > 0) {
		window_draw_face();
	} else {
		// Hack to clear face only
		//uint16_t y1 = windowOnTop ? TEXT_Y1_TOP : TEXT_Y1;
		//for(uint16_t y = y1; y < y1 + 6; y++) {
		//	for(uint16_t x = TEXT_X1; x < TEXT_X1 + 6; x++) {
		//		vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(4), x, y);
		//	}
		//}
		vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), TEXT_X1, 
				windowOnTop ? TEXT_Y1_TOP : TEXT_Y1, 6, 6, 0);
		//VDP_fillTileMapRect(VDP_PLAN_W, WINDOW_ATTR(4), TEXT_X1,
		//		windowOnTop ? TEXT_Y1_TOP : TEXT_Y1, 6, 6);
	}
}

void window_draw_char(uint8_t c) {
	if(c == '\n') {
		textRow++;
		textColumn = 0;
		spaceCounter = spaceOffset = 0;
		if(textRow > 2) {
			//if(textMode == TM_ALL) textMode = TM_NORMAL;
			window_scroll_text();
		} //else if(textMode == TM_LINE) {
		//	textMode = TM_NORMAL;
		//}
	} else {
		// Check if the line has leading spaces, and skip drawing a space occasionally,
		// so that the sign text will be centered
		if(textColumn == spaceCounter && c == ' ') {
			spaceCounter++;
		} else {
			spaceCounter = 0;
		}
		windowText[textRow][textColumn - spaceOffset] = c;
		// Figure out where this char is gonna go
		uint8_t msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
		msgTextX += textColumn - spaceOffset;
		uint8_t msgTextY = (windowOnTop ? TEXT_Y1_TOP:TEXT_Y1) + textRow * 2;
		// And draw it
		if(c >= 0x80) {
		    // Extended charset, you'll never guess where I put it
		    uint16_t index = (VDP_PLAN_W >> 5) + 3;
		    index += (c - 0x80) << 2;
            vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0, 1, 0, 0, index-4), msgTextX, msgTextY);
		} else {
		    // Low ASCII charset
            vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0, 1, 0, 0,
                    TILE_FONTINDEX + c - 0x20), msgTextX, msgTextY);
        }
		textColumn++;
		if(spaceCounter % 5 == 1 || spaceCounter == 2) spaceOffset++;
	}
}

static uint16_t getKanjiIndexForPos(uint8_t row, uint8_t col) {
	if(row) col += 18; // Second row
	col <<= 2; // 4 tiles per char
	if(col < 96) {
		return TILE_FONTINDEX + col;
	} else if(col < 96+28) {
		col -= 96;
		return (0xB000 >> 5) + 3 + 4 * col;
	} else if(col < 96+28+16) {
		return TILE_NAMEINDEX + (col - (96+28));
	} else {
		return TILE_NUMBERINDEX + (col - (96+28+16));
	}
}

void window_draw_jchar(uint8_t iskanji, uint16_t c) {
	if(vblank) aftervsync();
	vblank = 0;
	
	//vdp_set_display(FALSE);
	// For the 16x16 text, pretend the text array is [2][18]
	if(!iskanji && c == '\n') {
		textRow++;
		textColumn = 0;
		if(textRow > 1) {
			//if(textMode == TM_ALL) textMode = TM_NORMAL;
			window_scroll_jtext();
		} //else if(textMode == TM_LINE) {
		//	textMode = TM_NORMAL;
		//}
		//vdp_set_display(TRUE);
		//linesSinceLastNOD++;
		return;
	}
	// Don't let the text run off the end of the window
	if(textColumn > 18 - (showingFace ? 4 : 0)) {
		textRow++;
		textColumn = 0;
		if(textRow > 1) {
			//if(textMode == TM_ALL) textMode = TM_NORMAL;
			window_scroll_jtext();
		} //else if(textMode == TM_LINE) {
		//	textMode = TM_NORMAL;
		//}
	}
	if(iskanji) c += 0x100;
	jwindowText[textRow][textColumn] = c;
	// Figure out where this char is gonna go
	uint8_t msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
	msgTextX += textColumn * 2;
	uint8_t msgTextY = (windowOnTop ? TEXT_Y1_TOP:TEXT_Y1) + textRow * 3;
	// And draw it
	
	uint16_t vramIndex = getKanjiIndexForPos(textRow, textColumn);
	kanji_draw(VDP_PLAN_W, vramIndex, c, msgTextX, msgTextY, 2, FALSE);
	if(textColumn < 18 - (showingFace ? 4 : 0) && (iskanji || c != ' ')) {
		textColumn++;
	}
	//vdp_set_display(TRUE);
}

void window_scroll_text() {
	// Push bottom 2 rows to top
	for(uint8_t row = 0; row < 2; row++) {
		if(vblank) aftervsync(); // So we don't lag the music
		vblank = 0;
		
		uint8_t msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
		uint8_t msgTextY = (windowOnTop ? TEXT_Y1_TOP:TEXT_Y1) + row * 2;
		for(uint8_t col = 0; col < 36 - (showingFace > 0) * 8; col++) {
			windowText[row][col] = windowText[row + 1][col];
			uint8_t c = windowText[row][col];
            if(c >= 0x80) {
                // Extended charset, you'll never guess where I put it
                uint16_t index = (VDP_PLAN_W >> 5) + 3;
                index += (c - 0x80) << 2;
                vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0, 1, 0, 0, index-4), msgTextX, msgTextY);
            } else {
                vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0, 1, 0, 0,
                        TILE_FONTINDEX + c - 0x20), msgTextX, msgTextY);
            }
			msgTextX++;
		}
	}
	// Clear third row
	uint8_t msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
	uint8_t msgTextY = (windowOnTop ? TEXT_Y1_TOP:TEXT_Y1) + 4;
	uint8_t msgTextW = showingFace ? 29 : 36;
	memset(windowText[2], ' ', 36);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), msgTextX, msgTextY, msgTextW, 1, 0);
	// Reset to beginning of third row
	textRow = 2;
	textColumn = 0;
	spaceCounter = spaceOffset = 0;
}

void window_scroll_jtext() {
	// Push bottom row to top
	uint8_t msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
	uint8_t msgTextY = windowOnTop ? TEXT_Y1_TOP:TEXT_Y1;
	
	for(uint8_t col = 0; col < 18 - (showingFace ? 4 : 0); col++) {
		if(vblank) aftervsync(); // So we don't lag the music
		vblank = 0;
		
		jwindowText[0][col] = jwindowText[1][col];
		uint16_t vramIndex = getKanjiIndexForPos(0, col);
		if(jwindowText[0][col] == 0) {
			kanji_draw(VDP_PLAN_W, vramIndex, ' ', msgTextX, msgTextY, 2, FALSE);
		} else {
			kanji_draw(VDP_PLAN_W, vramIndex, jwindowText[0][col], msgTextX, msgTextY, 2, FALSE);
		}
		msgTextX += 2;
	}
	// Clear bottom row
	msgTextX = showingFace ? TEXT_X1_FACE : TEXT_X1;
	msgTextY = (windowOnTop ? TEXT_Y1_TOP:TEXT_Y1) + 3;
	uint8_t msgTextW = showingFace ? 28 : 36;
	memset(jwindowText[1], '\0', 36);
	vdp_map_fill_rect(VDP_PLAN_W, WINDOW_ATTR(4), msgTextX, msgTextY, msgTextW, 2, 0);
	// Reset to beginning of bottom row
	textRow = 1;
	textColumn = 0;
}

uint8_t window_get_textmode() {
	return textMode;
}

void window_set_textmode(uint8_t mode) {
	textMode = mode;
}

uint8_t window_tick() {
	if(textMode > 0) return TRUE;
	windowTextTick++;
	if(windowTextTick > 2 || (windowTextTick > 1 && (joystate&btn[cfg_btn_jump]))) {
		windowTextTick = 0;
		return TRUE;
	} else {
		return FALSE;
	}
}

void window_prompt_open() {
	promptAnswer = TRUE; // Yes is default
	sound_play(SND_MENU_PROMPT, 5);
	// Load hand sprite and move next to yes
	handSpr = (VDPSprite) {
		//.x = tile_to_pixel(PROMPT_X) - 4 + 128,
		//.y = tile_to_pixel(PROMPT_Y + 1) - 4 + 128,
		.size = SPRITE_SIZE(2, 2),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX)
	};
	int16_t cursor_x = (PROMPT_X << 3) - 9;
	if(!promptAnswer) cursor_x += cfg_language == LANG_JA ? 26 : 34; // "いいえ" starts more left than "No"
	int16_t cursor_y = (PROMPT_Y << 3) + 4;
	sprite_pos(handSpr, cursor_x, cursor_y);
	promptSpr[0] = (VDPSprite) {
		.x = tile_to_pixel(PROMPT_X) + 128,
		.y = tile_to_pixel(PROMPT_Y) + 128,
		.size = SPRITE_SIZE(4, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+4)
	};
	promptSpr[1] = (VDPSprite) {
		.x = tile_to_pixel(PROMPT_X) + 32 + 128,
		.y = tile_to_pixel(PROMPT_Y) + 128,
		.size = SPRITE_SIZE(4, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+16)
	};
	TILES_QUEUE(SPR_TILES(&SPR_Pointer,0,0), TILE_PROMPTINDEX, 4);
	const SpriteDefinition *spr = cfg_language == LANG_JA ? &SPR_J_Prompt : &SPR_Prompt;
	TILES_QUEUE(SPR_TILES(spr,0,0), TILE_PROMPTINDEX+4, 24);
}

void window_prompt_close() {
	window_clear();
}

uint8_t window_prompt_answer() {
	return promptAnswer;
}

uint8_t window_prompt_update() {
	if(joy_pressed(btn[cfg_btn_jump])) {
		sound_play(SND_MENU_SELECT, 5);
		window_prompt_close();
		return TRUE;
	} else if(joy_pressed(BUTTON_LEFT) || joy_pressed(BUTTON_RIGHT)) {
		promptAnswer = !promptAnswer;
		sound_play(SND_MENU_MOVE, 5);
		int16_t cursor_x = (PROMPT_X << 3) - 9;
		if(!promptAnswer) cursor_x += cfg_language == LANG_JA ? 26 : 34; // "いいえ" starts more left than "No"
		int16_t cursor_y = (PROMPT_Y << 3) + 4;
		sprite_pos(handSpr, cursor_x, cursor_y);
	}
vdp_sprite_add(&handSpr);
vdp_sprites_add(promptSpr, 2);
	return FALSE;
}

void window_draw_face() {
	vdp_tiles_load_from_rom(face_info[showingFace].tiles->tiles, TILE_FACEINDEX, face_info[showingFace].tiles->numTile);
	vdp_map_fill_rect(VDP_PLAN_W, TILE_ATTR(face_info[showingFace].palette, 1, 0, 0, TILE_FACEINDEX), 
			TEXT_X1, (windowOnTop ? TEXT_Y1_TOP : TEXT_Y1), 6, 6, 1);
	//uint16_t y1 = windowOnTop ? TEXT_Y1_TOP : TEXT_Y1;
	//uint16_t tile = TILE_ATTR(face_info[showingFace].palette, 1, 0, 0, TILE_FACEINDEX);
	//for(uint16_t y = y1; y < y1 + 6; y++) {
	//	for(uint16_t x = TEXT_X1; x < TEXT_X1 + 6; x++) {
	//		vdp_map_xy(VDP_PLAN_W, tile++, x, y);
	//	}
	//}
}

void window_show_item(uint16_t item) {
	showingItem = item;
	if(item == 0) return;
	// Wonky workaround to use either PAL_Sym or PAL_Main
	const SpriteDefinition *sprDef = &SPR_ItemImage;
	uint16_t pal = PAL1;
	if(ITEM_PAL[item]) {
		sprDef = &SPR_ItemImageG;
		pal = PAL0;
	}
	handSpr = (VDPSprite) {
		.x = SCREEN_HALF_W - 12 + 128,
		.y = ITEM_Y_START,
		.size = SPRITE_SIZE(3, 2),
		.attr = TILE_ATTR(pal,1,0,0,TILE_PROMPTINDEX)
	};
	promptSpr[0] = (VDPSprite) {
		.x = SCREEN_HALF_W - 24 + 128,
		.y = SCREEN_HALF_H + 8 + 128,
		.size = SPRITE_SIZE(3, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+6)
	};
	promptSpr[1] = (VDPSprite) {
		.x = SCREEN_HALF_W + 128,
		.y = SCREEN_HALF_H + 8 + 128,
		.size = SPRITE_SIZE(3, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+15)
	};
	TILES_QUEUE(SPR_TILES(sprDef,item,0), TILE_PROMPTINDEX, 6);
	TILES_QUEUE(SPR_TILES(&SPR_ItemWin,0,0), TILE_PROMPTINDEX+6, 18);
}

void window_show_weapon(uint16_t item) {
	showingItem = item;
	if(item == 0) return;
	handSpr = (VDPSprite) {
		.x = SCREEN_HALF_W - 8 + 128,
		.y = ITEM_Y_START,
		.size = SPRITE_SIZE(2, 2),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX)
	};
	promptSpr[0] = (VDPSprite) {
		.x = SCREEN_HALF_W - 24 + 128,
		.y = SCREEN_HALF_H + 8 + 128,
		.size = SPRITE_SIZE(3, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+6)
	};
	promptSpr[1] = (VDPSprite) {
		.x = SCREEN_HALF_W + 128,
		.y = SCREEN_HALF_H + 8 + 128,
		.size = SPRITE_SIZE(3, 3),
		.attr = TILE_ATTR(PAL0,1,0,0,TILE_PROMPTINDEX+15)
	};
	TILES_QUEUE(SPR_TILES(&SPR_ArmsImage,0,item), TILE_PROMPTINDEX, 6);
	TILES_QUEUE(SPR_TILES(&SPR_ItemWin,0,0), TILE_PROMPTINDEX+6, 18);
}

void window_update() {
	if(showingItem) {
		if(handSpr.y < ITEM_Y_END) handSpr.y++;
	    vdp_sprite_add(&handSpr);
	    vdp_sprites_add(promptSpr, 2);
	}
	if(tscState == TSC_WAITINPUT && textMode == TM_NORMAL) {
		uint8_t x = showingFace ? TEXT_X1_FACE : TEXT_X1;
		uint8_t y = windowOnTop ? TEXT_Y1_TOP : TEXT_Y1;
		uint16_t index;
		if(cfg_language == LANG_JA) {
			x += textColumn * 2;
			y += textRow * 3;
			index = (0xB000 >> 5) + 3 + (29 << 2);
		} else {
			x += textColumn - spaceOffset;
			y += textRow * 2;
			index = TILE_FONTINDEX - 0x20 + '_';
		}
        if(++blinkTime == 8) {
            vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0,1,0,0,index), x, y);
            if(cfg_language == LANG_JA) {
                vdp_map_xy(VDP_PLAN_W, TILE_ATTR(PAL0,1,0,0,index), x, y + 1);
            }
        } else if(blinkTime == 16) {
            vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(4), x, y);
            if(cfg_language == LANG_JA) {
                vdp_map_xy(VDP_PLAN_W, WINDOW_ATTR(4), x, y + 1);
            }
            blinkTime = 0;
        }
	}
}
