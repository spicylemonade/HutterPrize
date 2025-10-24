#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>
#include "dlz.h"

struct ZFuncs {
    void* handle = nullptr;
    const char* (*zlibVersion_f)() = nullptr;
    int (*deflateInit2__f)(z_streamp, int, int, int, int, int, const char*, int) = nullptr;
    int (*deflate_f)(z_streamp, int) = nullptr;
    int (*deflateEnd_f)(z_streamp) = nullptr;
    int (*inflateInit__f)(z_streamp, const char*, int) = nullptr;
    int (*inflate_f)(z_streamp, int) = nullptr;
    int (*inflateEnd_f)(z_streamp) = nullptr;
    bool inited = false;
    bool ok = false;
} zfx;

static void try_load(const char* name) {
    if (zfx.handle) return;
    zfx.handle = dlopen(name, RTLD_LAZY);
}

static void ensure_loaded() {
    if (zfx.inited) return;
    zfx.inited = true;
    // Try common sonames/paths
    try_load("libz.so.1");
    if (!zfx.handle) try_load("libz.so");
    if (!zfx.handle) try_load("/lib/x86_64-linux-gnu/libz.so.1");
    if (!zfx.handle) try_load("/usr/lib/x86_64-linux-gnu/libz.so.1");

    if (!zfx.handle) { zfx.ok = false; return; }

    zfx.zlibVersion_f   = (const char* (*)()) dlsym(zfx.handle, "zlibVersion");
    zfx.deflateInit2__f = (int(*)(z_streamp,int,int,int,int,int,const char*,int)) dlsym(zfx.handle, "deflateInit2_");
    zfx.deflate_f       = (int(*)(z_streamp,int)) dlsym(zfx.handle, "deflate");
    zfx.deflateEnd_f    = (int(*)(z_streamp)) dlsym(zfx.handle, "deflateEnd");
    zfx.inflateInit__f  = (int(*)(z_streamp,const char*,int)) dlsym(zfx.handle, "inflateInit_");
    zfx.inflate_f       = (int(*)(z_streamp,int)) dlsym(zfx.handle, "inflate");
    zfx.inflateEnd_f    = (int(*)(z_streamp)) dlsym(zfx.handle, "inflateEnd");

    zfx.ok = zfx.zlibVersion_f && zfx.deflateInit2__f && zfx.deflate_f && zfx.deflateEnd_f && zfx.inflateInit__f && zfx.inflate_f && zfx.inflateEnd_f;
}

bool dlz_available() {
    ensure_loaded();
    return zfx.ok;
}

int hpz_deflateInit2(z_stream* s, int level, int method, int windowBits, int memLevel, int strategy) {
    ensure_loaded();
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    const char* ver = zfx.zlibVersion_f();
    return zfx.deflateInit2__f(s, level, method, windowBits, memLevel, strategy, ver, (int)sizeof(z_stream));
}
int hpz_deflate(z_stream* s, int flush) {
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    return zfx.deflate_f(s, flush);
}
int hpz_deflateEnd(z_stream* s) {
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    return zfx.deflateEnd_f(s);
}
int hpz_inflateInit(z_stream* s) {
    ensure_loaded();
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    const char* ver = zfx.zlibVersion_f();
    return zfx.inflateInit__f(s, ver, (int)sizeof(z_stream));
}
int hpz_inflate(z_stream* s, int flush) {
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    return zfx.inflate_f(s, flush);
}
int hpz_inflateEnd(z_stream* s) {
    if (!zfx.ok) {
        return Z_STREAM_ERROR;
    }
    return zfx.inflateEnd_f(s);
}
