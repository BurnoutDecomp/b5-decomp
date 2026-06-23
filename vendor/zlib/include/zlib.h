#ifndef ZLIB_H
#define ZLIB_H

// Minimal zlib interface used by the Burnout X360 ARTIST decompressor path
// (CgsResource::Decompressor). The game links the stock zlib 1.1.3 inflate API; this
// header reconstructs only the pieces that build references -- the z_stream control block
// and the inflate-side entry points (inflateInit_/inflate/inflateEnd). Reconstructed in
// zlib's canonical vendor home; the on-target z_stream is 56 bytes / 32-bit fields, which
// the Decompressor embeds at +0x80 (see CgsResource::Decompressor::BeginDecompressingFirstEntry
// @ 0x82ACC9E0: memset 56, inflateInit_(&strm, "1.1.3", 56)).

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  Bytef;
typedef unsigned int   uInt;
typedef unsigned long  uLong;

typedef void* (*alloc_func)(void* opaque, uInt items, uInt size);
typedef void  (*free_func)(void* opaque, void* address);

struct internal_state;

// zlib z_stream. Field order is the stock zlib layout; on a 32-bit target it occupies
// 56 bytes (the size the decompressor passes to inflateInit_). next_in/avail_in are set
// per compressed entry; next_out/avail_out point at the destination buffer; zalloc/zfree/
// opaque carry the Decompressor's heap callbacks + the Decompressor `this`.
typedef struct z_stream_s {
    Bytef*               next_in;    // +0x00  next input byte
    uInt                 avail_in;   // +0x04  number of bytes available at next_in
    uLong                total_in;   // +0x08  total nb of input bytes read so far
    Bytef*               next_out;   // +0x0C  next output byte
    uInt                 avail_out;  // +0x10  remaining free space at next_out
    uLong                total_out;  // +0x14  total nb of bytes output so far
    const char*          msg;        // +0x18  last error message, NULL if no error
    struct internal_state* state;    // +0x1C  not visible by applications
    alloc_func           zalloc;     // +0x20  used to allocate the internal state
    free_func            zfree;      // +0x24  used to free the internal state
    void*                opaque;     // +0x28  private data object passed to zalloc/zfree
    int                  data_type;  // +0x2C  best guess about the data type
    uLong                adler;      // +0x30  adler32 value of the uncompressed data
    uLong                reserved;   // +0x34  reserved for future use
} z_stream;

typedef z_stream* z_streamp;

// Inflate flush values used by the decompressor (Z_SYNC_FLUSH == 2).
#define Z_NO_FLUSH      0
#define Z_SYNC_FLUSH    2
#define Z_FINISH        4

// Return codes.
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2

extern int inflateInit_(z_streamp strm, const char* version, int stream_size);
extern int inflate(z_streamp strm, int flush);
extern int inflateEnd(z_streamp strm);

#ifdef __cplusplus
}
#endif

#endif // ZLIB_H
