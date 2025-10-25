#ifndef HPZ_TRANSFORM_H
#define HPZ_TRANSFORM_H
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "dict.h"

namespace hpz {

static inline uint32_t hpz_crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256]; static bool have = false;
    if (!have) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); table[i] = c; } have = true; }
    crc ^= 0xFFFFFFFFu; for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8); return crc ^ 0xFFFFFFFFu;
}

static inline uint32_t rd_le32(const unsigned char* p) { uint32_t v = 0; for (int i = 3; i >= 0; --i) v = (v << 8) | p[i]; return v; }

struct TransformDecoder {
    // Header parsing
    unsigned char hdr[12]; size_t hdr_pos = 0; bool header_done = false; bool transforms = false; uint8_t hdr_ver = 0; uint8_t hdr_flags = 0; uint32_t dict_cksum = 0;
    // Escape decoding state machine
    enum EscState { ESC_NONE=0, ESC_SEEN00=1, ESC_SPACE=2, ESC_NL=3, ESC_DIGIT_LEN=4, ESC_DIGIT_COPY=5 };
    EscState esc = ESC_NONE; size_t digit_left = 0;

    void reset() { hdr_pos = 0; header_done = false; transforms = false; hdr_ver = 0; hdr_flags = 0; dict_cksum = 0; esc = ESC_NONE; digit_left = 0; }

    bool feed(const unsigned char* in, size_t n, FILE* fout, uint32_t& crc, uint64_t& written) {
        size_t i = 0;
        while (i < n) {
            // Header state
            if (!header_done) {
                // Read magic first
                while (hdr_pos < 4 && i < n) hdr[hdr_pos++] = in[i++];
                if (hdr_pos >= 4 && !header_done) {
                    if (!(hdr[0]=='H' && hdr[1]=='P' && hdr[2]=='Z' && hdr[3]=='T')) {
                        // No header: passthrough accumulated hdr bytes
                        if (std::fwrite(hdr, 1, hdr_pos, fout) != hdr_pos) return false;
                        crc = hpz_crc32_update(crc, hdr, hdr_pos); written += hdr_pos;
                        header_done = true; transforms = false; // passthrough mode
                        // continue processing remaining in passthrough mode in same call
                    } else {
                        // Read base 8 bytes first: magic + ver + flags + 2 pad
                        while (hdr_pos < 8 && i < n) hdr[hdr_pos++] = in[i++];
                        if (hdr_pos < 8) return true; // need more
                        hdr_ver = hdr[4]; hdr_flags = hdr[5];
                        size_t need = (hdr_ver >= 2) ? 12 : 8;
                        while (hdr_pos < need && i < n) hdr[hdr_pos++] = in[i++];
                        if (hdr_pos < need) return true; // need more
                        header_done = true; transforms = (hdr_flags & 0x0F) != 0;
                        if (hdr_ver >= 2) {
                            dict_cksum = rd_le32(hdr + 8);
                            uint32_t actual = hpz_dict_checksum();
                            if (dict_cksum != actual) {
                                std::fprintf(stderr, "[ERROR] DICT checksum mismatch: got 0x%08x, expected 0x%08x\n", actual, dict_cksum);
                                return false;
                            }
                        }
                        continue;
                    }
                }
                if (!header_done) continue;
            }

            if (!transforms) {
                size_t to = n - i;
                if (to) {
                    if (std::fwrite(in + i, 1, to, fout) != to) return false;
                    crc = hpz_crc32_update(crc, in + i, to); written += to; i += to;
                }
                return true;
            }

            // IMPORTANT: handle digit copy state BEFORE consuming any new byte
            if (esc == ESC_DIGIT_COPY) {
                size_t available = n - i;
                size_t can = (digit_left < available) ? digit_left : available;
                if (can > 0) {
                    const unsigned char* src = in + i;
                    if (std::fwrite(src, 1, can, fout) != can) return false;
                    crc = hpz_crc32_update(crc, src, can);
                    written += can;
                    i += can;
                    digit_left -= can;
                }
                if (digit_left == 0) {
                    esc = ESC_NONE;
                }
                // Either finished digits or need more input; in both cases continue loop
                continue;
            }

            // Fetch next byte only when not in bulk digit copy
            unsigned char b = in[i++];
            if (esc == ESC_NONE) {
                if (b != 0x00) {
                    if (std::fwrite(&b, 1, 1, fout) != 1) return false;
                    crc = hpz_crc32_update(crc, &b, 1); ++written;
                } else {
                    esc = ESC_SEEN00;
                }
            } else if (esc == ESC_SEEN00) {
                if (b == 0x00) {
                    unsigned char z = 0x00;
                    if (std::fwrite(&z, 1, 1, fout) != 1) return false;
                    crc = hpz_crc32_update(crc, &z, 1); ++written; esc = ESC_NONE;
                } else if (b == 0x80) {
                    esc = ESC_SPACE;
                } else if (b == 0x81) {
                    esc = ESC_NL;
                } else if (b == 0x82) {
                    esc = ESC_DIGIT_LEN;
                } else if (b >= 1 && b <= HPZ_DICT_SIZE) {
                    const char* s = HPZ_DICT[b - 1]; size_t L = std::strlen(s);
                    if (L) {
                        if (std::fwrite(s, 1, L, fout) != L) return false;
                        crc = hpz_crc32_update(crc, (const unsigned char*)s, L); written += L;
                    }
                    esc = ESC_NONE;
                } else {
                    std::fprintf(stderr, "[ERROR] Invalid transform token: 0x%02x\n", b); return false;
                }
            } else if (esc == ESC_SPACE) {
                size_t run = (size_t)b + 4;
                std::vector<unsigned char> sp(run, (unsigned char)' ');
                if (!sp.empty()) {
                    if (std::fwrite(sp.data(), 1, sp.size(), fout) != sp.size()) return false;
                    crc = hpz_crc32_update(crc, sp.data(), sp.size()); written += sp.size();
                }
                esc = ESC_NONE;
            } else if (esc == ESC_NL) {
                size_t run = (size_t)b + 2;
                std::vector<unsigned char> nl(run, (unsigned char)'\n');
                if (!nl.empty()) {
                    if (std::fwrite(nl.data(), 1, nl.size(), fout) != nl.size()) return false;
                    crc = hpz_crc32_update(crc, nl.data(), nl.size()); written += nl.size();
                }
                esc = ESC_NONE;
            } else if (esc == ESC_DIGIT_LEN) {
                digit_left = (size_t)b + 3;
                esc = ESC_DIGIT_COPY;
            }
        }
        return true;
    }

    bool finish_ok() const { return esc == ESC_NONE; }
};

} // namespace hpz

#endif // HPZ_TRANSFORM_H
