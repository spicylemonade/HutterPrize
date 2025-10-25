#ifndef HPZ_DICT_H
#define HPZ_DICT_H
#include <cstddef>
#include <cstdint>

// Unified dictionary used by both compressor and decompressor. Duplicate entries removed.
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
    // Additional common markers (de-duplicated)
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "{{Infobox", "{{infobox", "<redirect", "#REDIRECT"
};
static constexpr int HPZ_DICT_SIZE = (int)(sizeof(HPZ_DICT)/sizeof(HPZ_DICT[0]));

// Deterministic checksum over the dictionary contents and entry separators (simple FNV-1a-like)
static inline uint32_t hpz_dict_checksum() {
    uint32_t h = 2166136261u;
    for (int i = 0; i < HPZ_DICT_SIZE; ++i) {
        const unsigned char* p = (const unsigned char*)HPZ_DICT[i];
        while (*p) { h ^= *p++; h *= 16777619u; }
        // include separator
        h ^= 0u; h *= 16777619u;
    }
    return h;
}

#endif // HPZ_DICT_H
