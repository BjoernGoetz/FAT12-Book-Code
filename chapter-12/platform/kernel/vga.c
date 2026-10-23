#include <stdint.h>
#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* Where the VGA screen starts in memory, always at the same
   address. volatile because the display hardware reads it, not our
   code: without it, the compiler could drop or merge writes it
   thinks nothing ever reads */
static volatile uint16_t *vga = (uint16_t *)0xB8000;

/* Keeps track of where the last print ended, so the
   next one continues there */
static int cursor_row = 0;
static int cursor_col = 0;

/* Returns the 16-bit value for a cell showing the given character as
   white text on a black background */
static uint16_t char_to_cell(uint8_t character)
{
    return (0x0F << 8) | character;
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = char_to_cell(' ');
}

void vga_print(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        /* No scrolling: our kernel never prints more than one screen
           holds, so text past the last row is simply dropped */
        if (cursor_row >= VGA_HEIGHT)
            return;

        /* A newline prints nothing: it only moves the cursor to the
           start of the next row */
        if (str[i] == '\n') {
            cursor_col = 0;
            cursor_row++;
            continue;
        }

        /* Our lines always fit on the screen, so anything past the last
           column is simply dropped */
        if (cursor_col >= VGA_WIDTH)
            continue;

        vga[cursor_row * VGA_WIDTH + cursor_col] = char_to_cell(str[i]);
        cursor_col++;
    }
}
