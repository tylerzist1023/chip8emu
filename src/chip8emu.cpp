#include "imgui.h"
#include "chip8emu.h"
#include "chip8emu_platform.h"
#include <stdio.h>

namespace c8e
{

static inline void execute_op(uint16_t op);

void load_rom(plat::FilePath path)
{
    plat::FileContents contents = plat::load_entire_file(path);
    if(!contents.memory)
    {
        goto error;
    }
    if(contents.size > MEMORY_SIZE - PROGRAM_OFFSET)
    {
        goto error;
    }

    memset(c8.rom, 0, (MEMORY_SIZE - PROGRAM_OFFSET)*sizeof(*c8.rom));
    memcpy((c8.rom), contents.memory, contents.size);
    c8.loaded = false;

error:
    plat::unload_file(contents);
}

void reset()
{
    memset(c8.keys, 0, 16*sizeof(*c8.keys));
    memset(c8.stack, 0, 16*sizeof(*c8.stack));
    memset(c8.display, 0, 2048*sizeof(*c8.display));
    memset(c8.v, 0, 16*sizeof(*c8.v));
    memset(c8.memory, 0, MEMORY_SIZE*sizeof(*c8.memory));

    c8.display_w = 64;
    c8.display_h = 32;
    c8.loaded = false;
    c8.update_display = true;
    c8.pc = PROGRAM_OFFSET;
    c8.sp = 0;
    c8.ips = 600;
    c8.i = 0;
    c8.vd = 0;
    c8.vs = 0;
    c8.sp = 0;

    memcpy((c8.memory + FONT_OFFSET), FONT, 5*16);
    memcpy((c8.memory + PROGRAM_OFFSET), (c8.rom), MEMORY_SIZE - PROGRAM_OFFSET);
}

void initialize(ImGuiIO& io)
{
#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = ImGui::GetIO();
    ImGui::StyleColorsDark();
#endif

    memset(&c8, 0, sizeof(c8));

    reset();
}

#ifdef USE_IMGUI
void imgui_generic()
{
    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("ROM"))
        {
            if(ImGui::MenuItem("Open"))
            {   
                plat::FilePath path = {0};
                if(plat::show_file_prompt(&path))
                {
                    load_rom(path);
                    reset();
                    c8.loaded = true;
                }
                else
                {
                    // Clicked cancel or something.                    
                }
                unload_path(path);
            }
            else if(ImGui::MenuItem("Reset"))
            {
                if(c8.loaded)
                {
                    reset();
                    c8.loaded = true;
                }
                else
                {
                    // TODO: Need to load a ROM first!
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
#else
#define imgui_generic(...)
#endif

void next_op()
{
    uint16_t op = (c8.memory[c8.pc] << 8) | (c8.memory[c8.pc+1]);
    execute_op(op);
    c8.pc+=2;
    assert(c8.pc < MEMORY_SIZE);
}

void update_timers()
{
    if(c8.vd > 0)
        c8.vd--;
    if(c8.vs > 0)
        c8.vs--;
}

static inline void op_cls()
{
    memset(c8.display, 0, 64*32*sizeof(*c8.display));
    c8.update_display = true;
}

static inline void op_ret()
{
    // See op_call comment, the same is true here. Cowgod says what should actually be the opposite, unless Cowgod knows something I don't
    assert(c8.sp > 0);
    c8.pc = c8.stack[--c8.sp];
    //c8.pc-=2;
}

static inline void op_jp(uint16_t addr)
{
    c8.pc = addr;
    c8.pc-=2;
}

static inline void op_call(uint16_t addr)
{
    // Cowgod says to increment BEFORE placing on the stack, but that doesn't make any sense to me. 
    // If we increment first, then the 0th index is not used.
    assert(c8.sp < 16);
    c8.stack[c8.sp++] = c8.pc;
    op_jp(addr);
}

static inline void op_se_imm(uint8_t x, uint8_t byte)
{
    if(c8.v[x] == byte)
    {
        c8.pc+=2;
    }
}

static inline void op_sne_imm(uint8_t x, uint8_t byte)
{
    if(c8.v[x] != byte)
    {
        c8.pc+=2;
    }
}

static inline void op_se(uint8_t x, uint8_t y)
{
    if(c8.v[x] == c8.v[y])
    {
        c8.pc+=2;
    }
}

static inline void op_ld_imm(uint8_t x, uint8_t byte)
{
    c8.v[x] = byte;
}

static inline void op_add_imm(uint8_t x, uint8_t byte)
{
    c8.v[x] += byte;
}

static inline void op_ld(uint8_t x, uint8_t y)
{
    c8.v[x] = c8.v[y];
}

static inline void op_or(uint8_t x, uint8_t y)
{
    c8.v[x] |= c8.v[y];
}

static inline void op_and(uint8_t x, uint8_t y)
{
    c8.v[x] &= c8.v[y];
}

static inline void op_xor(uint8_t x, uint8_t y)
{
    c8.v[x] ^= c8.v[y];
}

static inline void op_add(uint8_t x, uint8_t y)
{
    c8.v[0xF] = (c8.v[y] > (0xFF - c8.v[x]));
    c8.v[x] += c8.v[y];
}

static inline void op_sub(uint8_t x, uint8_t y)
{
    c8.v[0xF] = !(c8.v[y] > c8.v[x]);
    c8.v[x] -= c8.v[y];
}

static inline void op_shr(uint8_t x, uint8_t y)
{
    // TODO: vy is unused?
    c8.v[0xF] = c8.v[x] & 1;
    c8.v[x] >>= 1;
}

static inline void op_subn(uint8_t x, uint8_t y)
{
    op_sub(y,x);
}

static inline void op_shl(uint8_t x, uint8_t y)
{
    // TODO: vy is unused?
    c8.v[0xF] = c8.v[x] & 0x8000;
    c8.v[x] <<= 1;
}

static inline void op_sne(uint8_t x, uint8_t y)
{
    if(c8.v[x] != c8.v[y])
    {
        c8.pc+=2;
    }
}

static inline void op_st_i(uint16_t addr)
{
    c8.i = addr;
}

static inline void op_jp_v0(uint8_t addr)
{
    op_jp(addr + c8.v[0]);
}

static inline void op_rnd(uint8_t x, uint8_t byte)
{
    c8.v[x] = rand() & byte;
}

static inline void op_drw(uint8_t x, uint8_t y, uint8_t nibble)
{
    uint8_t x_coord = c8.v[x] % c8.display_w;
    uint8_t y_coord = c8.v[y] % c8.display_h;
    c8.v[0xF] = 0;

    for(uint8_t i = 0; i < nibble; i++)
    {
        uint8_t sprite_data = c8.memory[c8.i+i];
        for(int j = 0; j < 8; j++)
        {
            if((sprite_data & 0x80) && c8.display[x_coord+j + (y_coord+i)*c8.display_w])
            {
                c8.v[0xF] = 1;
            }

            c8.display[x_coord+j + (y_coord+i)*c8.display_w] ^= (sprite_data & 0x80) ? 0xFFFFFFFF : 0x00000000;

            sprite_data <<= 1;

            if(x_coord+j >= c8e::c8.display_w)
            {
                break;
            }
        }
        if(y_coord+i >= c8e::c8.display_h)
        {
            break;
        }
    }

    c8.update_display = true;
}

static inline void op_skp(uint8_t x)
{
    // TODO: I am skeptical of whether or not the index is vx or just x...
    //plat::update_input();
    if(c8.keys[c8.v[x]])
    {
        c8.pc+=2;
    }
}

static inline void op_sknp(uint8_t x)
{
    // TODO: I am skeptical of whether or not the index is vx or just x...
    //plat::update_input();
    if(!c8.keys[c8.v[x]])
    {
        c8.pc+=2;
    }
}

static inline void op_ld_vd(uint8_t x)
{
    c8.v[x] = c8.vd;
}

static inline void op_ld_key(uint8_t x)
{
    //plat::update_input();
    for(int i = 0; i < 16; i++)
    {
        if(c8.keys[i])
        {
            c8.v[x] = i;
            return;
        }
    }
    c8.pc -= 2; // TODO: This is hacky. A better way may be to return a flag to tell the virtual CPU to update the program counter
}

static inline void op_st_vd(uint8_t x)
{
    c8.vd = c8.v[x];
}

static inline void op_st_vs(uint8_t x)
{
    c8.vs = c8.v[x];
}

static inline void op_add_i(uint8_t x)
{
    c8.v[0xF] = (c8.i + c8.v[x] > 0xFFF);
    c8.i += c8.v[x];
}

static inline void op_ld_f(uint8_t x)
{
    c8.i = FONT_OFFSET + (c8.v[x])*5;
}

static inline void op_ld_b(uint8_t x)
{
    assert(c8.i+2 < MEMORY_SIZE && c8.i < MEMORY_SIZE);

    int hundreds = c8.v[x] / 100;
    int tens = (c8.v[x] / 10) % 10;
    int ones = c8.v[x] % 10;

    c8.memory[c8.i] = hundreds;
    c8.memory[c8.i+1] = tens;
    c8.memory[c8.i+2] = ones;
}

static inline void op_ld_v(uint8_t x)
{
    assert(c8.i+x < MEMORY_SIZE && c8.i < MEMORY_SIZE);

    for(int i = 0; i <= x; i++)
    {
        c8.memory[c8.i + i] = c8.v[i];
    }
    //c8.i += x + 1;
}

static inline void op_st_v(uint8_t x)
{
    assert(c8.i+x < MEMORY_SIZE && c8.i < MEMORY_SIZE);
    
    for(int i = 0; i <= x; i++)
    {
        c8.v[i] = c8.memory[c8.i + i];
    }
    //c8.i += x + 1;
}

#define op_nnn(nnn) (nnn & 0xFFF)
#define op_x(x) ((x & 0xF00) >> 8)
#define op_y(y) ((y & 0xF0) >> 4)
#define op_kk(kk) (kk & 0xFF)
#define op_n(n) (n & 0xF)

static inline void execute_op(uint16_t op)
{
    switch(op & 0xF000)
    {
        case 0x0000:
            switch(op)
            {
                case 0x00E0:
                    op_cls();
                    break;
                case 0x00EE:
                    op_ret();
                    break;
            }
            break;
        case 0x1000:
            op_jp(op_nnn(op));
            break;
        case 0x2000:
            op_call(op_nnn(op));
            break;
        case 0x3000:
            op_se_imm(op_x(op), op_kk(op));
            break;
        case 0x4000:
            op_sne_imm(op_x(op), op_kk(op));
            break;
        case 0x5000:
            op_se(op_x(op), op_y(op));
            break;
        case 0x6000:
            op_ld_imm(op_x(op), op_kk(op));
            break;
        case 0x7000:
            op_add_imm(op_x(op), op_kk(op));
            break;
        case 0x8000:
            switch(op & 0xF)
            {
                case 0:
                    op_ld(op_x(op), op_y(op));
                    break;
                case 1:
                    op_or(op_x(op), op_y(op));
                    break;
                case 2:
                    op_and(op_x(op), op_y(op));
                    break;
                case 3:
                    op_xor(op_x(op), op_y(op));
                    break;
                case 4:
                    op_add(op_x(op), op_y(op));
                    break;
                case 5:
                    op_sub(op_x(op), op_y(op));
                    break;
                case 6:
                    op_shr(op_x(op), op_y(op));
                    break;
                case 7:
                    op_subn(op_x(op), op_y(op));
                    break;
                case 0xE:
                    op_shl(op_x(op), op_y(op));
                    break;
            }
            break;
        case 0x9000:
            op_sne(op_x(op), op_y(op));
            break;
        case 0xA000:
            op_st_i(op_nnn(op));
            break;
        case 0xB000:
            op_jp_v0(op_nnn(op));
            break;
        case 0xC000:
            op_rnd(op_x(op), op_nnn(op));
            break;
        case 0xD000:
            op_drw(op_x(op), op_y(op), op_n(op));
            break;
        case 0xE000:
            switch(op & 0xFF)
            {
                case 0x9E:
                    op_skp(op_x(op));
                    break;
                case 0xA1:
                    op_skp(op_x(op));
                    break;
            }
            break;
        case 0xF000:
            switch(op & 0xFF)
            {
                case 0x07:
                    op_ld_vd(op_x(op));
                    break;
                case 0x0A:
                    op_ld_key(op_x(op));
                    break;
                case 0x15:
                    op_st_vd(op_x(op));
                    break;
                case 0x18:
                    op_st_vs(op_x(op));
                    break;
                case 0x1E:
                    op_add_i(op_x(op));
                    break;
                case 0x29:
                    op_ld_f(op_x(op));
                    break;
                case 0x33:
                    op_ld_b(op_x(op));
                    break;
                case 0x55:
                    op_ld_v(op_x(op));
                    break;
                case 0x65:
                    op_st_v(op_x(op));
                    break;
            }
            break;
    }
}

};