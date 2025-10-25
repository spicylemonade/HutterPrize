#ifndef HPZT_DICT_H
#define HPZT_DICT_H
#include <cstddef>
#include <cstdint>

// Unified dictionary shared by encoder and decoder.
// IMPORTANT: Keep order stable. No duplicates.
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
    // Additional common markers (no duplicates; note: removed second "[[Category:")
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "{{Infobox", "{{infobox", "<redirect", "#REDIRECT"
};
static constexpr int HPZT_DICT_SIZE = (int)(sizeof(HPZT_DICT)/sizeof(HPZT_DICT[0]));

// Inline Fowler–Noll–Vo variant checksum over the dictionary bytes, including a 0 delimiter per entry.
static inline uint32_t hpzt_dict_checksum() {
    uint32_t h = 2166136261u; // FNV-1a basis
    for (int i = 0; i < HPZT_DICT_SIZE; ++i) {
        const unsigned char* s = (const unsigned char*)HPZT_DICT[i];
        while (*s) { h ^= *s++; h *= 16777619u; }
        // include delimiter to avoid ambiguity between concatenated strings
        h ^= 0u; h *= 16777619u;
    }
    // mix in size for extra guard
    h ^= (uint32_t)HPZT_DICT_SIZE; h *= 16777619u;
    return h;
}

#endif
