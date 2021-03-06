#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <pcx.h>
#include <stdio.h>
#include <stdlib.h>
#include "fastmath.h"
#include "Graphics.h"
#include "tonc_input.h"
#include "plot.h"
#include "Sprite.h"
#include <gba_sprites.h>
#include "oam_manager.h"
#include <string.h>
#include "debug.h"
#include "tile.h"

#include <gbfs.h>

#define RGB16(r,g,b)  ((r)+(g<<5)+(b<<10))


int frame = 0;

void WaitForVblank(void)
{
	while (REG_VCOUNT >= 160);   // wait till VDraw
	while (REG_VCOUNT < 160);    // wait till VBlank
}

typedef struct
{

	int x, y;

}sPoint2D;



typedef struct
{

	int x, y;

	sPoint2D p[4];

	u8 color;

	int angle;
}sBox;

void rotate(sBox* box, int amount)
{
	box->angle += amount;	

	if(box->angle < 0) box->angle += 360;

	box->angle %= 360;
}

void drawBox(Graphics* g, sBox* box)
{

	int angle = box->angle;

	sPoint2D p[4];
	
	int i;
	for(i = 0; i < 4; i++) {
		p[i].x = ( ( cos(angle) * (box->p[i].x >> 14) - sin(angle) * (box->p[i].y >> 14) ) ) + box->x;
		p[i].y = ( ( sin(angle) * (box->p[i].x >> 14) + cos(angle) * (box->p[i].y >> 14) ) ) + box->y;
	}

 
	slow_line(p[0].x >> 14, p[0].y >> 14, p[1].x >> 14, p[1].y >> 14, box->color, g);

	slow_line(p[1].x >> 14, p[1].y >> 14, p[2].x >> 14, p[2].y >> 14, box->color, g);

	slow_line(p[2].x >> 14, p[2].y >> 14, p[3].x >> 14, p[3].y >> 14, box->color, g);

	slow_line(p[3].x >> 14, p[3].y >> 14, p[0].x >> 14, p[0].y >> 14, box->color, g);
}

//WaitForVblank and Draw line not shown...code is same as before

int fixed_mul(int fa, int fb, int shift)
{
	return (fa * fb) >> shift;
}

void boxTest(Graphics* context)
{
	Graphics_setMode(context, MODE_4 | BG2_ENABLE);

	sBox box = {

		50 << 14, 40 << 14, //x,y

		{

			{ -20 << 14, 10 << 14 }, //points

			{ 20 << 14, 10 << 14 },

			{ 20 << 14, -10  << 14},

			{ -20 << 14, -10 << 14 }

		},

	        2, //color

		90 //angle

	};

	context->paletteBuffer[1] = RGB16(20, 5, 20);
	context->paletteBuffer[2] = RGB16(0, 31, 31);

	while (1)
	{
		clearScreen(context, 1);

		drawBox(context, &box);
		
		rotate(&box, 2 * key_tri_horz());

		int forward = key_tri_vert();

		if(key_held(KEY_A)) forward *= 3;

		box.x += forward * cos(box.angle);
		box.y += forward * sin(box.angle);

		VBlankIntrWait();

		flip(context);
	}
}

void consoleTest(const GBFS_FILE* dat, Graphics* context)
{
	Graphics_setMode(context, 0);

	consoleDemoInit();

	// ansi escape sequence to set print co-ordinates
	// /x1b[line;columnH

	const char * text = NULL;
	u32 text_len = 0;

	text = gbfs_get_obj(dat, "test.txt", &text_len);
	
	iprintf("\x1b[5;5H%s\nSize of text in bytes: %u\n", text, text_len);

	while(1) { VBlankIntrWait(); }
}

void imageTest(const GBFS_FILE* dat, Graphics* context)
{
	Graphics_setMode(context, MODE_4 | BG2_ENABLE);

	const u8* image = gbfs_get_obj(dat, "splash.pcx", NULL);
	
	DecodePCX(image, (u16*)context->currentBuffer, (u16*)context->paletteBuffer);

	flip(context);
	
	while(1) { VBlankIntrWait(); }
}



void spriteTest(const GBFS_FILE* dat, Graphics* context)
{
	Graphics_setMode(context, OBJ_ENABLE | OBJ_1D_MAP );

	u32 metr_size = 0;
	const TILE* metr_tiles = gbfs_get_obj(dat, "metr.tiles", &metr_size);
	metr_size /= sizeof(TILE);

	u32 metr_pal_len = 0;
	const u16* metr_pal = gbfs_get_obj(dat, "metr.pal", &metr_pal_len);

	u32 guy_size = 0;
	const TILE* guy_tiles = gbfs_get_obj(dat, "guy.tiles", &guy_size);
	guy_size /= sizeof(TILE);

	u32 guy_pal_len = 0;
	const u16* guy_pal = gbfs_get_obj(dat, "guy.pal", &guy_pal_len);

	const u8* guy_ani = gbfs_get_obj(dat, "guy.ani", NULL);	

	tile_copy(&tile_mem[4][0], metr_tiles, metr_size);
    memcpy(SPRITE_PALETTE, metr_pal, metr_pal_len);

    tile_copy(&tile_mem[4][metr_size], guy_tiles, guy_size);
    memcpy(&SPRITE_PALETTE[16 * 6], guy_pal, guy_pal_len);

    Sprite sprite1;

	Sprite_construct(&sprite1, 50, 50, ATTR0_SQUARE, ATTR1_SIZE_64, 0, 0);

	Sprite sprite;

	Sprite_construct(&sprite, 100, 50, ATTR0_SQUARE, ATTR1_SIZE_32, 6, metr_size);

	AnimContainer_decode(&sprite.anims, guy_ani);

	u32 t = 0;

	u32 frame = 0;

	while (1)
	{
		if(frame == 60)
		{
			debug_print("x: %d y: %d\n", sprite.x, sprite.y);
			frame = 0;
		}

		int dx = key_tri_horz();
		int dy = key_tri_vert();

		static int anim = 0;

		switch(dx)
		{
			case -1: anim = 0; break;
			case 1: anim = 2; break;
			default:
				switch(dy)
				{
					case 1: anim = 1; break;
					case -1: anim = 3; break;
				}
		}


		sprite.x += 2 * dx;
		sprite.y += 2 * dy;

		Sprite_play(&sprite, anim);

		sprite1.x = 100 + (50 * cos(t) >> 14);
		sprite1.y = 50 + (50 * sin(t) >> 14);

		sprite.pal += bit_tribool(key_hit(-1), KI_R, KI_L);

		t = (t + 1) % 360;

		Sprite_draw(&sprite);

		Sprite_draw(&sprite1);

		if(key_hit(KEY_A))
		{
			sprite.oam->attr1 ^= ATTR1_FLIP_X;
		}

		if(key_hit(KEY_B))
		{
			sprite.oam->attr1 ^= ATTR1_FLIP_Y;
		}

		VBlankIntrWait();
	}

	Sprite_destroy(&sprite);
	Sprite_destroy(&sprite1);
}

//---------------------------------------------------------------------------------
void VblankInterrupt()
//---------------------------------------------------------------------------------

{
	key_poll();
	update_oam();
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------


	// the vblank interrupt must be enabled for VBlankIntrWait() to work
	// since the default dispatcher handles the bios flags no vblank handler
	// is required
	irqInit();
	irqSet(IRQ_VBLANK, VblankInterrupt);
	irqEnable(IRQ_VBLANK);

	const GBFS_FILE* dat = find_first_gbfs_file(find_first_gbfs_file);

	Graphics context = Graphics_new(0);

	init_oam();

	//boxTest(&context);
    //imageTest(dat, &context);
	//consoleTest(dat, &context);

	spriteTest(dat, &context);

	return 0;
}


