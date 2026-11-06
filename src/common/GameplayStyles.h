#pragma once


enum class TeamCollisionStyle : unsigned char {
    Off,
    Assist,
    On,
};


/// Who chooses the character and team of the CPU players on the team select screen
enum class CpuSelectStyle : unsigned char {
    MainPlayer,  ///< The first human player chooses for every CPU
    AllPlayers,  ///< The CPUs are shared between the human players
};


enum class TournamentControlStyle : unsigned char {
    All,
    GameWinner,
    GameLoser,
    LeadingTeams,
    TrailingTeams,
    Random,
    RandomLoser,
    RoundRobin,
};


enum class DeathStyle : unsigned char {
    Respawn,
    Shield,
};


enum class ScoringStyle : unsigned char {
    AllKills,
    PushOnly,
};


enum class JailStyle : unsigned char {
    Classic,
    Owned,
    FreeForAll,
};


enum class ShieldStyle : unsigned char {
    NoShield,
    Soft,
    SoftWithStomp,
    Hard,
};


enum class StarStyle : unsigned char {
    Ztar,
    Shine,
    Multi,
    Random,
};


enum class WarpLockStyle : unsigned char {
    EntranceOnly,
    ExitOnly,
    EntranceAndExit,
    EntireConnection,
    AllWarps,
};


enum class BoomerangStyle : unsigned char {
    Flat,
    SMB3,
    Zelda,
    Random,
};


enum class MysteryMushroomSwapStyle : unsigned char {
    Walk,
    Blink,
    Instant,
};


enum class BonusWheelStyle : unsigned char {
    Off,
    TournamentWin,
    EveryGame,
};


enum class PowerupDropRateStyle : unsigned char {
    UseMap,
    UseGame,
    SimpleAverage,
    WeightedAverage,
};
