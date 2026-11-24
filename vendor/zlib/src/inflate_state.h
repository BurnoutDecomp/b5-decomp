/* inflate_state.h -- private state shared by the inflate decoders
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h.
 */

#ifndef INFLATE_STATE_H
#define INFLATE_STATE_H

typedef enum {
      INFLATE_METHOD,   /* waiting for method byte */
      INFLATE_FLAG,     /* waiting for flag byte */
      INFLATE_DICT4,    /* four dictionary check bytes to go */
      INFLATE_DICT3,    /* three dictionary check bytes to go */
      INFLATE_DICT2,    /* two dictionary check bytes to go */
      INFLATE_DICT1,    /* one dictionary check byte to go */
      INFLATE_DICT0,    /* waiting for inflateSetDictionary */
      INFLATE_BLOCKS,   /* decompressing blocks */
      INFLATE_CHECK4,   /* four check bytes to go */
      INFLATE_CHECK3,   /* three check bytes to go */
      INFLATE_CHECK2,   /* two check bytes to go */
      INFLATE_CHECK1,   /* one check byte to go */
      INFLATE_DONE,     /* finished check, done */
      INFLATE_BAD}      /* got an error--stay here */
inflate_mode;

/* inflate private state */
struct internal_state {

  /* mode */
  inflate_mode  mode;   /* current inflate mode */

  /* mode dependent information */
  union {
    uInt method;        /* if FLAGS, method byte */
    struct {
      uLong was;                /* computed check value */
      uLong need;               /* stream check value */
    } check;            /* if CHECK, check values to compare */
    uInt marker;        /* if INFLATE_BAD, inflateSync's marker bytes count */
  } sub;        /* submode */

  /* mode independent information */
  uInt method;          /* ARTIST +0x0c: initialized at 0x82ACCFD0, updated at 0x82ACD1D0 */
  int  nowrap;          /* flag for no wrapper */
  uInt wbits;           /* log2(window size)  (8..15, defaults to 15) */
  inflate_blocks_statef *blocks; /* current inflate_blocks state */

};

#endif
