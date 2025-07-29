/*
 * Mesa 3-D graphics library
 * Version:  6.4.2
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
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
 *    Nejc Dolicanin - Updated for Mesa 6.4.2 renderbuffer infrastructure
 */

/* fxrenderbuffer.c - 3Dfx VooDoo Mesa renderbuffer functions */

#ifdef HAVE_CONFIG_H
#include "conf.h"
#endif

#if defined(FX)

#include "fxdrv.h"
#include "fxglidew.h"
#include "fxrenderbuffer.h"
#include "renderbuffer.h"
#include "swrast/swrast.h"
#include "enums.h"


/**
 * Renderbuffer storage allocation for color buffers
 */
static GLboolean
fxAllocColorRenderbuffer(GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLenum internalFormat, GLuint width, GLuint height)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxAllocColorRenderbuffer(%s, %d, %d)\n",
              _mesa_lookup_enum_by_nr(internalFormat), width, height);
   }

   /* Color renderbuffers are managed by hardware - no software allocation needed */
   rb->Width = width;
   rb->Height = height;
   rb->InternalFormat = internalFormat;
   
   /* Set format based on hardware capabilities */
   if (fxMesa->colDepth == 32) {
      rb->_BaseFormat = GL_RGBA;
      rb->DataType = GL_UNSIGNED_BYTE;
      /* Mesa 6.3+ removed direct bit fields from renderbuffer */
      /* Bit information is now derived from InternalFormat */
   } else if (fxMesa->colDepth == 16) {
      rb->_BaseFormat = GL_RGB;
      rb->DataType = GL_UNSIGNED_SHORT_5_6_5;
      /* Mesa 6.3+ removed direct bit fields from renderbuffer */
   } else { /* 15 bit */
      rb->_BaseFormat = GL_RGBA;
      rb->DataType = GL_UNSIGNED_SHORT_1_5_5_5_REV;
      /* Mesa 6.3+ removed direct bit fields from renderbuffer */
   }
   
   return GL_TRUE;
}

/**
 * Renderbuffer storage allocation for depth buffer
 */
static GLboolean
fxAllocDepthRenderbuffer(GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLenum internalFormat, GLuint width, GLuint height)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxAllocDepthRenderbuffer(%s, %d, %d)\n",
              _mesa_lookup_enum_by_nr(internalFormat), width, height);
   }

   if (!fxMesa->haveZBuffer) {
      return GL_FALSE;
   }

   /* Depth renderbuffers are managed by hardware - no software allocation needed */
   rb->Width = width;
   rb->Height = height;
   rb->InternalFormat = internalFormat;
   rb->_BaseFormat = GL_DEPTH_COMPONENT;
   
   /* Set depth format based on hardware */
   if (fxMesa->haveHwStencil) {
      /* 24-bit depth + 8-bit stencil */
      rb->DataType = GL_UNSIGNED_INT;
      /* Mesa 6.3+ removed direct bit fields from renderbuffer */
   } else {
      /* 16-bit depth */
      rb->DataType = GL_UNSIGNED_SHORT;
      /* Mesa 6.3+ removed direct bit fields from renderbuffer */
   }
   
   return GL_TRUE;
}

/**
 * Renderbuffer storage allocation for stencil buffer
 */
static GLboolean
fxAllocStencilRenderbuffer(GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLenum internalFormat, GLuint width, GLuint height)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxAllocStencilRenderbuffer(%s, %d, %d)\n",
              _mesa_lookup_enum_by_nr(internalFormat), width, height);
   }

   if (!fxMesa->haveHwStencil) {
      return GL_FALSE;
   }

   /* Stencil renderbuffers are managed by hardware - no software allocation needed */
   rb->Width = width;
   rb->Height = height;
   rb->InternalFormat = internalFormat;
   rb->_BaseFormat = GL_STENCIL_INDEX;
   rb->DataType = GL_UNSIGNED_BYTE;
   /* Mesa 6.3+ removed direct bit fields from renderbuffer */
   
   return GL_TRUE;
}

/**
 * Delete a renderbuffer
 */
static void
fxDeleteRenderbuffer(struct gl_renderbuffer *rb)
{
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxDeleteRenderbuffer(%p)\n", rb);
   }
   
   /* Hardware renderbuffers don't need special cleanup */
   _mesa_delete_renderbuffer(rb);
}

/**
 * Create a new color renderbuffer
 */
struct gl_renderbuffer *
fxNewColorRenderbuffer(GLcontext *ctx, GLuint name, GLenum internalFormat)
{
   struct gl_renderbuffer *rb;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxNewColorRenderbuffer(%d, %s)\n", 
              name, _mesa_lookup_enum_by_nr(internalFormat));
   }
   
   rb = _mesa_new_renderbuffer(ctx, name);
   if (!rb) {
      return NULL;
   }
   
   rb->InternalFormat = internalFormat;
   rb->AllocStorage = fxAllocColorRenderbuffer;
   rb->Delete = fxDeleteRenderbuffer;
   
   /* Span functions will be set up later in framebuffer creation */
   
   return rb;
}

/**
 * Create a new depth renderbuffer
 */
struct gl_renderbuffer *
fxNewDepthRenderbuffer(GLcontext *ctx, GLuint name)
{
   struct gl_renderbuffer *rb;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxNewDepthRenderbuffer(%d)\n", name);
   }
   
   rb = _mesa_new_renderbuffer(ctx, name);
   if (!rb) {
      return NULL;
   }
   
   rb->InternalFormat = GL_DEPTH_COMPONENT16; /* or GL_DEPTH_COMPONENT24 */
   rb->AllocStorage = fxAllocDepthRenderbuffer;
   rb->Delete = fxDeleteRenderbuffer;
   
   /* Span functions will be set up later in framebuffer creation */
   
   return rb;
}

/**
 * Create a new stencil renderbuffer
 */
struct gl_renderbuffer *
fxNewStencilRenderbuffer(GLcontext *ctx, GLuint name)
{
   struct gl_renderbuffer *rb;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxNewStencilRenderbuffer(%d)\n", name);
   }
   
   rb = _mesa_new_renderbuffer(ctx, name);
   if (!rb) {
      return NULL;
   }
   
   rb->InternalFormat = GL_STENCIL_INDEX8_EXT;
   rb->AllocStorage = fxAllocStencilRenderbuffer;
   rb->Delete = fxDeleteRenderbuffer;
   
   /* Span functions will be set up later in framebuffer creation */
   
   return rb;
}

/**
 * Set up span functions for a renderbuffer based on its format
 * Following the tdfx DRI driver pattern - no context needed during setup
 */
void
fxSetSpanFunctions(struct gl_renderbuffer *rb, const GLvisual *vis)
{
   //Debug
   FILE *debug_log = fopen("Mesa.log", "a");
   if (debug_log) {
      fprintf(debug_log, "fxSetSpanFunctions: Starting - rb=%p, format=%s\n", 
              (void*)rb, _mesa_lookup_enum_by_nr(rb->InternalFormat));
      fclose(debug_log);
   }

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxSetSpanFunctions(%p, format=%s)\n", 
              (void*)rb, _mesa_lookup_enum_by_nr(rb->InternalFormat));
   }
   
   if (rb->InternalFormat == GL_RGBA || rb->InternalFormat == GL_RGB) {
      /* Color buffer - determine format from visual like tdfx driver */
      if (vis->redBits == 8 && vis->greenBits == 8 && vis->blueBits == 8 && vis->alphaBits == 8) {
         /* 32-bit ARGB8888 */
         rb->GetRow = fxReadRGBASpan_ARGB8888;
         rb->GetValues = fxReadRGBAPixels_ARGB8888;
         rb->PutRow = fxWriteRGBASpan_ARGB8888;
         rb->PutRowRGB = fxWriteRGBSpan_ARGB8888;
         rb->PutMonoRow = fxWriteMonoRGBASpan_ARGB8888;
         rb->PutValues = fxWriteRGBAPixels_ARGB8888;
         rb->PutMonoValues = fxWriteMonoRGBAPixels_ARGB8888;
      } else if (vis->redBits == 5 && vis->greenBits == 6 && vis->blueBits == 5) {
         /* 16-bit RGB565 */
         rb->GetRow = fxReadRGBASpan_RGB565;
         rb->GetValues = fxReadRGBAPixels_RGB565;
         rb->PutRow = fxWriteRGBASpan_RGB565;
         rb->PutRowRGB = fxWriteRGBSpan_RGB565;
         rb->PutMonoRow = fxWriteMonoRGBASpan_RGB565;
         rb->PutValues = fxWriteRGBAPixels_RGB565;
         rb->PutMonoValues = fxWriteMonoRGBAPixels_RGB565;
      } else if (vis->redBits == 5 && vis->greenBits == 5 && vis->blueBits == 5) {
         /* 15-bit ARGB1555 */
         rb->GetRow = fxReadRGBASpan_ARGB1555;
         rb->GetValues = fxReadRGBAPixels_ARGB1555;
         rb->PutRow = fxWriteRGBASpan_ARGB1555;
         rb->PutRowRGB = fxWriteRGBSpan_ARGB1555;
         rb->PutMonoRow = fxWriteMonoRGBASpan_ARGB1555;
         rb->PutValues = fxWriteRGBAPixels_ARGB1555;
         rb->PutMonoValues = fxWriteMonoRGBAPixels_ARGB1555;
      } else {
         /* Default to 16-bit RGB565 */
         rb->GetRow = fxReadRGBASpan_RGB565;
         rb->GetValues = fxReadRGBAPixels_RGB565;
         rb->PutRow = fxWriteRGBASpan_RGB565;
         rb->PutRowRGB = fxWriteRGBSpan_RGB565;
         rb->PutMonoRow = fxWriteMonoRGBASpan_RGB565;
         rb->PutValues = fxWriteRGBAPixels_RGB565;
         rb->PutMonoValues = fxWriteMonoRGBAPixels_RGB565;
      }
   } else if (rb->InternalFormat == GL_DEPTH_COMPONENT16 ||
              rb->InternalFormat == GL_DEPTH_COMPONENT24) {
      /* Depth buffer - following DRI tdfx pattern */
      rb->GetRow = fxReadDepthSpan_Z24;
      rb->GetValues = fxReadDepthPixels_Z24;
      rb->PutRow = fxWriteDepthSpan_Z24;
      rb->PutMonoRow = fxWriteMonoDepthSpan_Z24;
      rb->PutValues = fxWriteDepthPixels_Z24;
      rb->PutMonoValues = NULL; /* Following DRI tdfx pattern */ //fxWriteMonoDepthPixels_Z24 not implemented
   } else if (rb->InternalFormat == GL_STENCIL_INDEX8_EXT) {
      /* Stencil buffer - following DRI tdfx pattern */
      rb->GetRow = fxReadStencilSpan;
      rb->GetValues = fxReadStencilPixels;
      rb->PutRow = fxWriteStencilSpan;
      rb->PutMonoRow = fxWriteMonoStencilSpan;
      rb->PutValues = fxWriteStencilPixels;
      rb->PutMonoValues = NULL; /* Following DRI tdfx pattern */ //fxWriteMonoStencilPixels not implemented
   }
}

/* Mesa 6.3+ Renderbuffer PutRow/GetRow Functions */

/**
 * Read a row of ARGB8888 pixels from color buffer
 */
void fxReadRGBASpan_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_ARGB8888(%d, %d, %d)\n", count, x, y);
   }
   
   grLfbReadRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y, count, 1, count * 4, rgba);
   
   /* Convert from BGRA to RGBA */
   for (i = 0; i < count; i++) {
      GLubyte c = rgba[i][0];
      rgba[i][0] = rgba[i][2];
      rgba[i][2] = c;
   }
}

/**
 * Read ARGB8888 pixels from color buffer
 */
void fxReadRGBAPixels_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                               GLuint count, const GLint x[], const GLint y[],
                               void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBAPixels_ARGB8888(%d)\n", count);
   }
   
   for (i = 0; i < count; i++) {
      GLuint pixel;
      grLfbReadRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i], 1, 1, 4, &pixel);
      
      /* Convert from BGRA to RGBA */
      rgba[i][0] = (pixel >> 16) & 0xff; /* R */
      rgba[i][1] = (pixel >> 8) & 0xff;  /* G */
      rgba[i][2] = pixel & 0xff;         /* B */
      rgba[i][3] = (pixel >> 24) & 0xff; /* A */
   }
}

/**
 * Write RGBA span to ARGB8888 color buffer
 */
void fxWriteRGBASpan_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                              GLuint count, GLint x, GLint y, const void *values,
                              const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLuint pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBASpan_ARGB8888(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to BGRA */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = (rgba[i][3] << 24) |  /* A */
                     (rgba[i][0] << 16) |  /* R */
                     (rgba[i][1] << 8) |   /* G */
                     rgba[i][2];           /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y, 
                             GR_LFB_SRC_FMT_8888, 1, 1, FXFALSE, 4, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_8888, count, 1, FXFALSE, count * 4, pixels);
   }
}

/**
 * Write RGB span to ARGB8888 color buffer
 */
void fxWriteRGBSpan_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint count, GLint x, GLint y, const void *values,
                             const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgb)[3] = (const GLubyte (*)[3]) values;
   GLuint pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBSpan_ARGB8888(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGB to BGRA with alpha = 255 */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = (255 << 24) |         /* A */
                     (rgb[i][0] << 16) |   /* R */
                     (rgb[i][1] << 8) |    /* G */
                     rgb[i][2];            /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                             GR_LFB_SRC_FMT_8888, 1, 1, FXFALSE, 4, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_8888, count, 1, FXFALSE, count * 4, pixels);
   }
}

/**
 * Write mono RGBA span to ARGB8888 color buffer
 */
void fxWriteMonoRGBASpan_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                                  GLuint count, GLint x, GLint y, const void *value,
                                  const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLuint pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBASpan_ARGB8888(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to BGRA */
   pixel = (color[3] << 24) |  /* A */
           (color[0] << 16) |  /* R */
           (color[1] << 8) |   /* G */
           color[2];           /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                          GR_LFB_SRC_FMT_8888, 1, 1, FXFALSE, 4, &pixel);
      }
   }
}

/**
 * Write RGBA pixels to ARGB8888 color buffer
 */
void fxWriteRGBAPixels_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                                GLuint count, const GLint x[], const GLint y[],
                                const void *values, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBAPixels_ARGB8888(%d)\n", count);
   }
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLuint pixel = (rgba[i][3] << 24) |  /* A */
                        (rgba[i][0] << 16) |  /* R */
                        (rgba[i][1] << 8) |   /* G */
                        rgba[i][2];           /* B */
         
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_8888, 1, 1, FXFALSE, 4, &pixel);
      }
   }
}

/**
 * Write mono RGBA pixels to ARGB8888 color buffer
 */
void fxWriteMonoRGBAPixels_ARGB8888(GLcontext *ctx, struct gl_renderbuffer *rb,
                                    GLuint count, const GLint x[], const GLint y[],
                                    const void *value, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLuint pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBAPixels_ARGB8888(%d)\n", count);
   }
   
   /* Convert from RGBA to BGRA */
   pixel = (color[3] << 24) |  /* A */
           (color[0] << 16) |  /* R */
           (color[1] << 8) |   /* G */
           color[2];           /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_8888, 1, 1, FXFALSE, 4, &pixel);
      }
   }
}

/**
 * Read a row of RGB565 pixels from color buffer
 */
void fxReadRGBASpan_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GrLfbInfo_t info;
   GLuint i, j;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_RGB565(%d, %d, %d)\n", count, x, y);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                   (winY - y) * info.strideInBytes +
                                                   x * 2);
      const GLuint *data32 = (const GLuint *)data16;
      GLuint extraPixel = (count & 1);
      GLuint n32 = count - extraPixel;

      for (i = j = 0; i < n32; i += 2, j++) {
         GLuint pixel = data32[j];
         rgba[i][0] = FX_rgb_scale_5[(pixel >> 11) & 0x1F];
         rgba[i][1] = FX_rgb_scale_6[(pixel >> 5) & 0x3F];
         rgba[i][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[i][3] = 255;
         rgba[i + 1][0] = FX_rgb_scale_5[(pixel >> 27) & 0x1F];
         rgba[i + 1][1] = FX_rgb_scale_6[(pixel >> 21) & 0x3F];
         rgba[i + 1][2] = FX_rgb_scale_5[(pixel >> 16) & 0x1F];
         rgba[i + 1][3] = 255;
      }
      if (extraPixel) {
         GLushort pixel = data16[n32];
         rgba[n32][0] = FX_rgb_scale_5[(pixel >> 11) & 0x1F];
         rgba[n32][1] = FX_rgb_scale_6[(pixel >> 5) & 0x3F];
         rgba[n32][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[n32][3] = 255;
      }

      grLfbUnlock(GR_LFB_READ_ONLY, fxMesa->currentFB);
   }
}

/**
 * Read RGB565 pixels from color buffer
 */
void fxReadRGBAPixels_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint count, const GLint x[], const GLint y[],
                             void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GrLfbInfo_t info;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBAPixels_RGB565(%d)\n", count);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      
      for (i = 0; i < count; i++) {
         const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                      (winY - y[i]) * info.strideInBytes +
                                                      x[i] * 2);
         GLushort pixel = *data16;
         rgba[i][0] = FX_rgb_scale_5[(pixel >> 11) & 0x1F];
         rgba[i][1] = FX_rgb_scale_6[(pixel >> 5) & 0x3F];
         rgba[i][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[i][3] = 255;
      }
      
      grLfbUnlock(GR_LFB_READ_ONLY, fxMesa->currentFB);
   }
}

/**
 * Write RGBA span to RGB565 color buffer
 */
void fxWriteRGBASpan_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                            GLuint count, GLint x, GLint y, const void *values,
                            const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLushort pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBASpan_RGB565(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to RGB565 */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = ((rgba[i][0] & 0xf8) << 8) |  /* R */
                     ((rgba[i][1] & 0xfc) << 3) |  /* G */
                     (rgba[i][2] >> 3);            /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                             GR_LFB_SRC_FMT_565, 1, 1, FXFALSE, 2, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_565, count, 1, FXFALSE, count * 2, pixels);
   }
}

/**
 * Write RGB span to RGB565 color buffer
 */
void fxWriteRGBSpan_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLuint count, GLint x, GLint y, const void *values,
                           const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgb)[3] = (const GLubyte (*)[3]) values;
   GLushort pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBSpan_RGB565(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGB to RGB565 */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = ((rgb[i][0] & 0xf8) << 8) |  /* R */
                     ((rgb[i][1] & 0xfc) << 3) |  /* G */
                     (rgb[i][2] >> 3);            /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                             GR_LFB_SRC_FMT_565, 1, 1, FXFALSE, 2, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_565, count, 1, FXFALSE, count * 2, pixels);
   }
}

/**
 * Write mono RGBA span to RGB565 color buffer
 */
void fxWriteMonoRGBASpan_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                                GLuint count, GLint x, GLint y, const void *value,
                                const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLushort pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBASpan_RGB565(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to RGB565 */
   pixel = ((color[0] & 0xf8) << 8) |  /* R */
           ((color[1] & 0xfc) << 3) |  /* G */
           (color[2] >> 3);            /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                          GR_LFB_SRC_FMT_565, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Write RGBA pixels to RGB565 color buffer
 */
void fxWriteRGBAPixels_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                              GLuint count, const GLint x[], const GLint y[],
                              const void *values, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBAPixels_RGB565(%d)\n", count);
   }
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLushort pixel = ((rgba[i][0] & 0xf8) << 8) |  /* R */
                          ((rgba[i][1] & 0xfc) << 3) |  /* G */
                          (rgba[i][2] >> 3);            /* B */
         
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_565, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Write mono RGBA pixels to RGB565 color buffer
 */
void fxWriteMonoRGBAPixels_RGB565(GLcontext *ctx, struct gl_renderbuffer *rb,
                                  GLuint count, const GLint x[], const GLint y[],
                                  const void *value, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLushort pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBAPixels_RGB565(%d)\n", count);
   }
   
   /* Convert from RGBA to RGB565 */
   pixel = ((color[0] & 0xf8) << 8) |  /* R */
           ((color[1] & 0xfc) << 3) |  /* G */
           (color[2] >> 3);            /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_565, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Read a row of ARGB1555 pixels from color buffer
 */
void fxReadRGBASpan_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GrLfbInfo_t info;
   GLuint i, j;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_ARGB1555(%d, %d, %d)\n", count, x, y);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                   (winY - y) * info.strideInBytes +
                                                   x * 2);
      const GLuint *data32 = (const GLuint *)data16;
      GLuint extraPixel = (count & 1);
      GLuint n32 = count - extraPixel;

      for (i = j = 0; i < n32; i += 2, j++) {
         GLuint pixel = data32[j];
         rgba[i][0] = FX_rgb_scale_5[(pixel >> 10) & 0x1F];
         rgba[i][1] = FX_rgb_scale_5[(pixel >> 5) & 0x1F];
         rgba[i][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[i][3] = (pixel & 0x8000) ? 255 : 0;
         rgba[i + 1][0] = FX_rgb_scale_5[(pixel >> 26) & 0x1F];
         rgba[i + 1][1] = FX_rgb_scale_5[(pixel >> 21) & 0x1F];
         rgba[i + 1][2] = FX_rgb_scale_5[(pixel >> 16) & 0x1F];
         rgba[i + 1][3] = (pixel & 0x80000000) ? 255 : 0;
      }
      if (extraPixel) {
         GLushort pixel = data16[n32];
         rgba[n32][0] = FX_rgb_scale_5[(pixel >> 10) & 0x1F];
         rgba[n32][1] = FX_rgb_scale_5[(pixel >> 5) & 0x1F];
         rgba[n32][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[n32][3] = (pixel & 0x8000) ? 255 : 0;
      }

      grLfbUnlock(GR_LFB_READ_ONLY, fxMesa->currentFB);
   }
}

/**
 * Read ARGB1555 pixels from color buffer
 */
void fxReadRGBAPixels_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                               GLuint count, const GLint x[], const GLint y[],
                               void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte (*rgba)[4] = (GLubyte (*)[4]) values;
   GrLfbInfo_t info;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBAPixels_ARGB1555(%d)\n", count);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      
      for (i = 0; i < count; i++) {
         const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                      (winY - y[i]) * info.strideInBytes +
                                                      x[i] * 2);
         GLushort pixel = *data16;
         rgba[i][0] = FX_rgb_scale_5[(pixel >> 10) & 0x1F];
         rgba[i][1] = FX_rgb_scale_5[(pixel >> 5) & 0x1F];
         rgba[i][2] = FX_rgb_scale_5[pixel & 0x1F];
         rgba[i][3] = (pixel & 0x8000) ? 255 : 0;
      }
      
      grLfbUnlock(GR_LFB_READ_ONLY, fxMesa->currentFB);
   }
}

/**
 * Write RGBA span to ARGB1555 color buffer
 */
void fxWriteRGBASpan_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                              GLuint count, GLint x, GLint y, const void *values,
                              const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLushort pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBASpan_ARGB1555(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to ARGB1555 */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = ((rgba[i][3] > 127) ? 0x8000 : 0) |  /* A */
                     ((rgba[i][0] & 0xf8) << 7) |         /* R */
                     ((rgba[i][1] & 0xf8) << 2) |         /* G */
                     (rgba[i][2] >> 3);                   /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                             GR_LFB_SRC_FMT_1555, 1, 1, FXFALSE, 2, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_1555, count, 1, FXFALSE, count * 2, pixels);
   }
}

/**
 * Write RGB span to ARGB1555 color buffer
 */
void fxWriteRGBSpan_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint count, GLint x, GLint y, const void *values,
                             const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgb)[3] = (const GLubyte (*)[3]) values;
   GLushort pixels[MAX_WIDTH];
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBSpan_ARGB1555(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGB to ARGB1555 with alpha = 1 */
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         pixels[i] = 0x8000 |                             /* A = 1 */
                     ((rgb[i][0] & 0xf8) << 7) |          /* R */
                     ((rgb[i][1] & 0xf8) << 2) |          /* G */
                     (rgb[i][2] >> 3);                    /* B */
      }
   }
   
   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                             GR_LFB_SRC_FMT_1555, 1, 1, FXFALSE, 2, &pixels[i]);
         }
      }
   } else {
      grLfbWriteRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y,
                       GR_LFB_SRC_FMT_1555, count, 1, FXFALSE, count * 2, pixels);
   }
}

/**
 * Write mono RGBA span to ARGB1555 color buffer
 */
void fxWriteMonoRGBASpan_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                                  GLuint count, GLint x, GLint y, const void *value,
                                  const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLushort pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBASpan_ARGB1555(%d, %d, %d)\n", count, x, y);
   }
   
   /* Convert from RGBA to ARGB1555 */
   pixel = ((color[3] > 127) ? 0x8000 : 0) |  /* A */
           ((color[0] & 0xf8) << 7) |         /* R */
           ((color[1] & 0xf8) << 2) |         /* G */
           (color[2] >> 3);                   /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x + i, fxMesa->height - 1 - y,
                          GR_LFB_SRC_FMT_1555, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Write RGBA pixels to ARGB1555 color buffer
 */
void fxWriteRGBAPixels_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                                GLuint count, const GLint x[], const GLint y[],
                                const void *values, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte (*rgba)[4] = (const GLubyte (*)[4]) values;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteRGBAPixels_ARGB1555(%d)\n", count);
   }
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLushort pixel = ((rgba[i][3] > 127) ? 0x8000 : 0) |  /* A */
                          ((rgba[i][0] & 0xf8) << 7) |         /* R */
                          ((rgba[i][1] & 0xf8) << 2) |         /* G */
                          (rgba[i][2] >> 3);                   /* B */
         
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_1555, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Write mono RGBA pixels to ARGB1555 color buffer
 */
void fxWriteMonoRGBAPixels_ARGB1555(GLcontext *ctx, struct gl_renderbuffer *rb,
                                    GLuint count, const GLint x[], const GLint y[],
                                    const void *value, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte *color = (const GLubyte *) value;
   GLushort pixel;
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoRGBAPixels_ARGB1555(%d)\n", count);
   }
   
   /* Convert from RGBA to ARGB1555 */
   pixel = ((color[3] > 127) ? 0x8000 : 0) |  /* A */
           ((color[0] & 0xf8) << 7) |         /* R */
           ((color[1] & 0xf8) << 2) |         /* G */
           (color[2] >> 3);                   /* B */
   
   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(fxMesa->currentFB, x[i], fxMesa->height - 1 - y[i],
                          GR_LFB_SRC_FMT_1555, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Read 24-bit depth pixels from depth buffer
 */
void fxReadDepthPixels_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLuint count, const GLint x[], const GLint y[],
                           void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLuint *depth = (GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthPixels_Z24(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      GLuint pixel;
      grLfbReadRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i], 1, 1, 0, &pixel);
      depth[i] = pixel & 0xffffff;
   }
}

/**
 * Write depth span to 24-bit depth buffer
 */
void fxWriteDepthSpan_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
                          GLuint count, GLint x, GLint y, const void *values,
                          const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLuint *depth = (const GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteDepthSpan_Z24(%d, %d, %d)\n", count, x, y);
   }

   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            GLuint pixel = depth[i] & 0xffffff;
            grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y, 
                             GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &pixel);
         }
      }
   } else {
      GLuint pixels[MAX_WIDTH];
      for (i = 0; i < count; i++) {
         pixels[i] = depth[i] & 0xffffff;
      }
      grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x, bottom - y,
                       GR_LFB_SRC_FMT_ZA16, count, 1, FXFALSE, count * 4, pixels);
   }
}

/**
 * Write mono depth span to 24-bit depth buffer
 */
void fxWriteMonoDepthSpan_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
                              GLuint count, GLint x, GLint y, const void *value,
                              const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLuint depthVal = *((const GLuint *) value) & 0xffffff;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoDepthSpan_Z24(%d, %d, %d)\n", count, x, y);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y,
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &depthVal);
      }
   }
}

/**
 * Write depth pixels to 24-bit depth buffer
 */
void fxWriteDepthPixels_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
                            GLuint count, const GLint x[], const GLint y[],
                            const void *values, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLuint *depth = (const GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteDepthPixels_Z24(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLuint pixel = depth[i] & 0xffffff;
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i],
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &pixel);
      }
   }
}

/**
 * Write mono depth pixels to 24-bit depth buffer - Not implemented
 * (This function is not implemented in the original code, but provided here for completeness.)
 */
// void fxWriteMonoDepthPixels_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
//                                 GLuint count, const GLint x[], const GLint y[],
//                                 const void *value, const GLubyte *mask)
// {
//    fxMesaContext fxMesa = FX_CONTEXT(ctx);
//    const GLuint depthVal = *((const GLuint *) value) & 0xffffff;
//    GLint bottom = fxMesa->height - 1;
//    GLuint i;

//    if (TDFX_DEBUG & VERBOSE_DRIVER) {
//       fprintf(stderr, "fxWriteMonoDepthPixels_Z24(%d)\n", count);
//    }

//    for (i = 0; i < count; i++) {
//       if (!mask || mask[i]) {
//          grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i],
//                           GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &depthVal);
//       }
//    }
// }

/**
 * Read a row of 24-bit depth values
 */
void fxReadDepthSpan_Z24(GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLuint *depth = (GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthSpan_Z24(%d, %d, %d)\n", count, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, count, 1, 0, depth);
   for (i = 0; i < count; i++) {
      depth[i] &= 0xffffff;
   }
}

/**
 * Read 16-bit depth pixels from depth buffer
 */
void fxReadDepthPixels_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLuint count, const GLint x[], const GLint y[],
                           void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLuint *depth = (GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthPixels_Z16(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      GLushort pixel;
      grLfbReadRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i], 1, 1, 0, &pixel);
      depth[i] = pixel;
   }
}

/**
 * Write depth span to 16-bit depth buffer
 */
void fxWriteDepthSpan_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                          GLuint count, GLint x, GLint y, const void *values,
                          const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLuint *depth = (const GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteDepthSpan_Z16(%d, %d, %d)\n", count, x, y);
   }

   if (mask) {
      for (i = 0; i < count; i++) {
         if (mask[i]) {
            GLushort pixel = (GLushort)(depth[i] & 0xffff);
            grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y, 
                             GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 2, &pixel);
         }
      }
   } else {
      GLushort pixels[MAX_WIDTH];
      for (i = 0; i < count; i++) {
         pixels[i] = (GLushort)(depth[i] & 0xffff);
      }
      grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x, bottom - y,
                       GR_LFB_SRC_FMT_ZA16, count, 1, FXFALSE, count * 2, pixels);
   }
}

/**
 * Write mono depth span to 16-bit depth buffer
 */
void fxWriteMonoDepthSpan_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                              GLuint count, GLint x, GLint y, const void *value,
                              const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLushort depthVal = (GLushort)(*((const GLuint *) value) & 0xffff);
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoDepthSpan_Z16(%d, %d, %d)\n", count, x, y);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y,
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 2, &depthVal);
      }
   }
}

/**
 * Write depth pixels to 16-bit depth buffer
 */
void fxWriteDepthPixels_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                            GLuint count, const GLint x[], const GLint y[],
                            const void *values, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLuint *depth = (const GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteDepthPixels_Z16(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLushort pixel = (GLushort)(depth[i] & 0xffff);
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i],
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 2, &pixel);
      }
   }
}

/**
 * Write mono depth pixels to 16-bit depth buffer
 */
void fxWriteMonoDepthPixels_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                                GLuint count, const GLint x[], const GLint y[],
                                const void *value, const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLushort depthVal = (GLushort)(*((const GLuint *) value) & 0xffff);
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoDepthPixels_Z16(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i],
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 2, &depthVal);
      }
   }
}

/**
 * Read a row of 16-bit depth values
 */
void fxReadDepthSpan_Z16(GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLuint *depth = (GLuint *) values;
   GLint bottom = fxMesa->height - 1;
   GLushort depth16[MAX_WIDTH];
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthSpan_Z16(%d, %d, %d)\n", count, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, count, 1, 0, depth16);
   for (i = 0; i < count; i++) {
      depth[i] = depth16[i];
   }
}

/**
 * Read stencil pixels from stencil buffer
 */
void fxReadStencilPixels(GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLuint count, const GLint x[], const GLint y[],
                         void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte *stencil = (GLubyte *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadStencilPixels(%d)\n", count);
   }

   for (i = 0; i < count; i++) {
      GLuint zs32;
      grLfbReadRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i], 1, 1, 0, &zs32);
      stencil[i] = zs32 >> 24;
   }
}

/**
 * Write mono stencil span to stencil buffer
 */
void fxWriteMonoStencilSpan(GLcontext *ctx, struct gl_renderbuffer *rb,
                            GLuint count, GLint x, GLint y, const void *value,
                            const GLubyte *mask)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   const GLubyte stencilVal = *((const GLubyte *) value);
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteMonoStencilSpan(%d, %d, %d)\n", count, x, y);
   }

   for (i = 0; i < count; i++) {
      if (!mask || mask[i]) {
         GLuint zs32;
         /* Read current depth/stencil value */
         grLfbReadRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y, 1, 1, 0, &zs32);
         /* Update stencil bits only */
         zs32 = (zs32 & 0x00ffffff) | (stencilVal << 24);
         grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x + i, bottom - y,
                          GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &zs32);
      }
   }
}

/**
 * Write mono stencil pixels to stencil buffer
 * not implemented in the original code, but provided here for completeness.
 */
// void fxWriteMonoStencilPixels(GLcontext *ctx, struct gl_renderbuffer *rb,
//                               GLuint count, const GLint x[], const GLint y[],
//                               const void *value, const GLubyte *mask)
// {
//    fxMesaContext fxMesa = FX_CONTEXT(ctx);
//    const GLubyte stencilVal = *((const GLubyte *) value);
//    GLint bottom = fxMesa->height - 1;
//    GLuint i;

//    if (TDFX_DEBUG & VERBOSE_DRIVER) {
//       fprintf(stderr, "fxWriteMonoStencilPixels(%d)\n", count);
//    }

//    for (i = 0; i < count; i++) {
//       if (!mask || mask[i]) {
//          GLuint zs32;
//          /* Read current depth/stencil value */
//          grLfbReadRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i], 1, 1, 0, &zs32);
//          /* Update stencil bits only */
//          zs32 = (zs32 & 0x00ffffff) | (stencilVal << 24);
//          grLfbWriteRegion(GR_BUFFER_AUXBUFFER, x[i], bottom - y[i],
//                           GR_LFB_SRC_FMT_ZA16, 1, 1, FXFALSE, 4, &zs32);
//       }
//    }
// }

/**
 * Read a row of stencil values
 */
void fxReadStencilSpan(GLcontext *ctx, struct gl_renderbuffer *rb,
                       GLuint count, GLint x, GLint y, void *values)
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLubyte *stencil = (GLubyte *) values;
   GLint bottom = fxMesa->height - 1;
   GLuint zs32[MAX_WIDTH];
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadStencilSpan(%d, %d, %d)\n", count, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, count, 1, 0, zs32);
   for (i = 0; i < count; i++) {
      stencil[i] = zs32[i] >> 24;
   }
}

/**
 * Write a row of stencil values
 */
void fxWriteStencilSpan(GLcontext *ctx, struct gl_renderbuffer *rb,
                        GLuint count, GLint x, GLint y, const void *values,
                        const GLubyte *mask)
{
   /* TODO: Implement stencil writing */
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteStencilSpan(%d, %d, %d) - TODO\n", count, x, y);
   }
}

/**
 * Write stencil pixels
 */
void fxWriteStencilPixels(GLcontext *ctx, struct gl_renderbuffer *rb,
                          GLuint count, const GLint x[], const GLint y[],
                          const void *values, const GLubyte *mask)
{
   /* TODO: Implement stencil pixel writing */
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteStencilPixels(%d) - TODO\n", count);
   }
}



#else

/*
 * Need this to provide at least one external definition.
 */
extern int gl_fx_dummy_function_renderbuffer(void);
int
gl_fx_dummy_function_renderbuffer(void)
{
   return 0;
}

#endif /* FX */
