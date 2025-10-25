#ifndef TRANSFORM_DICT_H
#define TRANSFORM_DICT_H
#include <cstddef>

// Shared dictionary for HPZT transforms (encoder/decoder).
// IMPORTANT: Keep this single source of truth to avoid ID skew.
// Note: Removed duplicate "[[Category:" entry present in earlier encoder.
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
    // Additional common markers
    "|year", "|month", "|day", "|access-date", "|access-date=",
    "{{Infobox", "{{infobox", "<redirect", "#REDIRECT"
};
static constexpr int HPZT_DICT_SIZE = (int)(sizeof(HPZT_DICT)/sizeof(HPZT_DICT[0]));

#endif
