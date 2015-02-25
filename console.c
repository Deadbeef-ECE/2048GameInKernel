/** @file console.c 
 *
 *  @brief Skeleton implementation of device-driver library.
 *  This file exists so that the "null project" builds and
 *  links without error.
 *
 *  @author Yuhang Jiang(yuhangj)
 *  @bug This file should shrink repeatedly AND THEN BE DELETED.
 */

#include <p1kern.h>
#include <stdio.h>
#include <x86/asm.h>
#include <simics.h>

#define CONSOLE_SZ 80*25
/* 8bits in total : 1111 1111(0xff) */
#define COLOR_BOUND 0x100
#define VISIBLE 0
#define HIDDEN 1

typedef struct
{
	int row;
	int col;
}cursor_t;

cursor_t cursor;
int cursor_hidden = VISIBLE;
int cursor_color = FGND_LGRAY;

/** @brief  Scroll the screen with one line
 *
 *  @return The input character
 */
void console_scroll(){
	char* head = (char*) CONSOLE_MEM_BASE;
	char* start = (char*)(CONSOLE_MEM_BASE + CONSOLE_WIDTH * 2);
	char* end = (char*)(CONSOLE_MEM_BASE + CONSOLE_SZ * 2);

	while(start != end)
		*head++ = *start++;
	start = (end - CONSOLE_WIDTH * 2);
	while(start != end){
		*start++ = ' ';
		*start++ = cursor_color;
	}
	return;
}

/** @brief Prints character ch at the current location
 *         of the cursor.
 *
 *  If the character is a newline ('\n'), the cursor is
 *  be moved to the beginning of the next line (scrolling if necessary).
 *  If the character is a carriage return ('\r'), the cursor
 *  is immediately reset to the beginning of the current
 *  line, causing any future output to overwrite any existing
 *  output on the line.  If backspace ('\b') is encountered,
 *  the previous character is erased.  See the main console.c description
 *  for more backspace behavior.
 *
 *  @param ch the character to print
 *  @return The input character
 */
int putbyte( char ch )
{
	int pos;
	pos = cursor.row * CONSOLE_WIDTH + cursor.col;
	switch(ch) {
		case '\n':
			cursor.row += 1;
			cursor.col = 0;
			if(cursor.row >= CONSOLE_HEIGHT){
				cursor.row = CONSOLE_HEIGHT - 1;
				console_scroll();
			}
			break;
		case '\r':
			cursor.row = 0;
			break; 
		case '\b':
			pos -= 1;
			if(pos < 0)
				pos  = 0;
			cursor.row = pos / CONSOLE_WIDTH;
			cursor.col = pos % CONSOLE_WIDTH;
			draw_char(cursor.row, cursor.col, ' ', cursor_color);
			break;
		default:
			draw_char(cursor.row, cursor.col, ch, cursor_color);

			pos += 1;
			cursor.row = pos / CONSOLE_WIDTH;
			cursor.col = pos % CONSOLE_WIDTH;

			if(cursor.row >= CONSOLE_HEIGHT){
				cursor.row = CONSOLE_HEIGHT - 1;
				console_scroll();
			}
			break;
	}
	set_cursor(cursor.row, cursor.col);
	if(!cursor_hidden)
		show_cursor();

	return ch; 
}

/** @brief Prints the string s, starting at the current
 *         location of the cursor.
 *
 *  If the string is longer than the current line, the
 *  string fills up the current line and then
 *  continues on the next line. If the string exceeds
 *  available space on the entire console, the screen
 *  scrolls up one line, and then the string
 *  continues on the new line.  If '\n', '\r', and '\b' are
 *  encountered within the string, they are handled
 *  as per putbyte. If len is not a positive integer or s
 *  is null, the function has no effect.
 *
 *  @param s The string to be printed.
 *  @param len The length of the string s.
 *  @return Void.
 */
void 
putbytes( const char *s, int len )
{
	int i = 0;
	const char *p = s;
	for( ; i < len; i++){
		putbyte(*p++);
	}
}

/** @brief Changes the foreground and background color
 *         of future characters printed on the console.
 *
 *  If the color code is invalid, the function has no effect.
 *
 *  @param color The new color code.
 *  @return 0 on success or integer error code less than 0 if
 *          color code is invalid.
 */
int
set_term_color( int color )
{
  	if(color >= COLOR_BOUND || color < 0)
  		return -1;
  	else {
  		cursor_color = color;
  		return 0;
  	}
}

/** @brief Writes the current foreground and background
 *         color of characters printed on the console
 *         into the argument color.
 *  @param color The address to which the current color
 *         information will be written.
 *  @return Void.
 */
void
get_term_color( int *color )
{
	*color = cursor_color;
}

/** @brief Sets the position of the cursor to the
 *         position (row, col).
 *
 *  Subsequent calls to putbytes should cause the console
 *  output to begin at the new position. If the cursor is
 *  currently hidden, a call to set_cursor() does not show
 *  the cursor.
 *
 *  @param row The new row for the cursor.
 *  @param col The new column for the cursor.
 *  @return 0 on success or integer error code less than 0 if
 *          cursor location is invalid.
 */
int
set_cursor( int row, int col )
{
	if(row < 0 || row >= CONSOLE_HEIGHT ||
		col < 0 ||  col >= CONSOLE_WIDTH ){
		return -1;
	}
	else{
		cursor.row = row;
		cursor.col = col;
		return 0;
	}
}

/** @brief Writes the current position of the cursor
 *         into the arguments row and col.
 *  @param row The address to which the current cursor
 *         row will be written.
 *  @param col The address to which the current cursor
 *         column will be written.
 *  @return Void.
 */
void
get_cursor( int *row, int *col )
{
	*row = cursor.row;
	*col = cursor.col;
}

/** @brief Hides the cursor.
 *
 *  Subsequent calls to putbytes do not cause the
 *  cursor to show again.
 *
 *  @return Void.
 */
void
hide_cursor()
{
	int pos = CONSOLE_SZ + 1;

	int lsb_index = pos & 0xff;
	int msb_index = (pos >> 8) & 0xff;
	
	outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
	outb(CRTC_DATA_REG, msb_index);

	outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
	outb(CRTC_DATA_REG, lsb_index);

	cursor_hidden = HIDDEN;
}

/** @brief Shows the cursor.
 *  
 *  If the cursor is already shown, the function has no effect.
 *
 *  @return Void.
 */
void
show_cursor()
{
	int pos = cursor.col + cursor.row * CONSOLE_WIDTH;

	int lsb_index = pos & 0xff;
	int msb_index = (pos >> 8) & 0xff;

	outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
	outb(CRTC_DATA_REG, msb_index);

	outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
	outb(CRTC_DATA_REG, lsb_index);
}

/** @brief Clears the entire console.
 *
 * The cursor is reset to the first row and column
 *
 *  @return Void.
 */
void 
clear_console()
{
	int i;
	int end = CONSOLE_SZ;
	char *p = (char*)CONSOLE_MEM_BASE;
	for(i = 0; i < end; i++){
		*p++ = ' ';
		*p++ = cursor_color;
	}

	set_cursor(0, 0);

	if(!cursor_hidden) {
		show_cursor();    
  }
}

/** @brief Prints character ch with the specified color
 *         at position (row, col).
 *
 *  If any argument is invalid, the function has no effect.
 *
 *  @param row The row in which to display the character.
 *  @param col The column in which to display the character.
 *  @param ch The character to display.
 *  @param color The color to use to display the character.
 *  @return Void.
 */
void
draw_char( int row, int col, int ch, int color )
{
	if(row < 0 || row >= CONSOLE_HEIGHT ||
		col < 0 ||  col >= CONSOLE_WIDTH ||
		color >= COLOR_BOUND || color < 0)
		return;
	int pos = row * CONSOLE_WIDTH + col;
	*(char*)(CONSOLE_MEM_BASE + pos * 2) = ch;
	*(char*)(CONSOLE_MEM_BASE + pos * 2 + 1) = color;
}

/** @brief Returns the character displayed at position (row, col).
 *  @param row Row of the character.
 *  @param col Column of the character.
 *  @return The character at (row, col).
 */
char
get_char( int row, int col )
{
	int pos = row * CONSOLE_WIDTH + col;
	return *(char*)(CONSOLE_MEM_BASE + pos * 2);
}

