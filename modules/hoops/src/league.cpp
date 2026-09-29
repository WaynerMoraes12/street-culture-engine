#include "street/hoops/league.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "street/core/rng.hpp"
#include "street/core/text.hpp"

namespace street::hoops {
namespace {

// ── archetypes ───────────────────────────────────────────────────────────────
// Ratings order: inside, mid, three, free_throw, passing, handling,
//                perimeter_d, interior_d, rebounding, athleticism, stamina, clutch.
enum Arch : std::uint8_t {
    FloorGeneral,
    Sharpshooter,
    Slasher,
    TwoWayWing,
    BucketGetter,
    StretchBig,
    PostScorer,
    RimProtector,
};

constexpr std::array<Archetype, 8> kArchetypes = {{
    {"Floor General", "Runs the show: vision, handle, pull-up game.",
     {60, 70, 72, 82, 88, 86, 66, 38, 42, 70, 80, 74}},
    {"Sharpshooter", "Lives beyond the arc; defenses must chase him off screens.",
     {55, 76, 88, 88, 64, 70, 62, 38, 42, 64, 78, 76}},
    {"Slasher", "Explosive first step, attacks the rim, draws contact.",
     {86, 64, 62, 74, 64, 78, 64, 44, 52, 90, 78, 70}},
    {"Two-Way Wing", "Guards the other team's best scorer and hits open shots.",
     {70, 70, 74, 76, 62, 66, 86, 60, 58, 80, 82, 68}},
    {"Bucket Getter", "Three-level scorer who wants the ball with the game on the line.",
     {76, 84, 78, 84, 66, 86, 56, 36, 44, 78, 76, 84}},
    {"Stretch Big", "Pulls the opposing center out to the three-point line.",
     {70, 72, 78, 78, 60, 54, 54, 68, 72, 64, 74, 66}},
    {"Post Scorer", "Back-to-the-basket bruiser with soft touch and strong hands.",
     {88, 70, 38, 68, 62, 50, 48, 70, 82, 70, 74, 66}},
    {"Rim Protector", "Blocks, boards and lobs: the paint is his house.",
     {78, 48, 25, 58, 50, 36, 50, 90, 88, 78, 72, 55}},
}};

// ── rosters ──────────────────────────────────────────────────────────────────
enum class Tier : std::uint8_t { Star, Starter, Rotation, Bench };

struct PlayerSeed {
    const char* name;
    const char* nickname;
    Position position;
    int jersey;
    int age;
    Arch arch;
    Tier tier;
};

struct TeamSeed {
    const char* city;
    const char* name;
    const char* abbrev;
    term::Rgb color;
    const char* identity;
    const char* arena;
    const char* sound;
    std::array<PlayerSeed, 10> roster; // five starters, then the bench
};

using P = Position;
using T = Tier;

// Twelve cities that shaped the culture. Every name here is fictional.
const std::array<TeamSeed, 12> kTeams = {{
    {"Queens", "Bridge Poets", "QNS", {37, 99, 235}, "Pace & Space", "The Cipher", "boombap",
     {{{"Darius Greene", "Metaphor", P::PG, 3, 27, FloorGeneral, T::Star},
       {"Kenji Alvarez", "Wax", P::SG, 11, 29, Sharpshooter, T::Starter},
       {"Marcus Reyes", "Hi-Top", P::SF, 23, 25, TwoWayWing, T::Starter},
       {"Theo Whitfield", "Stanza", P::PF, 34, 28, StretchBig, T::Starter},
       {"Andre Nwosu", "Big Apple", P::C, 50, 30, RimProtector, T::Starter},
       {"Jaylen Brooks", "Subway", P::PG, 1, 23, FloorGeneral, T::Rotation},
       {"Victor Chen", "Haiku", P::SG, 8, 26, Sharpshooter, T::Rotation},
       {"Luis Santana", "Corona", P::SF, 15, 24, Slasher, T::Rotation},
       {"Malik Jenkins", "Freestyle", P::PF, 41, 31, PostScorer, T::Bench},
       {"Omar Haddad", "Verse", P::C, 44, 22, RimProtector, T::Bench}}}},
    {"Bronx", "Breakbeats", "BRX", {250, 190, 20}, "Run & Gun", "Sedgwick Hall", "boombap",
     {{{"Rico Batista", "Scratch", P::PG, 7, 26, BucketGetter, T::Star},
       {"Dante Pryor", "Needle Drop", P::SG, 13, 28, Sharpshooter, T::Starter},
       {"Isaiah Mercer", "Windmill", P::SF, 0, 24, Slasher, T::Star},
       {"Hector Quiñones", "Headspin", P::PF, 33, 29, StretchBig, T::Starter},
       {"Samuel Adeyemi", "The Wall", P::C, 21, 27, RimProtector, T::Starter},
       {"Tyrell Knox", "Top Rock", P::PG, 2, 30, FloorGeneral, T::Rotation},
       {"Carlos Peña", "Six-Step", P::SG, 10, 22, Slasher, T::Rotation},
       {"Jerome Vance", "Freeze", P::SF, 14, 25, TwoWayWing, T::Rotation},
       {"Nate Ibekwe", "Uprock", P::PF, 42, 23, PostScorer, T::Bench},
       {"Luther Grimes", "Cypher", P::C, 55, 33, RimProtector, T::Bench}}}},
    {"Atlanta", "808s", "ATL", {255, 140, 90}, "Three-Point Barrage", "Peachtree Pit", "trap",
     {{{"Quentin Mayfield", "Triplet", P::PG, 5, 25, FloorGeneral, T::Starter},
       {"Jalen Pettaway", "Snare", P::SG, 30, 27, Sharpshooter, T::Star},
       {"Trevon Maddox", "Sub Bass", P::SF, 9, 26, TwoWayWing, T::Starter},
       {"Cedric Oyelaran", "Rolls", P::PF, 4, 24, StretchBig, T::Star},
       {"Marvin Tolliver", "Boom", P::C, 12, 29, RimProtector, T::Starter},
       {"Deshawn Crump", "Hi-Hat", P::PG, 3, 22, Sharpshooter, T::Rotation},
       {"Kordell Banks", "Clap", P::SG, 25, 28, Sharpshooter, T::Rotation},
       {"Avery Lusk", "Pitch", P::SF, 17, 23, Slasher, T::Rotation},
       {"Brandon Echols", "Reverb", P::PF, 35, 30, StretchBig, T::Bench},
       {"Ellis Sutton", "Low End", P::C, 52, 26, RimProtector, T::Bench}}}},
    {"Houston", "Candy Paint", "HOU", {168, 85, 247}, "Grit & Grind", "Third Ward Dome", "trap",
     {{{"Terrence Holloway", "Slow Motion", P::PG, 10, 31, FloorGeneral, T::Starter},
       {"Marquise Dupree", "Swangin'", P::SG, 22, 26, Slasher, T::Starter},
       {"Calvin Mosby", "Chrome", P::SF, 6, 28, TwoWayWing, T::Starter},
       {"Rodney Thibodeaux", "Big Tex", P::PF, 32, 27, PostScorer, T::Star},
       {"Jamal Nnamdi", "Slab", P::C, 45, 29, RimProtector, T::Starter},
       {"Bryce Lemons", "Pop Trunk", P::PG, 0, 24, BucketGetter, T::Rotation},
       {"Fernando Garza", "Elbows", P::SG, 8, 27, Sharpshooter, T::Rotation},
       {"Keon Rideau", "Candy", P::SF, 19, 25, TwoWayWing, T::Rotation},
       {"Darnell Fontenot", "Screw", P::PF, 40, 32, StretchBig, T::Bench},
       {"Otis Broussard", "Wood Grain", P::C, 54, 28, RimProtector, T::Bench}}}},
    {"Compton", "Lowriders", "CPT", {20, 184, 166}, "Paint Beasts", "Rosecrans Arena", "lofi",
     {{{"Andre Velasquez", "Hydraulics", P::PG, 1, 28, FloorGeneral, T::Starter},
       {"DeMarco Price", "Three-Wheel", P::SG, 2, 25, Slasher, T::Star},
       {"Javier Montoya", "Switches", P::SF, 18, 27, TwoWayWing, T::Starter},
       {"Lamont Truitt", "Bounce", P::PF, 31, 29, PostScorer, T::Starter},
       {"Kofi Asante", "Big Body", P::C, 34, 26, PostScorer, T::Star},
       {"Ramon Ibarra", "Candy Apple", P::PG, 11, 24, BucketGetter, T::Rotation},
       {"Terrell Hawkins", "Dayton", P::SG, 24, 30, Sharpshooter, T::Rotation},
       {"Cornell Tate", "Pinstripe", P::SF, 16, 22, Slasher, T::Rotation},
       {"Shaun Whitmore", "Hopper", P::PF, 43, 27, RimProtector, T::Bench},
       {"Gilberto Rojas", "Low Low", P::C, 50, 31, RimProtector, T::Bench}}}},
    {"Oakland", "Sideshow", "OAK", {22, 163, 74}, "Run & Gun", "Lake Merritt Arena", "boombap",
     {{{"Khalil Watts", "Ghost Ride", P::PG, 0, 24, BucketGetter, T::Star},
       {"Rashad Tillman", "Donuts", P::SG, 3, 27, Sharpshooter, T::Starter},
       {"Mateo Fuentes", "Scraper", P::SF, 15, 25, Slasher, T::Starter},
       {"Deion Pickett", "Hyphy", P::PF, 9, 28, StretchBig, T::Starter},
       {"Obi Mensah", "Skyline", P::C, 33, 26, RimProtector, T::Starter},
       {"Tariq Hollins", "Go Dumb", P::PG, 5, 23, FloorGeneral, T::Rotation},
       {"Nico Tran", "Turf", P::SG, 7, 25, Sharpshooter, T::Rotation},
       {"Dorian Pace", "Stunna", P::SF, 12, 24, Slasher, T::Rotation},
       {"Marlon Kirby", "Whip", P::PF, 41, 29, PostScorer, T::Bench},
       {"Grant Ellison", "Foothill", P::C, 44, 32, RimProtector, T::Bench}}}},
    {"Chicago", "Footwork", "CHI", {56, 189, 248}, "Iso Heavy", "South Side Fieldhouse", "drill",
     {{{"Devonte Sykes", "Juke", P::PG, 1, 26, FloorGeneral, T::Star},
       {"Cortez Lattimore", "Windy", P::SG, 14, 29, Sharpshooter, T::Starter},
       {"Marcellus Boone", "Footwork", P::SF, 8, 25, BucketGetter, T::Star},
       {"Jarrett Kowalski", "Deep Dish", P::PF, 40, 30, StretchBig, T::Starter},
       {"Emmanuel Oduya", "Lakeshore", P::C, 50, 27, RimProtector, T::Starter},
       {"Terrance Wade", "Bop", P::PG, 0, 22, BucketGetter, T::Rotation},
       {"Ricky Delgado", "Red Line", P::SG, 21, 26, TwoWayWing, T::Rotation},
       {"Lionel Parrish", "El Train", P::SF, 11, 24, Slasher, T::Rotation},
       {"Byron Ashby", "Blizzard", P::PF, 32, 31, PostScorer, T::Bench},
       {"Stanislav Novak", "Mag Mile", P::C, 55, 28, RimProtector, T::Bench}}}},
    {"Detroit", "Crate Diggers", "DET", {156, 163, 175}, "Lockdown D", "Eight Mile Arena", "lofi",
     {{{"Curtis Bellamy", "B-Side", P::PG, 4, 29, FloorGeneral, T::Starter},
       {"Anton Grier", "Dusty Fingers", P::SG, 20, 31, Sharpshooter, T::Starter},
       {"Xavier Holt", "Lockbox", P::SF, 7, 26, TwoWayWing, T::Star},
       {"Roland Pruitt", "Motor", P::PF, 35, 28, PostScorer, T::Starter},
       {"Kwame Owusu", "Vinyl", P::C, 15, 27, RimProtector, T::Starter},
       {"Julian Voss", "Loop", P::PG, 2, 24, FloorGeneral, T::Rotation},
       {"Tavon Ricks", "Crossfader", P::SG, 13, 25, TwoWayWing, T::Rotation},
       {"Micah Stroud", "Sample", P::SF, 22, 23, Sharpshooter, T::Rotation},
       {"Lorenzo Pike", "Chop", P::PF, 42, 30, StretchBig, T::Bench},
       {"Everett Cole", "Deep Cut", P::C, 51, 34, RimProtector, T::Bench}}}},
    {"Memphis", "Phonk", "MEM", {129, 140, 248}, "Grit & Grind", "Orange Mound Coliseum", "trap",
     {{{"Demetrius Bland", "Cowbell", P::PG, 11, 27, FloorGeneral, T::Starter},
       {"Jaquan Mims", "Mixtape", P::SG, 3, 25, BucketGetter, T::Star},
       {"Rodell Harmon", "Grit", P::SF, 24, 29, TwoWayWing, T::Starter},
       {"Leon Tubbs", "Bluff City", P::PF, 33, 30, PostScorer, T::Starter},
       {"Moussa Diallo", "Blackout", P::C, 13, 26, RimProtector, T::Star},
       {"Kendrick Ware", "Tape Hiss", P::PG, 6, 23, FloorGeneral, T::Rotation},
       {"Alonzo Givens", "Cassette", P::SG, 9, 28, Sharpshooter, T::Rotation},
       {"Floyd Mabry", "Beale", P::SF, 21, 26, TwoWayWing, T::Rotation},
       {"Cleveland Hurd", "Barbecue", P::PF, 44, 32, PostScorer, T::Bench},
       {"Tobias Ekwueme", "Drift", P::C, 50, 24, RimProtector, T::Bench}}}},
    {"London", "Roadmen", "LDN", {239, 68, 68}, "Pace & Space", "Brixton Arena", "drill",
     {{{"Kieran Osei", "Oyster", P::PG, 8, 26, FloorGeneral, T::Starter},
       {"Reece Ashworth", "Grime", P::SG, 23, 27, Sharpshooter, T::Star},
       {"Tyrese Adu", "Peckham", P::SF, 10, 24, Slasher, T::Starter},
       {"Oliver Hargreaves", "Big Ben", P::PF, 31, 29, StretchBig, T::Starter},
       {"Chidi Okonjo", "Tower", P::C, 14, 27, RimProtector, T::Starter},
       {"Alfie Clarke", "Tube", P::PG, 3, 22, FloorGeneral, T::Rotation},
       {"Jordan Whitlock", "Night Bus", P::SG, 12, 25, Sharpshooter, T::Rotation},
       {"Kwesi Danso", "Roadman", P::SF, 17, 26, TwoWayWing, T::Rotation},
       {"Harry Pembroke", "Garage", P::PF, 40, 30, StretchBig, T::Bench},
       {"Femi Adebola", "Brixton", P::C, 44, 28, RimProtector, T::Bench}}}},
    {"São Paulo", "Grafiteiros", "SAO", {236, 72, 153}, "Jogo Bonito", "Arena Vila Madalena",
     "boombap",
     {{{"Thiago Nascimento", "Maestro", P::PG, 10, 28, FloorGeneral, T::Star},
       {"Rafael Moreira", "Garoa", P::SG, 7, 26, Sharpshooter, T::Starter},
       {"Vinícius Araújo", "Pixo", P::SF, 11, 24, Slasher, T::Star},
       {"Bruno Teixeira", "Marginal", P::PF, 15, 29, StretchBig, T::Starter},
       {"Leandro Cardoso", "Paredão", P::C, 13, 30, RimProtector, T::Starter},
       {"Caio Ribeiro", "Sampa", P::PG, 5, 23, BucketGetter, T::Rotation},
       {"Diego Fernandes", "Spray", P::SG, 9, 27, Sharpshooter, T::Rotation},
       {"Wellington Barbosa", "Quebrada", P::SF, 21, 25, TwoWayWing, T::Rotation},
       {"Guilherme Rocha", "Mano", P::PF, 33, 31, PostScorer, T::Bench},
       {"Anderson Lima", "Viaduto", P::C, 44, 22, RimProtector, T::Bench}}}},
    {"Rio de Janeiro", "Tamborzão", "RIO", {132, 204, 22}, "Three-Point Barrage",
     "Arena Madureira", "funk",
     {{{"Gabriel Souza", "Ginga", P::PG, 8, 27, FloorGeneral, T::Starter},
       {"Mateus Carvalho", "Passinho", P::SG, 14, 25, Sharpshooter, T::Star},
       {"Felipe Almeida", "Malandro", P::SF, 6, 28, BucketGetter, T::Starter},
       {"Rodrigo Pereira", "Pancadão", P::PF, 12, 30, StretchBig, T::Starter},
       {"Jefferson Santos", "Cristo", P::C, 16, 26, RimProtector, T::Starter},
       {"Lucas Oliveira", "Carioca", P::PG, 4, 22, Sharpshooter, T::Rotation},
       {"Igor Gomes", "Batidão", P::SG, 20, 24, Sharpshooter, T::Rotation},
       {"Renan Costa", "Samba", P::SF, 9, 26, Slasher, T::Rotation},
       {"Marcelo Dias", "Maracanã", P::PF, 31, 32, StretchBig, T::Bench},
       {"Wesley Monteiro", "Baile", P::C, 45, 23, PostScorer, T::Bench}}}},
}};

int tier_offset(Tier tier) {
    switch (tier) {
    case Tier::Star: return 11;
    case Tier::Starter: return 2;
    case Tier::Rotation: return -3;
    case Tier::Bench: return -8;
    }
    return 0;
}

/// Archetype template + tier + age curve + small per-player variation. The
/// variation comes from a generator seeded by the player's name, so each
/// player's ratings are stable even if rosters are reordered.
Ratings make_ratings(const PlayerSeed& seed) {
    Ratings r = kArchetypes[seed.arch].base;
    Rng rng(hash_seed(seed.name), 0x400B5);
    const int offset = tier_offset(seed.tier);
    const auto adjust = [&](int& value, int extra) {
        value = std::clamp(value + offset + extra + rng.range(-4, 4), 25, 99);
    };
    const int age_over = std::max(0, seed.age - 29); // legs fade after 29...
    const int youth = std::max(0, 24 - seed.age);    // ...and the kids are still learning
    const int star_poise = seed.tier == Tier::Star ? 4 : 0;
    adjust(r.inside, 0);
    adjust(r.mid, age_over / 2);
    adjust(r.three, 0);
    adjust(r.free_throw, age_over / 2);
    adjust(r.passing, age_over - youth);
    adjust(r.handling, 0);
    adjust(r.perimeter_d, -age_over);
    adjust(r.interior_d, 0);
    adjust(r.rebounding, 0);
    adjust(r.athleticism, youth - 2 * age_over);
    adjust(r.stamina, youth - age_over);
    adjust(r.clutch, star_poise + age_over - 2 * youth);
    return r;
}

} // namespace

std::span<const Archetype> archetypes() noexcept { return kArchetypes; }

std::vector<Team> default_league() {
    std::vector<Team> league;
    league.reserve(kTeams.size());
    std::uint32_t team_id = 1;
    for (const auto& seed : kTeams) {
        Team team;
        team.id = team_id;
        team.city = seed.city;
        team.name = seed.name;
        team.abbrev = seed.abbrev;
        team.color = seed.color;
        team.identity = seed.identity;
        team.arena = seed.arena;
        team.sound = seed.sound;
        std::uint32_t slot = 1;
        for (const auto& ps : seed.roster) {
            Player p;
            p.id = team_id * 100 + slot++;
            p.name = ps.name;
            p.nickname = ps.nickname;
            p.position = ps.position;
            p.jersey = ps.jersey;
            p.age = ps.age;
            p.archetype = std::string(kArchetypes[ps.arch].name);
            p.ratings = make_ratings(ps);
            p.star = ps.tier == Tier::Star;
            team.roster.push_back(std::move(p));
        }
        league.push_back(std::move(team));
        ++team_id;
    }
    return league;
}

const Team* find_team(const std::vector<Team>& league, std::string_view abbrev_or_name) {
    const std::string_view key = text::trim(abbrev_or_name);
    if (key.empty()) return nullptr;
    for (const auto& team : league) {
        if (text::iequals(team.abbrev, key) || text::iequals(team.name, key) ||
            text::iequals(team.city, key) || text::iequals(team.full_name(), key))
            return &team;
    }
    // Accent-insensitive second pass so "Sao Paulo" and "tamborzao" work on any keyboard.
    const std::string folded = text::fold_accents(key);
    for (const auto& team : league) {
        if (text::iequals(text::fold_accents(team.name), folded) ||
            text::iequals(text::fold_accents(team.city), folded) ||
            text::iequals(text::fold_accents(team.full_name()), folded))
            return &team;
    }
    return nullptr;
}

} // namespace street::hoops
