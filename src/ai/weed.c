#include "ai.h"

#include <genesis.h>
#include "audio.h"
#include "player.h"
#include "stage.h"
#include "tables.h"
#include "tsc.h"
#include "effect.h"
#include "camera.h"
#include "system.h"
#include "sheet.h"
#include "resources.h"

void ai_jelly(Entity *e) {
	ANIMATE(e, 16, 0,1,2,3,4,5);
	switch(e->state) {
		case 0:
		{
			e->enableSlopes = FALSE;
			e->timer = random() % TIME(20);
			e->x_mark = e->x;
			e->y_mark = e->y;
			if(e->eflags & NPC_OPTION2) e->dir = 1;
			MOVE_X(SPEED(0x100));
			e->state = 1;
		}
		/* no break */
		case 1:
		{
			if(e->timer == 0) {
				e->state = 10;
			} else {
				e->timer--;
				break;
			}
		}
		/* no break */
		case 10:
		{
			if(++e->timer > TIME(10)) {
				e->timer = 0;
				e->state = 11;
			}
		}
		break;
		case 11:
		{
			if(++e->timer == TIME(12)) {
				MOVE_X(SPEED(0x100));
				e->y_speed -= SPEED(0x200);
			} else if(e->timer > TIME(16)) {
				e->state = 12;
				e->timer = 0;
			}
		}
		break;
		case 12:
		{
			e->timer++;
			if(e->y > e->y_mark && e->timer > TIME(10)) {
				e->timer = 0;
				e->state = 10;
			}
		}
		break;
	}
	e->dir = e->x < e->x_mark;
	if(e->y <= e->y_mark) {
		e->y_speed += SPEED(0x20);
		LIMIT_Y(SPEED(0x200));
	}
	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;
	if(e->x_speed < 0) collide_stage_leftwall(e);
	if(e->x_speed > 0) collide_stage_rightwall(e);
	if(e->y_speed < 0 && collide_stage_ceiling(e)) e->y_speed = SPEED(0x100);
	if(e->y_speed > 0 && collide_stage_floor(e)) e->y_speed = SPEED(-0x200);
	e->x = e->x_next;
	e->y = e->y_next;
}

void ai_kulala(Entity *e) {
	switch(e->state) {
		case 0:		// frozen/in stasis. waiting for player to shoot.
		{
			e->frame = 4;
			e->state = 1;
		}
		/* no break */
		case 1:
		{
			if(e->damage_time) {
				camera_shake(30);
				e->state = 10;
				e->frame = 0;
				e->timer = 0;
			}
		}
		break;
		case 10:	// falling
		{
			e->eflags &= ~NPC_INVINCIBLE;
			if(++e->timer > TIME(40)) {
				e->timer = 0;
				e->state = 11;
			}
		}
		break;
		case 11:	// animate thrust
		{
			e->timer++;
			if(e->timer % TIME(5) == 0) {
				if(++e->frame >= 3) {
					e->state = 12;
					e->timer = 0;
				}
			}
		}
		break;
		case 12:	// thrusting upwards
		{
			e->y_speed = SPEED(-0x155);
			if(++e->timer > TIME(20)) {
				e->state = 10;
				e->frame = 0;
				e->timer = 0;
			}
		}
		break;
		case 20:	// shot/freeze over/go invulnerable
		{
			e->frame = 4;
			e->x_speed >>= 1;
			e->y_speed += SPEED(0x20);
			if(!e->damage_time) {
				e->state = 10;
				e->frame = 0;
				e->timer = TIME(30);
			}
		}
		break;
	}
	
	if(e->damage_time) {
		// x_mark unused so use it as a second timer
		if(++e->x_mark > TIME(12)) {
			e->state = 20;
			e->frame = 4;
			e->eflags |= NPC_INVINCIBLE;
		}
	} else {
		e->x_mark = 0;
	}
	
	if(e->state >= 10) {
		e->x_next = e->x + e->x_speed;
		e->y_next = e->y + e->y_speed;
		e->y_speed += SPEED(0x10);
		if(collide_stage_floor(e)) e->y_speed = SPEED(-0x300);
		else if(e->y_speed < 0) collide_stage_ceiling(e);
		
		// Unused y_mark for third timer
		if(collide_stage_leftwall(e)) {
			e->y_mark = TIME(50);
			e->dir = 1;
		}
		if(collide_stage_rightwall(e)) {
			e->y_mark = TIME(50);
			e->dir = 0;
		}
		
		if(e->y_mark > 0) {
			e->y_mark--;
			ACCEL_X(SPEED(0x80));
		} else {
			e->y_mark = TIME(50);
			FACE_PLAYER(e);
		}
		LIMIT_X(SPEED(0x100));
		LIMIT_Y(SPEED(0x300));
		e->x = e->x_next;
		e->y = e->y_next;
	}
}

void ai_mannan(Entity *e) {
	if(e->state < 3 && e->health < 90) {
		sound_play(e->deathSound, 5);
		effect_create_smoke(sub_to_pixel(e->x), sub_to_pixel(e->y));
		entity_drop_powerup(e);
		// Face sprite remains after defeated
		e->eflags &= ~NPC_SHOOTABLE;
		e->nflags &= ~NPC_SHOOTABLE;
		e->frame = 2;
		e->attack = 0;
		e->state = 3;
		return;
	} else if(e->state == 0 && e->damage_time == 29) {
		e->state = 1;
		e->timer = 0;
		e->frame = 1;
		Entity *shot = entity_create(e->x, e->y, OBJ_MANNAN_SHOT, 0);
		shot->dir = e->dir;
		// We want the bullet to delete itself offscreen, it can't do this while inactive
		shot->alwaysActive = TRUE;
	} else if(e->state == 1 && ++e->timer > 24) {
		e->state = 0;
		e->timer = 0;
		e->frame = 0;
	}
}

void ai_mannanShot(Entity *e) {
	ACCEL_X(SPEED(0x20));
	if((e->timer % 8) == 1) {
		sound_play(SND_IRONH_SHOT_FLY, 2);
	}
	if(++e->timer > TIME(120)) e->state = STATE_DELETE;
	e->x += e->x_speed;
}

void ai_malco(Entity *e) {
	
}

void onspawn_malcoBroken(Entity *e) {
	e->frame = 6;
}

void onspawn_powerc(Entity *e) {
	e->y -= 8 << CSF;
}

void ai_powerc(Entity *e) {
	ANIMATE(e, 8, 0,1);
}

void ai_powers(Entity *e) {
	ANIMATE(e, 4, 0,1,2,3);
}

void ai_press(Entity *e) {
	switch(e->state) {
		case 0:
			e->x_next = e->x;
			e->y_next = e->y + 0x200;
			e->grounded = collide_stage_floor(e);
			if(!e->grounded) {
				e->state = 10;
				e->timer = 0;
				e->frame = 1;
			}
		break;
		case 10:		// fall
			e->timer++;
			if(e->timer == 4) {
				e->frame = 2;
			}
			e->y_speed += 0x20;
			if(e->y_speed > 0x5FF) e->y_speed = 0x5FF;
			e->y_next = e->y + e->y_speed;
			if(e->y < player.y) {
				e->eflags &= ~NPC_SOLID;
				e->attack = 127;
			} else {
				e->eflags |= NPC_SOLID;
				e->attack = 0;
			}
			e->grounded = collide_stage_floor(e);
			if(e->grounded) {
				//SmokeSide(o, 4, DOWN);
				camera_shake(10);
				e->state = 11;
				e->frame = 0;
				e->attack = 0;
				e->eflags |= NPC_SOLID;
			}
			e->y = e->y_next;
		break;
	}
}

void ai_frog(Entity *e) {
	if(!e->grounded) e->y_speed += SPEED(0x80);
	LIMIT_Y(SPEED(0x5FF));

	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;

	switch(e->state) {
		case 0:
		{
			e->timer = 0;
			e->x_speed = 0;
			e->y_speed = 0;
			// Balfrog sets OPTION1
			if(e->eflags & NPC_OPTION1) {
				e->dir = random() & 1;
				e->eflags |= NPC_IGNORESOLID;
				e->state = 3;
				e->frame = 2;
			} else {
				e->grounded = TRUE;
				e->eflags &= ~NPC_IGNORESOLID;
				e->state = 1;
			}
		}
		/* no break */
		case 1:		// standing
		case 2:
		{
			e->frame = 0;
			RANDBLINK(e, 1, 100);
			e->timer++;
		}
		break;
		case 3:		// falling out of ceiling during balfrog fight
		{
			if(++e->timer > TIME(40)) {
				e->eflags &= ~NPC_IGNORESOLID;
				if((e->grounded = collide_stage_floor(e))) {
					e->state = 1;
					e->frame = 0;
					e->timer = 0;
				}
			}
		}
		break;
		case 10:	// jumping
		case 11:
		{
			if (e->x_speed < 0 && collide_stage_leftwall(e)) {
				TURN_AROUND(e);
				MOVE_X(abs(e->x_speed));
			}
			if (e->x_speed > 0 && collide_stage_rightwall(e)) {
				TURN_AROUND(e);
				MOVE_X(abs(e->x_speed));
			}
			if (e->y_speed >= 0 && (e->grounded = collide_stage_floor(e))) {
				e->state = 0;
				e->frame = 0;
				e->timer = 0;
			}
		}
		break;
	}
	// random jumping, and jump when shot
	if (e->state < 3 && e->timer > TIME(10)) {
		u8 dojump = FALSE;
		if(e->damage_time) {
			dojump = TRUE;
		} else if(PLAYER_DIST_X(0x14000) && PLAYER_DIST_Y(0x8000)) {
			if((random() % TIME(50)) == 0) {
				dojump = TRUE;
			}
		}
		if (dojump) {
			FACE_PLAYER(e);
			e->state = 10;
			e->frame = 2;
			e->y_speed = SPEED(-0x5ff);
			e->grounded = FALSE;

			// no jumping sound in cutscenes at ending
			//if (!player->inputs_locked && !player->disabled)
			//	sound(SND_ENEMY_JUMP);

			MOVE_X(SPEED(0x200));
		}
	}

	e->x = e->x_next;
	e->y = e->y_next;
}

void ai_hey(Entity *e) {
	switch(e->state) {
		case 0:
		{
			e->display_box.left = e->display_box.top = 8;
			e->y -= 8 << CSF;
			e->state = 1;
		}
		/* no break */
		case 1:
		{
			if(++e->timer >= TIME(50)) {
				e->hidden ^= 1;
				e->timer = 0;
			}
		}
		break;
	}
}

void ai_motorbike(Entity *e) {
	switch(e->state) {
		case 0:		// parked
		break;
		case 10:	// kazuma and booster mounted
		{
			e->alwaysActive = TRUE;
			e->y -= 0x400;
			////SPR_SAFEADD(e->sprite, SPR_Buggy3, 0, 0, TILE_ATTR(PAL3, 0, 0, e->dir), 5);
			e->state++;
		}
		break;
		case 20:	// kazuma and booster start the engine
		{
			e->state = 21;
			e->timer = 1;
			e->x_mark = e->x;
			e->y_mark = e->y;
		}
		/* no break */
		case 21:
		{
			e->x = e->x_mark + 0x200 - (random() % 0x400);
			e->y = e->y_mark + 0x200 - (random() % 0x400);
			if(++e->timer > 30) {
				e->state = 30;
			}
		}
		break;
		case 30:	// kazuma and booster take off
		{
			e->state = 31;
			e->timer = 1;
			e->x_speed = -0x800;
			e->y_mark = e->y;
			sound_play(SND_MISSILE_HIT, 5);
		}
		/* no break */
		case 31:
		{
			e->x_speed += 0x20;
			e->timer++;
			e->y = e->y_mark + 0x200 - (random() % 0x400);
			if (e->timer > 10)  e->dir = 1;
			if (e->timer > 200) e->state = 40;
		}
		break;
		
		case 40:		// flying away (fast out-of-control)
		{
			e->state = 41;
			e->timer = 2;
			e->dir = 0;
			e->y -= pixel_to_sub(48);		// move up...
			e->x_speed = -0x1000;		// ...and fly fast
		}
		/* no break */
		case 41:
		{
			e->timer += 2;	// makes exhaust sound go faster
			if(e->timer > 1200) e->state = STATE_DELETE;
		}
		break;
	}
	if(e->state >= 20 && (e->timer & 3) == 0) {
		sound_play(SND_FIREBALL, 5);
		// make exhaust puffs, and make them go out horizontal
		// instead of straight up as this effect usually does
		//Caret *puff = effect(o->ActionPointX(), o->ActionPointY(), EFFECT_SMOKETRAIL_SLOW);
		//puff->yinertia = 0;
		//puff->xinertia = (o->dir == LEFT) ? 0x280 : -0x280;
	}
}
