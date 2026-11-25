/* inflate.c -- zlib interface to inflate modules
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 * Modified for the Xbox 360 ARTIST inflate state and initializer.
 */

#include "zutil.h"
#include "infblock.h"

struct inflate_blocks_state {int dummy;}; /* for buggy compilers */

#include "inflate_state.h"



int ZEXPORT inflateReset(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  z->total_in = z->total_out = 0;
  z->msg = Z_NULL;
  z->state->mode = z->state->nowrap ? INFLATE_BLOCKS : INFLATE_METHOD;
  inflate_blocks_reset(z->state->blocks, z, Z_NULL);
  Tracev((stderr, "inflate: reset\n"));
  return Z_OK;
}


int ZEXPORT inflateEnd(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->zfree == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->blocks != Z_NULL)
    inflate_blocks_free(z->state->blocks, z);
  ZFREE(z, z->state);
  z->state = Z_NULL;
  Tracev((stderr, "inflate: end\n"));
  return Z_OK;
}


/* ARTIST's extended initializer takes the compression method before w. */
int ZEXPORT inflateInit3_(z, method, w, version, stream_size)
z_streamp z;
int method;
int w;
const char *version;
int stream_size;
{
  if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
      stream_size != sizeof(z_stream))
      return Z_VERSION_ERROR;

  /* initialize state */
  if (method != Z_DEFLATED || z == Z_NULL)
    return Z_STREAM_ERROR;
  z->msg = Z_NULL;
  if (z->zalloc == Z_NULL)
  {
    z->zalloc = zcalloc;
    z->opaque = (voidpf)0;
  }
  if (z->zfree == Z_NULL) z->zfree = zcfree;
  if ((z->state = (struct internal_state FAR *)
       ZALLOC(z,1,sizeof(struct internal_state))) == Z_NULL)
    return Z_MEM_ERROR;
  z->state->blocks = Z_NULL;

  /* handle undocumented nowrap option (no zlib header or check) */
  z->state->nowrap = 0;
  if (w < 0)
  {
    w = - w;
    z->state->nowrap = 1;
  }

  /* set window size */
  if (w < 8 || w > 15)
  {
    inflateEnd(z);
    return Z_STREAM_ERROR;
  }
  z->state->method = method;
  z->state->wbits = (uInt)w;

  /* create inflate_blocks state */
  if ((z->state->blocks =
      inflate_blocks_new(z, z->state->nowrap ? Z_NULL : adler32, (uInt)1 << w))
      == Z_NULL)
  {
    inflateEnd(z);
    return Z_MEM_ERROR;
  }
  Tracev((stderr, "inflate: allocated\n"));

  /* reset state */
  inflateReset(z);
  return Z_OK;
}


int ZEXPORT inflateInit2_(z, w, version, stream_size)
z_streamp z;
int w;
const char *version;
int stream_size;
{
  return inflateInit3_(z, Z_DEFLATED, w, version, stream_size);
}


int ZEXPORT inflateInit_(z, version, stream_size)
z_streamp z;
const char *version;
int stream_size;
{
  return inflateInit3_(z, Z_DEFLATED, DEF_WBITS, version, stream_size);
}


#define NEEDBYTE {if(z->avail_in==0)return r;r=f;}
#define NEXTBYTE (z->avail_in--,z->total_in++,*z->next_in++)

int ZEXPORT inflate(z, f)
z_streamp z;
int f;
{
  int r;
  uInt b;

  if (z == Z_NULL || z->state == Z_NULL || z->next_in == Z_NULL)
    return Z_STREAM_ERROR;
  f = f == Z_FINISH ? Z_BUF_ERROR : Z_OK;
  r = Z_BUF_ERROR;
  while (1) switch (z->state->mode)
  {
    case INFLATE_METHOD:
      NEEDBYTE
      /* ARTIST stores the method byte before advancing next_in (0x82ACD1A8). */
      z->avail_in--;
      z->total_in++;
      b = *z->next_in;
      z->state->sub.method = b;
      ++z->next_in;
      if ((b & 0xf) != Z_DEFLATED)
      {
        z->state->mode = INFLATE_BAD;
        z->msg = (char*)"unknown compression method";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->method = b;
      if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
      {
        z->state->mode = INFLATE_BAD;
        z->msg = (char*)"invalid window size";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->mode = INFLATE_FLAG;
    case INFLATE_FLAG:
      NEEDBYTE
      b = NEXTBYTE;
      if (((z->state->sub.method << 8) + b) % 31)
      {
        z->state->mode = INFLATE_BAD;
        z->msg = (char*)"incorrect header check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      Tracev((stderr, "inflate: zlib header ok\n"));
      if (!(b & PRESET_DICT))
      {
        z->state->mode = INFLATE_BLOCKS;
        break;
      }
      z->state->mode = INFLATE_DICT4;
    case INFLATE_DICT4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = INFLATE_DICT3;
    case INFLATE_DICT3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = INFLATE_DICT2;
    case INFLATE_DICT2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = INFLATE_DICT1;
    case INFLATE_DICT1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;
      z->adler = z->state->sub.check.need;
      z->state->mode = INFLATE_DICT0;
      return Z_NEED_DICT;
    case INFLATE_DICT0:
      z->state->mode = INFLATE_BAD;
      z->msg = (char*)"need dictionary";
      z->state->sub.marker = 0;       /* can try inflateSync */
      return Z_STREAM_ERROR;
    case INFLATE_BLOCKS:
      r = inflate_blocks(z->state->blocks, z, r);
      if (r == Z_DATA_ERROR)
      {
        z->state->mode = INFLATE_BAD;
        z->state->sub.marker = 0;       /* can try inflateSync */
        break;
      }
      if (r == Z_OK)
        r = f;
      if (r != Z_STREAM_END)
        return r;
      r = f;
      inflate_blocks_reset(z->state->blocks, z, &z->state->sub.check.was);
      if (z->state->nowrap)
      {
        z->state->mode = INFLATE_DONE;
        break;
      }
      z->state->mode = INFLATE_CHECK4;
    case INFLATE_CHECK4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = INFLATE_CHECK3;
    case INFLATE_CHECK3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = INFLATE_CHECK2;
    case INFLATE_CHECK2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = INFLATE_CHECK1;
    case INFLATE_CHECK1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;

      if (z->state->sub.check.was != z->state->sub.check.need)
      {
        z->state->mode = INFLATE_BAD;
        z->msg = (char*)"incorrect data check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      Tracev((stderr, "inflate: zlib check ok\n"));
      z->state->mode = INFLATE_DONE;
    case INFLATE_DONE:
      return Z_STREAM_END;
    case INFLATE_BAD:
      return Z_DATA_ERROR;
    default:
      return Z_STREAM_ERROR;
  }
#ifdef NEED_DUMMY_RETURN
  return Z_STREAM_ERROR;  /* Some dumb compilers complain without this */
#endif
}


int ZEXPORT inflateSetDictionary(z, dictionary, dictLength)
z_streamp z;
const Bytef *dictionary;
uInt  dictLength;
{
  uInt length = dictLength;

  if (z == Z_NULL || z->state == Z_NULL || z->state->mode != INFLATE_DICT0)
    return Z_STREAM_ERROR;

  if (adler32(1L, dictionary, dictLength) != z->adler) return Z_DATA_ERROR;
  z->adler = 1L;

  if (length >= ((uInt)1<<z->state->wbits))
  {
    length = (1<<z->state->wbits)-1;
    dictionary += dictLength - length;
  }
  inflate_set_dictionary(z->state->blocks, dictionary, length);
  z->state->mode = INFLATE_BLOCKS;
  return Z_OK;
}


int ZEXPORT inflateSync(z)
z_streamp z;
{
  uInt n;       /* number of bytes to look at */
  Bytef *p;     /* pointer to bytes */
  uInt m;       /* number of marker bytes found in a row */
  uLong r, w;   /* temporaries to save total_in and total_out */

  /* set up */
  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->mode != INFLATE_BAD)
  {
    z->state->mode = INFLATE_BAD;
    z->state->sub.marker = 0;
  }
  if ((n = z->avail_in) == 0)
    return Z_BUF_ERROR;
  p = z->next_in;
  m = z->state->sub.marker;

  /* search */
  while (n && m < 4)
  {
    static const Byte mark[4] = {0, 0, 0xff, 0xff};
    if (*p == mark[m])
      m++;
    else if (*p)
      m = 0;
    else
      m = 4 - m;
    p++, n--;
  }

  /* restore */
  z->total_in += p - z->next_in;
  z->next_in = p;
  z->avail_in = n;
  z->state->sub.marker = m;

  /* return no joy or set up to restart on a new block */
  if (m != 4)
    return Z_DATA_ERROR;
  r = z->total_in;  w = z->total_out;
  inflateReset(z);
  z->total_in = r;  z->total_out = w;
  z->state->mode = INFLATE_BLOCKS;
  return Z_OK;
}


/* Returns true if inflate is currently at the end of a block generated
 * by Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
 * implementation to provide an additional safety check. PPP uses Z_SYNC_FLUSH
 * but removes the length bytes of the resulting empty stored block. When
 * decompressing, PPP checks that at the end of input packet, inflate is
 * waiting for these length bytes.
 */
int ZEXPORT inflateSyncPoint(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->state->blocks == Z_NULL)
    return Z_STREAM_ERROR;
  return inflate_blocks_sync_point(z->state->blocks);
}
