#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <string.h>

static uint8_t running = 1;
static uint8_t should_play_sound;

static uint8_t memory[4096];
static uint8_t V[16];
static uint16_t I;
static uint16_t PC = 0x200;
static uint8_t SP;
static uint16_t stack[16];
static uint8_t video_memory[8 * 32];
static uint8_t DT;
static uint8_t ST;
static uint8_t key[16];

static uint8_t font_sprites[] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, //0
	0x20, 0x60, 0x20, 0x20, 0x70, //1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
	0x90, 0x90, 0xF0, 0x10, 0x10, //4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
	0xF0, 0x10, 0x20, 0x40, 0x40, //7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
	0xF0, 0x90, 0xF0, 0x90, 0x90, //A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
	0xF0, 0x80, 0x80, 0x80, 0xF0, //C
	0xE0, 0x90, 0x90, 0x90, 0xE0, //D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
	0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

static uint8_t overflow_check(uint8_t a, uint8_t b)
{
	if (a >= 0)
	{
		if (b > (INT8_MAX - a))
			return 1;
	}
	else
	{
		if (b < (INT8_MIN - a))
			return -1;
	}
	return 0;
}

static void ZERO()
{
	if (memory[PC] == 0x00)
	{
		if (memory[PC + 1] == 0xE0)
			memset(video_memory, 0, sizeof(video_memory));
		else if (memory[PC + 1] == 0xEE)
		{
			PC = stack[--SP];
		}
		else
			printf_s("Unknown opcode %2X%2X\n", memory[PC], memory[PC]);
	}
}

static void ONE()
{
	PC = (memory[PC] & 0xF) << 8 | memory[PC + 1];
	PC -= 2;
}

static void TWO()
{
	stack[SP++] = PC;
	PC = (memory[PC] & 0xF) << 8 | memory[PC + 1];
	PC -= 2;
}

static void THREE()
{
	if (V[memory[PC] & 0xF] == memory[PC + 1])
		PC += 2;
}

static void FOUR()
{
	if (V[memory[PC] & 0xF] != memory[PC + 1])
		PC += 2;
}

static void FIVE()
{
	if (V[memory[PC] & 0xF] == V[memory[PC + 1] >> 4])
		PC += 2;
}

static void SIX()
{
	V[memory[PC] & 0xF] = memory[PC + 1];
}

static void SEVEN()
{
	V[memory[PC] & 0xF] += memory[PC + 1];
}

static void EIGHT()
{
	switch (memory[PC + 1] & 0xF)
	{
	case 0x0:
		V[memory[PC] & 0xF] = V[memory[PC + 1] >> 4];
		break;
	case 0x1:
		V[memory[PC] & 0xF] |= V[memory[PC + 1] >> 4];
		break;
	case 0x2:
		V[memory[PC] & 0xF] &= V[memory[PC + 1] >> 4];
		break;
	case 0x3:
		V[memory[PC] & 0xF] ^= V[memory[PC + 1] >> 4];
		break;
	case 0x4:
		V[0xF] = overflow_check(V[memory[PC] & 0xF], V[memory[PC + 1] >> 4]);
		V[memory[PC] & 0xF] += V[memory[PC + 1] >> 4];
		break;
	case 0x5:
		if (V[memory[PC] & 0xF] > V[memory[PC + 1] >> 4])
			V[0xF] = 1;
		else
			V[0xF] = 0;
		V[memory[PC] & 0xF] -= V[memory[PC + 1] >> 4];
		break;
	case 0x6:
		V[0xF] = V[memory[PC + 1] >> 4] & 1;
		V[memory[PC] & 0xF] = V[memory[PC + 1] >> 4] >> 1;
		break;
	case 0x7:
		if (V[memory[PC] & 0xF] < V[memory[PC + 1] >> 4])
			V[0xF] = 1;
		else
			V[0xF] = 0;
		V[memory[PC] & 0xF] -= V[memory[PC + 1] >> 4];
		break;
	case 0xE:
		V[0xF] = V[memory[PC + 1] >> 4] & 0x80;
		V[memory[PC] & 0xF] = V[memory[PC + 1] >> 4] << 1;
		break;
	default:
		printf_s("Unknown opcode %2X%2X\n", memory[PC], memory[PC]);
	}
}

static void NINE()
{
	if (V[memory[PC] & 0xF] != V[memory[PC + 1] >> 4])
		PC += 2;
}

static void A()
{
	I = (memory[PC] & 0xF) << 8 | memory[PC + 1];
}

static void B()
{
	PC = (memory[PC] & 0xF) << 8 | memory[PC + 1];
	PC -= 2;
}

static void C()
{
	V[memory[PC] & 0xF] = (rand() % 256) & memory[PC + 1];
}

static void D()
{
	uint8_t x = V[memory[PC] & 0xF];
	uint8_t y = V[memory[PC + 1] >> 4];
	uint8_t n = memory[PC + 1] & 0xF;
	V[0xF] = 0;
	for (uint8_t row = 0; row < n; ++row)
	{
		uint8_t line = memory[I + row];
		for (uint8_t col = 0; col < 8; ++col)
		{
			if (line & (0x80 >> col))
			{
				uint8_t next = ((x % 8) + col) > 7;
				if (video_memory[((x / 8 + (y + row) * 8) + next) % (32 * 8)] & (0x80 >> (col + (x % 8)) % 8))
					V[0xF] = 1;
				video_memory[((x / 8 + (y + row) * 8) + next) % (32 * 8)] ^= (0x80 >> (col + (x % 8)) % 8);
			}
		}
	}
}

static void E()
{
	if (memory[PC + 1] == 0x9E)
	{
		if (key[V[memory[PC] & 0xF]])
			PC += 2;
	}
	else if (memory[PC + 1] == 0xA1)
	{
		if (!key[V[memory[PC] & 0xF]])
			PC += 2;
	}
	else
		printf_s("Unknown opcode %2X%2X\n", memory[PC], memory[PC]);
}

static void F()
{
	switch (memory[PC + 1])
	{
	case 0x07:
		V[memory[PC] & 0xF] = DT;
		break;
	case 0x0A:
		printf_s("Unimplemented opcode %2X%2X\n", memory[PC], memory[PC]);
		break;
	case 0x15:
		DT = V[memory[PC] & 0xF];
		break;
	case 0x18:
		ST = V[memory[PC] & 0xF];
		break;
	case 0x1E:
		V[0xF] = overflow_check(I, V[memory[PC] & 0xF]);
		I += V[memory[PC] & 0xF];
		break;
	case 0x29:
		I = V[memory[PC] & 0xF] * 5;
		break;
	case 0x33:
		memory[I] = V[memory[PC] & 0xF] / 100;
		memory[I + 1] = (V[memory[PC] & 0xF] / 10) % 10;
		memory[I + 2] = (V[memory[PC] & 0xF] % 100) % 10;
		break;
	case 0x55:
		for (uint8_t i = 0; i <= (memory[PC] & 0xF); ++i)
			memory[I++] = V[i];
		break;
	case 0x65:
		for (uint8_t i = 0; i <= (memory[PC] & 0xF); ++i)
			V[i] = memory[I++];
		break;
	default:
		printf_s("Unknown opcode %2X%2X\n", memory[PC], memory[PC]);
	}
}

static void (*instruction_table[16])() = 
{
	ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN,
	EIGHT, NINE, A, B, C, D, E, F
};

#include <SDL2/SDL.h>

static SDL_Window* window;
static SDL_Surface* surface;
static SDL_Surface* rgb_surface;
static SDL_Surface* window_surface;

static void initialize()
{
	srand(time(NULL));
	memcpy_s(memory, sizeof(font_sprites), font_sprites, sizeof(font_sprites));
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
	window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 320, SDL_WINDOW_SHOWN);
	surface = SDL_CreateRGBSurfaceWithFormatFrom(video_memory, 64, 32, 1, 8, SDL_PIXELFORMAT_INDEX1MSB);
	rgb_surface = SDL_CreateRGBSurface(0, 64, 32, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	window_surface = SDL_GetWindowSurface(window);
}

static void read_rom(const char* path)
{
	FILE* file = NULL;
	fopen_s(&file, path, "rb");
	fseek(file, 0, SEEK_END);
	int file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(&memory[0x200], file_size, 1, file);
	fclose(file);
}

static void emulate_cycle()
{
	should_play_sound = 0;	
	instruction_table[(memory[PC] >> 4)]();
	if (DT > 0)
		--DT;
	if (ST > 0)
	{
		if (ST)
			should_play_sound = 1;
		--ST;
	}
	PC += 2;
}

static void handle_events()
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_QUIT:
			running = 0;
			break;
		case SDL_KEYDOWN:
			switch (e.key.keysym.scancode)
			{
			case SDL_SCANCODE_1: key[0x1] = 1; break;
			case SDL_SCANCODE_2: key[0x2] = 1; break;
			case SDL_SCANCODE_3: key[0x3] = 1; break;
			case SDL_SCANCODE_4: key[0xC] = 1; break;

			case SDL_SCANCODE_Q: key[0x4] = 1; break;
			case SDL_SCANCODE_W: key[0x5] = 1; break;
			case SDL_SCANCODE_E: key[0x6] = 1; break;
			case SDL_SCANCODE_R: key[0xD] = 1; break;

			case SDL_SCANCODE_A: key[0x7] = 1; break;
			case SDL_SCANCODE_S: key[0x8] = 1; break;
			case SDL_SCANCODE_D: key[0x9] = 1; break;
			case SDL_SCANCODE_F: key[0xE] = 1; break;

			case SDL_SCANCODE_Z: key[0xA] = 1; break;
			case SDL_SCANCODE_X: key[0x0] = 1; break;
			case SDL_SCANCODE_C: key[0xB] = 1; break;
			case SDL_SCANCODE_V: key[0xF] = 1; break;
			}
			break;
		case SDL_KEYUP:
			switch (e.key.keysym.scancode)
			{
			case SDL_SCANCODE_1: key[0x1] = 0; break;
			case SDL_SCANCODE_2: key[0x2] = 0; break;
			case SDL_SCANCODE_3: key[0x3] = 0; break;
			case SDL_SCANCODE_4: key[0xC] = 0; break;

			case SDL_SCANCODE_Q: key[0x4] = 0; break;
			case SDL_SCANCODE_W: key[0x5] = 0; break;
			case SDL_SCANCODE_E: key[0x6] = 0; break;
			case SDL_SCANCODE_R: key[0xD] = 0; break;

			case SDL_SCANCODE_A: key[0x7] = 0; break;
			case SDL_SCANCODE_S: key[0x8] = 0; break;
			case SDL_SCANCODE_D: key[0x9] = 0; break;
			case SDL_SCANCODE_F: key[0xE] = 0; break;

			case SDL_SCANCODE_Z: key[0xA] = 0; break;
			case SDL_SCANCODE_X: key[0x0] = 0; break;
			case SDL_SCANCODE_C: key[0xB] = 0; break;
			case SDL_SCANCODE_V: key[0xF] = 0; break;
			}
			break;
		}
	}
}

static void draw()
{
	SDL_BlitSurface(surface, NULL, rgb_surface, NULL);
	SDL_BlitScaled(rgb_surface, NULL, window_surface, NULL);
	SDL_UpdateWindowSurface(window);
}

int main(int argc, char** argv)
{
	if (argc > 1)
		read_rom(argv[1]);
	else
		read_rom("tetris.c8");
	initialize();

	while (running)
	{
		emulate_cycle();
		draw();
		handle_events();
	}

	return 0;
}
