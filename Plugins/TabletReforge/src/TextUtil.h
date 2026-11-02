// Text helpers that need no SDK (STL only), so the tests build them: a slider's number format
// from a translated text, and character counts of UTF-8 text.
#pragma once

#include <string>
#include <string_view>

namespace TabletHelper {

// Marks where a slider's own number goes in a translated text (a TrF argument).
inline constexpr const char* kNumberMark = "\x01";

// The ImGui number format of a slider from a translated text: the text's own '%' are doubled
// (a translation is never a format), the FIRST kNumberMark becomes `conversion`. A later mark
// (a translation naming {value} twice) is written back as "{value}": ImGui passes one number,
// so a second conversion would read an argument that is not there.
inline std::string NumberFormat(std::string_view text, const char* conversion = "%d") {
    std::string out;
    out.reserve(text.size() + 4);
    bool placed = false;
    for (const char c : text) {
        if (c == '%') {
            out += "%%";
        } else if (c == kNumberMark[0]) {
            out += placed ? "{value}" : (conversion ? conversion : "");
            placed = true;
        } else {
            out += c;
        }
    }
    return out;
}

// The characters (code points) of UTF-8 text: continuation bytes are not counted.
inline size_t Utf8CharCount(std::string_view s) {
    size_t n = 0;
    for (const char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

// A text cut with "..." whose kept part has fewer than minChars characters (not bytes: a
// Russian letter is two bytes, a Thai one three).
inline bool CutBelow(std::string_view text, size_t minChars) {
    constexpr std::string_view dots = "...";
    if (text.size() < dots.size() || text.substr(text.size() - dots.size()) != dots) return false;
    return Utf8CharCount(text.substr(0, text.size() - dots.size())) < minChars;
}

// Lower case for searching translated texts: ASCII, Latin-1 letters and Cyrillic
// (the scripts of the program languages that have case), other bytes unchanged.
inline std::string FoldCase(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 'A' && c <= 'Z') {
            out += static_cast<char>(c + ('a' - 'A'));
        } else if ((c == 0xC3) && i + 1 < s.size()) {
            unsigned char d = static_cast<unsigned char>(s[i + 1]);
            if (d >= 0x80 && d <= 0x9E && d != 0x97) d = static_cast<unsigned char>(d + 0x20);  // U+00C0..U+00DE
            out += static_cast<char>(c);
            out += static_cast<char>(d);
            ++i;
        } else if ((c == 0xD0) && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            if (d >= 0x90 && d <= 0x9F) {          // U+0410..U+041F -> U+0430..U+043F
                out += static_cast<char>(0xD0);
                out += static_cast<char>(d + 0x20);
            } else if (d >= 0xA0 && d <= 0xAF) {   // U+0420..U+042F -> U+0440..U+044F
                out += static_cast<char>(0xD1);
                out += static_cast<char>(d - 0x20);
            } else if (d >= 0x80 && d <= 0x8F) {   // U+0400..U+040F -> U+0450..U+045F
                out += static_cast<char>(0xD1);
                out += static_cast<char>(d + 0x10);
            } else {
                out += static_cast<char>(c);
                out += static_cast<char>(d);
            }
            ++i;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// A bonus row matches the search (already folded) by its shown name, its English name (the
// same search finds it in any program language) or its mod id.
inline bool BonusMatchesSearch(const std::string& foldedNeedle, std::string_view shown, std::string_view english,
                               std::string_view id) {
    if (foldedNeedle.empty()) return true;
    for (const std::string_view text : { shown, english, id })
        if (FoldCase(text).find(foldedNeedle) != std::string::npos) return true;
    return false;
}

} // namespace TabletHelper
