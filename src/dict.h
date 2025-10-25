#ifndef HPZ_DICT_H
#define HPZ_DICT_H
#include <cstddef>
#include <cstdint>
#include <cstring>

// Unified dictionary shared by encoder and decoder (duplicate removed).
// NOTE: Keep this list strictly in sync across versions. Any change must bump
// the HPZT header version and/or update checksum validation expectations.
static const char* const HPZ_DICT[] = {
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
    // Additional common markers (duplicate of "[[Category:" removed)
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "{{Infobox", "{{infobox", "<redirect", "#REDIRECT"
};
static constexpr int HPZ_DICT_SIZE = (int)(sizeof(HPZ_DICT)/sizeof(HPZ_DICT[0]));

// Local CRC32 (poly 0xEDB88320), identical logic on both sides to compute dict checksum.
static inline uint32_t hpzt_crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256]; static bool have = false;
    if (!have) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); table[i] = c; } have = true; }
    crc ^= 0xFFFFFFFFu; for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8); return crc ^ 0xFFFFFFFFu;
}

// Compute a checksum for the dictionary contents and order. We include the terminating '\0'
// for each string so that both content and boundaries affect the hash.
static inline uint32_t hpzt_dict_crc32() {
    uint32_t crc = 0u;
    for (int i = 0; i < HPZ_DICT_SIZE; ++i) {
        const char* s = HPZ_DICT[i]; size_t L = std::strlen(s) + 1; // include NUL
        crc = hpzt_crc32_update(crc, reinterpret_cast<const unsigned char*>(s), L);
    }
    // Also fold in the dictionary size for extra safety.
    unsigned char szb[4]; szb[0] = (unsigned char)(HPZ_DICT_SIZE & 0xFF); szb[1] = (unsigned char)((HPZ_DICT_SIZE >> 8) & 0xFF);
    szb[2] = (unsigned char)((HPZ_DICT_SIZE >> 16) & 0xFF); szb[3] = (unsigned char)((HPZ_DICT_SIZE >> 24) & 0xFF);
    crc = hpzt_crc32_update(crc, szb, 4);
    return crc;
}

#endif // HPZ_DICT_H
