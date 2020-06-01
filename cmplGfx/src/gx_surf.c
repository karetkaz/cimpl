#include "gx_color.h"
#include "gx_surf.h"
#include "g3_math.h"

#include <stdlib.h>

GxImage createImage(GxImage recycle, int width, int height, int depth, ImageFlags flags) {
	if (recycle == NULL) {
		recycle = (GxImage) malloc(sizeof(struct GxImage));
		if (recycle == NULL) {
			return NULL;
		}
		flags |= freeImage;
	}
	else if (recycle->flags & freeData) {
		free(recycle->basePtr);
	}

	recycle->x0 = recycle->y0 = 0;
	recycle->width = (uint16_t) width;
	recycle->height = (uint16_t) height;
	recycle->depth = (uint8_t) depth;
	recycle->flags = flags;
	recycle->clipPtr = NULL;
	recycle->basePtr = NULL;
	recycle->tempPtr = NULL;

	if (width != recycle->width || height != recycle->height) {
		// dimension did not fit into 16 bit integer
		destroyImage(recycle);
		return NULL;
	}

	switch (depth) {
		default:
			destroyImage(recycle);
			return NULL;

		case 32:
			recycle->pixeLen = 4;
			break;

		case 24:
			recycle->pixeLen = 3;
			break;

		case 16:
		case 15:
			recycle->pixeLen = 2;
			break;

		case 8:
			recycle->pixeLen = 1;
			break;
	}

	recycle->scanLen = ((unsigned) width * recycle->pixeLen + 3) & ~3;
	if (width > 0 && height > 0 && depth > 0) {
		size_t size = recycle->scanLen * (size_t) height;
		size_t offs = 0;

		switch (flags & ImageType) {
			default:
				break;

			case Image2d:
				break;

			case ImageIdx:
				offs = size;
				size += sizeof(struct GxCLut);
				break;

			case ImageFnt:
				offs = size;
				size += sizeof(struct GxFLut);
				break;

			case Image3d:
				offs = size;
				size += width * (size_t) height * 4;    // z-buffer
				break;
		}

		if ((recycle->basePtr = malloc(size)) == NULL) {
			destroyImage(recycle);
			return NULL;
		}

		recycle->flags |= freeData;
		if (offs != 0) {
			recycle->tempPtr = recycle->basePtr + offs;
		}
	}

	return recycle;
}

GxImage sliceImage(GxImage recycle, GxImage parent, const GxRect roi) {
	ImageFlags flags = parent->flags & ~(freeImage|freeData);
	if (recycle == NULL) {
		recycle = (GxImage) malloc(sizeof(struct GxImage));
		if (recycle == NULL) {
			return NULL;
		}
		flags |= freeImage;
	}
	else if (recycle->flags & freeData) {
		free(recycle->basePtr);
	}

	recycle->x0 = recycle->y0 = 0;
	recycle->width = parent->width;
	recycle->height = parent->height;
	recycle->depth = parent->depth;
	recycle->flags = flags;
	recycle->pixeLen = parent->pixeLen;
	recycle->scanLen = parent->scanLen;
	recycle->clipPtr = parent->clipPtr;
	recycle->basePtr = parent->basePtr;
	recycle->tempPtr = parent->tempPtr;

	if (roi != NULL) {
		struct GxRect rect = *roi;
		recycle->basePtr = clipRect(parent, &rect);
		recycle->width = rect.width;
		recycle->height = rect.height;
	}
	return recycle;
}

void destroyImage(GxImage image) {
	if (image->flags & freeData) {
		image->flags &= ~freeData;
		if (image->basePtr != NULL) {
			free(image->basePtr);
		}
	}
	image->basePtr = NULL;
	if (image->flags & freeImage) {
		image->flags &= ~freeImage;
		free(image);
	}
}

void* clipRect(GxImage image, GxRect roi) {
	GxClip clp = getClip(image);

	roi->w += roi->x;
	roi->h += roi->y;

	if (clp->l > roi->x) {
		roi->x = clp->l;
	}
	if (clp->t > roi->y) {
		roi->y = clp->t;
	}
	if (clp->r < roi->w) {
		roi->w = clp->r;
	}
	if (clp->b < roi->h) {
		roi->h = clp->b;
	}

	if ((roi->w -= roi->x) <= 0) {
		return NULL;
	}

	if ((roi->h -= roi->y) <= 0) {
		return NULL;
	}

	return getPAddr(image, roi->x, roi->y);
}

int blitImage(GxImage image, int x, int y, GxImage src, GxRect roi, void *extra, bltProc blt) {
	struct GxRect clip;
	clip.x = roi ? roi->x : 0;
	clip.y = roi ? roi->y : 0;
	clip.w = roi ? roi->w : src->width;
	clip.h = roi ? roi->h : src->height;

	if (blt == NULL) {
		// error: operation is invalid or not implemented
		return -1;
	}

	if (x < 0) {
		clip.x -= x;
		clip.w += x;
	}
	if (y < 0) {
		clip.y -= y;
		clip.h += y;
	}
	char *sptr = clipRect(src, &clip);
	if (sptr == NULL) {
		// there is noting to copy
		return 0;
	}

	clip.x = x;
	clip.y = y;
	if (x < 0) {
		clip.w -= x;
	}
	if (y < 0) {
		clip.h -= y;
	}
	char *dptr = clipRect(image, &clip);
	if (dptr == NULL) {
		// there are no pixels to set
		return 0;
	}

	while (clip.h--) {
		if (blt(dptr, sptr, extra, (size_t) clip.w) < 0) {
			return -1;
		};
		dptr += image->scanLen;
		sptr += src->scanLen;
	}
	return 0;
}

int resizeImage(GxImage image, GxRect rect, GxImage src, GxRect roi, int interpolation) {
	if (image->depth != src->depth) {
		return -2;
	}

	struct GxRect srec;
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
		if (roi->w < srec.w) {
			srec.w = roi->w;
		}
		if (roi->h < srec.h) {
			srec.h = roi->h;
		}
	}

	struct GxRect drec;
	if (rect != NULL) {
		drec = *rect;
	}
	else {
		drec.x = drec.y = 0;
		drec.w = image->width;
		drec.h = image->height;
	}

	if (drec.w <= 0 || drec.h <= 0) {
		return 0;
	}
	if (srec.w <= 0 || srec.h <= 0) {
		return 0;
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

	char *dptr = clipRect(image, &drec);
	if (dptr == NULL) {
		return 0;
	}

	if (!interpolation || dx >= 0x20000 || dy >= 0x20000) {
		for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
			for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
				setPixel(image, drec.x + x, drec.y + y, getPixel(src, sx >> 16, sy >> 16));
			}
		}
		return 0;
	}

	x0 -= 0x8000;
	y0 -= 0x8000;
	for (int y = 0, sy = y0; y < drec.h; ++y, sy += dy) {
		for (int x = 0, sx = x0; x < drec.w; ++x, sx += dx) {
			setPixel(image, drec.x + x, drec.y + y, getPixelLinear(src, sx, sy));
		}
	}
	return 0;
}

static inline double gauss(double x, double sigma) {
	double t = x / sigma;
	double SQRT_2_PI_INV = 0.398942280401432677939946059935;
	return SQRT_2_PI_INV * exp(-0.5 * t * t) / sigma;
}

int blurImage(GxImage image, int radius, double sigma) {

	int size = radius * 2 + 1;
	if (image->depth != 32) {
		return -1;
	}
	if (size >= 1024) {
		return -1;
	}
	if (size < 1) {
		return 0;
	}

	double kernelSum = 0;
	double kernelFlt[1024];
	for (int i = 0; i < size; i += 1) {
		kernelFlt[i] = gauss(radius - i, sigma);
		kernelSum += kernelFlt[i];
	}

	uint32_t kernel[1024];
	for (int i = 0; i < size; i++) {
		kernel[i] = 65536 * (kernelFlt[i] / kernelSum);
	}

	int width = image->width;
	int height = image->height;
	GxImage tmp = createImage(NULL, width, height, image->depth, Image2d);

	if (tmp == NULL) {
		return -1;
	}

	// x direction: inout -> temp
	for (int y = 0; y < height; y += 1) {
		for (int x = 0; x < width; x += 1) {
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;
			for (int i = 0; i < size; i += 1) {
				int _x = x + i - radius;
				if (_x >= 0 && _x < width) {
					uint32_t col = getPixel(image, _x, y);
					int32_t _k = kernel[i];
					r += _k * rch(col);
					g += _k * gch(col);
					b += _k * bch(col);
				}
			}
			setPixel(tmp, x, y, sat_srgb(255, r >> 16, g >> 16, b >> 16).val);
		}
	}

	// y direction: temp -> inout
	for (int y = 0; y < height; y += 1) {
		for (int x = 0; x < width; x += 1) {
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;
			for (int i = 0; i < size; i += 1) {
				int _y = y + i - radius;
				if (_y >= 0 || _y < height) {
					uint32_t col = getPixel(tmp, x, _y);
					int32_t _k = kernel[i];
					r += _k * rch(col);
					g += _k * gch(col);
					b += _k * bch(col);
				}
			}
			setPixel(image, x, y, sat_srgb(255, r >> 16, g >> 16, b >> 16).val);
		}
	}
	destroyImage(tmp);
	return 0;
}
