#ifndef ZLITE_H
#define ZLITE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  Bytef;
typedef unsigned int   uInt;
typedef unsigned long  uLong;

typedef struct z_stream_s {
    Bytef    *next_in;   /* next input byte */
    uInt      avail_in;  /* number of bytes available at next_in */
    uLong     total_in;  /* total number of input bytes read so far */

    Bytef    *next_out;  /* next output byte should be put there */
    uInt      avail_out; /* remaining free space at next_out */
    uLong     total_out; /* total number of bytes output so far */

    const char *msg;     /* last error message, NULL if no error */
    void      *state;    /* not visible by applications */

    void      *zalloc;   /* used to allocate the internal state */
    void      *zfree;    /* used to free the internal state */
    void      *opaque;   /* private data object passed to zalloc/zfree */

    int        data_type;/* best guess about the data type: binary or text */
    uLong      adler;    /* adler32 value of the uncompressed data */
    uLong      reserved; /* reserved for future use */
} z_stream, *z_streamp;

/* Flush values */
#define Z_NO_FLUSH      0
#define Z_FINISH        4

/* Return codes */
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)

/* Methods/params */
#define Z_DEFLATED      8

/* zlib base API symbols (resolved dynamically) */
const char* zlibVersion(void);
int deflateInit2_(z_streamp strm, int level, int method, int windowBits, int memLevel, int strategy, const char* version, int stream_size);
int deflate(z_streamp strm, int flush);
int deflateEnd(z_streamp strm);
int inflateInit_(z_streamp strm, const char* version, int stream_size);
int inflate(z_streamp strm, int flush);
int inflateEnd(z_streamp strm);

#ifdef __cplusplus
}
#endif
#endif
