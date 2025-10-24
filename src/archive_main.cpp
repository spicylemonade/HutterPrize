#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "dlz.h"

#if defined(__linux__)
#include <limits.h>
#endif

static constexpr size_t IN_CHUNK  = 1 << 20; // 1 MiB
static constexpr size_t OUT_CHUNK = 1 << 20; // 1 MiB

enum Method : uint8_t { METHOD_STORE = 0, METHOD_ZLIB = 1 };

static inline uint64_t read_le64(const unsigned char* p) { uint64_t v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | p[i]; return v; }
static inline uint32_t read_le32(const unsigned char* p) { uint32_t v = 0; for (int i = 3; i >= 0; --i) v = (v << 8) | p[i]; return v; }

static off_t file_size_of(const char* path) { struct stat st{}; if (stat(path, &st) != 0) return -1; return st.st_size; }
static std::string self_path() {
#if defined(__linux__)
    char buf[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1); if (n < 0) return std::string(); buf[n] = '\0'; return std::string(buf);
#else
    return std::string();
#endif
}

// CRC32
static uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256]; static bool have = false;
    if (!have) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); table[i] = c; } have = true; }
    crc ^= 0xFFFFFFFFu; for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8); return crc ^ 0xFFFFFFFFu;
}

// Same dictionary as compressor
static const char* const DICT[] = {
    "<page>", "</page>", "<title>", "</title>", "<id>", "</id>",
    "<revision>", "</revision>", "<timestamp>", "</timestamp>",
    "<contributor>", "</contributor>", "<username>", "</username>",
    "<minor/>", "<minor />", "<comment>", "</comment>",
    "<model>wikitext</model>", "<format>text/x-wiki</format>",
    "<ns>", "</ns>", "<siteinfo>", "</siteinfo>",
    "<sitename>", "</sitename>", "<base>", "</base>",
    "<generator>", "</generator>", "<case>", "</case>",
    "<namespaces>", "</namespaces>", "<namespace key=\"", "</namespace>",
    "<mediawiki", "</mediawiki>",
    "<text xml:space=\"preserve\">", "</text>", "<text ",
    "[[", "]]", "{{", "}}", "[[Category:", "[[File:", "[[Image:",
    "<ref>", "</ref>", "<ref", "<!--", "-->",
    "==", "===", "====", "{{cite", "{{citation", "|author", "|title",
    "|url", "|publisher", "|date", "|accessdate", "|work", "|pages",
    "|isbn", "|doi", "|issue", "|volume", "|journal", "|language",
    "|archiveurl", "|archivedate", "|quote", "|trans-title", "|location",
    "|ref", "|last", "|first",
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "[[Category:", "{{Infobox", "{{infobox", "<redirect", "#REDIRECT"
};
static constexpr int DICT_SIZE = (int)(sizeof(DICT)/sizeof(DICT[0]));

struct TransformDecoder {
    // Header parsing
    unsigned char hdr[8]; size_t hdr_pos = 0; bool header_done = false; bool transforms = false;
    // Escape decoding state machine
    enum EscState { ESC_NONE=0, ESC_SEEN00=1, ESC_SPACE=2, ESC_NL=3, ESC_DIGIT_LEN=4, ESC_DIGIT_COPY=5 };
    EscState esc = ESC_NONE; size_t digit_left = 0;

    void reset() { hdr_pos = 0; header_done = false; transforms = false; esc = ESC_NONE; digit_left = 0; }

    bool feed(const unsigned char* in, size_t n, FILE* fout, uint32_t& crc, uint64_t& written) {
        size_t i = 0;
        while (i < n) {
            // Header state
            if (!header_done) {
                while (hdr_pos < 4 && i < n) hdr[hdr_pos++] = in[i++];
                if (hdr_pos >= 4 && !header_done) {
                    if (!(hdr[0]=='H' && hdr[1]=='P' && hdr[2]=='Z' && hdr[3]=='T')) {
                        // No header: passthrough accumulated hdr bytes
                        if (std::fwrite(hdr, 1, hdr_pos, fout) != hdr_pos) return false;
                        crc = crc32_update(crc, hdr, hdr_pos); written += hdr_pos;
                        header_done = true; transforms = false; // passthrough mode
                        // continue processing remaining in passthrough mode in same call
                    } else {
                        // HPZT present: need 4 more bytes (ver, flags, pad)
                        while (hdr_pos < 8 && i < n) hdr[hdr_pos++] = in[i++];
                        if (hdr_pos < 8) return true;
                        header_done = true; transforms = (hdr[5] & 0x0F) != 0;
                        continue;
                    }
                }
                if (!header_done) continue;
            }
            if (!transforms) {
                size_t to = n - i; if (to) { if (std::fwrite(in + i, 1, to, fout) != to) return false; crc = crc32_update(crc, in + i, to); written += to; i += to; }
                return true;
            }
            // Transform decoding
            unsigned char b = in[i++];
            if (esc == ESC_NONE) {
                if (b != 0x00) { if (std::fwrite(&b, 1, 1, fout) != 1) return false; crc = crc32_update(crc, &b, 1); ++written; }
                else esc = ESC_SEEN00;
            } else if (esc == ESC_SEEN00) {
                if (b == 0x00) {
                    unsigned char z = 0x00; if (std::fwrite(&z, 1, 1, fout) != 1) return false; crc = crc32_update(crc, &z, 1); ++written; esc = ESC_NONE;
                } else if (b == 0x80) {
                    esc = ESC_SPACE;
                } else if (b == 0x81) {
                    esc = ESC_NL;
                } else if (b == 0x82) {
                    esc = ESC_DIGIT_LEN;
                } else if (b >= 1 && b <= DICT_SIZE) {
                    const char* s = DICT[b - 1]; size_t L = std::strlen(s);
                    if (L) { if (std::fwrite(s, 1, L, fout) != L) return false; crc = crc32_update(crc, (const unsigned char*)s, L); written += L; }
                    esc = ESC_NONE;
                } else {
                    std::fprintf(stderr, "[ERROR] Invalid transform token: 0x%02x\n", b); return false;
                }
            } else if (esc == ESC_SPACE) {
                size_t run = (size_t)b + 4; std::vector<unsigned char> sp(run, (unsigned char)' ');
                if (!sp.empty()) { if (std::fwrite(sp.data(), 1, sp.size(), fout) != sp.size()) return false; crc = crc32_update(crc, sp.data(), sp.size()); written += sp.size(); }
                esc = ESC_NONE;
            } else if (esc == ESC_NL) {
                size_t run = (size_t)b + 2; std::vector<unsigned char> nl(run, (unsigned char)'\n');
                if (!nl.empty()) { if (std::fwrite(nl.data(), 1, nl.size(), fout) != nl.size()) return false; crc = crc32_update(crc, nl.data(), nl.size()); written += nl.size(); }
                esc = ESC_NONE;
            } else if (esc == ESC_DIGIT_LEN) {
                digit_left = (size_t)b + 3; esc = ESC_DIGIT_COPY;
            } else if (esc == ESC_DIGIT_COPY) {
                size_t can = std::min(digit_left, n - (i - 1)); // include current b already fetched
                // We already consumed one byte into b; put it back logically
                i--; // step back to include b in bulk copy
                const unsigned char* src = in + i;
                if (std::fwrite(src, 1, can, fout) != can) return false; crc = crc32_update(crc, src, can); written += can; i += can; digit_left -= can;
                if (digit_left == 0) esc = ESC_NONE;
            }
        }
        return true;
    }

    bool finish_ok() const { return esc == ESC_NONE; }
};

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::string exe = self_path(); if (exe.empty()) { if (argc > 0 && argv && argv[0]) exe = argv[0]; }
    if (exe.empty()) { std::fprintf(stderr, "[ERROR] Cannot determine self path.\n"); return 2; }

    FILE* f = std::fopen(exe.c_str(), "rb"); if (!f) { std::fprintf(stderr, "[ERROR] Cannot open self: %s (%s)\n", exe.c_str(), std::strerror(errno)); return 1; }
    off_t fsz = file_size_of(exe.c_str()); if (fsz < (off_t)(24)) { std::fprintf(stderr, "[ERROR] File too small to contain payload.\n"); std::fclose(f); return 1; }

    // Try HPZ2 (28 bytes), else HPZ1 (24 bytes)
    unsigned char footer28[28]; bool hpz2 = false; Method method = METHOD_ZLIB; uint64_t orig_size=0, comp_size=0; uint32_t expected_crc=0; off_t payload_off=0;

    if (fsz >= (off_t)28) {
        if (fseeko(f, fsz - (off_t)28, SEEK_SET) == 0 && std::fread(footer28, 1, 28, f) == 28) {
            if (footer28[0]=='H' && footer28[1]=='P' && footer28[2]=='Z' && footer28[3]=='2') {
                hpz2 = true; method = (Method)footer28[4]; orig_size = read_le64(footer28 + 8); comp_size = read_le64(footer28 + 16); expected_crc = read_le32(footer28 + 24); payload_off = fsz - (off_t)28 - (off_t)comp_size;
            }
        }
    }
    if (!hpz2) {
        unsigned char footer24[24];
        if (fseeko(f, fsz - (off_t)24, SEEK_SET) != 0 || std::fread(footer24, 1, 24, f) != 24 || !(footer24[0]=='H' && footer24[1]=='P' && footer24[2]=='Z' && footer24[3]=='1')) {
            std::fprintf(stderr, "[ERROR] Footer not found or invalid.\n"); std::fclose(f); return 1;
        }
        method = METHOD_ZLIB; // HPZ1 implies zlib
        orig_size = read_le64(footer24 + 4); comp_size = read_le64(footer24 + 12); expected_crc = read_le32(footer24 + 20); payload_off = fsz - (off_t)24 - (off_t)comp_size;
    }

    if (payload_off <= 0) { std::fprintf(stderr, "[ERROR] Invalid payload offset.\n"); std::fclose(f); return 1; }
    if (fseeko(f, payload_off, SEEK_SET) != 0) { std::fprintf(stderr, "[ERROR] fseeko payload failed (%s)\n", std::strerror(errno)); std::fclose(f); return 1; }

    const char* out_name = "enwik9.out"; FILE* fout = std::fopen(out_name, "wb"); if (!fout) { std::fprintf(stderr, "[ERROR] Cannot open output %s: %s\n", out_name, std::strerror(errno)); std::fclose(f); return 1; }

    std::vector<unsigned char> inbuf(IN_CHUNK); std::vector<unsigned char> outbuf(OUT_CHUNK);
    uint64_t written = 0; uint32_t crc = 0u; uint64_t remaining = comp_size;
    TransformDecoder dec; dec.reset();

    if (method == METHOD_STORE) {
        while (remaining > 0) {
            size_t to_read = remaining > IN_CHUNK ? IN_CHUNK : (size_t)remaining;
            size_t r = std::fread(inbuf.data(), 1, to_read, f);
            if (r == 0) { if (!std::feof(f)) { std::fprintf(stderr, "[ERROR] Reading STORE payload failed (%s)\n", std::strerror(errno)); std::fclose(f); std::fclose(fout); return 1; } break; }
            remaining -= r;
            if (!dec.feed(inbuf.data(), r, fout, crc, written)) { std::fprintf(stderr, "[ERROR] Transform decode failed (STORE).\n"); std::fclose(f); std::fclose(fout); return 1; }
        }
    } else if (method == METHOD_ZLIB) {
        if (!dlz_available()) { std::fprintf(stderr, "[ERROR] zlib not available for ZLIB payload.\n"); std::fclose(f); std::fclose(fout); return 1; }
        z_stream strm{}; if (hpz_inflateInit(&strm) != Z_OK) { std::fprintf(stderr, "[ERROR] inflateInit failed\n"); std::fclose(f); std::fclose(fout); return 1; }
        int zret = Z_OK;
        while (zret == Z_OK && remaining > 0) {
            size_t to_read = remaining > IN_CHUNK ? IN_CHUNK : (size_t)remaining;
            size_t r = std::fread(inbuf.data(), 1, to_read, f);
            if (r == 0) { if (!std::feof(f)) { std::fprintf(stderr, "[ERROR] Reading compressed payload failed (%s)\n", std::strerror(errno)); hpz_inflateEnd(&strm); std::fclose(f); std::fclose(fout); return 1; } break; }
            remaining -= r;
            strm.next_in = inbuf.data(); strm.avail_in = (uInt)r;
            while (strm.avail_in > 0) {
                strm.next_out = outbuf.data(); strm.avail_out = (uInt)outbuf.size();
                zret = hpz_inflate(&strm, 0);
                if (zret != Z_OK && zret != Z_STREAM_END) { std::fprintf(stderr, "[ERROR] inflate failed: %d\n", zret); hpz_inflateEnd(&strm); std::fclose(f); std::fclose(fout); return 1; }
                size_t have = outbuf.size() - strm.avail_out;
                if (have) {
                    if (!dec.feed(outbuf.data(), have, fout, crc, written)) { std::fprintf(stderr, "[ERROR] Transform decode failed (ZLIB).\n"); hpz_inflateEnd(&strm); std::fclose(f); std::fclose(fout); return 1; }
                }
                if (zret == Z_STREAM_END) break;
            }
            if (zret == Z_STREAM_END) break;
        }
        if (zret != Z_STREAM_END) {
            for (;;) {
                strm.next_out = outbuf.data(); strm.avail_out = (uInt)outbuf.size();
                zret = hpz_inflate(&strm, Z_FINISH);
                if (zret != Z_OK && zret != Z_STREAM_END) break;
                size_t have = outbuf.size() - strm.avail_out;
                if (have) {
                    if (!dec.feed(outbuf.data(), have, fout, crc, written)) { std::fprintf(stderr, "[ERROR] Transform decode failed (finish).\n"); hpz_inflateEnd(&strm); std::fclose(f); std::fclose(fout); return 1; }
                }
                if (zret == Z_STREAM_END) break;
            }
        }
        hpz_inflateEnd(&strm);
    } else { std::fprintf(stderr, "[ERROR] Unknown method %u\n", (unsigned)method); std::fclose(f); std::fclose(fout); return 1; }

    if (!dec.finish_ok()) { std::fprintf(stderr, "[ERROR] Incomplete transform escape sequence at end of stream.\n"); std::fclose(f); std::fclose(fout); return 1; }

    if (std::fclose(fout) != 0) { std::fprintf(stderr, "[ERROR] Closing output failed (%s)\n", std::strerror(errno)); std::fclose(f); return 1; }
    std::fclose(f);

    if (written != orig_size) { std::fprintf(stderr, "[ERROR] Output size mismatch: wrote %llu, expected %llu\n", (unsigned long long)written, (unsigned long long)orig_size); return 1; }
    if (crc != expected_crc) { std::fprintf(stderr, "[ERROR] CRC mismatch: got 0x%08x, expected 0x%08x\n", crc, expected_crc); return 1; }

    std::fprintf(stderr, "[OK] Wrote enwik9.out (%llu bytes)\n", (unsigned long long)written);
    return 0;
}
