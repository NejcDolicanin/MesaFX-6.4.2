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

#ifndef FXRENDERBUFFER_H
#define FXRENDERBUFFER_H

#ifdef HAVE_CONFIG_H
#include "conf.h"
#endif

#if defined(FX)

#include "mtypes.h"

/* Function prototypes */
extern struct gl_renderbuffer *
fxNewColorRenderbuffer(GLcontext *ctx, GLuint name, GLenum internalFormat);

extern struct gl_renderbuffer *
fxNewDepthRenderbuffer(GLcontext *ctx, GLuint name);

extern struct gl_renderbuffer *
fxNewStencilRenderbuffer(GLcontext *ctx, GLuint name);

extern void
fxSetSpanFunctions(struct gl_renderbuffer *rb, const GLvisual *vis);

#endif /* FX */

#endif /* FXRENDERBUFFER_H */
