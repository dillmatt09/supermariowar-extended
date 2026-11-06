#include "Localization.h"

#include "path.h"

#include "toml.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace l10n {
namespace {
std::string g_language(DEFAULT_LANGUAGE);
struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};
using StringMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

StringMap g_strings;
// Results of translate() for texts that went through number matching, so that they don't need
// to be rebuilt every frame. Also keeps the returned views valid.
StringMap g_cache;

constexpr size_t MAX_CACHE_SIZE = 4096;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

/// Replaces every number (digits, optionally with a decimal part) with "{}"
std::string makeTemplate(std::string_view text, std::vector<std::string_view>& numbers)
{
    std::string result;
    result.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (!isDigit(text[i])) {
            result += text[i++];
            continue;
        }
        const size_t start = i;
        while (i < text.size() && isDigit(text[i]))
            i++;
        if (i + 1 < text.size() && text[i] == '.' && isDigit(text[i + 1])) {
            i++;
            while (i < text.size() && isDigit(text[i]))
                i++;
        }
        numbers.push_back(text.substr(start, i - start));
        result += "{}";
    }
    return result;
}

std::string fillTemplate(std::string_view translation, const std::vector<std::string_view>& numbers)
{
    std::string result;
    size_t next = 0;
    size_t i = 0;
    while (i < translation.size()) {
        if (translation.compare(i, 2, "{}") == 0 && next < numbers.size()) {
            result += numbers[next++];
            i += 2;
        } else {
            result += translation[i++];
        }
    }
    return result;
}

const std::string* findExact(std::string_view text)
{
    auto it = g_strings.find(text);
    return it == g_strings.end() ? nullptr : &it->second;
}

bool parse(const toml::value& root, StringMap& strings)
{
    if (!root.contains("strings") || !root.at("strings").is_table())
        return false;
    for (const auto& [key, value] : root.at("strings").as_table()) {
        if (value.is_string() && !value.as_string().empty())
            strings.emplace(key, value.as_string());
    }
    return true;
}

fs::path localesDir()
{
    return fs::path(convertPath("locales/"));
}
}  // namespace

std::vector<LocaleInfo> listLocales()
{
    std::vector<LocaleInfo> locales;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(localesDir(), ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".toml")
            continue;

        LocaleInfo info;
        info.code = entry.path().stem().string();
        info.name = info.code;
        try {
            const toml::value root = toml::parse(entry.path().string());
            if (root.contains("meta") && root.at("meta").contains("name"))
                info.name = root.at("meta").at("name").as_string();
        } catch (const std::exception& e) {
            std::cerr << "[l10n] Could not read " << entry.path().string() << ": " << e.what() << '\n';
            continue;
        }
        locales.push_back(std::move(info));
    }
    std::sort(locales.begin(), locales.end(),
        [](const LocaleInfo& a, const LocaleInfo& b) { return a.code < b.code; });
    return locales;
}

void setLanguage(const std::string& code)
{
    g_strings.clear();
    g_cache.clear();
    g_language = std::string(DEFAULT_LANGUAGE);

    if (code.empty() || code == DEFAULT_LANGUAGE)
        return;
    if (code.find_first_of("/\\.") != std::string::npos)
        return;

    const fs::path file = localesDir() / (code + ".toml");
    try {
        StringMap strings;
        if (!parse(toml::parse(file.string()), strings)) {
            std::cerr << "[l10n] No [strings] table in " << file.string() << '\n';
            return;
        }
        g_strings = std::move(strings);
        g_language = code;
    } catch (const std::exception& e) {
        std::cerr << "[l10n] Could not load " << file.string() << ": " << e.what() << '\n';
    }
}

const std::string& language()
{
    return g_language;
}

bool loadFromString(const std::string& tomlText)
{
    g_strings.clear();
    g_cache.clear();
    try {
        std::istringstream stream(tomlText);
        return parse(toml::parse(stream, "locale"), g_strings);
    } catch (const std::exception&) {
        return false;
    }
}

std::string_view translateKey(std::string_view key, std::string_view fallback)
{
    if (const std::string* translation = findExact(key))
        return *translation;
    return fallback;
}

std::string_view translate(std::string_view text)
{
    if (g_strings.empty() || text.empty())
        return text;

    if (const std::string* exact = findExact(text))
        return *exact;

    // Texts are sometimes drawn with padding spaces around them
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
        return text;
    const size_t last = text.find_last_not_of(' ');
    const bool padded = first > 0 || last + 1 < text.size();
    const bool hasDigit = std::any_of(text.begin(), text.end(), isDigit);
    if (!padded && !hasDigit)
        return text;

    if (auto it = g_cache.find(text); it != g_cache.end())
        return it->second;

    const std::string_view core = text.substr(first, last - first + 1);
    std::string result;
    bool found = false;
    if (const std::string* exact = findExact(core)) {
        result = *exact;
        found = true;
    } else if (hasDigit) {
        std::vector<std::string_view> numbers;
        if (const std::string* translation = findExact(makeTemplate(core, numbers))) {
            result = fillTemplate(*translation, numbers);
            found = true;
        }
    }
    if (!found)
        result = std::string(core);
    result = std::string(text.substr(0, first)) + result + std::string(text.substr(last + 1));

    if (g_cache.size() >= MAX_CACHE_SIZE)
        g_cache.clear();
    return g_cache.emplace(std::string(text), std::move(result)).first->second;
}

}  // namespace l10n
