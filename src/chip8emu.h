#pragma once
#include <stdint.h>
#include "chip8emu_platform.h"

namespace c8e
{

const uint8_t FONT[] = 
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};
const uint16_t FONT_OFFSET = 0x50;
const uint16_t PROGRAM_OFFSET = 0x200;
const uint16_t MEMORY_SIZE = 4096;

struct Chip8
{
	int display_w;
	int display_h;

	int keys[16];
	uint16_t stack[16];
	uint32_t display[2048]; // bools
	uint8_t memory[MEMORY_SIZE];
	uint8_t rom[MEMORY_SIZE - PROGRAM_OFFSET];
	uint8_t v[16]; // general-purpose registers
	uint16_t i;
	uint16_t pc;
	uint8_t vd;
	uint8_t vs;
	uint8_t sp;

	bool loaded;
	bool update_display;

	long long ips;

} c8; // TODO: this is a global for now.

void load_rom(plat::FilePath path);
void reset();
void initialize();
void imgui_generic();
void next_op();
void update_timers();
};