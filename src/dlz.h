#ifndef DLZ_H
#define DLZ_H
#include <stdbool.h>
#include "third_party/zlite.h"

bool dlz_available();
int  hpz_deflateInit2(z_stream* s, int level, int method, int windowBits, int memLevel, int strategy);
int  hpz_deflate(z_stream* s, int flush);
int  hpz_deflateEnd(z_stream* s);
int  hpz_inflateInit(z_stream* s);
int  hpz_inflate(z_stream* s, int flush);
int  hpz_inflateEnd(z_stream* s);

#endif
