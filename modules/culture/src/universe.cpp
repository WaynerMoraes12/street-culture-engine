#include "street/culture/universe.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <stdexcept>

#include "street/beats/beats.hpp"
#include "street/core/rng.hpp"
#include "street/core/stats.hpp"
#include "street/core/text.hpp"
#include "street/hoops/league.hpp"

namespace street::culture {
namespace {

std::uint64_t derive(std::uint64_t seed, std::uint64_t salt) noexcept {
    return mix64(seed ^ mix64(salt));
}

std::string moniker(const hoops::Player& player) {
    return player.nickname.empty() ? player.last_name() : player.nickname;
}

std::string big_night_headline(const hoops::Player& player, const hoops::PlayerLine& line,
                               const hoops::Team& opponent, bool won) {
    const int doubles = (line.pts >= 10) + (line.reb() >= 10) + (line.ast >= 10);
    const std::string result = won ? "in a win over" : "in a loss to";
    if (doubles >= 3)
        return std::format("{} triple-double: {} pts, {} reb, {} ast {} {}", player.short_name(),
                           line.pts, line.reb(), line.ast, result, opponent.abbrev);
    return std::format("{} erupts for {} ({}-{} FG, {} reb, {} ast) {} {}", player.short_name(),
                       line.pts, line.fgm, line.fga, line.reb(), line.ast, result, opponent.abbrev);
}

} // namespace

// The bridges between engines. Each reaction is one subscription, so the whole
// "who influences whom" map of the universe reads top to bottom right here.
struct Universe::Bridges {
    explicit Bridges(Universe& u) {
        // hoops → kicks: box scores become headlines and hype shocks.
        u.bus_.subscribe<GameFinal>([&u](const GameFinal& e) { hype_desk(u, e); });
        // hoops → bars: the Player of the Week gets a verse, analyzed on the spot.
        u.bus_.subscribe<PlayerOfTheWeek>([&u](const PlayerOfTheWeek& e) { booth(u, e); });
        // hoops → beats: ...and an anthem in their city's sound.
        if (u.config_.anthems)
            u.bus_.subscribe<PlayerOfTheWeek>([&u](const PlayerOfTheWeek& e) { studio(u, e); });

        // The weekly report is just another subscriber.
        u.bus_.subscribe<BigNight>([&u](const BigNight& e) {
            if (u.current_) u.current_->big_nights.push_back(e);
        });
        u.bus_.subscribe<HypeMove>([&u](const HypeMove& e) {
            if (u.current_) u.current_->hype_moves.push_back(e);
        });
        u.bus_.subscribe<VerseWritten>([&u](const VerseWritten& e) {
            if (u.current_) u.current_->verse = e;
        });
        u.bus_.subscribe<AnthemDropped>([&u](const AnthemDropped& e) {
            if (u.current_) u.current_->anthem = e;
            u.mixtape_.push_back(e);
        });
    }

    static void hype_desk(Universe& u, const GameFinal& e) {
        const hoops::GameResult& game = *e.result;
        for (const int side : {hoops::kHome, hoops::kAway}) {
            const bool won = game.winner_side() == side;
            const hoops::Team& opponent = u.season_.team(game.team_id(1 - side));
            for (const hoops::PlayerLine& line : game.box[static_cast<std::size_t>(side)]) {
                if (line.minutes <= 0.0) continue;
                const hoops::Player& player = u.season_.player(line.player_id);
                if (is_big_night(line)) {
                    u.bus_.publish(BigNight{e.day, player.id, game.team_id(side), opponent.id, line,
                                            won, big_night_headline(player, line, opponent, won)});
                }

                const SignatureShoe* shoe = u.signature_of(player.id);
                if (shoe == nullptr) continue;
                const auto index = static_cast<std::size_t>(shoe - u.signatures_.data());
                u.todays_game_score_[index] = line.game_score();

                // The season totals already include tonight; the market's expectation is
                // the average of the games before it (or a rating prior early on).
                const hoops::PlayerSeason& season = u.season_.player_stats(player.id);
                const double expected =
                    season.games > 3
                        ? (season.game_score_total - line.game_score()) / (season.games - 1)
                        : prior_game_score(player);
                const auto impulse = performance_impulse(line, expected, won, moniker(player),
                                                         shoe->sneaker.name);
                if (!impulse) continue;
                u.market_.shock(shoe->sneaker.sku, impulse->magnitude, impulse->headline);
                u.bus_.publish(HypeMove{e.day, player.id, shoe->sneaker.sku, impulse->magnitude,
                                        impulse->headline});
            }
        }
    }

    static void booth(Universe& u, const PlayerOfTheWeek& e) {
        const hoops::Player& player = u.season_.player(e.player_id);
        const hoops::Team& team = u.season_.team(e.team_id);
        const SignatureShoe* shoe = u.signature_of(player.id);
        const std::uint64_t seed = derive(u.config_.seed, 0xB00711ull * 1000 + e.week * 97 + player.id);

        bars::VerseRequest request;
        request.subject = moniker(player);
        request.team = team.name;
        if (shoe != nullptr) {
            request.shoe = shoe->sneaker.name;
        } else { // everyone else laces up a classic from the exchange
            const auto& catalog = kicks::classic_catalog();
            request.shoe = catalog[seed % catalog.size()].name;
        }
        request.points = e.best_points;
        request.language = u.config_.language;
        request.seed = seed;
        request.lines = 4;

        VerseWritten verse;
        verse.week = e.week;
        verse.player_id = player.id;
        verse.text = bars::ghostwrite(request);
        const bars::Analysis analysis = bars::analyze(verse.text, u.config_.language);
        verse.scheme = analysis.scheme;
        verse.rhyme_density = analysis.rhyme_density;
        verse.syllables = analysis.total_syllables;
        bars::RenderOptions options;
        options.show_stats = false;
        options.width = 88; // fits inside the Player of the Week panel
        verse.rendered = bars::render(analysis, options);
        u.bus_.publish(verse);
    }

    static void studio(Universe& u, const PlayerOfTheWeek& e) {
        const hoops::Player& player = u.season_.player(e.player_id);
        const hoops::Team& team = u.season_.team(e.team_id);

        beats::BeatSpec spec;
        spec.style = beats::parse_style(team.sound).value_or(beats::Style::BoomBap);
        spec.bars = u.config_.anthem_bars;
        spec.seed = derive(u.config_.seed, 0xA17E3ull * 1000 + e.week * 97 + player.id);
        spec.title = moniker(player) + " Anthem";
        const auto earlier = std::count_if(u.mixtape_.begin(), u.mixtape_.end(),
                                           [&](const AnthemDropped& a) { return a.player_id == player.id; });
        if (earlier == 1) spec.title += " (Remix)";
        if (earlier > 1) spec.title += std::format(" (Remix {})", earlier);
        const beats::Beat beat = beats::generate(spec);

        AnthemDropped anthem;
        anthem.week = e.week;
        anthem.player_id = player.id;
        anthem.title = spec.title;
        anthem.style = std::string(beats::info(beat.spec.style).id);
        anthem.bpm = beat.spec.bpm;
        anthem.key = beat.spec.key;
        anthem.chords = beat.chords;
        anthem.seconds = beat.audio.seconds();
        anthem.waveform = beats::render_waveform(beat.audio, 64, 2);
        if (u.config_.mixtape) {
            std::filesystem::create_directories(*u.config_.mixtape);
            const auto file = *u.config_.mixtape / std::format("wk{:02}-{}-{}.wav", e.week,
                                                               text::slug(moniker(player)), anthem.style);
            beats::write_wav(file, beat.audio);
            anthem.file = file;
        }
        u.bus_.publish(anthem);
    }
};

Universe::Universe(Config config)
    : config_(std::move(config)),
      season_(hoops::default_league(), config_.seed),
      market_(kicks::classic_catalog(), derive(config_.seed, 0x4B1C5ull)) {
    if (config_.days_per_week < 1) throw std::invalid_argument("days_per_week must be at least 1");
    if (config_.weeks < 0) throw std::invalid_argument("weeks cannot be negative");

    // Opening night: the league's best players each drop a signature shoe.
    std::vector<std::string> taken = market_.skus();
    for (const hoops::Player* athlete : signature_athletes(season_.teams(), config_.signatures)) {
        SignatureShoe shoe = design_signature(*athlete, season_.team_of(athlete->id), taken);
        taken.push_back(shoe.sneaker.sku);
        market_.release(shoe.sneaker);
        signatures_.push_back(std::move(shoe));
    }
    performance_log_.resize(signatures_.size());
    todays_game_score_.resize(signatures_.size());
    bridges_ = std::make_unique<Bridges>(*this);
}

Universe::~Universe() = default;

int Universe::total_weeks() const noexcept {
    const int full = (season_.total_days() + config_.days_per_week - 1) / config_.days_per_week;
    return config_.weeks > 0 ? std::min(config_.weeks, full) : full;
}

bool Universe::done() const noexcept {
    return week_ >= total_weeks() || season_.regular_season_done();
}

const SignatureShoe* Universe::signature_of(std::uint32_t player_id) const noexcept {
    const auto it = std::find_if(signatures_.begin(), signatures_.end(),
                                 [&](const SignatureShoe& s) { return s.player_id == player_id; });
    return it == signatures_.end() ? nullptr : &*it;
}

void Universe::play_day(WeekReport& report) {
    const int day = season_.day() + 1;
    std::fill(todays_game_score_.begin(), todays_game_score_.end(), std::nullopt);

    std::vector<hoops::GameResult> results = season_.play_next_day(hoops::GameOptions::fast());
    for (const auto& game : results) bus_.publish(GameFinal{day, &game});

    // Tonight's shocks are already queued, so this session trades on them.
    market_.step_day();

    for (std::size_t i = 0; i < signatures_.size(); ++i) {
        if (!todays_game_score_[i]) continue;
        const auto& candles = market_.candles(signatures_[i].sneaker.sku);
        if (candles.size() < 2) continue;
        const double previous = static_cast<double>(candles[candles.size() - 2].close);
        const double today = static_cast<double>(candles.back().close);
        if (previous > 0.0) performance_log_[i].emplace_back(*todays_game_score_[i], today / previous - 1.0);
    }
    for (auto& game : results) report.games.push_back(std::move(game));
}

std::optional<PlayerOfTheWeek>
Universe::pick_player_of_the_week(const WeekReport& report) const {
    struct Totals {
        std::uint32_t team_id = 0;
        int games = 0, pts = 0, reb = 0, ast = 0, best = 0;
        double game_score = 0.0;
    };
    std::map<std::uint32_t, Totals> totals; // ordered: ties resolve by player id
    int most_games = 0;
    for (const auto& game : report.games) {
        for (const int side : {hoops::kHome, hoops::kAway}) {
            for (const auto& line : game.box[static_cast<std::size_t>(side)]) {
                if (line.minutes <= 0.0) continue;
                Totals& t = totals[line.player_id];
                t.team_id = game.team_id(side);
                ++t.games;
                t.pts += line.pts;
                t.reb += line.reb();
                t.ast += line.ast;
                t.best = std::max(t.best, line.pts);
                t.game_score += line.game_score();
                most_games = std::max(most_games, t.games);
            }
        }
    }
    // Only players who played (nearly) every game of the week are eligible.
    const int min_games = std::max(1, most_games - 1);
    const std::pair<const std::uint32_t, Totals>* best = nullptr;
    for (const auto& entry : totals) {
        if (entry.second.games < min_games) continue;
        const double avg = entry.second.game_score / entry.second.games;
        if (best == nullptr || avg > best->second.game_score / best->second.games) best = &entry;
    }
    if (best == nullptr) return std::nullopt;

    const Totals& t = best->second;
    const double g = t.games;
    return PlayerOfTheWeek{report.week, best->first, t.team_id, t.games, t.pts / g, t.reb / g,
                           t.ast / g, t.game_score / g, t.best};
}

WeekReport Universe::play_week() {
    if (done()) throw std::logic_error("Universe::play_week: no weeks left to play");
    WeekReport report;
    report.week = ++week_;
    report.first_day = season_.day() + 1;
    current_ = &report;
    for (int d = 0; d < config_.days_per_week && !season_.regular_season_done(); ++d) play_day(report);
    report.last_day = season_.day();

    report.player_of_the_week = pick_player_of_the_week(report);
    if (report.player_of_the_week) {
        const auto& potw = *report.player_of_the_week;
        bus_.publish(potw); // the booth and the studio go to work
        if (const SignatureShoe* shoe = signature_of(potw.player_id)) {
            const std::string headline =
                std::format("{} named Player of the Week", moniker(season_.player(potw.player_id)));
            market_.shock(shoe->sneaker.sku, 0.06, headline);
            bus_.publish(HypeMove{report.last_day, potw.player_id, shoe->sneaker.sku, 0.06, headline});
        }
    }
    current_ = nullptr;
    return report;
}

Recap Universe::finish() {
    Recap recap;
    recap.weeks_played = week_;
    recap.days_played = season_.day();
    recap.season_complete = season_.regular_season_done();
    if (recap.season_complete && config_.playoff_teams >= 2)
        recap.playoffs = season_.play_playoffs(config_.playoff_teams);

    for (std::size_t i = 0; i < signatures_.size(); ++i) {
        SignatureReport report;
        report.shoe = signatures_[i];
        const auto& candles = market_.candles(report.shoe.sneaker.sku);
        for (const auto& candle : candles) {
            report.closes.push_back(static_cast<double>(candle.close) / 100.0);
            report.high = std::max(report.high, candle.high);
        }
        if (!candles.empty()) {
            report.open = candles.front().close;
            report.close = candles.back().close;
            if (report.open > 0) report.change = static_cast<double>(report.close) / report.open - 1.0;
        }
        report.premium =
            static_cast<double>(report.close) / static_cast<double>(report.shoe.sneaker.retail) - 1.0;

        std::vector<double> scores, returns;
        for (const auto& [score, ret] : performance_log_[i]) {
            scores.push_back(score);
            returns.push_back(ret);
        }
        report.games = static_cast<int>(scores.size());
        if (scores.size() >= 5) report.correlation = stats::pearson(scores, returns);
        recap.signatures.push_back(std::move(report));
    }
    std::stable_sort(recap.signatures.begin(), recap.signatures.end(),
                     [](const SignatureReport& a, const SignatureReport& b) { return a.change > b.change; });
    recap.mixtape = mixtape_;
    return recap;
}

} // namespace street::culture
