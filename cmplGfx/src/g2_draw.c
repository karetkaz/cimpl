#include "gx_surf.h"

void drawRect(GxImage image, int x1, int y1, int x2, int y2, uint32_t color) {
	if (x1 > x2) {
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	struct GxRect clip = {
		.x = x1,
		.y = y1,
		.w = x2 - x1 + 1,
		.h = y2 - y1 + 1,
	};
	bltProc blt = getBltProc(blt_set_col, image->depth);
	char *dptr = (char*) clipRect(image, &clip);
	if (dptr == NULL || blt == NULL) {
		return;
	}

	if (y2 == y1) {
		// special case: horizontal line
		blt(dptr, NULL, &color, clip.w);
		return;
	}
	if (x2 == x1) {
		// special case: vertical line
		for (int y = 0; y < clip.h; y++) {
			blt(dptr, NULL, &color, 1);
			dptr += image->scanLen;
		}
		return;
	}

	int top = y1 >= clip.y;
	int left = x1 >= clip.x;
	int right = x2 <= clip.x + clip.w;
	int bottom = y2 <= clip.y + clip.h;

	if (top) {
		blt(dptr, NULL, &color, clip.w);
		dptr += image->scanLen;
	}
	if (left && right) {
		size_t r = (clip.w - 1) * image->pixelLen;
		for (int y = top; y < clip.h - bottom; y++) {
			blt(dptr, NULL, &color, 1);
			blt(dptr + r, NULL, &color, 1);
			dptr += image->scanLen;
		}
	}
	else if (left) {
		for (int y = top; y < clip.h - bottom; y++) {
			blt(dptr, NULL, &color, 1);
			dptr += image->scanLen;
		}
	}
	else if (right) {
		size_t r = (clip.w - 1) * image->pixelLen;
		for (int y = top; y < clip.h - bottom; y++) {
			blt(dptr + r, NULL, &color, 1);
			dptr += image->scanLen;
		}
	}
	else {
		dptr += (clip.h - top - bottom) * image->scanLen;
	}
	if (bottom) {
		blt(dptr, NULL, &color, clip.w);
	}
}
void fillRect(GxImage image, int x1, int y1, int x2, int y2, uint32_t color) {
	if (x1 > x2) {
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	struct GxRect clip = {
		.x = x1,
		.y = y1,
		.w = x2 - x1 + 1,
		.h = y2 - y1 + 1,
	};
	bltProc blt = getBltProc(blt_set_col, image->depth);
	char *dptr = (char*) clipRect(image, &clip);
	if (dptr == NULL || blt == NULL) {
		return;
	}

	while (clip.h-- > 0) {
		blt(dptr, NULL, &color, clip.w);
		dptr += image->scanLen;
	}
}

void drawOval(GxImage image, int x1, int y1, int x2, int y2, uint32_t color) {
	if (x1 > x2) {
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	int dx = x2 - x1;
	int dy = y2 - y1;

	x2 = x1 += dx >> 1;
	x1 += dx & 1;

	dx += dx & 1;
	dy += dy & 1;

	int sx = dx * dx;
	int sy = dy * dy;
	int r = sx * dy >> 2;

	dx = 0;
	dy = r << 1;

	while (y1 < y2) {
		setPixel(image, x1, y1, color);
		setPixel(image, x2, y1, color);
		setPixel(image, x1, y2, color);
		setPixel(image, x2, y2, color);

		if (r >= 0) {
			x1 -= 1;
			x2 += 1;
			r -= dx += sy;
		}

		if (r < 0) {
			y1 += 1;
			y2 -= 1;
			r += dy -= sx;
		}
	}
	setPixel(image, x1, y1, color);
	setPixel(image, x2, y1, color);
	setPixel(image, x1, y2, color);
	setPixel(image, x2, y2, color);
}
void fillOval(GxImage image, int x1, int y1, int x2, int y2, uint32_t color) {
	if (x1 > x2) {
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2) {
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	int dx = x2 - x1;
	int dy = y2 - y1;

	x2 = x1 += dx >> 1;
	x1 += dx & 1;

	dx += dx & 1;
	dy += dy & 1;

	int sx = dx * dx;
	int sy = dy * dy;

	int r = sx * dy >> 2;
	dx = 0;
	dy = r << 1;

	while (y1 < y2) {
		fillRect(image, x1, y1, x2, y1, color);
		fillRect(image, x1, y2, x2, y2, color);

		if (r >= 0) {
			x1 -= 1;
			x2 += 1;
			r -= dx += sy;
		}

		if (r < 0) {
			y1 += 1;
			y2 -= 1;
			r += dy -= sx;
		}
	}
	fillRect(image, x1, y1, x2, y2, color);
}

void drawLine(GxImage image, int x1, int y1, int x2, int y2, uint32_t color) {
	int dx = ((x2 - x1) << 16) / (y2 == y1 ? 1 : y2 - y1);
	if ((dx >> 16) == (dx >> 31)) {
		if (y1 > y2) {
			int y = y1;
			y1 = y2;
			y2 = y;
			x1 = x2;
		}
		int x = (x1 << 16) + 0x8000;
		if (y1 < 0) {
			x -= dx * y1;
			y1 = 0;
		}
		if (y2 > image->height) {
			y2 = image->height;
		}

		for (int y = y1; y <= y2; y++) {
			setPixel(image, x >> 16, y, color);
			x += dx;
		}
	} else {
		int dy = ((y2 - y1) << 16) / (x2 == x1 ? 1 : x2 - x1);
		if (x1 > x2) {
			int x = x1;
			x1 = x2;
			x2 = x;
			y1 = y2;
		}
		int y = (y1 << 16) + 0x8000;
		if (x1 < 0) {
			y -= dy * x1;
			x1 = 0;
		}
		if (x2 > image->width) {
			x2 = image->width;
		}

		for (int x = x1; x <= x2; x++) {
			setPixel(image, x, y >> 16, color);
			y += dy;
		}
	}
}

void drawBez2(GxImage image, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {

	int px_0 = x1;
	int py_0 = y1;
	int px_1 = 2 * (x2 - x1);
	int py_1 = 2 * (y2 - y1);
	int px_2 = x3 - 2 * x2 + x1;
	int py_2 = y3 - 2 * y2 + y1;

	double dt = 1. / 128;
	for (double t = dt; t < 1; t += dt) {
		x2 = (px_2 * t + px_1) * t + px_0;
		y2 = (py_2 * t + py_1) * t + py_0;
		drawLine(image, x1, y1, x2, y2, color);
		x1 = x2;
		y1 = y2;
	}
	drawLine(image, x1, y1, x3, y3, color);
}
void drawBez3(GxImage image, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32_t color) {

	int px_0 = x1;
	int py_0 = y1;
	int px_1 = 3 * (x2 - x1);
	int py_1 = 3 * (y2 - y1);
	int px_2 = 3 * (x3 - x2) - px_1;
	int py_2 = 3 * (y3 - y2) - py_1;
	int px_3 = x4 - px_2 - px_1 - px_0;
	int py_3 = y4 - py_2 - py_1 - py_0;

	double dt = 1. / 128;
	for (double t = dt; t < 1; t += dt) {
		x2 = ((px_3 * t + px_2) * t + px_1) * t + px_0;
		y2 = ((py_3 * t + py_2) * t + py_1) * t + py_0;
		drawLine(image, x1, y1, x2, y2, color);
		x1 = x2;
		y1 = y2;
	}
	drawLine(image, x1, y1, x4, y4, color);
}

void clipText(GxRect rect, GxImage font, const char *text) {
	char chr;
	int x = 0;
	int y = 0;
	int x0 = x;

	if ((font->flags & ImageType) != ImageFnt) {
		return;
	}
	if (font->basePtr == NULL) {
		return;
	}

	rect->w = 0;
	rect->h = font->height;
	if (text == NULL) {
		return;
	}

	while ((chr = *text++) != 0) {
		switch (chr) {
			default:
				x += font->LLUTPtr->data[chr & 0xff].width;
				break;

			case '\r':
				if (*text == '\n') {
					// windows style line end (CrLf)
					break;
				}
				// fall through
			case '\n':
				if (*text == '\0') {
					// new line at end ot text
					break;
				}
				y += font->height;
				x = x0;
				break;

				/* todo calculate correct position based on the current width
				case '\t':
					width += 8 * face->width;
					break;
				// */
		}

		if (rect->w < x) {
			rect->w = x;
		}
	}
	rect->h += y;
}
void drawChar(GxImage image, int x, int y, GxImage font, int chr, uint32_t color) {

	if ((font->flags & ImageType) != ImageFnt) {
		return;
	}

	struct GxFace *face = &font->LLUTPtr->data[chr & 255];
	struct GxRect clip;

	clip.x = x + face->pad_x;
	clip.y = y + face->pad_y;
	clip.w = face->width;
	clip.h = font->height;
	if (clip.w <= 0 || clip.h <= 0) {
		return;
	}

	char *sptr = (char*)face->basePtr;
	char *dptr = (char*) clipRect(image, &clip);
	bltProc blt = getBltProc(blt_set_mix, image->depth);
	if (dptr == NULL || sptr == NULL || blt == NULL) {
		return;
	}

	while (clip.h-- > 0) {
		blt(dptr, sptr, &color, clip.w);
		dptr += image->scanLen;
		sptr += font->scanLen;
	}
}
void drawText(GxImage image, int x, int y, GxImage font, const char *text, uint32_t color) {
	int chr, x0 = x;

	if ((font->flags & ImageType) != ImageFnt) {
		return;
	}
	if (!font->basePtr) {
		return;
	}
	if (text == NULL) {
		return;
	}

	while ((chr = *text++) != 0) {
		switch (chr) {
			default:
				drawChar(image, x, y, font, chr, color);
				x += font->LLUTPtr->data[chr].width;
				break;

			case '\r':
				if (*text == '\n') {
					// windows style line end (CrLf)
					break;
				}
				// fall through
			case '\n':
				if (*text == '\0') {
					// new line at end of text
					break;
				}
				y += font->height;
				x = x0;
				break;
		}
	}
}
