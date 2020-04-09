#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gx_surf.h"

#define BMP_OS2 0x02

#pragma pack (push, 1)
typedef struct {	// BMP_HDR
	uint16_t manfact;
	uint32_t filesize;
	uint32_t version;
	uint32_t imgstart;
	uint32_t hdrsize;
} BMP_HDR;

typedef struct {	// BMP_INF
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t depth;
	uint32_t encoding;		// 0:no compression;1: RLE 8 bit;2: RLE 4 bit
	uint32_t imgsize;
	uint32_t hdpi;
	uint32_t vdpi;
	uint32_t usedcol;
	uint32_t impcol;
	uint8_t b_enc;
	uint8_t g_enc;
	uint8_t r_enc;
	uint8_t filler;
	// os/2 2+
	uint8_t res_889[24];
} BMP_INF;

typedef struct {	// CUR_HDR
	uint16_t idreserved;
	uint16_t idtype;
	uint16_t idcount;
} CUR_HDR;

typedef struct {	// CUR_DIR		CURSORDIRENTRY
	uint8_t width;
	uint8_t height;
	uint8_t colorcount;
	uint8_t breserved;
	uint16_t hotspotx;
	uint16_t hotspoty;
	uint32_t lbytesinres;
	uint32_t imageoffset;
} CUR_DIR;

typedef struct {	// CUR_BMP		BITMAPINFOHEADER
	uint32_t size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t depth;
	uint32_t encoding;
	uint32_t imgsize;
	int32_t hdpi;
	int32_t vdpi;
	uint32_t usedcol;
	uint32_t impcol;
} CUR_BMP;
#pragma pack(pop)


// Bitmap line readers
typedef int (*readBmp)(unsigned char*, FILE*, unsigned);
static int readRaw08(unsigned char *dst, FILE *src, unsigned cnt) {
	return !fread(dst, cnt, 1, src);
}
static int readRle08(unsigned char *dst, FILE *src, unsigned cnt) {
	for (;;) {
		if (fread(dst, 2, 1, src) == 0) {
			return 0;
		}
		if (*dst == 0) { // read n pixels
			switch (dst[1]) {
				case 0:
					return 1;                // eol
				case 1:
					return 1;                // eof
				case 2:
					fseek(src, 2, SEEK_CUR);
					break;
				default : {
					unsigned char *tmp = dst + ((cnt = dst[1]) >> 1);
					if (!fread(tmp, (cnt + 1) & (~1), 1, src)) {
						return 0;
					}
					while (cnt--) {
						*dst++ = *tmp++;
					}
					break;
				}
			}
		}
		else { // fill n pixels
			int tmp = dst[1];
			cnt = dst[0];
			while (cnt--) {
				*dst++ = tmp;
			}
		}
	}
	return 1;
}
static int readRaw04(unsigned char *dst, FILE *src, unsigned cnt) {
	unsigned char* tmp = dst + (cnt);

	if (!fread(tmp, cnt, 1, src))
		return 0;

	while (cnt--) {
		*dst++ = (*tmp >> 4) & 0x0f;
		*dst++ = (*tmp >> 0) & 0x0f;
		tmp++;
	}
	return 1;
}
static int readRle04(unsigned char *dst, FILE *src, unsigned cnt) {
	for (;;) {
		if (fread(dst, 2, 1, src) == 0)
			return 0;
		if (dst[0] == 0) {
			switch (dst[1]) {
				case 0  : return 1;				// eol
				case 1  : return 1;				// eof
				case 2  : fseek(src,2,SEEK_CUR); break;		// jmp
				default : {
					int pos = 0;
					unsigned char *tmp = dst + ((cnt = dst[1]) >> 0);
					if (!fread(tmp, ((cnt + 3) >> 1) & (~1), 1, src)) {
						return 0;
					}
					while (cnt--) {
						*dst++ = (*tmp >> (pos = 4 - pos)) & 0x0f;
						if (!pos) {
							tmp++;
						}
					}
				}
					break;
			}
		}
		else {                                // fill cnt pixels
			int pos = 0;
			int tmp = dst[1];
			cnt = dst[0];
			while (cnt--) {
				*dst++ = (tmp >> (pos = 4 - pos)) & 0x0f;
			}
		}
	}
	return 1;
}
static int readRaw01(unsigned char *dst, FILE *src, unsigned cnt) {
	unsigned char* tmp = dst + (cnt<<3) - cnt;

	if (!fread(tmp, cnt, 1, src)) {
		return 0;
	}

	while (cnt--) {
		*dst++ = (*tmp >> 7) & 0x01;
		*dst++ = (*tmp >> 6) & 0x01;
		*dst++ = (*tmp >> 5) & 0x01;
		*dst++ = (*tmp >> 4) & 0x01;
		*dst++ = (*tmp >> 3) & 0x01;
		*dst++ = (*tmp >> 2) & 0x01;
		*dst++ = (*tmp >> 1) & 0x01;
		*dst++ = (*tmp >> 0) & 0x01;
		tmp++;
	}
	return 1;
}

// Bitmap line writers
typedef int (*writeBmp)(FILE*, unsigned char*, unsigned);
static int writeRaw08(FILE *dst, unsigned char *src, unsigned cnt) {
	return !fwrite(src, cnt, 1, dst);
}
static int writeRaw04(FILE *dst, unsigned char *src, unsigned cnt) {
	while (cnt--) {
		int tmp = 0;
		tmp |= (*src++ & 0x0f) << 4;
		tmp |= (*src++ & 0x0f) << 0;
		fputc(tmp,dst);
	}
	return 1;
}
static int writeRaw01(FILE *dst, unsigned char *src, unsigned cnt) {
	while (cnt--) {
		int tmp = 0;
		tmp |= (*src++ & 0x01) << 7;
		tmp |= (*src++ & 0x01) << 6;
		tmp |= (*src++ & 0x01) << 5;
		tmp |= (*src++ & 0x01) << 4;
		tmp |= (*src++ & 0x01) << 3;
		tmp |= (*src++ & 0x01) << 2;
		tmp |= (*src++ & 0x01) << 1;
		tmp |= (*src++ & 0x01) << 0;
		fputc(tmp,dst);
	}
	return 1;
}

// Read / Write image formats
GxImage loadBmp(GxImage dst, const char *src, int depth) {
	FILE *fin = fopen(src, "rb");
	if (fin == NULL) {
		gx_debug("file open error: %s", src);
		return NULL;
	}

	BMP_HDR header;
	if (!fread(&header, sizeof(BMP_HDR), 1, fin)) {
		gx_debug("Invalid header");
		fclose(fin);
		return NULL;
	}

	switch (header.manfact) {
		default:
			gx_debug("Invalid bmp format: 0x%04x", header.manfact);
			fclose(fin);
			return NULL;

		// Valid Bitmap Formats
		case 0x4d42:        // BM -
		case 0x4142:        // BA - Bitmap Array
		case 0x4943:        // CI - Cursor Icon
		case 0x5043:        // CP -
		case 0x4349:        // IC -
		case 0x5450:        // PT - Pointer
			break;
	}

	switch (header.hdrsize) {
		default:
			gx_debug("Invalid header size: %d", header.hdrsize);
			fclose(fin);
			return NULL;

		case 40:        // Windows;
		case 64:        // OS/2 2+;
			break;

		/*TODO:
		case 12:		// OS/2 1.x;
			typedef struct {
				uint16_t width;
				uint16_t height;
				uint16_t planes;
				uint16_t depth;
			} BMPINF2;

			infoHeader.depth = (*(BMPINF2*)&infoHeader).depth;
			infoHeader.planes = (*(BMPINF2*)&infoHeader).planes;
			infoHeader.height = (*(BMPINF2*)&infoHeader).height;
			infoHeader.width = (*(BMPINF2*)&infoHeader).width;
			break;
		// */
	}

	BMP_INF infoHeader;
	if (!fread(&infoHeader, header.hdrsize - 4, 1, fin)) {
		gx_debug("Invalid info header");
		fclose(fin);
		return NULL;
	}

	struct GxCLut palette;
	readBmp lineReader = NULL;
	switch (infoHeader.depth) {
		default:
			gx_debug("Invalid depth in header: %d", infoHeader.depth);
			fclose(fin);
			return NULL;

		case 24:
		case 32:
			lineReader = readRaw08;
			break;

		case 8:
			if (fread(&palette.data, sizeof(argb), 256, fin)) {
				lineReader = readRaw08;
				palette.count = 256;
			}
			break;

		case 4:
			if (fread(&palette.data, sizeof(argb), 16, fin)) {
				lineReader = readRaw04;
				palette.count = 16;
			}
			break;

		case 1:
			if (fread(&palette.data, sizeof(argb), 2, fin)) {
				lineReader = readRaw01;
				palette.count = 2;
			}
			break;
	}

	switch (infoHeader.encoding) {
		default:
			gx_debug("Invalid encoding in header: %d", infoHeader.encoding);
			fclose(fin);
			return NULL;

		case 2:
			if (lineReader != readRaw04) {
				gx_debug("Invalid bitmap");
				fclose(fin);
				return NULL;
			}
			lineReader = readRle04;
			break;

		case 1:
			if (lineReader != readRaw08) {
				gx_debug("Invalid bitmap");
				fclose(fin);
				return NULL;
			}
			lineReader = readRle08;
			break;

		case 0:        // no compression
			break;
	}
	if (lineReader == NULL) {
		gx_debug("Invalid data format");
		fclose(fin);
		return NULL;
	}

	bltProc blt = getBltProc(depth, infoHeader.depth < 8 ? 8 : infoHeader.depth);
	if (blt == NULL) {
		gx_debug("Invalid color conversion");
		fclose(fin);
		return NULL;
	}

	dst = createImage(dst, infoHeader.width, infoHeader.height, depth, 0);
	if (dst == NULL) {
		gx_debug("Failed to init image");
		fclose(fin);
		return NULL;
	}

	if (depth == 8) {
		gx_debug("TODO: allocate and copy palette");
		fclose(fin);
		return NULL;
	}

	char *ptr = dst->basePtr + dst->scanLen * (size_t) dst->height;
	unsigned lineSize = ((infoHeader.depth * infoHeader.width >> 3) + 3) & ~3;

	fseek(fin, header.imgstart, SEEK_SET);
	while (infoHeader.height--) {
		unsigned char tmpbuff[65535*4];	// FIXME: temp buffer
		ptr -= dst->scanLen;
		lineReader(tmpbuff, fin, lineSize);
		blt(ptr, tmpbuff, palette.data, infoHeader.width);
	}

	fclose(fin);
	return dst;
}
int saveBmp(const char *dst, GxImage src, int flags) {
	BMP_INF infoHeader;
	memset(&infoHeader, 0, sizeof(BMP_INF));

	// bitmap palette
	struct GxCLut palette;
	memset(&palette, 0, sizeof(struct GxCLut));

	switch (src->depth) {
		default:
			gx_debug("Invalid depth: %d", src->depth);
			return 2;

		case 32:
		case 24:
		case 16:
		case 15:
			infoHeader.depth = 24;
			break;

		case 8:
			if ((src->flags & ImageType) == ImageIdx) {
				memcpy(&palette, src->CLUTPtr, sizeof(palette));
				if (palette.count <= 2) {
					infoHeader.depth = 1;
				}
				else if (palette.count <= 16) {
					infoHeader.depth = 4;
				}
				else {
					infoHeader.depth = 8;
				}
			}
			else {	// use gray palette
				for (int i = 0; i < 256; ++i) {
					palette.data[i] = make_rgb(255, i, i, i);
				}
				palette.count = 256;
				infoHeader.depth = 8;
			}
			break;
	}

	writeBmp lineWriter = NULL;
	switch (infoHeader.depth) {
		default:
			gx_debug("Invalid depth: %d", infoHeader.depth);
			return 2;

		case 24:
			lineWriter = writeRaw08;
			// no compression
			break;

		case 8:
			lineWriter = writeRaw08;
			/*TODO: if (flags & BMP_RLE) {
				lineWriter = bmpwln_08rle;
				infoHeader.encoding = 1;
			}*/
			break;

		case 4:
			lineWriter = writeRaw04;
			/* TODO: if (flags & BMP_RLE) {
				lineWriter = bmpwln_04rle;
				infoHeader.encoding = 2;
			}*/
			break;

		case 1:
			lineWriter = writeRaw01;
			// no compression
			break;
	}

	bltProc blt = getBltProc(infoHeader.depth < 8 ? 8 : infoHeader.depth, src->depth);
	if (blt == NULL) {
		gx_debug("Invalid color conversion");
		return 3;
	}

	FILE *out = fopen(dst, "wb");
	if (out == NULL) {
		gx_debug("file open error: %s", dst);
		return 1;
	}

	char *ptr = src->basePtr + src->scanLen * (size_t) src->height;
	unsigned lineSize = ((infoHeader.depth * src->width >> 3) + 3) & ~3;

	infoHeader.width = src->width;
	infoHeader.height = src->height;
	infoHeader.planes = 1;
	infoHeader.hdpi = infoHeader.vdpi = 3779;
	infoHeader.imgsize = lineSize * infoHeader.height;

	// bitmap header
	BMP_HDR header;
	header.version = 0;
	header.manfact = 0x4d42; //'MB';
	header.hdrsize = (flags & BMP_OS2) ? 64 : 40;
	header.imgstart = sizeof(BMP_HDR) - 4 + header.hdrsize + (palette.count << 2);
	header.filesize = header.imgstart + infoHeader.imgsize;

	fwrite(&header, sizeof(BMP_HDR), 1, out);
	fwrite(&infoHeader, header.hdrsize - 4, 1, out);
	fwrite(&palette.data, palette.count, 4, out);

	while (infoHeader.height--) {
		unsigned char tmpbuff[65535*4];	// FIXME: temp buffer
		ptr -= src->scanLen;
		blt(tmpbuff, ptr, palette.data, infoHeader.width);
		lineWriter(out, tmpbuff, lineSize);
	}
	fclose(out);
	return 0;
}

GxImage loadFnt(GxImage dst, const char *src) {
	FILE *fin = fopen(src, "rb");
	if (fin == NULL) {
		gx_debug("file open error: %s", src);
		return NULL;
	}

	char tmp[4096];
	size_t fsize = fread(tmp, 1, sizeof(tmp), fin);
	fclose(fin);

	uint16_t width = 8;
	uint16_t height = (uint16_t) (fsize >> 8);

	dst = createImage(dst, 256 * width, height, 8, ImageFnt);
	if (dst == NULL) {
		gx_debug("Failed to init image");
		return NULL;
	}

	unsigned char *chrPtr = (void *) dst->basePtr;
	GxFLut lut = dst->LLUTPtr;
	lut->count = 256;
	for (int i = 0; i < 256; ++i) {
		lut->data[i].pad_x = 0;
		lut->data[i].pad_y = 0;
		lut->data[i].width = width;
		lut->data[i].basePtr = chrPtr;
		unsigned char *ptr = chrPtr;
		for (int y = 0; y < height; ++y) {
			ptr[0] = (tmp[i * height + y] & 0X80) ? 255 : 0;
			ptr[1] = (tmp[i * height + y] & 0X40) ? 255 : 0;
			ptr[2] = (tmp[i * height + y] & 0X20) ? 255 : 0;
			ptr[3] = (tmp[i * height + y] & 0X10) ? 255 : 0;
			ptr[4] = (tmp[i * height + y] & 0X08) ? 255 : 0;
			ptr[5] = (tmp[i * height + y] & 0X04) ? 255 : 0;
			ptr[6] = (tmp[i * height + y] & 0X02) ? 255 : 0;
			ptr[7] = (tmp[i * height + y] & 0X01) ? 255 : 0;
			ptr += dst->scanLen;
		}
		chrPtr += width;
	}
	return dst;
}

#ifndef USE_JPEG
GxImage loadJpg(GxImage dst, const char *src, int depth) {
#ifdef MOC_JPEG
	dst = createImage(dst, 512, 512, depth, Image2d);
	for (int y = 0; y < dst->height; ++y) {
		for (int x = 0; x < dst->width; ++x) {
			int r = x ^ y;
			int g = x & y;
			int b = x | y;
			setPixel(dst, x, y, make_rgb(0, r, g, b).val);
		}
	}
	return dst;
#endif
	(void) dst;
	(void) src;
	(void) depth;
	return NULL;
}
#else

#include <jpeglib.h>
GxImage loadJpg(GxImage dst, const char *src, int depth) {

	if (depth != 32) {
		return NULL;
	}

	FILE *fin = fopen(src, "rb");
	if (fin == NULL) {
		gx_debug("file open error: %s", src);
		return NULL;
	}
	/* We set up the normal JPEG error routines, then override error_exit. */
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fin);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	bltProc blt = NULL;
	switch (cinfo.jpeg_color_space) {
		default:
			fclose(fin);
			return NULL;

		case JCS_GRAYSCALE:    /* monochrome */
			blt = getBltProc(cblt_conv_08, depth);
			break;

		case JCS_RGB:         /* red/green/blue */
		case JCS_YCbCr:       /* Y/Cb/Cr (also known as YUV) */
		case JCS_CMYK:        /* C/M/Y/K */
		case JCS_YCCK:        /* Y/Cb/Cr/K */
			blt = (bltProc) colcpy_32_bgr;
			break;
	}

	dst = createImage(dst, cinfo.output_width, cinfo.output_height, depth, 0);
	if (dst == NULL) {
		fclose(fin);
		return NULL;
	}

	/* Step 6: while (scan lines remain to be read) */
	unsigned char *ptr = (void *) dst->basePtr;
	while (cinfo.output_scanline < cinfo.output_height) {
		unsigned char buff[65535 * 4];
		unsigned char *tmpbuff[] = {buff};
		jpeg_read_scanlines(&cinfo, tmpbuff, 1);
		blt(ptr, buff, NULL, dst->width);
		ptr += dst->scanLen;
	}

	/* Step 7: Finish decompression */
	jpeg_finish_decompress(&cinfo);

	/* Step 8: Release JPEG decompression object */
	jpeg_destroy_decompress(&cinfo);

	fclose(fin);
	return dst;
}
#endif

#ifndef USE_PNG
GxImage loadPng(GxImage dst, const char *src, int depth) {
#ifdef MOC_PNG
	dst = createImage(dst, 512, 512, depth, Image2d);
	for (int y = 0; y < dst->height; ++y) {
		for (int x = 0; x < dst->width; ++x) {
			int r = x ^ y;
			int g = x & y;
			int b = x | y;
			setPixel(dst, x, y, make_rgb(0, r, g, b).val);
		}
	}
	return dst;
#endif
	(void) dst;
	(void) src;
	(void) depth;
	return NULL;
}
#else

#include <png.h>
GxImage loadPng(GxImage dst, const char *src, int depth) {

	// 8 is the maximum size that can be checked
	unsigned char header[8];

	if (depth != 32) {
		gx_debug("Invalid depth: %d", depth);
		return NULL;
	}

	/* open file and test for it being a png */
	FILE *fin = fopen(src, "rb");
	if (fin == NULL) {
		gx_debug("file open error: %s", src);
		return NULL;
	}

	size_t y = fread(header, 1, 8, fin);

	if (y != 8 || png_sig_cmp(header, 0, 8)) {
		gx_debug("Invalid header signature");
		fclose(fin);
		return NULL;
	}

	/* initialize stuff */
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (png_ptr == NULL) {
		gx_debug("Failed to create read struct");
		fclose(fin);
		return NULL;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		gx_debug("Failed to create info struct");
		fclose(fin);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		gx_debug("Error reading image");
		fclose(fin);
		return NULL;
	}

	png_init_io(png_ptr, fin);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	int pngwidth = png_get_image_width(png_ptr, info_ptr);
	int pngheight = png_get_image_height(png_ptr, info_ptr);
	int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	int pngdepth = bit_depth * png_get_channels(png_ptr, info_ptr);
	int color_type = png_get_color_type(png_ptr, info_ptr);

	GxImage result = createImage(dst, pngwidth, pngheight, depth, 0);
	if (result == NULL) {
		gx_debug("out of memory");
		fclose(fin);
		return NULL;
	}

	bltProc blt = NULL;
	switch (pngdepth) {
		default:
			gx_debug("Invalid color conversion");
			fclose(fin);
			return NULL;

		case 32:
			blt = (bltProc) colcpy_32_abgr;
			break;

		case 24:
			blt = (bltProc) colcpy_32_bgr;
			break;

		case 8:
		case 4:
		case 2:
		case 1:
			// png_set_expand converts gray and paletted colors
			blt = (bltProc) colcpy_32_bgr;
			break;
	}

	int number_of_passes = png_set_interlace_handling(png_ptr);
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand(png_ptr);
	}
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_expand(png_ptr);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}

	png_read_update_info(png_ptr, info_ptr);

	/* read file */
	while (number_of_passes > 0) {
		unsigned char *ptr = (void*)result->basePtr;
		for (y = 0; y < result->height; y += 1) {
			unsigned char tmpbuff[65535*4]; // FIXME: bitmap temp buffer
			png_read_row(png_ptr, tmpbuff, NULL);
			blt(ptr, tmpbuff, NULL, result->width);
			ptr += result->scanLen;
		}
		number_of_passes -= 1;
	}

	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	fclose(fin);
	return result;
}
#endif
