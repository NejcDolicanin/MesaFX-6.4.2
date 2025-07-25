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
#include "renderbuffer.h"
#include "swrast/swrast.h"

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
   
   /* Set up span functions based on format */
   /* These will be set up in fxSetSpanFunctions() */
   
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
   
   return rb;
}

/**
 * Set up span functions for a renderbuffer based on its format
 */
void
fxSetSpanFunctions(struct gl_renderbuffer *rb, const GLvisual *vis)
{
   /* Mesa 6.3+ removed ctx from renderbuffer - get context from current context */
   GET_CURRENT_CONTEXT(ctx);
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxSetSpanFunctions(%p, format=%s)\n", 
              rb, _mesa_lookup_enum_by_nr(rb->InternalFormat));
   }
   
   if (rb->InternalFormat == GL_RGBA || rb->InternalFormat == GL_RGB) {
      /* Color buffer */
      if (fxMesa->colDepth == 32) {
         /* 32-bit ARGB8888 */
         rb->GetRow = fxReadRGBASpan_ARGB8888;
         rb->GetValues = NULL; /* TODO: implement pixel functions */
         rb->PutRow = NULL; /* TODO: implement write functions */
         rb->PutRowRGB = NULL;
         rb->PutMonoRow = NULL;
         rb->PutValues = NULL;
         rb->PutMonoValues = NULL;
      } else if (fxMesa->colDepth == 16) {
         /* 16-bit RGB565 */
         rb->GetRow = fxReadRGBASpan_RGB565;
         rb->GetValues = NULL; /* TODO: implement pixel functions */
         rb->PutRow = NULL; /* TODO: implement write functions */
         rb->PutRowRGB = NULL;
         rb->PutMonoRow = NULL;
         rb->PutValues = NULL;
         rb->PutMonoValues = NULL;
      } else {
         /* 15-bit ARGB1555 */
         rb->GetRow = fxReadRGBASpan_ARGB1555;
         rb->GetValues = NULL; /* TODO: implement pixel functions */
         rb->PutRow = NULL; /* TODO: implement write functions */
         rb->PutRowRGB = NULL;
         rb->PutMonoRow = NULL;
         rb->PutValues = NULL;
         rb->PutMonoValues = NULL;
      }
   } else if (rb->InternalFormat == GL_DEPTH_COMPONENT16 ||
              rb->InternalFormat == GL_DEPTH_COMPONENT24) {
      /* Depth buffer */
      if (fxMesa->haveHwStencil) {
         /* 24-bit depth */
         rb->GetRow = fxReadDepthSpan_Z24;
         rb->GetValues = NULL; /* TODO: implement pixel functions */
         rb->PutRow = NULL; /* TODO: implement write functions */
         rb->PutMonoRow = NULL;
         rb->PutValues = NULL;
         rb->PutMonoValues = NULL;
      } else {
         /* 16-bit depth */
         rb->GetRow = fxReadDepthSpan_Z16;
         rb->GetValues = NULL; /* TODO: implement pixel functions */
         rb->PutRow = NULL; /* TODO: implement write functions */
         rb->PutMonoRow = NULL;
         rb->PutValues = NULL;
         rb->PutMonoValues = NULL;
      }
   } else if (rb->InternalFormat == GL_STENCIL_INDEX8_EXT) {
      /* Stencil buffer */
      rb->GetRow = fxReadStencilSpan;
      rb->GetValues = NULL; /* TODO: implement pixel functions */
      rb->PutRow = fxWriteStencilSpan;
      rb->PutMonoRow = NULL; /* TODO: implement mono functions */
      rb->PutValues = fxWriteStencilPixels;
      rb->PutMonoValues = NULL;
   }
}

/* Mesa 6.3+ Renderbuffer PutRow/GetRow Functions */

/**
 * Read a row of ARGB8888 pixels from color buffer
 */
void fxReadRGBASpan_ARGB8888(const GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint n, GLint x, GLint y, GLubyte rgba[][4])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLuint i;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_ARGB8888(%d, %d, %d)\n", n, x, y);
   }
   
   grLfbReadRegion(fxMesa->currentFB, x, fxMesa->height - 1 - y, n, 1, n * 4, rgba);
   
   /* Convert from BGRA to RGBA */
   for (i = 0; i < n; i++) {
      GLubyte c = rgba[i][0];
      rgba[i][0] = rgba[i][2];
      rgba[i][2] = c;
   }
}

/**
 * Read a row of RGB565 pixels from color buffer
 */
void fxReadRGBASpan_RGB565(const GLcontext *ctx, struct gl_renderbuffer *rb,
                           GLuint n, GLint x, GLint y, GLubyte rgba[][4])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GrLfbInfo_t info;
   GLuint i, j;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_RGB565(%d, %d, %d)\n", n, x, y);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                   (winY - y) * info.strideInBytes +
                                                   x * 2);
      const GLuint *data32 = (const GLuint *)data16;
      GLuint extraPixel = (n & 1);
      GLuint n32 = n - extraPixel;

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
 * Read a row of ARGB1555 pixels from color buffer
 */
void fxReadRGBASpan_ARGB1555(const GLcontext *ctx, struct gl_renderbuffer *rb,
                             GLuint n, GLint x, GLint y, GLubyte rgba[][4])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GrLfbInfo_t info;
   GLuint i, j;
   
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadRGBASpan_ARGB1555(%d, %d, %d)\n", n, x, y);
   }
   
   info.size = sizeof(GrLfbInfo_t);
   if (grLfbLock(GR_LFB_READ_ONLY, fxMesa->currentFB,
                 GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info)) {
      const GLint winY = fxMesa->height - 1;
      const GLushort *data16 = (const GLushort *)((const GLubyte *)info.lfbPtr +
                                                   (winY - y) * info.strideInBytes +
                                                   x * 2);
      const GLuint *data32 = (const GLuint *)data16;
      GLuint extraPixel = (n & 1);
      GLuint n32 = n - extraPixel;

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
 * Read a row of 24-bit depth values
 */
void fxReadDepthSpan_Z24(const GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLuint n, GLint x, GLint y, GLuint depth[])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLint bottom = fxMesa->height - 1;
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthSpan_Z24(%d, %d, %d)\n", n, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, n, 1, 0, depth);
   for (i = 0; i < n; i++) {
      depth[i] &= 0xffffff;
   }
}

/**
 * Read a row of 16-bit depth values
 */
void fxReadDepthSpan_Z16(const GLcontext *ctx, struct gl_renderbuffer *rb,
                         GLuint n, GLint x, GLint y, GLuint depth[])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLint bottom = fxMesa->height - 1;
   GLushort depth16[MAX_WIDTH];
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadDepthSpan_Z16(%d, %d, %d)\n", n, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, n, 1, 0, depth16);
   for (i = 0; i < n; i++) {
      depth[i] = depth16[i];
   }
}

/**
 * Read a row of stencil values
 */
void fxReadStencilSpan(const GLcontext *ctx, struct gl_renderbuffer *rb,
                       GLuint n, GLint x, GLint y, GLubyte stencil[])
{
   fxMesaContext fxMesa = FX_CONTEXT(ctx);
   GLint bottom = fxMesa->height - 1;
   GLuint zs32[MAX_WIDTH];
   GLuint i;

   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxReadStencilSpan(%d, %d, %d)\n", n, x, y);
   }

   grLfbReadRegion(GR_BUFFER_AUXBUFFER, x, bottom - y, n, 1, 0, zs32);
   for (i = 0; i < n; i++) {
      stencil[i] = zs32[i] >> 24;
   }
}

/**
 * Write a row of stencil values
 */
void fxWriteStencilSpan(const GLcontext *ctx, struct gl_renderbuffer *rb,
                        GLuint n, GLint x, GLint y, const GLubyte stencil[],
                        const GLubyte mask[])
{
   /* TODO: Implement stencil writing */
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteStencilSpan(%d, %d, %d) - TODO\n", n, x, y);
   }
}

/**
 * Write stencil pixels
 */
void fxWriteStencilPixels(const GLcontext *ctx, struct gl_renderbuffer *rb,
                          GLuint n, const GLint x[], const GLint y[],
                          const GLubyte stencil[], const GLubyte mask[])
{
   /* TODO: Implement stencil pixel writing */
   if (TDFX_DEBUG & VERBOSE_DRIVER) {
      fprintf(stderr, "fxWriteStencilPixels(%d) - TODO\n", n);
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
