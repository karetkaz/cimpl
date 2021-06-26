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

	bltProc blt = getBltProc(infoHeader.depth < 8 ? 8 : infoHeader.depth, depth);
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

	bltProc blt = getBltProc(src->depth, infoHeader.depth < 8 ? 8 : infoHeader.depth);
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

	char buff[32768], *data = buff;
	size_t fsize = fread(data, 1, sizeof(buff), fin);
	if (strncmp(buff, "\vGSCharset>>", 12) == 0) {
		data += fsize & 255;  // skip the header
		for (size_t i = fsize & 255; i < fsize; ++i) {
			int b = buff[i];
			b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
			b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
			b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
			buff[i] = b;
		}
	}
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
			ptr[0] = (data[i * height + y] & 0X80) ? 255 : 0;
			ptr[1] = (data[i * height + y] & 0X40) ? 255 : 0;
			ptr[2] = (data[i * height + y] & 0X20) ? 255 : 0;
			ptr[3] = (data[i * height + y] & 0X10) ? 255 : 0;
			ptr[4] = (data[i * height + y] & 0X08) ? 255 : 0;
			ptr[5] = (data[i * height + y] & 0X04) ? 255 : 0;
			ptr[6] = (data[i * height + y] & 0X02) ? 255 : 0;
			ptr[7] = (data[i * height + y] & 0X01) ? 255 : 0;
			ptr += dst->scanLen;
		}
		chrPtr += width;
	}
	return dst;
}

#ifndef NO_LIBJPEG
#include <jpeglib.h>
GxImage loadJpg(GxImage dst, const char *src, int depth) {
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
			gx_debug("Invalid jpeg_color_space: %d", cinfo.jpeg_color_space);
			fclose(fin);
			return NULL;

		case JCS_GRAYSCALE:    /* monochrome */
			blt = getBltProc(blt_cvt_08, depth);
			break;

		case JCS_RGB:         /* red/green/blue */
		case JCS_YCbCr:       /* Y/Cb/Cr (also known as YUV) */
		case JCS_CMYK:        /* C/M/Y/K */
		case JCS_YCCK:        /* Y/Cb/Cr/K */
			blt = getBltProc(blt_cvt_bgr, depth);
			break;
	}

	if (blt == NULL) {
		gx_debug("failed to find converter to depth: %d", depth);
		fclose(fin);
		return NULL;
	}

	dst = createImage(dst, cinfo.output_width, cinfo.output_height, depth, 0);
	if (dst == NULL) {
		gx_debug("failed to create image");
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
#else
GxImage loadJpg(GxImage dst, const char *src, int depth) {
	gx_debug("no jpg support to open: %s", src);
	(void) dst;
	(void) src;
	(void) depth;
	return NULL;
}
#endif

#ifndef NO_LIBPNG
#include <png.h>
GxImage loadPng(GxImage dst, const char *src, int depth) {
	// 8 is the maximum size that can be checked
	unsigned char header[8];

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

	bltProc blt = NULL;
	int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	switch (png_get_color_type(png_ptr, info_ptr)) {
		case PNG_COLOR_TYPE_GRAY:
			if (bit_depth == 16) {
				png_set_strip_16(png_ptr);
			}
			png_set_expand_gray_1_2_4_to_8(png_ptr);
			png_read_update_info(png_ptr, info_ptr);
			blt = getBltProc(blt_cvt_08, depth);
			break;

		case PNG_COLOR_TYPE_GRAY_ALPHA:
			if (bit_depth == 16) {
				png_set_strip_16(png_ptr);
			}
			png_set_gray_to_rgb(png_ptr);
			png_read_update_info(png_ptr, info_ptr);
			blt = getBltProc(blt_cvt_bgra, depth);
			break;

		case PNG_COLOR_TYPE_PALETTE:
			if (bit_depth == 16) {
				png_set_strip_16(png_ptr);
			}
			png_set_palette_to_rgb(png_ptr);
			png_read_update_info(png_ptr, info_ptr);
			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
				blt = getBltProc(blt_cvt_bgra, depth);
			} else {
				blt = getBltProc(blt_cvt_bgr, depth);
			}
			break;

		case PNG_COLOR_TYPE_RGB:
			if (bit_depth == 16) {
				png_set_strip_16(png_ptr);
				png_read_update_info(png_ptr, info_ptr);
			}
			blt = getBltProc(blt_cvt_bgr, depth);
			break;

		case PNG_COLOR_TYPE_RGB_ALPHA:
			if (bit_depth == 16) {
				png_set_strip_16(png_ptr);
				png_read_update_info(png_ptr, info_ptr);
			}
			blt = getBltProc(blt_cvt_bgra, depth);
			break;
	}

	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	int channels = png_get_channels(png_ptr, info_ptr);
	if (blt == NULL || bit_depth != 8) {
		gx_debug("invalid decoding depth(from: %d * %d, to: %d)", bit_depth, channels, depth);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fin);
		return NULL;
	}

	int png_width = png_get_image_width(png_ptr, info_ptr);
	int png_height = png_get_image_height(png_ptr, info_ptr);
	GxImage result = createImage(dst, png_width, png_height, depth, 0);
	if (result == NULL) {
		gx_debug("out of memory");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fin);
		return NULL;
	}

	/* read file */
	int number_of_passes = png_set_interlace_handling(png_ptr);
	while (number_of_passes > 0) {
		unsigned char *ptr = (void *) result->basePtr;
		for (y = 0; y < result->height; y += 1) {
			unsigned char tmpbuff[65535 * 4]; // FIXME: bitmap temp buffer
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
#else
GxImage loadPng(GxImage dst, const char *src, int depth) {
	gx_debug("no png support to open: %s", src);
	(void) dst;
	(void) src;
	(void) depth;
	return NULL;
}
#endif

#pragma GCC diagnostic push
#ifndef __EMSCRIPTEN__
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#pragma GCC diagnostic ignored "-Wsign-compare"

// https://github.com/nothings/stb/blob/master/stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// https://github.com/nothings/stb/blob/master/stb_truetype.h
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#pragma GCC diagnostic pop

GxImage loadImg(GxImage dst, const char *src, int depth) {
	int width = 0;
	int height = 0;
	int channels = 0;
	bltProc blt = NULL;
	stbi_uc *pixels = stbi_load(src, &width, &height, &channels, STBI_rgb);
	switch (channels) {
		case STBI_grey:
			blt = getBltProc(blt_cvt_08, depth);
			break;

		case STBI_rgb:
			blt = getBltProc(blt_cvt_bgr, depth);
			break;

		case STBI_rgb_alpha:
			blt = getBltProc(blt_cvt_bgra, depth);
			break;
	}

	if (pixels == NULL || blt == NULL) {
		return NULL;
	}

	dst = createImage(dst, width, height, depth, Image2d);
	if (dst == NULL) {
		stbi_image_free(pixels);
		return NULL;
	}

	for (ssize_t y = 0; y < height; ++y) {
		size_t dstOffs = y * dst->scanLen;
		size_t srcOffs = y * width * channels;
		blt(dst->basePtr + dstOffs, pixels + srcOffs, NULL, width);
	}
	stbi_image_free(pixels);
	return dst;
}

GxImage loadTtf(GxImage dst, const char *src, int height, int firstChar, int lastChars) {
	const int numChars = lastChars - firstChar;
	if (numChars > 256) {
		// TODO: remove restriction from `struct GxFLut`
		return NULL;
	}
	FILE* fontFile = fopen(src, "rb");
	if (fontFile == NULL) {
		return NULL;
	}

	fseek(fontFile, 0, SEEK_END);
	long fileSize = ftell(fontFile);
	unsigned char *fontBuffer = malloc(fileSize);
	fseek(fontFile, 0, SEEK_SET);
	long readSize = fread(fontBuffer, fileSize, 1, fontFile);
	fclose(fontFile);
	if (readSize != 1) {
		return NULL;
	}

	stbtt_fontinfo font;
	if (!stbtt_InitFont(&font, fontBuffer, 0)) {
		free(fontBuffer);
		return NULL;
	}

	stbtt_bakedchar cdata[numChars];
	unsigned char temp_bitmap[512 * 512];

	// FIXME: no guarantee this fits!
	stbtt_BakeFontBitmap(fontBuffer, 0, height, temp_bitmap, 512, 512, firstChar, numChars, cdata);

	int width = 0;
	int maxHeight = 0;
	for (int i = 0; i < numChars; ++i) {
		stbtt_bakedchar *inf = &cdata[i];
		width += inf->xadvance;

		int yPos = 0;
		if (inf->yoff < 0) {
			yPos += height + inf->yoff;
		}
		int h = yPos + inf->y1 - inf->y0;
		if (maxHeight < h) {
			maxHeight = h;
		}
	}
	if (maxHeight < height) {
		maxHeight = height;
	}

	bltProc blt = getBltProc(blt_cvt_08, 8);
	dst = createImage(dst, width, maxHeight, 8, ImageFnt);
	if (dst == NULL) {
		free(fontBuffer);
		return NULL;
	}

	int x = 0;
	GxFLut lut = dst->LLUTPtr;
	lut->count = numChars;
	fillRect(dst, 0, 0, (1 << 16) - 1, (1 << 16) - 1, 0);
	for (int i = 0; i < numChars; ++i) {
		stbtt_bakedchar *inf = &cdata[i];
		int yLen = inf->y1 - inf->y0;
		int xLen = inf->x1 - inf->x0;
		int yPos = 0;
		if (inf->yoff < 0) {
			yPos += height + inf->yoff;
		}
		for (int y = 0; y < yLen; ++y) {
			if (y + yPos >= dst->height) {
				break;
			}
			int dstOffs = x * dst->pixelLen + (y + yPos) * dst->scanLen;
			int srcOffs = (inf->x0) + (y + inf->y0) * 512;
			blt(dst->basePtr + dstOffs, temp_bitmap + srcOffs, NULL, xLen);
		}

		lut->data[i + firstChar].basePtr = dst->basePtr + x * dst->pixelLen;
		lut->data[i + firstChar].width = inf->xadvance;
		lut->data[i + firstChar].pad_x = 0;
		lut->data[i + firstChar].pad_y = 0;
		x += cdata[i].xadvance;
	}
	for (int i = 0; i < firstChar; i++) {
		lut->data[i] = lut->data[firstChar];
	}

	free(fontBuffer);
	return dst;
}
