#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>

#include "types.h"
#include "ninepin_5X8.h"

struct display {
	int width;
	int height;
	bool buffer[1024];
	void (*set_pixel_cb)(void *data, int x, int y, bool value);
	void *set_pixel_data;
};

static void display_set_pixel(void *display_, int x, int y, bool value);

struct display disp = {
	.width  = 32,
	.height = 8,
	.set_pixel_cb   = display_set_pixel,
	.set_pixel_data = &disp
};

static const unsigned char *font_find_char(const struct font *font, char ch)
{
	unsigned off;

	if (ch < font->start_char || ch > font->end_char)
		return NULL;

	off = (ch - font->start_char) * font->char_height;

	return &font->p[off];
}

static void display_show(struct display *display)
{
	static int first_time = 1;
	int x, y;

	if (!first_time) {
		printf("\033[%dA\r", display->height);
		fflush(stdout);
	}
	first_time = 0;

	for (y = display->height - 1; y >= 0; y--) {
		for (x = 0; x < display->width; x++) {
			if (display->buffer[y * display->width + x])
				printf(" #");
			else
				printf(" .");
		}
		printf("\n");
	}
	fflush(stdout);
}

static void display_reset(struct display *display)
{
	memset(display->buffer, 0, sizeof(display->buffer));
}

static void display_set_pixel(void *display_, int x, int y, bool value)
{
	struct display *display = display_;

	assert(x < display->width);
	assert(y < display->height);

	display->buffer[y * display->width + x] = value;
}

static bool display_draw_text(struct display *display,
			      const struct font *font,
			      uint8_t spacing,
			      const char *text,
			      int x, int y)
{
	int ch_i, ch_x, ch_y, ch_h, ch_w;
	int text_w, chars_n;
	int beg_x, text_x_off;
	int beg_y, text_y_off;

	chars_n = strlen(text);

	spacing = (chars_n > 1 ? spacing : 0);
	text_w = chars_n * font->char_width + spacing * chars_n;

	if (x + text_w <= 0 || x >= display->width)
		return false;

	if (y + font->char_height <= 0 || y >= display->height)
		return false;

	/* Clamp x,y: should be >= 0*/
	beg_x = max(x, 0);
	beg_y = max(y, 0);

	text_x_off = beg_x - x;
	text_y_off = beg_y - y;

	x = beg_x;
	y = beg_y;

	ch_h = min(y + (font->char_height - text_y_off), display->height) - y;
	ch_w = font->char_width + spacing;

	ch_i = text_x_off / ch_w;
	ch_x = text_x_off % ch_w;
	ch_y = text_y_off;

	for ( ; ch_i < chars_n && x < display->width; ch_i++) {
		const unsigned char *ch_p;
		unsigned char px;
		int ch_x_, ch_y_;
		int x_, y_;

		ch_p = font_find_char(font, text[ch_i]);
		if (!ch_p)
			/* Leave blank for unknown char */
			goto next;

		for (ch_y_ = ch_y, y_ = 0; y_ < ch_h; ch_y_++, y_++) {
			px = ch_p[font->char_height - 1 - ch_y_];

			for (ch_x_ = ch_x, x_ = 0;
			     ch_x_ < font->char_width &&
				     x + ch_x_ < display->width;
			     ch_x_++, x_++) {
				display->set_pixel_cb(display->set_pixel_data,
						      x + x_, y + y_, px & (1<<ch_x_));
			}
		}
next:
		x += ch_w - ch_x;
		/* Drop for the following chars */
		ch_x = 0;
	}

	return true;
}

static void disable_icanon(void)
{
	static struct termios term;

	tcgetattr(STDIN_FILENO, &term);

	/*
	 * ICANON normally takes care that one line at a time will be processed
	 * that means it will return if it sees a "\n" or an EOF or an EOL
	 */
	term.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

#define INPUT

int main(int argc, char *argv[])
{
	const struct font *font = &font_ninepin_5X8;
	uint8_t s = 0;
	int x, y;
	bool res;
#ifdef INPUT
	int ch;
#endif

	disable_icanon();

#ifdef INPUT
	x = y = 0;
#else
	x = disp.width - 1;
	y = 0;
#endif

	while (1) {
		display_reset(&disp);
		res = display_draw_text(&disp, font, s, "Not nice!", x, y);
		display_show(&disp);

#ifdef INPUT
		(void)res;
		ch = getc(stdin);
		if (ch == 0x41)
			/* Up */
			y++;
		else if (ch == 0x42)
			/* Down */
			y--;
		else if (ch == 0x43)
			/* Right */
			x++;
		else if (ch == 0x44)
			/* Left */
			x--;
		else if (ch == 's') {
			/* Spacing */
			s++;
			if (s > 5)
				s = 0;
		}
#else
		if (res)
			x--;
		else
			x = disp.width - 1;

		usleep(70000);
#endif
	}

	return 0;
}
