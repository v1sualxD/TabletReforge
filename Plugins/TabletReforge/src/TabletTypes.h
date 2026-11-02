#pragma once

// Tablet type keys, detection and classification. Pure standard C++ (no host or
// ImGui dependency), shared by the scan worker, the matcher and the tests.

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace TabletHelper {

// Stable ASCII type keys: the JSON keys of settings.json and the catalog keys.
// Never localised.
namespace TypeKeys {
inline constexpr const char* Unknown    = "Unknown";
inline constexpr const char* Irradiated = "Irradiated";
inline constexpr const char* Breach     = "Breach";
inline constexpr const char* Delirium   = "Delirium";
inline constexpr const char* Expedition = "Expedition";
inline constexpr const char* Abyss      = "Abyss";
inline constexpr const char* Ritual     = "Ritual";
inline constexpr const char* Overseer   = "Overseer";
inline constexpr const char* Temple     = "Temple";
inline constexpr const char* Global     = "Global";  // the cross-type "All types" scope
}  // namespace TypeKeys

// The eight real tablet types in canonical order (Global is handled separately).
inline constexpr std::array<const char*, 8> kRealTypeKeys = {
    TypeKeys::Irradiated, TypeKeys::Breach, TypeKeys::Delirium, TypeKeys::Expedition,
    TypeKeys::Abyss,      TypeKeys::Ritual, TypeKeys::Overseer, TypeKeys::Temple};

inline std::string ToLowerCopy(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Allocation-free case-insensitive substring test. `needleLower` must already be
// lowercase.
inline bool ContainsCI(std::string_view hay, std::string_view needleLower) {
    if (needleLower.empty()) return true;
    if (needleLower.size() > hay.size()) return false;
    for (size_t i = 0; i + needleLower.size() <= hay.size(); ++i) {
        size_t j = 0;
        for (; j < needleLower.size(); ++j) {
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(hay[i + j])));
            if (c != needleLower[j]) break;
        }
        if (j == needleLower.size()) return true;
    }
    return false;
}

// Lowercase alphanumerics only: "TowerBreachBossChance1" -> "towerbreachbosschance1".
inline std::string NormalizeIdentifier(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value)
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Drops a trailing run of digits (tier suffix): "towerbreachbosschance1" ->
// "towerbreachbosschance".
inline std::string StripTrailingDigits(const std::string& v) {
    size_t end = v.size();
    while (end > 0 && std::isdigit(static_cast<unsigned char>(v[end - 1]))) --end;
    return v.substr(0, end);
}

// A Precursor Tablet has a metadata path under Metadata/Items/TowerAugment/
// (BreachAugment, GenericAugment, MapBossAugment, ...). The English base-type
// fallback only matters when a host leaves the path empty.
inline bool LooksLikeTablet(std::string_view path, std::string_view baseType) {
    if (ContainsCI(path, "toweraugment")) return true;
    if (ContainsCI(baseType, "precursor tablet")) return true;
    return false;
}

// Type from the metadata path (plus the base type as a safety net). Order
// matters: the first match wins. Unknown when nothing matched; the scanner then
// tries the implicit mods.
inline const char* ClassifyFromText(std::string_view path, std::string_view baseType) {
    auto has = [&](const char* kw) { return ContainsCI(path, kw) || ContainsCI(baseType, kw); };
    if (has("abyss")) return TypeKeys::Abyss;
    if (has("breach")) return TypeKeys::Breach;
    if (has("delirium")) return TypeKeys::Delirium;
    if (has("expedition")) return TypeKeys::Expedition;
    if (has("ritual")) return TypeKeys::Ritual;
    if (has("boss") || has("overseer")) return TypeKeys::Overseer;
    if (has("incursion") || has("temple") || has("vaal")) return TypeKeys::Temple;
    if (has("generic") || has("irradiated") || has("toweraugment")) return TypeKeys::Irradiated;
    return TypeKeys::Unknown;
}

// Fallback from a lowercased blob of the implicit mods' ids and stat keys.
inline const char* ClassifyFromModText(std::string_view lowerModText) {
    if (lowerModText.empty()) return TypeKeys::Unknown;
    auto has = [&](const char* kw) { return lowerModText.find(kw) != std::string_view::npos; };
    if (has("abyss")) return TypeKeys::Abyss;
    if (has("breach")) return TypeKeys::Breach;
    if (has("delirium")) return TypeKeys::Delirium;
    if (has("expedition")) return TypeKeys::Expedition;
    if (has("ritual")) return TypeKeys::Ritual;
    if (has("boss")) return TypeKeys::Overseer;
    if (has("incursion") || has("beacon") || has("vaal")) return TypeKeys::Temple;
    if (has("irradiated")) return TypeKeys::Irradiated;
    return TypeKeys::Unknown;
}

inline bool IsRealType(std::string_view key) {
    for (const char* k : kRealTypeKeys)
        if (key == k) return true;
    return false;
}

// A lang/en.json key written where it is looked up later, at drawing time (what
// PluginI18n.h's I18n::Key does; this header stays free of the SDK so the tests
// compile it alone). tools/plugin_i18n/gen.py --gate checks every Key("...")
// literal against lang/en.json.
constexpr const char* Key(const char* key) { return key; }

// Key of the type's name in the settings tab ("Breach Tablet").
inline const char* TypeNameKey(std::string_view key) {
    if (key == TypeKeys::Irradiated) return Key("type_name_irradiated");
    if (key == TypeKeys::Breach)     return Key("type_name_breach");
    if (key == TypeKeys::Delirium)   return Key("type_name_delirium");
    if (key == TypeKeys::Expedition) return Key("type_name_expedition");
    if (key == TypeKeys::Abyss)      return Key("type_name_abyss");
    if (key == TypeKeys::Ritual)     return Key("type_name_ritual");
    if (key == TypeKeys::Overseer)   return Key("type_name_overseer");
    if (key == TypeKeys::Temple)     return Key("type_name_temple");
    if (key == TypeKeys::Global)     return Key("type_name_global");
    return Key("type_name_unknown");
}

// Key of the label drawn on the tablet in game ("Breach").
inline const char* TypeShortKey(std::string_view key) {
    if (key == TypeKeys::Irradiated) return Key("type_short_irradiated");
    if (key == TypeKeys::Breach)     return Key("type_short_breach");
    if (key == TypeKeys::Delirium)   return Key("type_short_delirium");
    if (key == TypeKeys::Expedition) return Key("type_short_expedition");
    if (key == TypeKeys::Abyss)      return Key("type_short_abyss");
    if (key == TypeKeys::Ritual)     return Key("type_short_ritual");
    if (key == TypeKeys::Overseer)   return Key("type_short_overseer");
    if (key == TypeKeys::Temple)     return Key("type_short_temple");
    if (key == TypeKeys::Global)     return Key("type_name_global");
    return Key("type_short_unknown");
}

// Key of the three-letter label used when the short name does not fit the cell;
// nullptr for a type without one (drawn as "?").
inline const char* TypeAbbreviationKey(std::string_view key) {
    if (key == TypeKeys::Irradiated) return Key("type_abbr_irradiated");
    if (key == TypeKeys::Breach)     return Key("type_abbr_breach");
    if (key == TypeKeys::Delirium)   return Key("type_abbr_delirium");
    if (key == TypeKeys::Expedition) return Key("type_abbr_expedition");
    if (key == TypeKeys::Abyss)      return Key("type_abbr_abyss");
    if (key == TypeKeys::Ritual)     return Key("type_abbr_ritual");
    if (key == TypeKeys::Overseer)   return Key("type_abbr_overseer");
    if (key == TypeKeys::Temple)     return Key("type_abbr_temple");
    return nullptr;
}

}  // namespace TabletHelper
