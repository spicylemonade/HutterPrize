#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include "hpzt_dict.h"
#include "dlz.h"

#if defined(__linux__)
#include <limits.h>
#endif

enum Method : uint8_t { METHOD_STORE = 0, METHOD_ZLIB = 1 };

static inline uint64_t read_le64(const unsigned char* p) { uint64_t v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | p[i]; return v; }
static inline uint32_t read_le32(const unsigned char* p) { uint32_t v = 0; for (int i = 3; i >= 0; --i) v = (v << 8) | p[i]; return v; }

static off_t file_size_of(const char* path) { struct stat st{}; if (stat(path, &st) != 0) return -1; return st.st_size; }

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <archive>\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    FILE* f = std::fopen(path, "rb"); if (!f) { std::fprintf(stderr, "[ERROR] Cannot open %s: %s\n", path, std::strerror(errno)); return 1; }
    off_t fsz = file_size_of(path); if (fsz < (off_t)24) { std::fprintf(stderr, "[ERROR] File too small.\n"); std::fclose(f); return 1; }

    bool hpz2 = false; unsigned char footer28[28]; Method method = METHOD_ZLIB; uint64_t orig_size=0, comp_size=0; uint32_t expected_crc=0; off_t payload_off=0;
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
        method = METHOD_ZLIB; orig_size = read_le64(footer24 + 4); comp_size = read_le64(footer24 + 12); expected_crc = read_le32(footer24 + 20); payload_off = fsz - (off_t)24 - (off_t)comp_size;
    }
    if (payload_off <= 0) { std::fprintf(stderr, "[ERROR] Invalid payload offset.\n"); std::fclose(f); return 1; }

    // Read beginning of payload to inspect HPZT header
    if (fseeko(f, payload_off, SEEK_SET) != 0) { std::fprintf(stderr, "[ERROR] seek payload failed: %s\n", std::strerror(errno)); std::fclose(f); return 1; }
    unsigned char head[16]; size_t hr = std::fread(head, 1, sizeof(head), f);

    bool has_hpzt = false; unsigned ver = 0; unsigned flags = 0; bool have_dcrc=false; uint32_t dcrc_in=0; uint32_t dcrc_here=0; size_t hpzt_len=0;
    if (hr >= 8 && head[0]=='H' && head[1]=='P' && head[2]=='Z' && head[3]=='T') {
        has_hpzt = true; ver = head[4]; flags = head[5];
        if (ver >= 2 && hr >= 12) { have_dcrc = true; dcrc_in = read_le32(head + 8); dcrc_here = hpzt_dict_checksum(); hpzt_len = 12; }
        else { hpzt_len = 8; }
    }

    std::printf("archive=%s\n", path);
    std::printf("size_total=%lld\n", (long long)fsz);
    std::printf("method=%s\n", method==METHOD_ZLIB?"ZLIB":"STORE");
    std::printf("orig_size=%llu\n", (unsigned long long)orig_size);
    std::printf("comp_size=%llu\n", (unsigned long long)comp_size);
    std::printf("payload_off=%lld\n", (long long)payload_off);
    std::printf("footer=%s\n", hpz2?"HPZ2":"HPZ1");
    std::printf("payload_has_hpzt=%s\n", has_hpzt?"yes":"no");
    if (has_hpzt) {
        std::printf("hpzt_ver=%u\n", ver);
        std::printf("hpzt_flags=0x%02x\n", flags);
        std::printf("hpzt_header_len=%zu\n", hpzt_len);
        if (have_dcrc) {
            std::printf("dict_crc_in=0x%08x\n", dcrc_in);
            std::printf("dict_crc_here=0x%08x\n", dcrc_here);
            std::printf("dict_crc_match=%s\n", (dcrc_in==dcrc_here)?"yes":"no");
        }
    }

    std::fclose(f);
    return 0;
}
