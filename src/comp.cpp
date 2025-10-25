#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include "dlz.h"
#include "dict.h"

static constexpr size_t IN_CHUNK  = 1 << 20; // 1 MiB
static constexpr size_t OUT_CHUNK = 1 << 20; // 1 MiB
static constexpr size_t TBUF_FLUSH = 1 << 16; // 64 KiB

enum Method : uint8_t { METHOD_STORE = 0, METHOD_ZLIB = 1 };

// HPZT transform bits
static constexpr uint8_t T_DICT   = 1u << 0; // 0x01
static constexpr uint8_t T_SPACE  = 1u << 1; // 0x02
static constexpr uint8_t T_NL     = 1u << 2; // 0x04
static constexpr uint8_t T_DIGITS = 1u << 3; // 0x08
static constexpr uint8_t T_DASH   = 1u << 4; // 0x10
static constexpr uint8_t T_EQUAL  = 1u << 5; // 0x20
static constexpr uint8_t T_ALL    = T_DICT | T_SPACE | T_NL | T_DIGITS | T_DASH | T_EQUAL; // 0x3F

static inline void write_le64(FILE* f, uint64_t v) {
    unsigned char b[8]; for (int i = 0; i < 8; ++i) b[i] = (unsigned char)((v >> (8*i)) & 0xFF);
    if (fwrite(b, 1, 8, f) != 8) { perror("fwrite"); exit(1); }
}
static inline void write_le32(FILE* f, uint32_t v) {
    unsigned char b[4]; for (int i = 0; i < 4; ++i) b[i] = (unsigned char)((v >> (8*i)) & 0xFF);
    if (fwrite(b, 1, 4, f) != 4) { perror("fwrite"); exit(1); }
}
static std::string dirname_of(const std::string& path) {
    auto pos = path.find_last_of('/'); if (pos == std::string::npos) return std::string("."); if (pos == 0) return std::string("/"); return path.substr(0, pos);
}
static std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a == "/") return a + b;
    if (!a.empty() && a.back() == '/') return a + b;
    return a + "/" + b;
}

// CRC32 (poly 0xEDB88320)
static uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256]; static bool have = false;
    if (!have) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); table[i] = c; } have = true; }
    crc ^= 0xFFFFFFFFu; for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8); return crc ^ 0xFFFFFFFFu;
}

struct DictIndex {
    std::vector<int> heads[256];
    size_t maxLen = 0;
    DictIndex() {
        for (int i = 0; i < HPZT_DICT_SIZE; ++i) {
            const unsigned char c = (unsigned char)HPZT_DICT[i][0];
            heads[c].push_back(i);
            size_t L = std::strlen(HPZT_DICT[i]); if (L > maxLen) maxLen = L;
        }
        for (int c = 0; c < 256; ++c) {
            std::sort(heads[c].begin(), heads[c].end(), [](int a, int b){ return std::strlen(HPZT_DICT[a]) > std::strlen(HPZT_DICT[b]); });
        }
        if (maxLen == 0) maxLen = 1;
    }
};

struct Sink {
    FILE* fout{};
    Method method{METHOD_STORE};
    z_stream strm{};
    std::vector<unsigned char> z_out;
    uint64_t* total_out{};
    bool z_inited{false};
};

static bool sink_init(Sink& s, Method m, FILE* fout, uint64_t* total_out) {
    s.fout = fout; s.method = m; s.total_out = total_out; s.z_inited = false;
    if (m == METHOD_ZLIB) {
        s.z_out.resize(OUT_CHUNK);
        if (hpz_deflateInit2(&s.strm, 9, Z_DEFLATED, 15, 9, 0) != Z_OK) {
            return false;
        }
        s.z_inited = true;
    }
    return true;
}

static bool sink_write(Sink& s, const unsigned char* data, size_t n) {
    if (!n) return true;
    if (s.method == METHOD_STORE) {
        if (std::fwrite(data, 1, n, s.fout) != n) return false;
        *s.total_out += n; return true;
    }
    s.strm.next_in = const_cast<unsigned char*>(data);
    s.strm.avail_in = (uInt)n;
    while (s.strm.avail_in > 0) {
        s.strm.next_out = s.z_out.data();
        s.strm.avail_out = (uInt)s.z_out.size();
        int r = hpz_deflate(&s.strm, Z_NO_FLUSH);
        if (r != Z_OK) return false;
        size_t have = s.z_out.size() - s.strm.avail_out;
        if (have) {
            if (std::fwrite(s.z_out.data(), 1, have, s.fout) != have) return false;
            *s.total_out += have;
        }
    }
    return true;
}

static bool sink_finish(Sink& s) {
    if (s.method == METHOD_STORE) return true;
    for (;;) {
        s.strm.next_out = s.z_out.data();
        s.strm.avail_out = (uInt)s.z_out.size();
        int r = hpz_deflate(&s.strm, Z_FINISH);
        if (r != Z_OK && r != Z_STREAM_END) return false;
        size_t have = s.z_out.size() - s.strm.avail_out;
        if (have) {
            if (std::fwrite(s.z_out.data(), 1, have, s.fout) != have) return false;
            *s.total_out += have;
        }
        if (r == Z_STREAM_END) break;
    }
    if (s.z_inited) hpz_deflateEnd(&s.strm);
    return true;
}

// Reversible transform encoder with streaming output to sink
// Tokens:
//   0x00 0x00 -> literal 0x00
//   0x00 0x01..0x7F -> dictionary id (1..DICT_SIZE)
//   0x00 0x80 <L> -> space run of length (L+4)
//   0x00 0x81 <L> -> newline run of length (L+2)
//   0x00 0x82 <L> <digits...> -> digit run of length (L+3) followed by that many digit bytes
//   0x00 0x83 <L> -> dash ('-') run of length (L+4)
//   0x00 0x84 <L> -> equals ('=') run of length (L+5)
struct Encoder {
    DictIndex idx;
    std::string carry;
    std::vector<unsigned char> tbuf;
    Sink* sink;
    uint8_t flags{T_ALL};
    Encoder(Sink* s) : sink(s) { tbuf.reserve(TBUF_FLUSH); }
    void set_flags(uint8_t f) { flags = f; }
    void flush_tbuf() {
        if (!tbuf.empty()) {
            if (!sink_write(*sink, tbuf.data(), tbuf.size())) { std::fprintf(stderr, "[ERROR] sink_write failed while flushing transform buffer\n"); std::exit(1); }
            tbuf.clear();
        }
    }
    inline void emit_byte(unsigned char b) {
        tbuf.push_back(b);
        if (tbuf.size() >= TBUF_FLUSH) flush_tbuf();
    }
    inline void emit_data(const unsigned char* p, size_t n) {
        if (n == 0) return;
        if (tbuf.size() + n >= TBUF_FLUSH) flush_tbuf();
        if (n >= TBUF_FLUSH) {
            if (!sink_write(*sink, p, n)) { std::fprintf(stderr, "[ERROR] sink_write failed\n"); std::exit(1); }
        } else {
            tbuf.insert(tbuf.end(), p, p + n);
        }
    }
    inline void emit_token(uint8_t id) { emit_byte(0x00); emit_byte(id); }
    inline void emit_spaces(size_t n) {
        while (n >= 259) { emit_byte(0x00); emit_byte(0x80); emit_byte((unsigned char)(255)); n -= 259; }
        if (n >= 4) { emit_byte(0x00); emit_byte(0x80); emit_byte((unsigned char)(n - 4)); }
        else { for (size_t i = 0; i < n; ++i) emit_byte(' '); }
    }
    inline void emit_newlines(size_t n) {
        while (n >= 257) { emit_byte(0x00); emit_byte(0x81); emit_byte((unsigned char)(255)); n -= 257; }
        if (n >= 2) { emit_byte(0x00); emit_byte(0x81); emit_byte((unsigned char)(n - 2)); }
        else { for (size_t i = 0; i < n; ++i) emit_byte('\n'); }
    }
    inline void emit_digits_run(const unsigned char* s, size_t n) {
        while (n >= 3) {
            size_t chunk = n;
            if (chunk > 258) chunk = 258; // len-3 <= 255
            emit_byte(0x00); emit_byte(0x82); emit_byte((unsigned char)(chunk - 3));
            emit_data(s, chunk);
            s += chunk; n -= chunk;
        }
        for (size_t i = 0; i < n; ++i) emit_byte(s[i]);
    }
    inline void emit_dashes(size_t n) {
        while (n >= 259) { emit_byte(0x00); emit_byte(0x83); emit_byte((unsigned char)(255)); n -= 259; }
        if (n >= 4) { emit_byte(0x00); emit_byte(0x83); emit_byte((unsigned char)(n - 4)); }
        else { for (size_t i = 0; i < n; ++i) emit_byte('-'); }
    }
    inline void emit_equals(size_t n) {
        while (n >= 260) { emit_byte(0x00); emit_byte(0x84); emit_byte((unsigned char)(255)); n -= 260; }
        if (n >= 5) { emit_byte(0x00); emit_byte(0x84); emit_byte((unsigned char)(n - 5)); }
        else { for (size_t i = 0; i < n; ++i) emit_byte('='); }
    }
    void process_block(const unsigned char* data, size_t n, bool final) {
        std::string block; block.reserve(carry.size() + n);
        block.append(carry); carry.clear();
        if (data && n) block.append(reinterpret_cast<const char*>(data), n);
        size_t reserve = final ? 0 : (idx.maxLen > 0 ? idx.maxLen - 1 : 0);
        if (reserve > block.size()) reserve = 0;
        size_t limit = block.size() - reserve;
        const unsigned char* s = reinterpret_cast<const unsigned char*>(block.data());
        size_t i = 0;
        while (i < limit) {
            unsigned char c = s[i];
            if (c == 0x00) { emit_byte(0x00); emit_byte(0x00); ++i; continue; }
            // Dictionary match (longest first per head index)
            if (flags & T_DICT) {
                const auto& cand = idx.heads[c];
                bool matched = false;
                for (int di : cand) {
                    const char* t = HPZT_DICT[di]; size_t L = std::strlen(t);
                    if (i + L <= block.size() && std::memcmp(s + i, t, L) == 0) {
                        emit_token((uint8_t)(di + 1)); i += L; matched = true; break;
                    }
                }
                if (matched) continue;
            }
            // Space-run
            if ((flags & T_SPACE) && c == ' ') {
                size_t j = i; while (j < limit && s[j] == ' ') ++j; size_t run = j - i;
                if (run >= 4) { emit_spaces(run); i = j; continue; }
            }
            // Newline-run
            if ((flags & T_NL) && c == '\n') {
                size_t j = i; while (j < limit && s[j] == '\n') ++j; size_t run = j - i;
                if (run >= 2) { emit_newlines(run); i = j; continue; }
            }
            // Dash-run
            if ((flags & T_DASH) && c == '-') {
                size_t j = i; while (j < limit && s[j] == '-') ++j; size_t run = j - i;
                if (run >= 4) { emit_dashes(run); i = j; continue; }
            }
            // Equals-run (threshold 5 to avoid clashing with '====' dict token)
            if ((flags & T_EQUAL) && c == '=') {
                size_t j = i; while (j < limit && s[j] == '=') ++j; size_t run = j - i;
                if (run >= 5) { emit_equals(run); i = j; continue; }
            }
            // Digit-run (0-9)
            if ((flags & T_DIGITS) && (c >= '0' && c <= '9')) {
                size_t j = i; while (j < limit && s[j] >= '0' && s[j] <= '9') ++j; size_t run = j - i;
                if (run >= 3) { emit_digits_run(s + i, run); i = j; continue; }
            }
            // Literal
            emit_byte(c); ++i;
        }
        // Save carry
        if (!final && reserve) carry.assign(block.data() + limit, reserve);
        if (final && block.size() > limit) {
            // Process remaining bytes (no reserve)
            const unsigned char* rem = reinterpret_cast<const unsigned char*>(block.data() + limit);
            size_t rn = block.size() - limit;
            size_t k = 0;
            while (k < rn) {
                unsigned char cc = rem[k];
                if (cc == 0x00) { emit_byte(0x00); emit_byte(0x00); ++k; }
                else if ((flags & T_SPACE) && cc == ' ') {
                    size_t m = k; while (m < rn && rem[m] == ' ') ++m; size_t r = m - k;
                    if (r >= 4) { emit_spaces(r); k = m; } else { emit_byte(' '); ++k; }
                } else if ((flags & T_NL) && cc == '\n') {
                    size_t m = k; while (m < rn && rem[m] == '\n') ++m; size_t r = m - k;
                    if (r >= 2) { emit_newlines(r); k = m; } else { emit_byte('\n'); ++k; }
                } else if ((flags & T_DASH) && cc == '-') {
                    size_t m = k; while (m < rn && rem[m] == '-') ++m; size_t r = m - k;
                    if (r >= 4) { emit_dashes(r); k = m; } else { emit_byte('-'); ++k; }
                } else if ((flags & T_EQUAL) && cc == '=') {
                    size_t m = k; while (m < rn && rem[m] == '=') ++m; size_t r = m - k;
                    if (r >= 5) { emit_equals(r); k = m; } else { emit_byte('='); ++k; }
                } else if ((flags & T_DIGITS) && (cc >= '0' && cc <= '9')) {
                    size_t m = k; while (m < rn && rem[m] >= '0' && rem[m] <= '9') ++m; size_t r = m - k;
                    if (r >= 3) { emit_digits_run(rem + k, r); k = m; } else { emit_byte(rem[k]); ++k; }
                } else {
                    emit_byte(cc); ++k;
                }
            }
        }
    }
};

static void print_usage(const char* argv0) {
    std::fprintf(stderr, "Usage: %s [--method=zlib|store] [--no-transform | --no-dict --no-space-run --no-nl-run --no-digit-run --no-dash-run --no-equals-run] <enwik9 path> <archive out path>\n", argv0);
}

int main(int argc, char** argv) {
    if (argc < 3) { print_usage(argv[0]); return 2; }

    // Parse optional flags
    Method method = dlz_available() ? METHOD_ZLIB : METHOD_STORE;
    uint8_t hpzt_flags = T_ALL; // default: all transforms on
    bool transforms_any = true;

    int argi = 1;
    for (; argi < argc - 2; ++argi) {
        const char* a = argv[argi];
        if (std::strcmp(a, "--no-transform") == 0) { hpzt_flags = 0; transforms_any = false; continue; }
        if (std::strcmp(a, "--no-dict") == 0)       { hpzt_flags &= ~T_DICT; continue; }
        if (std::strcmp(a, "--no-space-run") == 0)  { hpzt_flags &= ~T_SPACE; continue; }
        if (std::strcmp(a, "--no-nl-run") == 0)     { hpzt_flags &= ~T_NL; continue; }
        if (std::strcmp(a, "--no-digit-run") == 0)  { hpzt_flags &= ~T_DIGITS; continue; }
        if (std::strcmp(a, "--no-dash-run") == 0)   { hpzt_flags &= ~T_DASH; continue; }
        if (std::strcmp(a, "--no-equals-run") == 0) { hpzt_flags &= ~T_EQUAL; continue; }
        if (std::strncmp(a, "--method=", 9) == 0) {
            const char* m = a + 9;
            if (!std::strcmp(m, "zlib")) method = METHOD_ZLIB; else if (!std::strcmp(m, "store")) method = METHOD_STORE; else { print_usage(argv[0]); return 2; }
            continue;
        }
        break;
    }
    if (argc - argi != 2) { print_usage(argv[0]); return 2; }

    const char* in_path = argv[argi];
    const char* out_path = argv[argi+1];

    // Locate archive_stub in the same dir as comp
    std::string exe_dir = dirname_of(argv[0]);
    std::string stub_path = join_path(exe_dir, "archive_stub");

    FILE* fin = std::fopen(in_path, "rb"); if (!fin) { std::fprintf(stderr, "[ERROR] Cannot open input: %s (%s)\n", in_path, std::strerror(errno)); return 1; }
    FILE* fstub = std::fopen(stub_path.c_str(), "rb"); if (!fstub) { std::fprintf(stderr, "[ERROR] Cannot open stub: %s (%s)\n", stub_path.c_str(), std::strerror(errno)); std::fclose(fin); return 1; }
    FILE* fout = std::fopen(out_path, "wb"); if (!fout) { std::fprintf(stderr, "[ERROR] Cannot create output: %s (%s)\n", out_path, std::strerror(errno)); std::fclose(fin); std::fclose(fstub); return 1; }

    // Copy stub
    {
        std::vector<unsigned char> buf(1 << 20); size_t r;
        while ((r = std::fread(buf.data(), 1, buf.size(), fstub)) > 0) {
            if (std::fwrite(buf.data(), 1, r, fout) != r) { std::fprintf(stderr, "[ERROR] Writing stub failed (%s)\n", std::strerror(errno)); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
        }
        if (std::ferror(fstub)) { std::fprintf(stderr, "[ERROR] Reading stub failed (%s)\n", std::strerror(errno)); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
    }

    // Validate zlib availability if requested
    if (method == METHOD_ZLIB && !dlz_available()) {
        std::fprintf(stderr, "[WARN] zlib not available at runtime; falling back to STORE.\n");
        method = METHOD_STORE;
    }

    uint64_t total_in = 0, total_out = 0; uint32_t crc = 0u;

    // Prepare sink
    Sink sink{};
    if (!sink_init(sink, method, fout, &total_out)) {
        if (method == METHOD_ZLIB) {
            std::fprintf(stderr, "[WARN] deflateInit2 failed; using STORE.\n");
            method = METHOD_STORE;
            if (!sink_init(sink, method, fout, &total_out)) { std::fprintf(stderr, "[ERROR] Sink init failed\n"); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
        } else {
            std::fprintf(stderr, "[ERROR] Sink init failed\n");
            std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1;
        }
    }

    // Optional HPZT header when transforms enabled (v2 with dict CRC)
    const bool apply_transforms = (hpzt_flags != 0);
    if (apply_transforms) {
        unsigned char hdr[12]; hdr[0]='H'; hdr[1]='P'; hdr[2]='Z'; hdr[3]='T'; hdr[4]=2; // version 2
        hdr[5]=hpzt_flags; hdr[6]=0; hdr[7]=0;
        uint32_t dcrc = hpzt_dict_crc32();
        hdr[8] = (unsigned char)(dcrc & 0xFF);
        hdr[9] = (unsigned char)((dcrc >> 8) & 0xFF);
        hdr[10] = (unsigned char)((dcrc >> 16) & 0xFF);
        hdr[11] = (unsigned char)((dcrc >> 24) & 0xFF);
        if (!sink_write(sink, hdr, sizeof(hdr))) { std::fprintf(stderr, "[ERROR] Writing transform header failed\n"); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
    }

    // Stream input -> transforms -> sink OR raw -> sink when transforms disabled
    Encoder enc(&sink); enc.set_flags(hpzt_flags);
    std::vector<unsigned char> inbuf; inbuf.resize(IN_CHUNK);
    for (;;) {
        size_t n = std::fread(inbuf.data(), 1, inbuf.size(), fin);
        if (n > 0) {
            crc = crc32_update(crc, inbuf.data(), n);
            total_in += n;
            if (apply_transforms) enc.process_block(inbuf.data(), n, false);
            else if (!sink_write(sink, inbuf.data(), n)) { std::fprintf(stderr, "[ERROR] sink_write failed\n"); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
        }
        if (n < inbuf.size()) {
            if (std::ferror(fin)) { std::fprintf(stderr, "[ERROR] Reading input failed (%s)\n", std::strerror(errno)); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
            break;
        }
    }

    if (apply_transforms) { enc.process_block(nullptr, 0, true); enc.flush_tbuf(); }

    if (!sink_finish(sink)) { std::fprintf(stderr, "[ERROR] Finishing sink failed\n"); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }

    // Footer HPZ2
    {
        const char magic2[4] = {'H','P','Z','2'};
        if (std::fwrite(magic2, 1, 4, fout) != 4) { std::fprintf(stderr, "[ERROR] Writing footer magic failed (%s)\n", std::strerror(errno)); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
        unsigned char method_and_pad[4] = { (unsigned char)method, 0, 0, 0 };
        if (std::fwrite(method_and_pad, 1, 4, fout) != 4) { std::fprintf(stderr, "[ERROR] Writing footer method failed (%s)\n", std::strerror(errno)); std::fclose(fin); std::fclose(fstub); std::fclose(fout); return 1; }
        write_le64(fout, total_in);
        write_le64(fout, total_out);
        write_le32(fout, crc);
    }

    std::fclose(fin); std::fclose(fstub);
    if (std::fclose(fout) != 0) { std::fprintf(stderr, "[ERROR] Closing archive failed (%s)\n", std::strerror(errno)); return 1; }

    chmod(out_path, 0755);
    std::fprintf(stderr, "[OK] Created archive: %s\n", out_path);
    std::fprintf(stderr, " Method:     %s\n", method == METHOD_ZLIB ? "ZLIB" : "STORE");
    if (apply_transforms) {
        std::fprintf(stderr, " Transforms: HPZT v2 mask=0x%02X (dict=%s, space=%s, nl=%s, digits=%s, dash=%s, equals=%s)\n",
            hpzt_flags,
            (hpzt_flags & T_DICT) ? "on" : "off",
            (hpzt_flags & T_SPACE) ? "on" : "off",
            (hpzt_flags & T_NL) ? "on" : "off",
            (hpzt_flags & T_DIGITS) ? "on" : "off",
            (hpzt_flags & T_DASH) ? "on" : "off",
            (hpzt_flags & T_EQUAL) ? "on" : "off");
    } else {
        std::fprintf(stderr, " Transforms: none\n");
    }
    std::fprintf(stderr, " Original:   %llu bytes\n", (unsigned long long) total_in);
    std::fprintf(stderr, " Payload:    %llu bytes\n", (unsigned long long) total_out);
    return 0;
}
