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

/* fxtexman.c - 3Dfx VooDoo texture memory functions */

#ifdef HAVE_CONFIG_H
#include "conf.h"
#endif
#include <stdlib.h>

/* FX */

#include "hash.h"
#include "fxdrv.h"

int texSwaps = 0;
static FxU32 texBoundMask;

/* NEJC SOF TMU Performance optimization: Track texture swapping to prevent thrashing */
static int consecutiveSwaps = 0;
static GLuint lastSwapFrame = 0;
static int g_max_consecutive_swaps = 8; /* Started with 3, after this many consecutive swaps, force new texture placements */
static int g_swap_reset_threshold = 10;

#define FX_2MB_SPLIT 0x200000

static struct gl_texture_object *fxTMFindOldestObject(fxMesaContext fxMesa,
                                                      int tmu);

#ifdef TEXSANITY
static void
fubar()
{
}

/* Sanity Check */
static void
sanity(fxMesaContext fxMesa)
{
   MemRange *tmp, *prev, *pos;

   prev = 0;
   tmp = fxMesa->tmFree[0];
   while (tmp)
   {
      if (!tmp->startAddr && !tmp->endAddr)
      {
         fprintf(stderr, "Textures fubar\n");
         fubar();
      }
      if (tmp->startAddr >= tmp->endAddr)
      {
         fprintf(stderr, "Node fubar\n");
         fubar();
      }
      if (prev && (prev->startAddr >= tmp->startAddr ||
                   prev->endAddr > tmp->startAddr))
      {
         fprintf(stderr, "Sorting fubar\n");
         fubar();
      }
      prev = tmp;
      tmp = tmp->next;
   }
   prev = 0;
   tmp = fxMesa->tmFree[1];
   while (tmp)
   {
      if (!tmp->startAddr && !tmp->endAddr)
      {
         fprintf(stderr, "Textures fubar\n");
         fubar();
      }
      if (tmp->startAddr >= tmp->endAddr)
      {
         fprintf(stderr, "Node fubar\n");
         fubar();
      }
      if (prev && (prev->startAddr >= tmp->startAddr ||
                   prev->endAddr > tmp->startAddr))
      {
         fprintf(stderr, "Sorting fubar\n");
         fubar();
      }
      prev = tmp;
      tmp = tmp->next;
   }
}
#endif

static MemRange *
fxTMNewRangeNode(fxMesaContext fxMesa, FxU32 start, FxU32 end)
{
   MemRange *result = 0;

   if (fxMesa->tmPool)
   {
      result = fxMesa->tmPool;
      fxMesa->tmPool = fxMesa->tmPool->next;
   }
   else
   {
      if (!(result = MALLOC(sizeof(MemRange))))
      {
         fprintf(stderr, "fxTMNewRangeNode: ERROR: out of memory!\n");
         fxCloseHardware();
         exit(-1);
      }
   }
   result->startAddr = start;
   result->endAddr = end;
   return result;
}

#if 1
#define fxTMDeleteRangeNode(fxMesa, range) \
   do                                      \
   {                                       \
      range->next = fxMesa->tmPool;        \
      fxMesa->tmPool = range;              \
   } while (0);
#else
static void
fxTMDeleteRangeNode(fxMesaContext fxMesa, MemRange *range)
{
   range->next = fxMesa->tmPool;
   fxMesa->tmPool = range;
}
#endif

static void
fxTMUInit(fxMesaContext fxMesa, int tmu)
{
   MemRange *tmn, *last;
   FxU32 start, end, blockstart, blockend, chunk;

   start = grTexMinAddress(tmu);
   end = grTexMaxAddress(tmu);

   chunk = (fxMesa->type >= GR_SSTTYPE_Banshee) ? (end - start) : FX_2MB_SPLIT;

   if (fxMesa->verbose)
   {
      fprintf(stderr, "Voodoo TMU%d configuration:\n", tmu);
   }

   fxMesa->freeTexMem[tmu] = end - start;
   fxMesa->tmFree[tmu] = NULL;

   last = 0;
   blockstart = start;
   while (blockstart < end)
   {
      if (blockstart + chunk > end)
         blockend = end;
      else
         blockend = blockstart + chunk;

      if (fxMesa->verbose)
         fprintf(stderr, "Voodoo   %08u-%08u\n",
                 (unsigned int)blockstart, (unsigned int)blockend);

      tmn = fxTMNewRangeNode(fxMesa, blockstart, blockend);
      tmn->next = NULL;

      if (last)
         last->next = tmn;
      else
         fxMesa->tmFree[tmu] = tmn;
      last = tmn;

      blockstart += chunk;
   }
}

static int
fxTMFindStartAddr(fxMesaContext fxMesa, GLint tmu, int size)
{
   MemRange *prev, *tmp;
   int result;
   struct gl_texture_object *obj;
   GLuint currentFrame = fxMesa->stats.swapBuffer;

   if (fxMesa->HaveTexUma)
   {
      tmu = FX_TMU0;
   }

   /* NEJC SOF TMU: Reset consecutive swaps counter if enough time has passed */
   if (currentFrame != lastSwapFrame)
   {
      if (currentFrame - lastSwapFrame > g_swap_reset_threshold)
      {
         consecutiveSwaps = 0;
      }
      lastSwapFrame = currentFrame;
   }

   while (1)
   {
      prev = 0;
      tmp = fxMesa->tmFree[tmu];
      while (tmp)
      {
         if (tmp->endAddr - tmp->startAddr >= size)
         { /* Fits here */
            result = tmp->startAddr;
            result = (result + 31) & ~31;   /* 32-byte alignment */
            tmp->startAddr = result + size; /* Unused padding space (alignment gap) is naturally consumed, not lost */
            if (tmp->startAddr == tmp->endAddr)
            { /* Empty */
               if (prev)
               {
                  prev->next = tmp->next;
               }
               else
               {
                  fxMesa->tmFree[tmu] = tmp->next;
               }
               fxTMDeleteRangeNode(fxMesa, tmp);
            }
            fxMesa->freeTexMem[tmu] -= size;
            /* NEJC SOF TMU: Reset swap counter on successful allocation */
            consecutiveSwaps = 0;
            return result;
         }
         prev = tmp;
         tmp = tmp->next;
      }

      /* NEJC SOF TMU: Check for texture thrashing before evicting */
      if (consecutiveSwaps >= g_max_consecutive_swaps)
      {
         if (TDFX_DEBUG & VERBOSE_TEXTURE)
         {
            fprintf(stderr, "fxTMFindStartAddr: Texture thrashing detected, failing allocation\n");
         }
         return -1; /* Force fallback to prevent thrashing */
      }

      /* No free space. Discard oldest */
      if (TDFX_DEBUG & VERBOSE_TEXTURE)
      {
         fprintf(stderr, "fxTMFindStartAddr: No free space. Discard oldest\n");
      }
      obj = fxTMFindOldestObject(fxMesa, tmu);
      if (!obj)
      {
         fprintf(stderr, "fxTMFindStartAddr: ERROR: No space for texture\n");
         return -1;
      }
      fxTMMoveOutTM(fxMesa, obj);
      texSwaps++;
      fxMesa->stats.evictions_per_frame++;
      consecutiveSwaps++; /* NEJC SOF TMU: Track consecutive swaps */
   }
}

int fxTMCheckStartAddr(fxMesaContext fxMesa, GLint tmu, tfxTexInfo *ti)
{
   MemRange *tmp;
   int size;

   if (fxMesa->HaveTexUma)
   {
      return FXTRUE;
   }

   size = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &(ti->info));

   tmp = fxMesa->tmFree[tmu];
   while (tmp)
   {
      if (tmp->endAddr - tmp->startAddr >= size)
      { /* Fits here */
         return FXTRUE;
      }
      tmp = tmp->next;
   }

   return FXFALSE;
}

static void
fxTMRemoveRange(fxMesaContext fxMesa, GLint tmu, MemRange *range)
{
   MemRange *tmp, *prev;

   if (fxMesa->HaveTexUma)
   {
      tmu = FX_TMU0;
   }

   if (range->startAddr == range->endAddr)
   {
      fxTMDeleteRangeNode(fxMesa, range);
      return;
   }
   fxMesa->freeTexMem[tmu] += range->endAddr - range->startAddr;
   prev = 0;
   tmp = fxMesa->tmFree[tmu];
   while (tmp)
   {
      if (range->startAddr > tmp->startAddr)
      {
         prev = tmp;
         tmp = tmp->next;
      }
      else
         break;
   }
   /* When we create the regions, we make a split at the 2MB boundary.
      Now we have to make sure we don't join those 2MB boundary regions
      back together again. */
   range->next = tmp;
   if (tmp)
   {
      if (range->endAddr == tmp->startAddr && tmp->startAddr & texBoundMask)
      {
         /* Combine */
         tmp->startAddr = range->startAddr;
         fxTMDeleteRangeNode(fxMesa, range);
         range = tmp;
      }
   }
   if (prev)
   {
      if (prev->endAddr == range->startAddr && range->startAddr & texBoundMask)
      {
         /* Combine */
         prev->endAddr = range->endAddr;
         prev->next = range->next;
         fxTMDeleteRangeNode(fxMesa, range);
      }
      else
         prev->next = range;
   }
   else
   {
      fxMesa->tmFree[tmu] = range;
   }
}

static struct gl_texture_object *
fxTMFindOldestObject(fxMesaContext fxMesa, int tmu)
{
   struct _mesa_HashTable *textures = fxMesa->glCtx->Shared->TexObjects;
   GLuint id = _mesa_HashFirstEntry(textures);
   struct gl_texture_object *best = NULL;
   GLfloat bestPri = 2.0F;
   GLuint bestAge = 0;
   GLuint bindnumber = fxMesa->texBindNumber;

   if (!id)
      return NULL;

   while (id)
   {
      struct gl_texture_object *tmp =
          (struct gl_texture_object *)_mesa_HashLookup(textures, id);
      tfxTexInfo *info = fxTMGetTexInfo(tmp);

      if (info && info->isInTM &&
          (fxMesa->HaveTexUma ||
           info->whichTMU == (FxU32)tmu ||
           info->whichTMU == FX_TMU_BOTH ||
           info->whichTMU == FX_TMU_SPLIT))
      {

         /* Respect pin window: skip as eviction candidate */
         if (info->pin_until_frame && info->pin_until_frame > fxMesa->frame_no)
         {
            id = _mesa_HashNextEntry(textures, id);
            continue;
         }

         /* Compute age based on bind number, handle wrap */
         GLuint age;
         if (info->lastTimeUsed > bindnumber)
            age = bindnumber + (UINT_MAX - info->lastTimeUsed + 1);
         else
            age = bindnumber - info->lastTimeUsed;

         /* Prefer lowest priority, then oldest age */
         if (tmp->Priority < bestPri ||
             (tmp->Priority == bestPri && age > bestAge))
         {
            bestPri = tmp->Priority;
            bestAge = age;
            best = tmp;
         }
      }

      id = _mesa_HashNextEntry(textures, id);
   }

   if ((TDFX_DEBUG & VERBOSE_TEXTURE) && best)
   {
      fprintf(stderr, "fxTMFindOldestObject: %d age=%u pri=%f\n",
              best->Name, bestAge, bestPri);
   }
   return best;
}

static MemRange *
fxTMAddObj(fxMesaContext fxMesa,
           struct gl_texture_object *tObj, GLint tmu, int texmemsize)
{
   FxI32 startAddr;
   MemRange *range;

   startAddr = fxTMFindStartAddr(fxMesa, tmu, texmemsize);
   if (startAddr < 0)
      return 0;
   range = fxTMNewRangeNode(fxMesa, startAddr, startAddr + texmemsize);
   return range;
}

/* External Functions */

void fxTMMoveInTM_NoLock(fxMesaContext fxMesa, struct gl_texture_object *tObj,
                         GLint where)
{
   tfxTexInfo *ti = fxTMGetTexInfo(tObj);
   int i, l;
   int texmemsize;

   if (TDFX_DEBUG & VERBOSE_DRIVER)
   {
      fprintf(stderr, "fxTMMoveInTM_NoLock(%d)\n", tObj->Name);
   }

   fxMesa->stats.reqTexUpload++;

   if (!ti->validated)
   {
      fprintf(stderr, "fxTMMoveInTM_NoLock: INTERNAL ERROR: not validated\n");
      fxCloseHardware();
      exit(-1);
   }

   if (ti->isInTM)
   {
      if (ti->whichTMU == where)
         return;
      if (where == FX_TMU_SPLIT || ti->whichTMU == FX_TMU_SPLIT)
         fxTMMoveOutTM_NoLock(fxMesa, tObj);
      else
      {
         if (ti->whichTMU == FX_TMU_BOTH)
            return;
         where = FX_TMU_BOTH;
      }
   }

   if (TDFX_DEBUG & (VERBOSE_DRIVER | VERBOSE_TEXTURE))
   {
      fprintf(stderr, "fxTMMoveInTM_NoLock: downloading %p (%d) in texture memory in %d\n",
              (void *)tObj, tObj->Name, where);
   }

   ti->whichTMU = (FxU32)where;

   /* TMU Optimizations - Replicate small hot textures to BOTH TMUs to avoid swaps on rapid alternation (e.g., muzzle flashes)
    * Only when we have two TMUs and are not in UMA mode.
    Lightmaps (LUMINANCE/INTENSITY) also qualify but with a tighter threshold.
    */
   if (!fxMesa->HaveTexUma && fxMesa->haveTwoTMUs)
   {
      int szBoth = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &(ti->info));
      int threshold = (ti->baseLevelInternalFormat == GL_LUMINANCE ||
                       ti->baseLevelInternalFormat == GL_INTENSITY) ? (8 * 1024) : (16 * 1024);
      if (szBoth <= threshold) {
         where = FX_TMU_BOTH;
         ti->whichTMU = FX_TMU_BOTH;
      }
   }

   /* NEJC SOF: UMA / pool selection sanity - lock pool choice on first residency */
   if (ti->pool < 0)
   {
      ti->pool = (fxMesa->HaveTexUma ? 0 : where);
   }

   switch (where)
   {
   case FX_TMU0:
   case FX_TMU1:
      texmemsize = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &(ti->info));
      ti->tm[where] = fxTMAddObj(fxMesa, tObj, ti->pool, texmemsize);
      fxMesa->stats.memTexUpload += texmemsize;
      fxMesa->stats.bytes_uploaded_tmu[where] += texmemsize;

      for (i = FX_largeLodValue(ti->info), l = ti->minLevel;
           i <= FX_smallLodValue(ti->info); i++, l++)
      {
         struct gl_texture_image *texImage = tObj->Image[0][l];
         grTexDownloadMipMapLevel(where,
                                  ti->tm[where]->startAddr,
                                  FX_valueToLod(i),
                                  FX_largeLodLog2(ti->info),
                                  FX_aspectRatioLog2(ti->info),
                                  ti->info.format,
                                  GR_MIPMAPLEVELMASK_BOTH,
                                  texImage->Data);

         fxMesa->stats.uploads_per_frame++;
      }
      break;
   case FX_TMU_SPLIT:
      texmemsize = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_ODD, &(ti->info));
      ti->tm[FX_TMU0] = fxTMAddObj(fxMesa, tObj, FX_TMU0, texmemsize);
      fxMesa->stats.memTexUpload += texmemsize;
      fxMesa->stats.bytes_uploaded_tmu[FX_TMU0] += texmemsize;

      texmemsize = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_EVEN, &(ti->info));
      ti->tm[FX_TMU1] = fxTMAddObj(fxMesa, tObj, FX_TMU1, texmemsize);
      fxMesa->stats.memTexUpload += texmemsize;
      fxMesa->stats.bytes_uploaded_tmu[FX_TMU1] += texmemsize;

      for (i = FX_largeLodValue(ti->info), l = ti->minLevel;
           i <= FX_smallLodValue(ti->info); i++, l++)
      {
         struct gl_texture_image *texImage = tObj->Image[0][l];

         grTexDownloadMipMapLevel(GR_TMU0,
                                  ti->tm[FX_TMU0]->startAddr,
                                  FX_valueToLod(i),
                                  FX_largeLodLog2(ti->info),
                                  FX_aspectRatioLog2(ti->info),
                                  ti->info.format,
                                  GR_MIPMAPLEVELMASK_ODD,
                                  texImage->Data);

         grTexDownloadMipMapLevel(GR_TMU1,
                                  ti->tm[FX_TMU1]->startAddr,
                                  FX_valueToLod(i),
                                  FX_largeLodLog2(ti->info),
                                  FX_aspectRatioLog2(ti->info),
                                  ti->info.format,
                                  GR_MIPMAPLEVELMASK_EVEN,
                                  texImage->Data);

         fxMesa->stats.uploads_per_frame += 2;
      }
      break;
   case FX_TMU_BOTH:
      texmemsize = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &(ti->info));
      ti->tm[FX_TMU0] = fxTMAddObj(fxMesa, tObj, FX_TMU0, texmemsize);
      fxMesa->stats.memTexUpload += texmemsize;
      fxMesa->stats.bytes_uploaded_tmu[FX_TMU0] += texmemsize;

      /*texmemsize = (int)grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &(ti->info));*/
      ti->tm[FX_TMU1] = fxTMAddObj(fxMesa, tObj, FX_TMU1, texmemsize);
      fxMesa->stats.memTexUpload += texmemsize;
      fxMesa->stats.bytes_uploaded_tmu[FX_TMU1] += texmemsize;

      for (i = FX_largeLodValue(ti->info), l = ti->minLevel;
           i <= FX_smallLodValue(ti->info); i++, l++)
      {
         struct gl_texture_image *texImage = tObj->Image[0][l];
         grTexDownloadMipMapLevel(GR_TMU0,
                                  ti->tm[FX_TMU0]->startAddr,
                                  FX_valueToLod(i),
                                  FX_largeLodLog2(ti->info),
                                  FX_aspectRatioLog2(ti->info),
                                  ti->info.format,
                                  GR_MIPMAPLEVELMASK_BOTH,
                                  texImage->Data);

         grTexDownloadMipMapLevel(GR_TMU1,
                                  ti->tm[FX_TMU1]->startAddr,
                                  FX_valueToLod(i),
                                  FX_largeLodLog2(ti->info),
                                  FX_aspectRatioLog2(ti->info),
                                  ti->info.format,
                                  GR_MIPMAPLEVELMASK_BOTH,
                                  texImage->Data);

         fxMesa->stats.uploads_per_frame += 2;
      }
      break;
   default:
      fprintf(stderr, "fxTMMoveInTM_NoLock: INTERNAL ERROR: wrong tmu (%d)\n", where);
      fxCloseHardware();
      exit(-1);
   }

   fxMesa->stats.texUpload++;

   ti->isInTM = GL_TRUE;
}

void fxTMMoveInTM(fxMesaContext fxMesa, struct gl_texture_object *tObj,
                  GLint where)
{
   BEGIN_BOARD_LOCK();
   fxTMMoveInTM_NoLock(fxMesa, tObj, where);
   END_BOARD_LOCK();
}

void fxTMReloadMipMapLevel(fxMesaContext fxMesa, struct gl_texture_object *tObj,
                           GLint level)
{
   tfxTexInfo *ti = fxTMGetTexInfo(tObj);
   GrLOD_t lodlevel;
   GLint tmu;
   struct gl_texture_image *texImage = tObj->Image[0][level];
   tfxMipMapLevel *mml = FX_MIPMAP_DATA(texImage);

   if (TDFX_DEBUG & VERBOSE_TEXTURE)
   {
      fprintf(stderr, "fxTMReloadMipMapLevel(%p (%d), %d)\n", (void *)tObj, tObj->Name, level);
   }

   assert(mml);
   assert(mml->width > 0);
   assert(mml->height > 0);
   assert(mml->glideFormat > 0);
   assert(ti->isInTM);

   if (!ti->validated)
   {
      fprintf(stderr, "fxTMReloadMipMapLevel: INTERNAL ERROR: not validated\n");
      fxCloseHardware();
      exit(-1);
   }

   tmu = (int)ti->whichTMU;
   fxMesa->stats.reqTexUpload++;
   fxMesa->stats.texUpload++;

   /* Compute Glide LOD level from level index */
   lodlevel = ti->info.largeLodLog2 - (level - ti->minLevel);

   switch (tmu)
   {
   case FX_TMU0:
   case FX_TMU1:
      grTexDownloadMipMapLevel(tmu,
                               ti->tm[tmu]->startAddr,
                               lodlevel,
                               FX_largeLodLog2(ti->info),
                               FX_aspectRatioLog2(ti->info),
                               ti->info.format,
                               GR_MIPMAPLEVELMASK_BOTH, texImage->Data);

      fxMesa->stats.uploads_per_frame++;
      break;
   case FX_TMU_SPLIT:
      grTexDownloadMipMapLevel(GR_TMU0,
                               ti->tm[GR_TMU0]->startAddr,
                               lodlevel,
                               FX_largeLodLog2(ti->info),
                               FX_aspectRatioLog2(ti->info),
                               ti->info.format,
                               GR_MIPMAPLEVELMASK_ODD, texImage->Data);

      grTexDownloadMipMapLevel(GR_TMU1,
                               ti->tm[GR_TMU1]->startAddr,
                               lodlevel,
                               FX_largeLodLog2(ti->info),
                               FX_aspectRatioLog2(ti->info),
                               ti->info.format,
                               GR_MIPMAPLEVELMASK_EVEN, texImage->Data);

      fxMesa->stats.uploads_per_frame += 2;
      break;
   case FX_TMU_BOTH:
      grTexDownloadMipMapLevel(GR_TMU0,
                               ti->tm[GR_TMU0]->startAddr,
                               lodlevel,
                               FX_largeLodLog2(ti->info),
                               FX_aspectRatioLog2(ti->info),
                               ti->info.format,
                               GR_MIPMAPLEVELMASK_BOTH, texImage->Data);

      grTexDownloadMipMapLevel(GR_TMU1,
                               ti->tm[GR_TMU1]->startAddr,
                               lodlevel,
                               FX_largeLodLog2(ti->info),
                               FX_aspectRatioLog2(ti->info),
                               ti->info.format,
                               GR_MIPMAPLEVELMASK_BOTH, texImage->Data);

      fxMesa->stats.uploads_per_frame += 2;
      break;

   default:
      fprintf(stderr, "fxTMReloadMipMapLevel: INTERNAL ERROR: wrong tmu (%d)\n", tmu);
      fxCloseHardware();
      exit(-1);
   }
}

void fxTMReloadSubMipMapLevel(fxMesaContext fxMesa,
                              struct gl_texture_object *tObj,
                              GLint level, GLint yoffset, GLint height)
{
   tfxTexInfo *ti = fxTMGetTexInfo(tObj);
   GrLOD_t lodlevel;
   void *data;
   GLint tmu;
   struct gl_texture_image *texImage = tObj->Image[0][level];
   tfxMipMapLevel *mml = FX_MIPMAP_DATA(texImage);

   assert(mml);

   if (!ti->validated)
   {
      fprintf(stderr, "fxTMReloadSubMipMapLevel: INTERNAL ERROR: not validated\n");
      fxCloseHardware();
      exit(-1);
   }

   /* Ensure residency only if needed */
   tmu = (int)ti->whichTMU;
   if (!ti->isInTM || ti->tm[tmu] == NULL)
   {
      fxTMMoveInTM(fxMesa, tObj, tmu);
   }

   /* Compute lod level consistent with full uploads */
   lodlevel = ti->info.largeLodLog2 - (level - ti->minLevel);

   /* Compute byte-accurate row pointer */
   {
      int bpp = 2;
      switch (ti->info.format)
      {
      case GR_TEXFMT_INTENSITY_8:
      case GR_TEXFMT_P_8:
      case GR_TEXFMT_ALPHA_8:
         bpp = 1;
         break;
      case GR_TEXFMT_ARGB_8888:
         bpp = 4;
         break;
      default:
         /* Most uncompressed Glide formats here are 16-bit (2 bytes) */
         bpp = 2;
         break;
      }
      data = (void *)((GLubyte *)texImage->Data + (yoffset * mml->width * bpp));
   }

   switch (tmu)
   {
   case FX_TMU0:
   case FX_TMU1:
      grTexDownloadMipMapLevelPartial(tmu,
                                      ti->tm[tmu]->startAddr,
                                      lodlevel,
                                      FX_largeLodLog2(ti->info),
                                      FX_aspectRatioLog2(ti->info),
                                      ti->info.format,
                                      GR_MIPMAPLEVELMASK_BOTH, data,
                                      yoffset, yoffset + height - 1);

      fxMesa->stats.subuploads_per_frame++;
      break;
   case FX_TMU_SPLIT:
      grTexDownloadMipMapLevelPartial(GR_TMU0,
                                      ti->tm[FX_TMU0]->startAddr,
                                      lodlevel,
                                      FX_largeLodLog2(ti->info),
                                      FX_aspectRatioLog2(ti->info),
                                      ti->info.format,
                                      GR_MIPMAPLEVELMASK_ODD, data,
                                      yoffset, yoffset + height - 1);

      grTexDownloadMipMapLevelPartial(GR_TMU1,
                                      ti->tm[FX_TMU1]->startAddr,
                                      lodlevel,
                                      FX_largeLodLog2(ti->info),
                                      FX_aspectRatioLog2(ti->info),
                                      ti->info.format,
                                      GR_MIPMAPLEVELMASK_EVEN, data,
                                      yoffset, yoffset + height - 1);

      fxMesa->stats.subuploads_per_frame += 2;
      break;
   case FX_TMU_BOTH:
      grTexDownloadMipMapLevelPartial(GR_TMU0,
                                      ti->tm[FX_TMU0]->startAddr,
                                      lodlevel,
                                      FX_largeLodLog2(ti->info),
                                      FX_aspectRatioLog2(ti->info),
                                      ti->info.format,
                                      GR_MIPMAPLEVELMASK_BOTH, data,
                                      yoffset, yoffset + height - 1);

      grTexDownloadMipMapLevelPartial(GR_TMU1,
                                      ti->tm[FX_TMU1]->startAddr,
                                      lodlevel,
                                      FX_largeLodLog2(ti->info),
                                      FX_aspectRatioLog2(ti->info),
                                      ti->info.format,
                                      GR_MIPMAPLEVELMASK_BOTH, data,
                                      yoffset, yoffset + height - 1);

      fxMesa->stats.subuploads_per_frame += 2;
      break;
   default:
      fprintf(stderr, "fxTMReloadSubMipMapLevel: INTERNAL ERROR: wrong tmu (%d)\n", tmu);
      fxCloseHardware();
      exit(-1);
   }
}

void fxTMMoveOutTM(fxMesaContext fxMesa, struct gl_texture_object *tObj)
{
   tfxTexInfo *ti = fxTMGetTexInfo(tObj);

   if (TDFX_DEBUG & VERBOSE_DRIVER)
   {
      fprintf(stderr, "fxTMMoveOutTM(%p (%d))\n", (void *)tObj, tObj->Name);
   }

   if (!ti->isInTM)
      return;

   switch (ti->whichTMU)
   {
   case FX_TMU0:
   case FX_TMU1:
      fxTMRemoveRange(fxMesa, (int)ti->whichTMU, ti->tm[ti->whichTMU]);
      break;
   case FX_TMU_SPLIT:
   case FX_TMU_BOTH:
      fxTMRemoveRange(fxMesa, FX_TMU0, ti->tm[FX_TMU0]);
      fxTMRemoveRange(fxMesa, FX_TMU1, ti->tm[FX_TMU1]);
      break;
   default:
      fprintf(stderr, "fxTMMoveOutTM: INTERNAL ERROR: bad TMU (%ld)\n", ti->whichTMU);
      fxCloseHardware();
      exit(-1);
   }

   ti->isInTM = GL_FALSE;
   ti->whichTMU = FX_TMU_NONE;
}

void fxTMFreeTexture(fxMesaContext fxMesa, struct gl_texture_object *tObj)
{
   tfxTexInfo *ti = fxTMGetTexInfo(tObj);
   int i;

   if (TDFX_DEBUG & VERBOSE_TEXTURE)
   {
      fprintf(stderr, "fxTMFreeTexture(%p (%d))\n", (void *)tObj, tObj->Name);
   }

   /* Move out of TMU first - this handles range cleanup for all TMUs */
   fxTMMoveOutTM(fxMesa, tObj);

   /* Clean up texture image driver data */
   for (i = 0; i < MAX_TEXTURE_LEVELS; i++)
   {
      struct gl_texture_image *texImage = tObj->Image[0][i];
      if (texImage)
      {
         if (texImage->DriverData)
         {
            FREE(texImage->DriverData);
            texImage->DriverData = NULL;
         }
      }
   }
}

void fxTMInit(fxMesaContext fxMesa)
{
   fxMesa->texBindNumber = 0;
   fxMesa->tmPool = 0;

   if (fxMesa->HaveTexUma)
   {
      grEnable(GR_TEXTURE_UMA_EXT);
   }

   fxTMUInit(fxMesa, FX_TMU0);

   if (!fxMesa->HaveTexUma && fxMesa->haveTwoTMUs)
      fxTMUInit(fxMesa, FX_TMU1);

   texBoundMask = (fxMesa->type >= GR_SSTTYPE_Banshee) ? -1 : (FX_2MB_SPLIT - 1);
}

void fxTMClose(fxMesaContext fxMesa)
{
   MemRange *tmp, *next;

   tmp = fxMesa->tmPool;
   while (tmp)
   {
      next = tmp->next;
      FREE(tmp);
      tmp = next;
   }
   tmp = fxMesa->tmFree[FX_TMU0];
   while (tmp)
   {
      next = tmp->next;
      FREE(tmp);
      tmp = next;
   }
   if (fxMesa->haveTwoTMUs)
   {
      tmp = fxMesa->tmFree[FX_TMU1];
      while (tmp)
      {
         next = tmp->next;
         FREE(tmp);
         tmp = next;
      }
   }
}

/* Batch subuploads disabled */
void fxTMFlushPendingSubUploads_NoLock(fxMesaContext ctx)
{
   (void)ctx;

   // int budget = 64 * 1024; /* cap partial upload traffic per frame to reduce stutter */
   // struct _mesa_HashTable *textures = ctx->glCtx->Shared->TexObjects;
   // GLuint id;

   // for (id = _mesa_HashFirstEntry(textures);
   //      id;
   //      id = _mesa_HashNextEntry(textures, id))
   // {
   //    struct gl_texture_object *tObj =
   //        (struct gl_texture_object *)_mesa_HashLookup(textures, id);
   //    tfxTexInfo *ti = fxTMGetTexInfo(tObj);

   //    if (!ti || !ti->has_dirty_subimage || !ti->isInTM)
   //       continue;

   //    /* Determine level interval to flush */
   //    GLint lmin = (ti->dirty_level_min >= 0) ? ti->dirty_level_min : ti->minLevel;
   //    GLint lmax = (ti->dirty_level_max >= 0) ? ti->dirty_level_max : lmin;

   //    /* Resolve target TMU configuration once */
   //    GLint tmu = (GLint)ti->whichTMU;

   //    for (GLint level = lmin; level <= lmax; ++level)
   //    {
   //       struct gl_texture_image *texImage = tObj->Image[0][level];
   //       if (!texImage)
   //          continue;

   //       tfxMipMapLevel *mml = FX_MIPMAP_DATA(texImage);
   //       if (!mml || mml->width <= 0 || mml->height <= 0)
   //          continue;

   //       /* Compute lodlevel for this level */
   //       GrLOD_t lodlevel = ti->info.largeLodLog2 - (level - ti->minLevel);

   //       /* Clamp dirty Y range to this level's height */
   //       GLint y0 = CLAMP(ti->dirty_minY, 0, mml->height - 1);
   //       GLint y1 = CLAMP(ti->dirty_maxY, y0, mml->height - 1);

   //       /* Provide pointer to first texel of the start row y0 */
   //       void *data;
   //       {
   //          const GLboolean is8 = (ti->info.format == GR_TEXFMT_INTENSITY_8) ||
   //                                (ti->info.format == GR_TEXFMT_P_8) ||
   //                                (ti->info.format == GR_TEXFMT_ALPHA_8);
   //          const int bpp = is8 ? 1 : 2;
   //          data = (void *)((GLubyte *)texImage->Data + y0 * mml->width * bpp);
   //       }

   //       /* Issue partial uploads according to residency */
   //       /* Compute how many rows we can upload within this frame's budget */
   //       int bpp = ((ti->info.format == GR_TEXFMT_INTENSITY_8) ||
   //                  (ti->info.format == GR_TEXFMT_P_8) ||
   //                  (ti->info.format == GR_TEXFMT_ALPHA_8))
   //                     ? 1
   //                     : 2;
   //       int rows_fit = (budget > 0) ? budget / (mml->width * bpp) : 0;
   //       if (rows_fit <= 0)
   //          rows_fit = 1;
   //       if (rows_fit > (y1 - y0 + 1))
   //          rows_fit = (y1 - y0 + 1);
   //       int y1_eff = y0 + rows_fit - 1;

   //       /* Issue partial uploads according to residency */
   //       switch (tmu)
   //       {
   //       case FX_TMU0:
   //       case FX_TMU1:
   //          grTexDownloadMipMapLevelPartial(tmu,
   //                                          ti->tm[tmu]->startAddr,
   //                                          lodlevel,
   //                                          FX_largeLodLog2(ti->info),
   //                                          FX_aspectRatioLog2(ti->info),
   //                                          ti->info.format,
   //                                          GR_MIPMAPLEVELMASK_BOTH,
   //                                          data,
   //                                          y0, y1_eff);
   //          ctx->stats.subuploads_per_frame++;
   //          /* budget accounting and early exit if we hit the cap */
   //          budget -= rows_fit * mml->width * bpp;
   //          if (budget <= 0 && (y1_eff < y1 || level < lmax))
   //          {
   //             ti->has_dirty_subimage = GL_TRUE;
   //             ti->dirty_minY = y1_eff + 1;
   //             ti->dirty_level_min = level;
   //             return;
   //          }
   //          ti->upload_stamp[tmu] = ctx->frame_no;
   //          break;

   //       case FX_TMU_SPLIT:
   //          grTexDownloadMipMapLevelPartial(GR_TMU0,
   //                                          ti->tm[FX_TMU0]->startAddr,
   //                                          lodlevel,
   //                                          FX_largeLodLog2(ti->info),
   //                                          FX_aspectRatioLog2(ti->info),
   //                                          ti->info.format,
   //                                          GR_MIPMAPLEVELMASK_ODD,
   //                                          data,
   //                                          y0, y1_eff);
   //          grTexDownloadMipMapLevelPartial(GR_TMU1,
   //                                          ti->tm[FX_TMU1]->startAddr,
   //                                          lodlevel,
   //                                          FX_largeLodLog2(ti->info),
   //                                          FX_aspectRatioLog2(ti->info),
   //                                          ti->info.format,
   //                                          GR_MIPMAPLEVELMASK_EVEN,
   //                                          data,
   //                                          y0, y1_eff);
   //          ctx->stats.subuploads_per_frame += 2;
   //          /* budget accounting and early exit if we hit the cap */
   //          budget -= 2 * rows_fit * mml->width * bpp;
   //          if (budget <= 0 && (y1_eff < y1 || level < lmax))
   //          {
   //             ti->has_dirty_subimage = GL_TRUE;
   //             ti->dirty_minY = y1_eff + 1;
   //             ti->dirty_level_min = level;
   //             return;
   //          }
   //          ti->upload_stamp[FX_TMU0] = ctx->frame_no;
   //          ti->upload_stamp[FX_TMU1] = ctx->frame_no;
   //          break;

   //       case FX_TMU_BOTH:
   //          grTexDownloadMipMapLevelPartial(GR_TMU0,
   //                                          ti->tm[FX_TMU0]->startAddr,
   //                                          lodlevel,
   //                                          FX_largeLodLog2(ti->info),
   //                                          FX_aspectRatioLog2(ti->info),
   //                                          ti->info.format,
   //                                          GR_MIPMAPLEVELMASK_BOTH,
   //                                          data,
   //                                          y0, y1_eff);
   //          grTexDownloadMipMapLevelPartial(GR_TMU1,
   //                                          ti->tm[FX_TMU1]->startAddr,
   //                                          lodlevel,
   //                                          FX_largeLodLog2(ti->info),
   //                                          FX_aspectRatioLog2(ti->info),
   //                                          ti->info.format,
   //                                          GR_MIPMAPLEVELMASK_BOTH,
   //                                          data,
   //                                          y0, y1_eff);
   //          ctx->stats.subuploads_per_frame += 2;
   //          /* budget accounting and early exit if we hit the cap */
   //          budget -= 2 * rows_fit * mml->width * bpp;
   //          if (budget <= 0 && (y1_eff < y1 || level < lmax))
   //          {
   //             ti->has_dirty_subimage = GL_TRUE;
   //             ti->dirty_minY = y1_eff + 1;
   //             ti->dirty_level_min = level;
   //             return;
   //          }
   //          ti->upload_stamp[FX_TMU0] = ctx->frame_no;
   //          ti->upload_stamp[FX_TMU1] = ctx->frame_no;
   //          break;

   //       default:
   //          /* do nothing */
   //          break;
   //       }
   //    }

   //    /* Clear dirty markers after flush */
   //    ti->has_dirty_subimage = GL_FALSE;
   //    ti->dirty_minY = ti->dirty_maxY = -1;
   //    ti->dirty_level_min = ti->dirty_level_max = -1;
   // }
}

void fxTMRestoreTextures_NoLock(fxMesaContext ctx)
{
   (void)ctx;

   // struct _mesa_HashTable *textures = ctx->glCtx->Shared->TexObjects;
   // GLuint id;

   // for (id = _mesa_HashFirstEntry(textures);
   //      id;
   //      id = _mesa_HashNextEntry(textures, id))
   // {
   //    struct gl_texture_object *tObj = (struct gl_texture_object *)_mesa_HashLookup(textures, id);
   //    tfxTexInfo *ti = fxTMGetTexInfo(tObj);
   //    if (ti && ti->isInTM)
   //    {
   //       int i;
   //       for (i = 0; i < MAX_TEXTURE_UNITS; i++)
   //       {
   //          if (ctx->glCtx->Texture.Unit[i]._Current == tObj)
   //          {
   //             /* Force the texture onto the board, as it could be in use */
   //             int where = ti->whichTMU;
   //             fxTMMoveOutTM_NoLock(ctx, tObj);
   //             fxTMMoveInTM_NoLock(ctx, tObj, where);
   //             break;
   //          }
   //       }
   //       if (i == MAX_TEXTURE_UNITS) /* Mark the texture as off the board */
   //          fxTMMoveOutTM_NoLock(ctx, tObj);
   //    }
   // }
}
