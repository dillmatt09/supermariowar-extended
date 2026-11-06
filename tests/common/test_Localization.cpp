#include "doctest.h"

#include "Localization.h"

#include <string>

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

    CHECK(l10n::translate("Options") == "Opciones");
    CHECK(l10n::translate("Player 7") == "Jugador 7");
    CHECK(l10n::translate("2.5 Seconds") == "2.5 segundos");
    CHECK(l10n::translate("2x Players - 3") == "2x Jugadores - 3");
    CHECK(l10n::translate("Player ") == "Jugador ");
    CHECK(l10n::translate("Mario") == "Mario");
    CHECK(l10n::translate("Player 7 Wins!") == "Player 7 Wins!");
    CHECK(l10n::translate("") == "");

    l10n::setLanguage("en");
    CHECK(l10n::translate("Options") == "Options");
}
