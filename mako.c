#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>
#include <SDL_video.h>

#include "constants.h"

static int32_t *m;

static void push(int32_t v)
{
	m[m[DP]++] = v;
}

static void rpush(int32_t v)
{
	m[m[RP]++] = v;
}

static int32_t pop()
{
	return m[--m[DP]];
}

static int32_t rpop()
{
	return m[--m[RP]];
}

static int32_t mod(int32_t a, int32_t b)
{
	a %= b;
	return a < 0 ? a+b : a;
}

static int32_t load(int32_t addr)
{
	if(addr == CO)
		return (int32_t)getchar();
	if(addr == RN)
		m[addr] = rand();
	return m[addr];
}

static void stor(int32_t addr, int32_t val)
{
	if(addr == CO)
		putchar(val);
	else
		m[addr] = val;
}

void tick() {
	int32_t o = m[m[PC]++];
	int32_t a, b;

	switch(o) {
	case OP_CONST:
		push(m[m[PC]++]);
		break;
	case OP_CALL:
		rpush(m[PC]+1);
		m[PC] = m[m[PC]];
		break;
	case OP_JUMP:
		m[PC] = m[m[PC]];
		break;
	case OP_JUMPZ:
		m[PC] = pop()==0 ? m[m[PC]] : m[PC]+1;
		break;
	case OP_JUMPIF:
		m[PC] = pop()!=0 ? m[m[PC]] : m[PC]+1;
		break;
	case OP_LOAD:
		push(load(pop()));
		break;
	case OP_STOR:
		a = pop();
		b = pop();
		stor(a, b);
		break;
	case OP_RETURN:
		m[PC] = rpop();
		break;
	case OP_DROP:
		pop();
		break;
	case OP_SWAP:
		a = pop();
		b = pop();
		push(a);
		push(b);
		break;
	case OP_DUP:
		push(m[m[DP]-1]);
		break;
	case OP_OVER:
		push(m[m[DP]-2]);
		break;
	case OP_STR:
		rpush(pop());
		break;
	case OP_RTS:
		push(rpop());
		break;
	case OP_ADD:
		a = pop();
		b = pop();
		push(b+a);
		break;
	case OP_SUB:
		a = pop();
		b = pop();
		push(b-a);
		break;
	case OP_MUL:
		a = pop();
		b = pop();
		push(b*a);
		break;
	case OP_DIV:
		a = pop();
		b = pop();
		push(b/a);
		break;
	case OP_MOD:
		a = pop();
		b = pop();
		push(mod(b,a));
		break;
	case OP_AND:
		a = pop();
		b = pop();
		push(b&a);
		break;
	case OP_OR:
		a = pop();
		b = pop();
		push(b|a);
		break;
	case OP_XOR:
		a = pop();
		b = pop();
		push(b^a);
		break;
	case OP_NOT:
		push(~pop());
		break;
	case OP_SGT:
		a = pop();
		b = pop();
		push(b>a ? -1:0);
		break;
	case OP_SLT:
		a = pop();
		b = pop();
		push(b<a ? -1:0);
		break;
	case OP_NEXT:
		m[PC] = --m[m[RP]-1]<0 ? m[PC]+1 : m[m[PC]];
		break;
	}
}

static void draw_pixel(SDL_Surface *scr, uint32_t x, uint32_t y, uint32_t col)
{
	if((col & 0xFF000000) != 0xFF000000) return;
	if(x < 0 || x >= 320 || y < 0 || y >= 240) return;
	uint32_t *buf = scr->pixels;
	buf[x + y*(scr->pitch/4)] = col;
}

static void draw_tile(SDL_Surface *scr, int32_t tile, int px, int py)
{
	if(tile < 0) return;
	tile &= ~GRID_Z_MASK;

	uint32_t i = m[GT] + tile * 64;

	for(int y = 0; y < 8; y++)
		for(int x = 0; x < 8; x++)
			draw_pixel(scr, x + px, y + py, m[i++]);
}

static void draw_grid(SDL_Surface *scr, int zbit)
{
	int i = m[GP];
	for(int y = 0; y < 31; y++) {
		for(int x = 0; x < 41; x++) {
			if(!zbit && (m[i] & GRID_Z_MASK) != 0) {
				i++;
				continue;
			}
			if(zbit && (m[i] & GRID_Z_MASK) == 0) {
				i++;
				continue;
			}
			draw_tile(scr, m[i++], x*8 - m[SX], y*8 - m[SY]);
		}
		i += m[GS];
	}
}

static void draw(SDL_Surface *scr)
{
	if(SDL_MUSTLOCK(scr))
		while(SDL_LockSurface(scr) != 0) SDL_Delay(10);

	SDL_FillRect(scr, NULL, m[CL]);

	draw_grid(scr, 0);
	draw_grid(scr, 1);

	if(SDL_MUSTLOCK(scr))
		SDL_UnlockSurface(scr);

	SDL_UpdateRect(scr, 0, 0, 0, 0);
}

int main(int argc, char **argv)
{
	if(argc == 1) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		exit(1);
	}

	int pos = 0;

	srand(time(0));

	FILE *f = fopen(argv[1], "r");
	if(!f || errno) goto onerr;
	
	m = calloc(1024, sizeof *m);
	int alloc_size = 1024;
	if(!m || errno) goto onerr;

	while(!feof(f)) {
		if(pos == alloc_size) {
			alloc_size *= 2;
			m = realloc(m, alloc_size * sizeof *m);
			if(!m || errno) goto onerr;
		}

		uint8_t buf[4];

		int n = fread(buf, sizeof *buf, 4, f);
		if(errno) goto onerr;
		if(n == 0) break;
		if(n != 4) {
			fprintf(stderr, "%s: The file was invalid.\n", argv[0]);
			exit(1);
		}
		m[pos] = (int32_t)buf[0] << 24 | (int32_t)buf[1] << 16 | (int32_t)buf[2] << 8 | (int32_t)buf[3];
		pos++;
	}

	fclose(f);
	if(errno) goto onerr;

	memset(m + pos, 0, (alloc_size - pos) * sizeof *m);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	atexit(SDL_Quit);

	SDL_Surface *scr = SDL_SetVideoMode(320, 240, 32, SDL_SWSURFACE);
	if(!scr) goto sdlerr;


	while(m[PC] != -1) {
		uint32_t start = SDL_GetTicks();
		
		while(m[PC] != -1 && m[m[PC]] != OP_SYNC)
			tick();
		if(m[PC] == -1) exit(0);

		SDL_Event event;		

		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_KEYDOWN: case SDL_KEYUP:
				switch(event.key.keysym.sym) {
#define SET_KEY(sdl, mako) case sdl : m[KY] ^= mako ; break;
				SET_KEY(SDLK_LEFT, KEY_LF);
				SET_KEY(SDLK_RIGHT, KEY_RT);
				SET_KEY(SDLK_UP, KEY_UP);
				SET_KEY(SDLK_DOWN, KEY_DN);
				SET_KEY(SDLK_RETURN, KEY_A);
				SET_KEY(SDLK_SPACE, KEY_A);
				SET_KEY(SDLK_z, KEY_A);
				SET_KEY(SDLK_x, KEY_B);
				SET_KEY(SDLK_LSHIFT, KEY_B);
				SET_KEY(SDLK_RSHIFT, KEY_B);
#undef SET_KEY
				}
				break;
			case SDL_QUIT:
				exit(0);
			}
		}

		draw(scr);

		uint32_t total = SDL_GetTicks() - start;
		
		if(total < 1000/60)
			SDL_Delay(1000/60 - total);

		m[PC]++;
	}
	exit(0);

onerr:
	perror(argv[0]);
	exit(1);
sdlerr:
	fprintf(stderr, "%s: %s\n", argv[0], SDL_GetError());
	exit(1);
}
