#include "ai.h"

#include <genesis.h>
#include "audio.h"
#include "player.h"
#include "stage.h"
#include "tables.h"
#include "tsc.h"
#include "camera.h"

void ai_pignon(Entity *e) {
	e->timer++;
	if(e->state < 3 && e->damage_time == 29) {
		//FACE_PLAYER(e);
		e->state = 3;
		e->timer = 0;
		e->y_speed = -0x100;
		if(e->type == OBJ_GIANT_MUSHROOM_ENEMY)
			e->x_speed = e->x > player.x ? -0x100 : 0x100;
		else
			e->x_speed = 0;
		//MOVE_X(-0x120);
		e->frame = 4;
	}
	switch(e->state) {
		case 0: // Standing
		{
			if(e->timer > 120 && (e->timer & 31) == 0) { 
				// Either blink or walk in a random direction
				u8 rnd = random() & 7;
				if(rnd == 0) {
					e->state = 1;
					e->timer = 0;
					e->frame = 2;
				} else if(rnd == 1) {
					e->state = 2;
					e->timer = 0;
					e->dir = random() & 1;
					e->x_speed = e->dir ? 0x100 : -0x100;
					e->frame = 1;
				}
			}
		}
		break;
		case 1: // Blink
		{
			if(e->timer >= 10) {
				e->state = 0;
				e->timer = 0;
				e->frame = 0;
			}
		}
		break;
		case 2: // Walking
		{
			ANIMATE(e, 8, 1,0,2,0);
			if(e->timer >= 30 && (random() & 31) == 0) {
				e->state = 0;
				e->timer = 0;
				e->x_speed = 0;
				e->frame = 0;
			}
		}
		break;
		case 3: // Hurt
		{
			e->x_speed -= e->x_speed > 0 ? 4 : -4; // Decellerate
			if(e->timer >= TIME(30)) {
				e->state = 0;
				e->timer = 0;
				e->x_speed = 0;
				e->frame = 0;
			}
		}
		break;
	}
	if(!e->grounded) e->y_speed += SPEED(0x20);
	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;
	// Don't test ceiling, only test sticking to ground while moving
	if(e->x_speed < 0) {
		collide_stage_leftwall(e);
	} else if(e->x_speed > 0) {
		collide_stage_rightwall(e);
	}
	if(e->grounded) {
		if(e->x_speed != 0) e->grounded = collide_stage_floor_grounded(e);
	} else {
		e->grounded = collide_stage_floor(e);
	}
	e->x = e->x_next;
	e->y = e->y_next;
}

void onspawn_gkeeper(Entity *e) {
	e->nflags &= ~NPC_INVINCIBLE;
	e->eflags |= NPC_SHOOTABLE | NPC_INVINCIBLE;
	// I expanded the sprite to 32 width so hflip will be symmetrical
	e->hit_box = (bounding_box) { 8, 11, 8, 11 };
	e->display_box = (bounding_box) { 16, 12, 16, 12 };
	e->attack = 0;
}

void ai_gkeeper(Entity *e) {
	switch(e->state) {
		case 0: // Standing
		e->frame = 0;
		FACE_PLAYER(e);
		// start walking when player comes near
		if(PLAYER_DIST_X(pixel_to_sub(128)) && 
			PLAYER_DIST_Y2(pixel_to_sub(48), pixel_to_sub(32))) {
			e->state = 1;
			e->frame = 1;
		}
		// start walking if shot
		if(e->damage_time > 0) {
			e->state = 1;
			e->frame = 1;
			e->eflags |= NPC_INVINCIBLE;
		}
		break;
		case 1: // Walking
		ANIMATE(e, 8, 1,0,2,0);
		FACE_PLAYER(e);
		e->x_speed = e->dir ? 0x100 : -0x100;
		// reached knife range of player?
		if(PLAYER_DIST_X(pixel_to_sub(10))) {
			e->state = 2;
			e->timer = 0;
			e->x_speed = 0;
			sound_play(SND_FIREBALL, 5);
			e->frame = 3;
			e->eflags &= ~NPC_INVINCIBLE;
		}
		break;
		case 2: // Knife raised
		if(++e->timer > 40) {
			e->state = 3;
			e->timer = 0;
			e->attack = 10;
			e->hit_box.left += 6;
			sound_play(SND_SLASH, 5);
			e->frame = 4;
		}
		break;
		case 3: // Knife frame 2
		if(++e->timer > 2) {
			e->state = 4;
			e->timer = 0;
			e->frame = 5;
		}
		break;
		case 4: // Knife frame 3
		if(++e->timer > 60) {
			e->state = 0;
			e->frame = 0;
			e->eflags |= NPC_INVINCIBLE;
			e->attack = 0;
			e->hit_box.left -= 6;
		}
		break;
	}
	if(!e->grounded) e->y_speed += GRAVITY_JUMP;
	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;
	// Don't test ceiling, only test sticking to ground while moving
	if(e->x_speed < 0) {
		collide_stage_leftwall(e);
	} else if(e->x_speed > 0) {
		collide_stage_rightwall(e);
	}
	if(e->grounded) {
		if(e->x_speed != 0) e->grounded = collide_stage_floor_grounded(e);
	} else {
		e->grounded = collide_stage_floor(e);
	}
	e->x = e->x_next;
	e->y = e->y_next;
}

enum {
	MP_Fight_Start		= 100,		// scripted
	MP_BaseState		= 110,
	MP_Jump				= 120,
	MP_In_Air			= 130,
	MP_Landed			= 140,
	MP_ChargeAttack		= 200,
	MP_Hit_Wall			= 220,
	MP_CloneAttack		= 300,
	MP_Fly_Up			= 310,
	MP_Spawn_Clones		= 330,
	MP_Defeated			= 500		// scripted
};

// NXEngine has a 3rd timer, we don't. Alias the unused jump_time
#define timer3	jump_time

void ai_ma_pignon(Entity *e) {
	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;
	
	// Remember x and y speed to handle bouncing
	e->x_mark = e->x_speed;
	e->y_mark = e->y_speed;
	
	u8 blockl = FALSE, blockr = FALSE;
	
	// Ma Pignon jumps off screen and should not collide with the map while doing so
	if (e->y > block_to_sub(8)) {
		blockl = e->x_speed < 0 && collide_stage_leftwall(e);
		blockr = e->x_speed > 0 && collide_stage_rightwall(e);
		if(!e->grounded) e->grounded = collide_stage_floor(e);
		else e->grounded = collide_stage_floor_grounded(e);
	}
	
	switch(e->state) {
		case 0:
		{
			SNAP_TO_GROUND(e);
			e->state = 1;
		}
		case 1:
		{
			FACE_PLAYER(e);
			e->frame = 0;
			RANDBLINK(e, 1, 200);
		}
		break;
		
		case MP_Fight_Start:		// set by script
		{
			e->state = MP_BaseState;
			e->timer = 0;
			e->timer2 = 0;
			e->eflags |= NPC_SHOOTABLE;
		}
		case MP_BaseState:
		{
			FACE_PLAYER(e);
			e->attack = 1;
			e->frame = 0;
			
			if (++e->timer > 4) {
				e->timer = 0;
				e->state = MP_Jump;
				
				if (++e->timer3 > 12) {
					e->timer3 = 0;
					e->state = MP_CloneAttack;
				}
			}
		}
		break;
		
		case MP_Jump:		// pause a moment and jump
		{
			e->frame = 2;
			if (++e->timer > 4) {
				e->state = MP_In_Air;
				e->frame = 3;
				
				e->x_speed = -SPEED(0x400) + (random() % SPEED(0x800));
				e->y_speed = -SPEED(0x800);
				e->grounded = FALSE;
				
				sound_play(SND_ENEMY_JUMP, 5);
				e->timer2++;
			}
		}
		break;
		
		case MP_In_Air:		// jumping or falling after clone attack
		{
			e->y_speed += SPEED(0x80);
			
			// for when falling back onscreen after clone attack
			//if (e->y > (8 * TILE_H) << CSF)
			//	e->eflags &= ~NPC_IGNORE_SOLID;
			//else
			//	e->blockd = false;
			
			// bounce off walls
			if (blockl || blockr) e->x_speed = -e->x_mark;
			
			FACE_PLAYER(e);
			
			// select frame
			if (e->y_speed < -SPEED(0x200)) {
				e->frame = 3;
			} else if (e->y_speed > SPEED(0x200)) {
				e->frame = 4;
			} else {
				e->frame = 0;
			}
			
			if (e->grounded) {
				e->state = MP_Landed;
				e->timer = 0;
				e->frame = 2;
				e->x_speed = 0;
			}
			
			if (e->timer2 > 4) {
				if (player.y < (e->y + 0x800)) {
					e->state = MP_ChargeAttack;
					e->timer = 0;
					e->x_speed = 0;
					e->y_speed = 0;
				}
			}
		}
		break;
		
		case MP_Landed:
		{
			e->frame = 2;
			if (++e->timer > 4) {
				e->state = MP_BaseState;
			}
		}
		break;
		
		case MP_ChargeAttack:		// charge attack
		{
			e->frame = 5;
			if (++e->timer > 10) {
				e->state = MP_ChargeAttack+1;
				e->frame = 6;
				
				MOVE_X(SPEED(0x5ff));
				sound_play(SND_FUNNY_EXPLODE, 5);
				
				e->eflags |= NPC_INVINCIBLE;
				e->attack = 10;
			}
		}
		break;
		
		case MP_ChargeAttack+1:		// in-air during charge attack
		{
			ANIMATE(e, 4, 6,7);
			if (blockl || blockr) e->state = MP_Hit_Wall;
		}
		break;
		
		case MP_Hit_Wall:		// hit wall
		{
			e->state++;
			e->timer = 0;
			camera_shake(16);
		}
		case MP_Hit_Wall+1:
		{
			e->attack = 4;
			ANIMATE(e, 4, 6,7);
			
			if ((++e->timer % 8) == 0) {
				s32 x = block_to_sub(4 + (random() % 12));
				entity_create(x, (16 << CSF), OBJ_MA_PIGNON_ROCK, 0);
			}
			
			if (e->timer > TIME(30)) {
				e->timer2 = 0;
				e->state = MP_In_Air;
				
				e->eflags &= ~NPC_INVINCIBLE;
				
				e->attack = 3;
			}
		}
		break;
		
		
		case MP_CloneAttack:	// begin clone-attack sequence
		{
			e->state++;
			e->frame = 9;
			FACE_PLAYER(e);
		}
		case MP_CloneAttack+1:	// walk at player before attack
		{
			ANIMATE(e, 4, 9,11);
			
			MOVE_X(SPEED(0x400));
			if (PLAYER_DIST_X(3 << CSF)) {
				e->state = MP_Fly_Up;
				e->timer = 0;
				e->frame = 2;
				e->x_speed = 0;
			}
		}
		break;
		
		case MP_Fly_Up:		// jump and fly up for clone attack
		{
			e->frame = 2;
			if (++e->timer > 4) {
				e->state++;
				e->frame = 12;
				e->y_speed = -SPEED(0x800);
				e->grounded = FALSE;
				sound_play(SND_FUNNY_EXPLODE, 5);
				
				e->eflags |= NPC_INVINCIBLE;
				
				e->attack = 10;
			}
		}
		break;
		case MP_Fly_Up+1:		// flying up
		{
			ANIMATE(e, 4, 12,13);
			
			if (e->y < (16<<CSF))
				e->state = MP_Spawn_Clones;
		}
		break;
		
		case MP_Spawn_Clones:	// offscreen, spawning clones
		{
			e->y_speed = 0;
			e->state++;
			e->timer = 0;
			
			camera_shake(10);
		}
		case MP_Spawn_Clones+1:
		{
			ANIMATE(e, 4, 12,13);
			
			if ((++e->timer % 8) == 0) {
				s32 x = block_to_sub(4 + (random() % 12));
				entity_create(x, (16 << CSF), OBJ_MA_PIGNON_CLONE, 0);
			}
			
			if (e->timer > TIME(30)) {
				e->timer2 = 0;
				e->state = MP_In_Air;	// fall back down to ground
				
				e->eflags &= ~NPC_INVINCIBLE;
			}
		}
		break;
		
		case MP_Defeated:			// defeated -- set by script
		{
			entities_clear_by_type(OBJ_MA_PIGNON_CLONE);
			e->eflags &= ~NPC_SHOOTABLE;
			e->state++;
			e->timer = 0;
			e->frame = 8;
			e->attack = 0;
		}
		case MP_Defeated+1:
		{
			e->y_speed += SPEED(0x20);
			if (e->grounded) {
				e->x_speed *= 7;
				e->x_speed /= 8;
			}
			
			e->display_box.left += (++e->timer & 1) ? 1 : -1;
		}
		break;
	}
	/*
	// ma pignon is invulnerable to missiles and Blade.
	// TODO: deal with this later
	if (e->state >= MP_Fight_Start && e->state < MP_Defeated)
	{
		// ....he's invulnerable anyway during these two states so don't mess with that.
		if (e->state != MP_ChargeAttack+1 && e->state != MP_Fly_Up+1)
		{
			bool found_weapons = false;
			if (e->type != OBJ_MA_PIGNON_CLONE)
			{
				Entity *c;
				FOREACH_OBJECT(c)
				{
					if (c->type == OBJ_MISSILE_SHOT ||
						c->type == OBJ_SUPERMISSILE_SHOT ||
						c->type == OBJ_MISSILE_BOOM_SPAWNER ||
						c->type == OBJ_BLADE12_SHOT ||
						c->type == OBJ_BLADE3_SHOT ||
						c->type == OBJ_BLADE_SLASH)
					{
						found_weapons = true;
						break;
					}
				}
			}
			
			if (found_weapons)
			{
				e->eflags &= ~NPC_SHOOTABLE;
				e->eflags |= NPC_INVINCIBLE;
			}
			else
			{
				e->eflags |= NPC_SHOOTABLE;
				e->eflags &= ~NPC_INVINCIBLE;
			}
		}
	}
	*/
	
	e->x = e->x_next;
	e->y = e->y_next;
	
	// don't use LIMIT_Y, it limits upwards too and breaks his jumps
	if (e->y_speed > SPEED(0x5ff)) e->y_speed = SPEED(0x5ff);
}

void ai_ma_pignon_rock(Entity *e) {
	ANIMATE(e, 8, 0,1,2);
	
	e->x_next = e->x + e->x_speed;
	e->y_next = e->y + e->y_speed;
	
	if (e->y > block_to_sub(8) && e->state < 2) e->grounded = collide_stage_floor(e);
	
	switch(e->state) {
		case 0:
		{
			e->timer3 = 0;
			e->state = 1;
			e->eflags |= NPC_INVINCIBLE;
			e->animtime = random() % (3 * 8);
			e->attack = 10;
		}
		case 1:
		{
			e->y_speed += SPEED(0x40);
			LIMIT_Y(SPEED(0x700));
			
			if (e->grounded) {
				e->y_speed = -SPEED(0x200);
				e->grounded = FALSE;
				e->state = 2;
				
				sound_play(SND_BLOCK_DESTROY, 5);
				camera_shake(10);
				
				// these smoke clouds appear BEHIND the map tiles
				//for(int i=0;i<2;i++) {
				//	Entity *smoke = entity_create(e->CenterX() + random(-12<<CSF, 12<<CSF),
				//								 e->Bottom()+(16<<CSF), OBJ_SMOKE_CLOUD);
				//	smoke->x_speed = random(-0x155, 0x155);
				//	smoke->y_speed = random(-0x600, 0);
				//}
			}
		}
		break;
		
		case 2:
		{
			e->y_speed += SPEED(0x40);
			if (e->y > block_to_sub(stageHeight)) e->state = STATE_DELETE;
		}
		break;
	}
	
	e->x = e->x_next;
	e->y = e->y_next;
}

void ai_ma_pignon_clone(Entity *e) {
	switch(e->state) {
		case 0:
		{
			e->frame = 3;
			e->y_speed += SPEED(0x80);
			LIMIT_Y(SPEED(0x5ff));
			e->y += e->y_speed;
			
			if (e->y > block_to_sub(8)) {
				e->state = 130;
			}
		}
		break;
		
		default:
			e->timer2 = e->timer3 = 0;
			ai_ma_pignon(e);
		break;
	}
	// Our engine has no substate, substitute id1
	if (++e->id > 300) {
		//effect(e->CenterX(), e->CenterY(), EFFECT_BOOMFLASH);
		e->state = STATE_DELETE;
	}
}
