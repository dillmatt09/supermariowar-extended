#pragma once

#include <string>
#include <string_view>
#include <vector>

/// Translations of the user interface text.
///
/// The locales are TOML files in the "locales" directory of the data, named after their
/// language code (en.toml, es.toml, fr.toml, ...):
///
///     [meta]
///     name = "Español"
///
///     [strings]
///     "Options" = "Opciones"
///     "Player {}" = "Jugador {}"
///
/// The keys are the English texts of the game. Every number in a text is matched by "{}"
/// in a key, and is put back in place of the "{}" of the translation in the same order.
/// Texts without a translation are shown as they are.
namespace l10n {

constexpr std::string_view DEFAULT_LANGUAGE = "en";

struct LocaleInfo {
    std::string code;  // file name without extension, e.g. "es"
    std::string name;  // the name of the language in itself, e.g. "Español"
};

/// The locales found in the locales directory, sorted by code
std::vector<LocaleInfo> listLocales();

/// Loads the translations of the given language; English or an unknown language clears them
void setLanguage(const std::string& code);

/// The code of the language whose translations are loaded
const std::string& language();

/// The translation of a text, or the text itself if there is none
std::string_view translate(std::string_view text);

/// The translation of a key that is not an English text itself (eg. "[icon] Player"),
/// or the fallback text if there is none
std::string_view translateKey(std::string_view key, std::string_view fallback);

/// Loads translations from the contents of a locale file (for tests)
bool loadFromString(const std::string& tomlText);

}  // namespace l10n
