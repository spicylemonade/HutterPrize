#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cinttypes>
#include "dict.h"
#include "transform.h"

static std::string dict_tok(const char* needle) {
    for (int i = 0; i < HPZ_DICT_SIZE; ++i) {
        if (std::strcmp(HPZ_DICT[i], needle) == 0) {
            unsigned char id = (unsigned char)(i + 1);
            std::string t; t.push_back((char)0x00); t.push_back((char)id);
            return t;
        }
    }
    std::fprintf(stderr, "[SELFTEST] Dictionary token not found: %s\n", needle);
    std::exit(1);
}

static std::vector<unsigned char> make_hpzt_header_v2() {
    std::vector<unsigned char> h(12);
    h[0]='H'; h[1]='P'; h[2]='Z'; h[3]='T';
    h[4]=2; // version
    h[5]=0x0F; // flags (all transforms available)
    h[6]=0; h[7]=0;
    uint32_t d = hpz_dict_checksum();
    h[8] = (unsigned char)(d & 0xFF);
    h[9] = (unsigned char)((d >> 8) & 0xFF);
    h[10]= (unsigned char)((d >> 16) & 0xFF);
    h[11]= (unsigned char)((d >> 24) & 0xFF);
    return h;
}

static void hex_dump(const char* label, const unsigned char* p, size_t n) {
    std::fprintf(stderr, "%s (%zu):", label, n);
    for (size_t i = 0; i < n; ++i) std::fprintf(stderr, " %02X", (unsigned)p[i]);
    std::fprintf(stderr, "\n");
}

static void run_case_chunked(const std::vector<unsigned char>& enc, const std::vector<size_t>& chunks, const std::vector<unsigned char>& expect) {
    FILE* tmp = tmpfile();
    if (!tmp) { std::perror("tmpfile"); std::exit(1); }
    uint32_t crc = 0; uint64_t written = 0;
    hpz::TransformDecoder dec; dec.reset();

    size_t off = 0;
    for (size_t len : chunks) {
        if (off + len > enc.size()) len = enc.size() - off;
        if (len == 0) continue;
        if (!dec.feed(enc.data() + off, len, tmp, crc, written)) {
            std::fprintf(stderr, "[SELFTEST] feed() failed during chunked run\n");
            std::exit(1);
        }
        off += len;
    }
    if (off < enc.size()) {
        if (!dec.feed(enc.data() + off, enc.size() - off, tmp, crc, written)) {
            std::fprintf(stderr, "[SELFTEST] feed() failed on tail\n");
            std::exit(1);
        }
    }
    if (!dec.finish_ok()) { std::fprintf(stderr, "[SELFTEST] finish_ok() failed\n"); std::exit(1); }

    fflush(tmp); fseeko(tmp, 0, SEEK_SET);
    std::vector<unsigned char> out; out.resize((size_t)written);
    size_t r = fread(out.data(), 1, out.size(), tmp);
    fclose(tmp);
    if (r != out.size()) { std::fprintf(stderr, "[SELFTEST] readback size mismatch (r=%zu, wrote=%zu)\n", r, (size_t)written); std::exit(1); }
    if (out != expect) {
        hex_dump("[SELFTEST] Expected", expect.data(), expect.size());
        hex_dump("[SELFTEST] Got     ", out.data(), out.size());
        std::fprintf(stderr, "[SELFTEST] output mismatch\n");
        std::exit(1);
    }
}

int main() {
    auto hdr = make_hpzt_header_v2();

    // Case 1: literal bytes including 0x00 escape; chunk split inside header and payload
    std::vector<unsigned char> enc1 = hdr;
    enc1.push_back('A');
    enc1.push_back(0x00); enc1.push_back(0x00); // literal 0x00
    enc1.push_back('B');
    std::vector<unsigned char> exp1 = {'A', 0x00, 'B'};
    run_case_chunked(enc1, {5, 5, 6}, exp1); // header split: 5+5+2, payload 4 in final chunk

    // Case 2: dictionary token for "[[" (simple)
    std::vector<unsigned char> enc2 = hdr;
    std::string tok = dict_tok("[[");
    enc2.insert(enc2.end(), tok.begin(), tok.end());
    std::vector<unsigned char> exp2 = {'[','['};
    run_case_chunked(enc2, {3, 3, 6}, exp2);

    // Case 3: space run of length 10, split so that length byte is last in a chunk
    std::vector<unsigned char> enc3 = hdr; enc3.push_back(0x00); enc3.push_back(0x80); enc3.push_back((unsigned char)(10-4));
    std::vector<unsigned char> exp3(10, (unsigned char)' ');
    run_case_chunked(enc3, {12, 1, 2}, exp3);

    // Case 4: newline run of length 5, split inside run expansion
    std::vector<unsigned char> enc4 = hdr; enc4.push_back(0x00); enc4.push_back(0x81); enc4.push_back((unsigned char)(5-2));
    std::vector<unsigned char> exp4(5, (unsigned char)'\n');
    run_case_chunked(enc4, {8, 4}, exp4);

    // Case 5: digit run of length 7 ("1234567"), split so digits cross boundaries
    std::vector<unsigned char> enc5 = hdr; enc5.push_back(0x00); enc5.push_back(0x82); enc5.push_back((unsigned char)(7-3));
    const char* digs = "1234567"; enc5.insert(enc5.end(), digs, digs+7);
    std::vector<unsigned char> exp5 = {'1','2','3','4','5','6','7'};
    run_case_chunked(enc5, {9, 3, 1, 3}, exp5);

    // Case 6: mixed sequence across boundaries: dict + spaces + digits + literal 0; ensure each token boundary is split
    std::vector<unsigned char> enc6 = hdr;
    std::string tok2 = dict_tok("{{"); enc6.insert(enc6.end(), tok2.begin(), tok2.end());
    enc6.push_back(0x00); enc6.push_back(0x80); enc6.push_back((unsigned char)(6-4)); // 6 spaces
    enc6.push_back(0x00); enc6.push_back(0x82); enc6.push_back((unsigned char)(4-3)); const char* d2 = "2024"; enc6.insert(enc6.end(), d2, d2+4);
    enc6.push_back(0x00); enc6.push_back(0x00);
    std::vector<unsigned char> exp6; exp6.insert(exp6.end(), {'{','{'}); exp6.insert(exp6.end(), 6, (unsigned char)' '); exp6.insert(exp6.end(), {'2','0','2','4'}); exp6.push_back(0x00);
    run_case_chunked(enc6, {7, 3, 4, 3, 99}, exp6);

    std::puts("[SELFTEST] OK");
    return 0;
}
