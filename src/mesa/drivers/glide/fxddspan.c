/*
 * Mesa 3-D graphics library
 * Version:  4.0
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Authors:
 *    David Bucciarelli
 *    Brian Paul
 *    Daryll Strauss
 *    Keith Whitwell
 *    Daniel Borca
 *    Hiroshi Morii
 */

/* fxdd.c - 3Dfx VooDoo Mesa span and pixel functions */

#ifdef HAVE_CONFIG_H
#include "conf.h"
#endif

#if defined(FX)

#include "fxdrv.h"
#include "fxglidew.h"
#include "swrast/swrast.h"

/************************************************************************/
/*****                    Span functions                            *****/
/************************************************************************/

#define DBG 0

#define LOCAL_VARS                                                   \
	GLuint pitch = info.strideInBytes;                               \
	GLuint height = fxMesa->height;                                  \
	char *buf = (char *)((char *)info.lfbPtr + 0 /* x, y offset */); \
	GLuint p;                                                        \
	(void)buf;                                                       \
	(void)p;

#define Y_FLIP(_y) (height - _y - 1)

#define HW_WRITE_LOCK()                                  \
	fxMesaContext fxMesa = FX_CONTEXT(ctx);              \
	GrLfbInfo_t info;                                    \
	info.size = sizeof(GrLfbInfo_t);                     \
	if (grLfbLock(GR_LFB_WRITE_ONLY,                     \
				  fxMesa->currentFB, LFB_MODE,           \
				  GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) \
	{

#define HW_WRITE_UNLOCK()                              \
	grLfbUnlock(GR_LFB_WRITE_ONLY, fxMesa->currentFB); \
	}

#define HW_READ_LOCK()                                             \
	fxMesaContext fxMesa = FX_CONTEXT(ctx);                        \
	GrLfbInfo_t info;                                              \
	info.size = sizeof(GrLfbInfo_t);                               \
	if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,             \
				  LFB_MODE, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) \
	{

#define HW_READ_UNLOCK()                              \
	grLfbUnlock(GR_LFB_READ_ONLY, fxMesa->currentFB); \
	}

#define HW_WRITE_CLIPLOOP()                              \
	do                                                   \
	{                                                    \
		/* remember, we need to flip the scissor, too */ \
		/* is it better to do it inside fxDDScissor? */  \
		const int minx = fxMesa->clipMinX;               \
		const int maxy = Y_FLIP(fxMesa->clipMinY);       \
		const int maxx = fxMesa->clipMaxX;               \
		const int miny = Y_FLIP(fxMesa->clipMaxY);

#define HW_READ_CLIPLOOP()                               \
	do                                                   \
	{                                                    \
		/* remember, we need to flip the scissor, too */ \
		/* is it better to do it inside fxDDScissor? */  \
		const int minx = fxMesa->clipMinX;               \
		const int maxy = Y_FLIP(fxMesa->clipMinY);       \
		const int maxx = fxMesa->clipMaxX;               \
		const int miny = Y_FLIP(fxMesa->clipMaxY);

#define HW_ENDCLIPLOOP() \
	}                    \
	while (0)

/* 16 bit, ARGB1555 color spanline and pixel functions */

#undef LFB_MODE
#define LFB_MODE GR_LFBWRITEMODE_1555

#undef BYTESPERPIXEL
#define BYTESPERPIXEL 2

#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
	p = TDFXPACKCOLOR1555(color[RCOMP], color[GCOMP], color[BCOMP], color[ACOMP])

#define WRITE_RGBA(_x, _y, r, g, b, a)                     \
	*(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch) = \
		TDFXPACKCOLOR1555(r, g, b, a)

#define WRITE_PIXEL(_x, _y, p) \
	*(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch) = p

#define READ_RGBA(rgba, _x, _y)                                            \
	do                                                                     \
	{                                                                      \
		GLushort p = *(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch); \
		rgba[0] = FX_rgb_scale_5[(p >> 10) & 0x1F];                        \
		rgba[1] = FX_rgb_scale_5[(p >> 5) & 0x1F];                         \
		rgba[2] = FX_rgb_scale_5[p & 0x1F];                                \
		rgba[3] = (p & 0x8000) ? 255 : 0;                                  \
	} while (0)

/* 16 bit, RGB565 color spanline and pixel functions */
/* [dBorca] Hack alert:
 * This is wrong. The alpha value is lost, even when we provide
 * HW alpha (565 w/o depth buffering). To really update alpha buffer,
 * we would need to do the 565 writings via 8888 colorformat and rely
 * on the Voodoo to perform color scaling. In which case our 565 span
 * would look nicer! But this violates FSAA rules...
 */

#undef LFB_MODE
#define LFB_MODE GR_LFBWRITEMODE_565

#undef BYTESPERPIXEL
#define BYTESPERPIXEL 2

#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
	p = TDFXPACKCOLOR565(color[RCOMP], color[GCOMP], color[BCOMP])

#define WRITE_RGBA(_x, _y, r, g, b, a)                     \
	*(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch) = \
		TDFXPACKCOLOR565(r, g, b)

#define WRITE_PIXEL(_x, _y, p) \
	*(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch) = p

#define READ_RGBA(rgba, _x, _y)                                            \
	do                                                                     \
	{                                                                      \
		GLushort p = *(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch); \
		rgba[0] = FX_rgb_scale_5[(p >> 11) & 0x1F];                        \
		rgba[1] = FX_rgb_scale_6[(p >> 5) & 0x3F];                         \
		rgba[2] = FX_rgb_scale_5[p & 0x1F];                                \
		rgba[3] = 0xff;                                                    \
	} while (0)


/* 32 bit, ARGB8888 color spanline and pixel functions */

#undef LFB_MODE
#define LFB_MODE GR_LFBWRITEMODE_8888

#undef BYTESPERPIXEL
#define BYTESPERPIXEL 4

#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
	p = TDFXPACKCOLOR8888(color[RCOMP], color[GCOMP], color[BCOMP], color[ACOMP])

#define WRITE_RGBA(_x, _y, r, g, b, a)                   \
	*(GLuint *)(buf + _x * BYTESPERPIXEL + _y * pitch) = \
		TDFXPACKCOLOR8888(r, g, b, a)

#define WRITE_PIXEL(_x, _y, p) \
	*(GLuint *)(buf + _x * BYTESPERPIXEL + _y * pitch) = p

#define READ_RGBA(rgba, _x, _y)                                        \
	do                                                                 \
	{                                                                  \
		GLuint p = *(GLuint *)(buf + _x * BYTESPERPIXEL + _y * pitch); \
		rgba[0] = (p >> 16) & 0xff;                                    \
		rgba[1] = (p >> 8) & 0xff;                                     \
		rgba[2] = (p >> 0) & 0xff;                                     \
		rgba[3] = (p >> 24) & 0xff;                                    \
	} while (0)


/************************************************************************/
/*****                    Depth functions                           *****/
/************************************************************************/

#define DBG 0

#undef HW_WRITE_LOCK
#undef HW_WRITE_UNLOCK
#undef HW_READ_LOCK
#undef HW_READ_UNLOCK

#define HW_CLIPLOOP HW_WRITE_CLIPLOOP

#define LOCAL_DEPTH_VARS                                             \
	GLuint pitch = info.strideInBytes;                               \
	GLuint height = fxMesa->height;                                  \
	char *buf = (char *)((char *)info.lfbPtr + 0 /* x, y offset */); \
	(void)buf;

#define HW_WRITE_LOCK()                                  \
	fxMesaContext fxMesa = FX_CONTEXT(ctx);              \
	GrLfbInfo_t info;                                    \
	info.size = sizeof(GrLfbInfo_t);                     \
	if (grLfbLock(GR_LFB_WRITE_ONLY,                     \
				  GR_BUFFER_AUXBUFFER, LFB_MODE,         \
				  GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) \
	{

#define HW_WRITE_UNLOCK()                                \
	grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_AUXBUFFER); \
	}

#define HW_READ_LOCK()                                             \
	fxMesaContext fxMesa = FX_CONTEXT(ctx);                        \
	GrLfbInfo_t info;                                              \
	info.size = sizeof(GrLfbInfo_t);                               \
	if (grLfbLock(GR_LFB_READ_ONLY, GR_BUFFER_AUXBUFFER,           \
				  LFB_MODE, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) \
	{

#define HW_READ_UNLOCK()                                \
	grLfbUnlock(GR_LFB_READ_ONLY, GR_BUFFER_AUXBUFFER); \
	}

/* 16 bit, depth spanline and pixel functions */

#undef LFB_MODE
#define LFB_MODE GR_LFBWRITEMODE_ZA16

#undef BYTESPERPIXEL
#define BYTESPERPIXEL 2

#define WRITE_DEPTH(_x, _y, d) \
	*(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch) = d

#define READ_DEPTH(d, _x, _y) \
	d = *(GLushort *)(buf + _x * BYTESPERPIXEL + _y * pitch)


/* 24 bit, depth spanline and pixel functions (for use w/ stencil) */
/* [dBorca] Hack alert:
 * This is evil. The incoming Mesa's 24bit depth value
 * is shifted left 8 bits, to obtain a full 32bit value,
 * which will be thrown into the framebuffer. We rely on
 * the fact that Voodoo hardware transforms a 32bit value
 * into 24bit value automatically and, MOST IMPORTANT, won't
 * alter the upper 8bits of the value already existing in the
 * framebuffer (where stencil resides).
 */

#undef LFB_MODE
#define LFB_MODE GR_LFBWRITEMODE_Z32

#undef BYTESPERPIXEL
#define BYTESPERPIXEL 4

#define WRITE_DEPTH(_x, _y, d) \
	*(GLuint *)(buf + _x * BYTESPERPIXEL + _y * pitch) = d << 8

#define READ_DEPTH(d, _x, _y) \
	d = (*(GLuint *)(buf + _x * BYTESPERPIXEL + _y * pitch)) & 0xffffff


/* 32 bit, depth spanline and pixel functions (for use w/o stencil) */
/* [dBorca] Hack alert:
 * This is more evil. We make Mesa run in 32bit depth, but
 * tha Voodoo HW can only handle 24bit depth. Well, exploiting
 * the pixel pipeline, we can achieve 24:8 format for greater
 * precision...
 * If anyone tells me how to really store 32bit values into the
 * depth buffer, I'll write the *_Z32 routines. Howver, bear in
 * mind that means running without stencil!
 */

#define FLUSH_BATCH(fxMesa)
#define LOCK_HARDWARE(fxMesa)
#define UNLOCK_HARDWARE(fxMesa)

void fxSpanRenderStart(GLcontext *ctx)
{
	fxMesaContext fxMesa = FX_CONTEXT(ctx);
	FLUSH_BATCH(fxMesa);
	LOCK_HARDWARE(fxMesa);
}

void fxSpanRenderFinish(GLcontext *ctx)
{
	fxMesaContext fxMesa = FX_CONTEXT(ctx);
	_swrast_flush(ctx);
	UNLOCK_HARDWARE(fxMesa);
}

/* Set the buffer used for reading */
static void fxDDSetBuffer(GLcontext *ctx, GLframebuffer *buffer, GLuint bufferBit)
{
	fxMesaContext fxMesa = FX_CONTEXT(ctx);
	(void)buffer;

	switch (bufferBit)
	{
	case BUFFER_BIT_FRONT_LEFT:
		fxMesa->currentFB = GR_BUFFER_FRONTBUFFER;
		break;
	case BUFFER_BIT_BACK_LEFT:
		fxMesa->currentFB = GR_BUFFER_BACKBUFFER;
		break;
	default:
		break;
	}
}

/* Nejc: Updated for Mesa 6.3+ - span functions now handled via renderbuffer interface */
void fxDDInitSpanFuncs(GLcontext *ctx)
{
	/* Mesa 6.3+ uses renderbuffer GetRow/PutRow functions instead of span functions */
	/* Span functions are now set up in fxSetSpanFunctions() in fxrenderbuffer.c */

	/* Keep the render start/finish functions for hardware locking */
	struct swrast_device_driver *swdd = _swrast_GetDeviceDriverReference(ctx);
	swdd->SetBuffer = fxDDSetBuffer;			 /* Use the proper SetBuffer function */
	swdd->SpanRenderStart = fxSpanRenderStart;	 /* BEGIN_BOARD_LOCK */
	swdd->SpanRenderFinish = fxSpanRenderFinish; /* END_BOARD_LOCK */
}

void fxSetupDDSpanPointers(GLcontext *ctx)
{
	/* Individual span functions are set up per-renderbuffer in fxSetSpanFunctions() */

	/* Keep render start/finish for compatibility */
	fxDDInitSpanFuncs(ctx);
}

#else

/*
 * Need this to provide at least one external definition.
 */

extern int gl_fx_dummy_function_span(void);
int gl_fx_dummy_function_span(void)
{
	return 0;
}

#endif /* FX */
