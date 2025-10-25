#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Canonical dictionary for HPZT transforms (shared by encoder/decoder).
// Duplicate entries removed and this is the single source of truth.
static const char* const HPZT_DICT[] = {
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
    // Additional common markers (deduped)
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "{{Infobox", "{{infobox", "<redirect", "#REDIRECT",
    // Newly added tokens (appended only)
    "http://", "https://", "://", "en.wikipedia.org", ".wikipedia.org",
    "<ref name=\"", "\"/>", "\" />",
    "&amp;", "&lt;", "&gt;",
    "== References ==", "== External links ==", "== See also ==",
    "{{cite web", "{{cite journal", "{{cite book",
    "{{reflist", "{{Reflist",
    "{{DEFAULTSORT:", "{{Convert", "{{convert",
    "<br/>", "<br />"
};
static constexpr int HPZT_DICT_SIZE = (int)(sizeof(HPZT_DICT)/sizeof(HPZT_DICT[0]));

// Compile-time guard: dictionary IDs are encoded as 0x00, id (single byte).
// Reserve 0x80.. for transform control codes. Ensure IDs never overlap [0x80..].
static_assert(HPZT_DICT_SIZE <= 127, "HPZT_DICT_SIZE must be <= 127 to avoid collisions with control codes (0x80..)");

// CRC32 for dictionary fingerprint (poly 0xEDB88320), TU-local and independent.
static inline uint32_t hpzt_crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    static uint32_t table[256];
    static bool have = false;
    if (!have) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        have = true;
    }
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Compute a stable fingerprint for the dictionary content and ordering by
// hashing each string including a trailing NUL separator.
static inline uint32_t hpzt_dict_crc32() {
    uint32_t crc = 0u;
    for (int i = 0; i < HPZT_DICT_SIZE; ++i) {
        const char* s = HPZT_DICT[i];
        size_t L = std::strlen(s);
        crc = hpzt_crc32_update(crc, reinterpret_cast<const unsigned char*>(s), L);
        unsigned char nul = 0; crc = hpzt_crc32_update(crc, &nul, 1);
    }
    return crc;
}
