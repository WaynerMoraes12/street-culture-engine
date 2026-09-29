#pragma once
// Events that flow through the culture layer's EventBus.
//
// The engines never talk to each other directly. The Universe publishes what
// happened (a game went final, a week ended) and small bridge objects turn
// those facts into consequences in other engines: hype shocks on the sneaker
// exchange, ghostwritten verses, synthesized anthems. Anything else — the CLI,
// a test, a future module — can subscribe to the same stream.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "street/hoops/game.hpp"

namespace street::culture {

/// A regular-season game went final. `result` is only valid during dispatch.
struct GameFinal {
    int day = 0;
    const hoops::GameResult* result = nullptr;
};

/// A performance big enough to make headlines (35+ points, a triple-double, a monster Game Score).
struct BigNight {
    int day = 0;
    std::uint32_t player_id = 0;
    std::uint32_t team_id = 0;
    std::uint32_t opponent_id = 0;
    hoops::PlayerLine line;
    bool won = false;
    std::string headline;
};

/// A performance moved a signature sneaker's hype on the exchange.
struct HypeMove {
    int day = 0;
    std::uint32_t player_id = 0;
    std::string sku;
    double magnitude = 0.0; ///< relative fair-value shock, e.g. +0.18
    std::string headline;
};

/// The best player of a culture week (highest average Game Score).
struct PlayerOfTheWeek {
    int week = 0;
    std::uint32_t player_id = 0;
    std::uint32_t team_id = 0;
    int games = 0;
    double ppg = 0.0, rpg = 0.0, apg = 0.0;
    double game_score = 0.0;
    int best_points = 0;
};

/// The ghostwritten verse for a Player of the Week, with its analysis summary.
struct VerseWritten {
    int week = 0;
    std::uint32_t player_id = 0;
    std::string text;
    std::string scheme;          ///< e.g. "AABB"
    double rhyme_density = 0.0;
    int syllables = 0;
    std::string rendered;        ///< bars::render output (colored when colors are on)
};

/// The anthem synthesized for a Player of the Week.
struct AnthemDropped {
    int week = 0;
    std::uint32_t player_id = 0;
    std::string title;
    std::string style;           ///< beats style id: boombap, trap, lofi, drill, funk
    int bpm = 0;
    std::string key;
    std::vector<std::string> chords;
    double seconds = 0.0;
    std::optional<std::filesystem::path> file; ///< set when a mixtape folder was given
    std::string waveform;        ///< compact render of the audio
};

} // namespace street::culture
