#include "doctest.h"

#include "Localization.h"

#include <string>

namespace {
// Compared as std::string, which doctest can print without <ostream> (MSVC needs it for string_view)
std::string tr(std::string_view text) { return std::string(l10n::translate(text)); }
}

TEST_CASE("Localization translates texts, numbers and padded texts")
{
    REQUIRE(l10n::loadFromString(R"(
[meta]
name = "Español"

[strings]
"Options" = "Opciones"
"Player {}" = "Jugador {}"
"{} Seconds" = "{} segundos"
"{}x Players - {}" = "{}x Jugadores - {}"
"Player" = "Jugador"
)"));

    CHECK(tr("Options") == "Opciones");
    CHECK(tr("Player 7") == "Jugador 7");
    CHECK(tr("2.5 Seconds") == "2.5 segundos");
    CHECK(tr("2x Players - 3") == "2x Jugadores - 3");
    CHECK(tr("Player ") == "Jugador ");
    CHECK(tr("Mario") == "Mario");
    CHECK(tr("Player 7 Wins!") == "Player 7 Wins!");
    CHECK(tr("") == "");

    l10n::setLanguage("en");
    CHECK(tr("Options") == "Options");
}
