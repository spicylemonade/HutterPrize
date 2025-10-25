#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "hpzt_dict.h"

// CRC32 (poly 0xEDB88320) for parity with main pipeline (used to cross-check if needed)
[[maybe_unused]] static uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256];
    static bool have = false;
    if (!have) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        have = true;
    }
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct DictIndex {
    std::vector<int> heads[256];
    size_t maxLen = 0;
    DictIndex() {
        for (int i = 0; i < HPZT_DICT_SIZE; ++i) {
            const unsigned char c = (unsigned char)HPZT_DICT[i][0];
            heads[c].push_back(i);
            size_t L = std::strlen(HPZT_DICT[i]);
            if (L > maxLen) {
                maxLen = L;
            }
        }
        for (int c = 0; c < 256; ++c) {
            std::sort(heads[c].begin(), heads[c].end(), [](int a, int b) {
                return std::strlen(HPZT_DICT[a]) > std::strlen(HPZT_DICT[b]);
            });
        }
        if (maxLen == 0) {
            maxLen = 1;
        }
    }
};

// Encoder: produces HPZT v2 header + transformed bytes (no compression)
struct Encoder {
    DictIndex idx;
    std::string carry;
    std::vector<unsigned char> out;
    Encoder() { out.reserve(1 << 20); }

    inline void emit_byte(unsigned char b) { out.push_back(b); }
    inline void emit_data(const unsigned char* p, size_t n) { out.insert(out.end(), p, p + n); }
    inline void emit_token(uint8_t id) { emit_byte(0x00); emit_byte(id); }

    inline void emit_spaces(size_t n) {
        while (n >= 259) {
            emit_byte(0x00);
            emit_byte(0x80);
            emit_byte((unsigned char)(255));
            n -= 259;
        }
        if (n >= 4) {
            emit_byte(0x00);
            emit_byte(0x80);
            emit_byte((unsigned char)(n - 4));
        } else {
            for (size_t i = 0; i < n; ++i) {
                emit_byte(' ');
            }
        }
    }

    inline void emit_newlines(size_t n) {
        while (n >= 257) {
            emit_byte(0x00);
            emit_byte(0x81);
            emit_byte((unsigned char)(255));
            n -= 257;
        }
        if (n >= 2) {
            emit_byte(0x00);
            emit_byte(0x81);
            emit_byte((unsigned char)(n - 2));
        } else {
            for (size_t i = 0; i < n; ++i) {
                emit_byte('\n');
            }
        }
    }

    inline void emit_digits_run(const unsigned char* s, size_t n) {
        while (n >= 3) {
            size_t chunk = n;
            if (chunk > 258) {
                chunk = 258;
            }
            emit_byte(0x00);
            emit_byte(0x82);
            emit_byte((unsigned char)(chunk - 3));
            emit_data(s, chunk);
            s += chunk;
            n -= chunk;
        }
        for (size_t i = 0; i < n; ++i) {
            emit_byte(s[i]);
        }
    }

    void process_block(const unsigned char* data, size_t n, bool final) {
        std::string block;
        block.reserve(carry.size() + n);
        block.append(carry);
        carry.clear();
        if (data && n) {
            block.append(reinterpret_cast<const char*>(data), n);
        }
        size_t reserve = final ? 0 : (idx.maxLen > 0 ? idx.maxLen - 1 : 0);
        if (reserve > block.size()) {
            reserve = 0;
        }
        size_t limit = block.size() - reserve;
        const unsigned char* s = reinterpret_cast<const unsigned char*>(block.data());
        size_t i = 0;
        while (i < limit) {
            unsigned char c = s[i];
            if (c == 0x00) {
                emit_byte(0x00);
                emit_byte(0x00);
                ++i;
                continue;
            }
            const auto& cand = idx.heads[c];
            bool matched = false;
            for (int di : cand) {
                const char* t = HPZT_DICT[di];
                size_t L = std::strlen(t);
                if (i + L <= block.size() && std::memcmp(s + i, t, L) == 0) {
                    emit_token((uint8_t)(di + 1));
                    i += L;
                    matched = true;
                    break;
                }
            }
            if (matched) {
                continue;
            }
            if (c == ' ') {
                size_t j = i;
                while (j < limit && s[j] == ' ') {
                    ++j;
                }
                size_t run = j - i;
                if (run >= 4) {
                    emit_spaces(run);
                    i = j;
                    continue;
                }
            }
            if (c == '\n') {
                size_t j = i;
                while (j < limit && s[j] == '\n') {
                    ++j;
                }
                size_t run = j - i;
                if (run >= 2) {
                    emit_newlines(run);
                    i = j;
                    continue;
                }
            }
            if (c >= '0' && c <= '9') {
                size_t j = i;
                while (j < limit && s[j] >= '0' && s[j] <= '9') {
                    ++j;
                }
                size_t run = j - i;
                if (run >= 3) {
                    emit_digits_run(s + i, run);
                    i = j;
                    continue;
                }
            }
            emit_byte(c);
            ++i;
        }
        if (!final && reserve) {
            carry.assign(block.data() + limit, reserve);
        }
        if (final && block.size() > limit) {
            const unsigned char* rem = reinterpret_cast<const unsigned char*>(block.data() + limit);
            size_t rn = block.size() - limit;
            size_t k = 0;
            while (k < rn) {
                unsigned char cc = rem[k];
                if (cc == 0x00) {
                    emit_byte(0x00);
                    emit_byte(0x00);
                    ++k;
                } else if (cc == ' ') {
                    size_t m = k;
                    while (m < rn && rem[m] == ' ') {
                        ++m;
                    }
                    size_t r = m - k;
                    if (r >= 4) {
                        emit_spaces(r);
                        k = m;
                    } else {
                        emit_byte(' ');
                        ++k;
                    }
                } else if (cc == '\n') {
                    size_t m = k;
                    while (m < rn && rem[m] == '\n') {
                        ++m;
                    }
                    size_t r = m - k;
                    if (r >= 2) {
                        emit_newlines(r);
                        k = m;
                    } else {
                        emit_byte('\n');
                        ++k;
                    }
                } else if (cc >= '0' && cc <= '9') {
                    size_t m = k;
                    while (m < rn && rem[m] >= '0' && rem[m] <= '9') {
                        ++m;
                    }
                    size_t r = m - k;
                    if (r >= 3) {
                        emit_digits_run(rem + k, r);
                        k = m;
                    } else {
                        emit_byte(rem[k]);
                        ++k;
                    }
                } else {
                    emit_byte(cc);
                    ++k;
                }
            }
        }
    }

    void write_header_v2() {
        unsigned char hdr[12];
        hdr[0] = 'H';
        hdr[1] = 'P';
        hdr[2] = 'Z';
        hdr[3] = 'T';
        hdr[4] = 2;
        hdr[5] = 0x0F;
        hdr[6] = 0;
        hdr[7] = 0;
        uint32_t dcrc = hpzt_dict_checksum();
        hdr[8] = (unsigned char)(dcrc & 0xFF);
        hdr[9] = (unsigned char)((dcrc >> 8) & 0xFF);
        hdr[10] = (unsigned char)((dcrc >> 16) & 0xFF);
        hdr[11] = (unsigned char)((dcrc >> 24) & 0xFF);
        emit_data(hdr, sizeof(hdr));
    }
};

static inline uint64_t read_le64(const unsigned char* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | p[i];
    }
    return v;
}

static inline uint32_t read_le32(const unsigned char* p) {
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i) {
        v = (v << 8) | p[i];
    }
    return v;
}

struct TransformDecoder {
    static constexpr size_t MIN_HDR = 8;
    static constexpr size_t HDR_V2 = 12;
    unsigned char hdr[12];
    size_t hdr_pos = 0;
    size_t hdr_need = MIN_HDR;
    bool header_done = false;
    bool transforms = false;
    bool have_dict_crc = false;
    uint32_t dict_crc_in = 0;
    enum EscState { ESC_NONE = 0, ESC_SEEN00 = 1, ESC_SPACE = 2, ESC_NL = 3, ESC_DIGIT_LEN = 4, ESC_DIGIT_COPY = 5 };
    EscState esc = ESC_NONE;
    size_t digit_left = 0;

    void reset() {
        hdr_pos = 0;
        hdr_need = MIN_HDR;
        header_done = false;
        transforms = false;
        have_dict_crc = false;
        dict_crc_in = 0;
        esc = ESC_NONE;
        digit_left = 0;
    }

    bool feed(const unsigned char* in, size_t n, std::vector<unsigned char>& out) {
        size_t i = 0;
        while (i < n) {
            if (!header_done) {
                while (hdr_pos < hdr_need && i < n) {
                    hdr[hdr_pos++] = in[i++];
                }
                if (hdr_pos < hdr_need) {
                    return true;
                }
                if (!(hdr[0] == 'H' && hdr[1] == 'P' && hdr[2] == 'Z' && hdr[3] == 'T')) {
                    out.insert(out.end(), hdr, hdr + hdr_pos);
                    header_done = true;
                    transforms = false;
                    continue;
                }
                unsigned ver = hdr[4];
                unsigned flags = hdr[5];
                if (hdr_need == MIN_HDR && ver >= 2) {
                    hdr_need = HDR_V2;
                    while (hdr_pos < hdr_need && i < n) {
                        hdr[hdr_pos++] = in[i++];
                    }
                    if (hdr_pos < hdr_need) {
                        return true;
                    }
                }
                header_done = true;
                transforms = (flags & 0x0F) != 0;
                have_dict_crc = (hdr_need == HDR_V2);
                if (have_dict_crc) {
                    dict_crc_in = read_le32(hdr + 8);
                    if (dict_crc_in != hpzt_dict_checksum()) {
                        std::fprintf(stderr, "[TEST] dict checksum mismatch: 0x%08x vs 0x%08x\n", dict_crc_in, hpzt_dict_checksum());
                        return false;
                    }
                }
                continue;
            }
            if (!transforms) {
                out.insert(out.end(), in + i, in + n);
                return true;
            }
            unsigned char b = in[i++];
            if (esc == ESC_NONE) {
                if (b != 0x00) {
                    out.push_back(b);
                } else {
                    esc = ESC_SEEN00;
                }
            } else if (esc == ESC_SEEN00) {
                if (b == 0x00) {
                    out.push_back(0x00);
                    esc = ESC_NONE;
                } else if (b == 0x80) {
                    esc = ESC_SPACE;
                } else if (b == 0x81) {
                    esc = ESC_NL;
                } else if (b == 0x82) {
                    esc = ESC_DIGIT_LEN;
                } else if (b >= 1 && b <= HPZT_DICT_SIZE) {
                    const char* s = HPZT_DICT[b - 1];
                    size_t L = std::strlen(s);
                    out.insert(out.end(), s, s + L);
                    esc = ESC_NONE;
                } else {
                    std::fprintf(stderr, "[TEST] Invalid token 0x%02x\n", b);
                    return false;
                }
            } else if (esc == ESC_SPACE) {
                size_t run = (size_t)b + 4;
                out.insert(out.end(), run, (unsigned char)' ');
                esc = ESC_NONE;
            } else if (esc == ESC_NL) {
                size_t run = (size_t)b + 2;
                out.insert(out.end(), run, (unsigned char)'\n');
                esc = ESC_NONE;
            } else if (esc == ESC_DIGIT_LEN) {
                digit_left = (size_t)b + 3;
                esc = ESC_DIGIT_COPY;
            } else if (esc == ESC_DIGIT_COPY) {
                size_t can = std::min(digit_left, n - (i - 1));
                i--;
                const unsigned char* src = in + i;
                out.insert(out.end(), src, src + can);
                i += can;
                digit_left -= can;
                if (digit_left == 0) {
                    esc = ESC_NONE;
                }
            }
        }
        return true;
    }

    bool finish_ok() const { return esc == ESC_NONE; }
};

static bool roundtrip_case(const std::vector<unsigned char>& input, size_t feed_chunk) {
    Encoder enc;
    enc.write_header_v2();
    size_t i = 0;
    size_t n = input.size();
    bool flip = false;
    while (i < n) {
        size_t ch = std::min(feed_chunk, n - i);
        enc.process_block(input.data() + i, ch, false);
        i += ch;
        if (flip && i < n) {
            size_t ch2 = std::min((size_t)3, n - i);
            enc.process_block(input.data() + i, ch2, false);
            i += ch2;
        }
        flip = !flip;
    }
    enc.process_block(nullptr, 0, true);

    TransformDecoder dec;
    dec.reset();
    std::vector<unsigned char> out;
    size_t pos = 0;
    size_t total = enc.out.size();
    while (pos < total) {
        size_t ch = std::min((size_t)7, total - pos);
        if (!dec.feed(enc.out.data() + pos, ch, out)) {
            return false;
        }
        pos += ch;
    }
    if (!dec.finish_ok()) {
        return false;
    }
    if (out != input) {
        auto diff = std::mismatch(input.begin(), input.end(), out.begin(), out.end());
        long idx = diff.first == input.end() ? (long)input.size() : (long)(diff.first - input.begin());
        std::fprintf(stderr, "[TEST] roundtrip mismatch: in=%zu out=%zu first_diff=%ld\n", input.size(), out.size(), idx);
        return false;
    }
    return true;
}

static bool checksum_negative_test() {
    std::vector<unsigned char> in{'[', '[', 'C', 'a', 't', 'e', 'g', 'o', 'r', 'y', ':', 'F', 'o', 'o', ']', ']', '\n'};
    Encoder enc;
    enc.write_header_v2();
    enc.process_block(in.data(), in.size(), true);
    if (enc.out.size() < 12) {
        return false;
    }
    enc.out[8] ^= 0xFF;
    TransformDecoder dec;
    dec.reset();
    std::vector<unsigned char> out;
    return dec.feed(enc.out.data(), enc.out.size(), out) == false;
}

int main() {
    bool ok = true;
    ok &= roundtrip_case(std::vector<unsigned char>{'a', 'b', 'c', 0x00, 'd', 'e', 'f'}, 5);
    ok &= roundtrip_case(std::vector<unsigned char>(10, ' '), 4);
    ok &= roundtrip_case(std::vector<unsigned char>({'\n', '\n', '\n', 'x', '\n', '\n'}), 2);
    {
        std::vector<unsigned char> v;
        v.insert(v.end(), {'1', '2', '3', '4', '5'});
        v.insert(v.end(), 300, (unsigned char)('0' + (rand() % 10)));
        ok &= roundtrip_case(v, 16);
    }
    {
        std::string s = std::string("[[Category:") + "Foo]]\n" + "{{Infobox" + " bar }}";
        std::vector<unsigned char> v(s.begin(), s.end());
        ok &= roundtrip_case(v, 8);
    }
    {
        std::string s = std::string("abc[[Category:") + std::string(20, ' ') + "]]def";
        std::vector<unsigned char> v(s.begin(), s.end());
        ok &= roundtrip_case(v, 5);
    }
    ok &= checksum_negative_test();

    if (!ok) {
        std::fprintf(stderr, "[TEST] hpzt_selftest FAILED\n");
        return 1;
    }
    std::fprintf(stderr, "[TEST] hpzt_selftest PASSED\n");
    return 0;
}
