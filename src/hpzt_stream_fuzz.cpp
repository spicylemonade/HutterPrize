#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include "hpzt_dict.h"

static constexpr size_t MAX_INPUT = 1<<16; // 64 KiB per case

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

struct Encoder {
    DictIndex idx;
    std::string carry;
    std::vector<unsigned char> out;
    void emit_byte(unsigned char b){ out.push_back(b);} 
    void emit_data(const unsigned char* p, size_t n){ out.insert(out.end(), p, p+n);} 
    void emit_token(uint8_t id){ emit_byte(0x00); emit_byte(id);} 
    void emit_spaces(size_t n){ while(n>=259){emit_byte(0x00);emit_byte(0x80);emit_byte(255);n-=259;} if(n>=4){emit_byte(0x00);emit_byte(0x80);emit_byte((unsigned char)(n-4));} else { for(size_t i=0;i<n;++i) emit_byte(' ');} }
    void emit_newlines(size_t n){ while(n>=257){emit_byte(0x00);emit_byte(0x81);emit_byte(255);n-=257;} if(n>=2){emit_byte(0x00);emit_byte(0x81);emit_byte((unsigned char)(n-2));} else { for(size_t i=0;i<n;++i) emit_byte('\n');} }
    void emit_digits_run(const unsigned char* s, size_t n){ while(n>=3){ size_t chunk = n>258?258:n; emit_byte(0x00); emit_byte(0x82); emit_byte((unsigned char)(chunk-3)); emit_data(s, chunk); s+=chunk; n-=chunk; } for(size_t i=0;i<n;++i) emit_byte(s[i]); }
    void write_header_v2(){ unsigned char hdr[12]; hdr[0]='H'; hdr[1]='P'; hdr[2]='Z'; hdr[3]='T'; hdr[4]=2; hdr[5]=0x0F; hdr[6]=0; hdr[7]=0; uint32_t dcrc = hpzt_dict_checksum(); hdr[8]=(unsigned char)(dcrc&0xFF); hdr[9]=(unsigned char)((dcrc>>8)&0xFF); hdr[10]=(unsigned char)((dcrc>>16)&0xFF); hdr[11]=(unsigned char)((dcrc>>24)&0xFF); emit_data(hdr, sizeof(hdr)); }
    void process_block(const unsigned char* data, size_t n, bool final){ std::string block; block.reserve(carry.size()+n); block.append(carry); carry.clear(); if(data&&n) block.append((const char*)data, n); size_t reserve = final?0:(idx.maxLen?idx.maxLen-1:0); if(reserve>block.size()) reserve=0; size_t limit = block.size()-reserve; const unsigned char* s = (const unsigned char*)block.data(); size_t i=0; while(i<limit){ unsigned char c=s[i]; if(c==0x00){ emit_byte(0x00); emit_byte(0x00); ++i; continue; } const auto& cand = idx.heads[c]; bool matched=false; for(int di: cand){ const char* t=HPZT_DICT[di]; size_t L=std::strlen(t); if(i+L<=block.size() && std::memcmp(s+i,t,L)==0){ emit_token((uint8_t)(di+1)); i+=L; matched=true; break; } } if(matched) continue; if(c==' '){ size_t j=i; while(j<limit && s[j]==' ') ++j; size_t run=j-i; if(run>=4){ emit_spaces(run); i=j; continue; } }
            if(c=='\n'){ size_t j=i; while(j<limit && s[j]=='\n') ++j; size_t run=j-i; if(run>=2){ emit_newlines(run); i=j; continue; } }
            if(c>='0'&&c<='9'){ size_t j=i; while(j<limit && s[j]>='0'&&s[j]<='9') ++j; size_t run=j-i; if(run>=3){ emit_digits_run(s+i, run); i=j; continue; } }
            emit_byte(c); ++i; }
        if(!final && reserve) carry.assign(block.data()+limit, reserve);
        if(final && block.size()>limit){ const unsigned char* rem=(const unsigned char*)block.data()+limit; size_t rn=block.size()-limit; size_t k=0; while(k<rn){ unsigned char cc=rem[k]; if(cc==0x00){ emit_byte(0x00); emit_byte(0x00); ++k; } else if(cc==' '){ size_t m=k; while(m<rn && rem[m]==' ') ++m; size_t r=m-k; if(r>=4){ emit_spaces(r); k=m; } else { emit_byte(' '); ++k; } } else if(cc=='\n'){ size_t m=k; while(m<rn && rem[m]=='\n') ++m; size_t r=m-k; if(r>=2){ emit_newlines(r); k=m; } else { emit_byte('\n'); ++k; } } else if(cc>='0'&&cc<='9'){ size_t m=k; while(m<rn && rem[m]>='0'&&rem[m]<='9') ++m; size_t r=m-k; if(r>=3){ emit_digits_run(rem+k, r); k=m; } else { emit_byte(rem[k]); ++k; } } else { emit_byte(cc); ++k; } } }
    }
};

static inline uint32_t read_le32(const unsigned char* p){ uint32_t v=0; for(int i=3;i>=0;--i) v=(v<<8)|p[i]; return v; }

struct Decoder {
    enum EscState { ESC_NONE=0, ESC_SEEN00=1, ESC_SPACE=2, ESC_NL=3, ESC_DIGIT_LEN=4, ESC_DIGIT_COPY=5 };
    unsigned char hdr[12]; size_t hdr_pos=0; size_t hdr_need=8; bool header_done=false; bool transforms=false; EscState esc=ESC_NONE; size_t digit_left=0; 
    bool feed(const unsigned char* in, size_t n, std::vector<unsigned char>& out){ size_t i=0; while(i<n){ if(!header_done){ while(hdr_pos<hdr_need && i<n) hdr[hdr_pos++]=in[i++]; if(hdr_pos<hdr_need) return true; if(!(hdr[0]=='H'&&hdr[1]=='P'&&hdr[2]=='Z'&&hdr[3]=='T')){ out.insert(out.end(), hdr, hdr+hdr_pos); header_done=true; transforms=false; continue; } unsigned ver=hdr[4], flags=hdr[5]; if(hdr_need==8 && ver>=2){ hdr_need=12; while(hdr_pos<hdr_need && i<n) hdr[hdr_pos++]=in[i++]; if(hdr_pos<hdr_need) return true; uint32_t dcrc_in=read_le32(hdr+8); if(dcrc_in!=hpzt_dict_checksum()) return false; }
                header_done=true; transforms=(flags&0x0F)!=0; continue; }
            if(!transforms){ out.insert(out.end(), in+i, in+n); return true; }
            unsigned char b=in[i++];
            if(esc==ESC_NONE){ if(b!=0x00) out.push_back(b); else esc=ESC_SEEN00; }
            else if(esc==ESC_SEEN00){ if(b==0x00){ out.push_back(0x00); esc=ESC_NONE; } else if(b==0x80){ esc=ESC_SPACE; } else if(b==0x81){ esc=ESC_NL; } else if(b==0x82){ esc=ESC_DIGIT_LEN; } else if(b>=1 && b<=HPZT_DICT_SIZE){ const char* s=HPZT_DICT[b-1]; size_t L=std::strlen(s); out.insert(out.end(), s, s+L); esc=ESC_NONE; } else { return false; } }
            else if(esc==ESC_SPACE){ out.insert(out.end(), (size_t)b+4, (unsigned char)' '); esc=ESC_NONE; }
            else if(esc==ESC_NL){ out.insert(out.end(), (size_t)b+2, (unsigned char)'\n'); esc=ESC_NONE; }
            else if(esc==ESC_DIGIT_LEN){ digit_left=(size_t)b+3; esc=ESC_DIGIT_COPY; }
            else if(esc==ESC_DIGIT_COPY){ size_t can = std::min(digit_left, n-(i-1)); i--; const unsigned char* src=in+i; out.insert(out.end(), src, src+can); i+=can; digit_left-=can; if(digit_left==0) esc=ESC_NONE; }
        }
        return true; }
    bool finish_ok() const { return esc==ESC_NONE; }
};

static std::string random_case(std::mt19937_64& rng){
    std::uniform_int_distribution<int> len_d(0, (int)MAX_INPUT);
    std::uniform_int_distribution<int> pick(0, 9);
    std::string s; s.reserve(1024);
    int L = len_d(rng);
    for(int i=0;i<L;++i){ int t=pick(rng); if(t<=1) s.push_back(' '); else if(t==2) s.push_back('\n'); else if(t==3) s.push_back((char)0x00); else if(t==4) s.push_back('0'+(rng()%10)); else if(t==5){ // insert dict token
            const char* tok = HPZT_DICT[rng()%HPZT_DICT_SIZE]; s.append(tok);
        } else { // random ascii
            char c = (char)(32 + (rng()%95)); s.push_back(c);
        } }
    // ensure some structured patterns
    if(rng()%2==0) s.append("[[Category:Foo]]\n");
    if(rng()%2==0) s.append("{{Infobox bar}}\n");
    return s;
}

int main(int argc, char** argv){
    uint64_t seed = 123456789ULL; int iters = 200; // bounded for CI speed
    if(argc>=2) seed = strtoull(argv[1], nullptr, 10);
    if(argc>=3) iters = std::max(1, atoi(argv[2]));
    std::mt19937_64 rng(seed);
    for(int it=0; it<iters; ++it){
        std::string in = random_case(rng);
        Encoder enc; enc.write_header_v2();
        // feed with varying chunk sizes
        size_t pos=0; bool flip=false;
        while(pos<in.size()){
            size_t ch = std::min<size_t>((flip?7:19), in.size()-pos);
            enc.process_block((const unsigned char*)in.data()+pos, ch, false);
            pos += ch; flip = !flip;
        }
        enc.process_block(nullptr, 0, true);
        Decoder dec; std::vector<unsigned char> out; size_t off=0; while(off<enc.out.size()){
            size_t ch = std::min<size_t>(13, enc.out.size()-off);
            if(!dec.feed(enc.out.data()+off, ch, out)) { std::fprintf(stderr, "[FUZZ] decode feed failed at iter %d (seed=%llu)\n", it, (unsigned long long)seed); return 1; }
            off += ch;
        }
        if(!dec.finish_ok()){ std::fprintf(stderr, "[FUZZ] decoder not finished cleanly (iter %d)\n", it); return 1; }
        if(out.size()!=in.size() || std::memcmp(out.data(), in.data(), in.size())!=0){
            // find first diff
            size_t m=0; while(m<in.size() && m<out.size() && in[m]==out[m]) ++m;
            std::fprintf(stderr, "[FUZZ] mismatch at iter %d pos %zu (in=%zu out=%zu)\n", it, m, in.size(), out.size());
            return 1;
        }
    }
    std::fprintf(stderr, "[FUZZ] hpzt_stream_fuzz PASSED (%d iters, seed=%llu)\n", iters, (unsigned long long)seed);
    return 0;
}
