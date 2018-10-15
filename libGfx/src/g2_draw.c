#include "gx_surf.h"

void g2_drawRect(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color) {
	g2_fillRect(surf, x1, y1, x2, y1 + 1, color);
	g2_fillRect(surf, x1, y2, x2 + 1, y2 + 1, color);
	g2_fillRect(surf, x1, y1, x1 + 1, y2, color);
	g2_fillRect(surf, x2, y1, x2 + 1, y2, color);
}
void g2_fillRect(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color) {
	gx_Clip roi = gx_getclip(surf);
	if (x1 > x2) {
		int t = x1;
		x1 = x2;
		x2 = t;
	}
	if (x1 < roi->xmin) {
		x1 = roi->xmin;
	}
	if (x2 > roi->xmax) {
		x2 = roi->xmax;
	}
	if (y1 > y2) {
		int t = y1;
		y1 = y2;
		y2 = t;
	}
	if (y1 < roi->ymin) {
		y1 = roi->ymin;
	}
	if (y2 > roi->ymax) {
		y2 = roi->ymax;
	}
	// TODO: optimize for hline, vline and pixel.
	for (int y = y1; y < y2; y++) {
		for (int x = x1; x < x2; x++) {
			gx_setpixel(surf, x, y, color);
		}
	}
}

void g2_drawOval(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color) {
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
		gx_setpixel(surf, x1, y1, color);
		gx_setpixel(surf, x1, y2, color);
		gx_setpixel(surf, x2, y1, color);
		gx_setpixel(surf, x2, y2, color);

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
	gx_setpixel(surf, x1, y1, color);
	gx_setpixel(surf, x1, y2, color);
	gx_setpixel(surf, x2, y1, color);
	gx_setpixel(surf, x2, y2, color);
}
void g2_fillOval(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color) {
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
	x1 +=dx & 1;

	dx += dx & 1;
	dy += dy & 1;

	int sx = dx * dx;
	int sy = dy * dy;

	int r = sx * dy >> 2;
	dx = 0;
	dy = r << 1;

	while (y1 < y2) {
		g2_fillRect(surf, x1, y1, x1 + 1, y2, color);
		g2_fillRect(surf, x2, y1, x2 + 1, y2, color);

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
	g2_fillRect(surf, x1, y1, x1 + 1, y2, color);
	g2_fillRect(surf, x2, y1, x2 + 1, y2, color);
}

void g2_drawLine(gx_Surf surf, int x1, int y1, int x2, int y2, uint32_t color) {
	//~ TODO : replace Bresenham with DDA, resolve clipping

	int dx = x2 - x1;
	int sx = 1;
	if (dx < 0) {
		dx = -dx;
		sx = -1;
	}

	int dy = y2 - y1;
	int sy = 1;
	if (dy < 0) {
		dy = -dy;
		sy = -1;
	}

	if (dx > dy) {
		int r = dx >> 1;
		while (x1 != x2) {
			gx_setpixel(surf, x1, y1, color);
			if ((r += dy) > dx) {
				y1 += sy;
				r -= dx;
			}
			x1 += sx;
		}
	}
	else {
		int r = dy >> 1;
		while (y1 != y2) {
			gx_setpixel(surf, x1, y1, color);
			if ((r+=dx) > dy) {
				x1 += sx;
				r -= dy;
			}
			y1 += sy;
		}
	}
	gx_setpixel(surf, x1, y1, color);
}
void g2_drawBez2(gx_Surf surf, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {

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
		g2_drawLine(surf, x1, y1, x2, y2, color);
		x1 = x2;
		y1 = y2;
	}
	g2_drawLine(surf, x1, y1, x3, y3, color);
}
void g2_drawBez3(gx_Surf surf, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32_t color) {

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
		g2_drawLine(surf, x1, y1, x2, y2, color);
		x1 = x2;
		y1 = y2;
	}
	g2_drawLine(surf, x1, y1, x4, y4, color);
}

void g2_drawChar(gx_Surf surf, int x, int y, gx_Surf font, int chr, uint32_t color) {

	if ((font->flags & SurfType) != Surf_fnt) {
		return;
	}

	struct gx_Rect clip;
	struct gx_FDIR *face = &font->LLUTPtr->data[chr & 255];

	clip.x = x + face->pad_x;
	clip.y = y + face->pad_y;
	clip.w = face->width;
	clip.h = font->LLUTPtr->height;
	if (clip.w <= 0 || clip.h <= 0) {
		return;
	}

	char *sptr = (char*)face->basePtr;
	char *dptr = (char*)gx_cliprect(surf, &clip);
	cblt_proc cblt = gx_getcbltf(cblt_set_mix, surf->depth);
	if (dptr == NULL || sptr == NULL || cblt == NULL) {
		return;
	}

	while (clip.h-- > 0) {
		cblt(dptr, sptr, (void*)(size_t)color, clip.w);
		dptr += surf->scanLen;
		sptr += face->width;
	}
}
void g2_clipText(gx_Rect rect, gx_Surf font, const char *text) {
	char chr;
	int x = 0;
	int y = 0;
	int x0 = x;

	if ((font->flags & SurfType) != Surf_fnt) {
		return;
	}
	if (!font->basePtr) {
		return;
	}

	rect->w = 0;
	rect->h = font->LLUTPtr->height;
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
				// fall-trough
			case '\n':
				if (*text == '\0') {
					// new line at end ot text
					break;
				}
				y += font->LLUTPtr->height;
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
void g2_drawText(gx_Surf surf, int x, int y, gx_Surf font, const char *text, uint32_t color) {
	int chr, x0 = x;

	if ((font->flags & SurfType) != Surf_fnt) {
		return;
	}
	if (!font->basePtr) {
		return;
	}

	while ((chr = *text++) != 0) {
		switch (chr) {
			default:
				g2_drawChar(surf, x, y, font, chr, color);
				x += font->LLUTPtr->data[chr].width;
				break;

			case '\r':
				if (*text == '\n') {
					// windows style line end (CrLf)
					break;
				}
				// fall-trough
			case '\n':
				if (*text == '\0') {
					// new line at end of text
					break;
				}
				y += font->LLUTPtr->height;
				x = x0;
				break;
		}
	}
}

int gx_copySurf(gx_Surf surf, int x, int y, gx_Surf src, gx_Rect roi) {
	struct gx_Rect clip;
	clip.x = roi ? roi->x : 0;
	clip.y = roi ? roi->y : 0;
	clip.w = roi ? roi->w : src->width;
	clip.h = roi ? roi->h : src->height;

	if (x < 0) {
		clip.x -= x;
	}
	if (y < 0) {
		clip.y -= y;
	}
	char *sptr = gx_cliprect(src, &clip);
	if (sptr == NULL) {
		return -1;
	}

	clip.x = x;
	clip.y = y;
	clip.w = roi ? roi->w : src->width;
	clip.h = roi ? roi->h : src->height;
	char *dptr = gx_cliprect(surf, &clip);
	if (dptr == NULL) {
		return -1;
	}

	cblt_proc cblt = gx_getcbltf(surf->depth, src->depth);
	if(cblt == NULL) {
		return -1;
	}

	while (clip.h--) {
		cblt(dptr, sptr, NULL, (size_t) clip.w);
		dptr += surf->scanLen;
		sptr += src->scanLen;
	}

	return 0;
}
int gx_zoomSurf(gx_Surf surf, gx_Rect rect, gx_Surf src, gx_Rect roi, int interpolate) {
	if (surf->depth != src->depth) {
		return -2;
	}

	struct gx_Rect srec;
	srec.x = srec.y = 0;
	srec.w = src->width;
	srec.h = src->height;
	if (roi != NULL) {
		if (roi->x > 0) {
			srec.w -= roi->x;
			srec.x = roi->x;
		}
		if (roi->y > 0) {
			srec.h -= roi->y;
			srec.y = roi->y;
		}
		if (roi->w < srec.w)
			srec.w = roi->w;
		if (roi->h < srec.h)
			srec.h = roi->h;
	}

	struct gx_Rect drec;
	if (rect != NULL) {
		drec = *rect;
	}
	else {
		drec.x = drec.y = 0;
		drec.w = surf->width;
		drec.h = surf->height;
	}

	if (drec.w <= 0 || drec.h <= 0) {
		return -1;
	}
	if (srec.w <= 0 || srec.h <= 0) {
		return -1;
	}

	int32_t x0 = 0;
	int32_t y0 = 0;
	int32_t dx = (srec.w << 16) / drec.w;
	int32_t dy = (srec.h << 16) / drec.h;

	if (drec.x < 0) {
		x0 = -drec.x;
	}
	if (drec.y < 0) {
		y0 = -drec.y;
	}

	char *dptr = gx_cliprect(surf, &drec);
	if (dptr == NULL) {
		return -1;
	}

	if (dx & 0xfffe0000 || dy & 0xfffe0000) {
		// TODO: use mip mapping
		interpolate = 0;
	}

	x0 = x0 * dx + (srec.x << 16);
	y0 = y0 * dy + (srec.y << 16);

	if (surf->depth == 32) {
		for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
			uint32_t *ptr = (void*)dptr;
			for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
				*ptr++ = gx_getpix16(src, sx, sy, interpolate);
			}
			dptr += surf->scanLen;
		}
	}
	else if (surf->depth == 8) {
		for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
			char *ptr = (void*)dptr;
			for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
				*ptr++ = gx_getpix16(src, sx, sy, interpolate);
			}
			dptr += surf->scanLen;
		}
	}
	else {
		for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
			for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
				gx_setpixel(surf, drec.x + x, drec.y + y, gx_getpix16(src, sx, sy, interpolate));
			}
		}
	}
	return 0;
}
