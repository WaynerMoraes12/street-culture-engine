// The possession engine behind simulate_game().
//
// A possession: bring the ball up (time drawn from both teams' pace), maybe
// turn it over, maybe draw a reach-in foul, then a shot. The shot resolves
// through a block check, a shooting-foul check and a make probability; misses
// go to a rebound battle, offensive boards restart the sequence. Clock
// management (holding for the last shot, milking a lead, intentional fouls,
// desperation heaves) kicks in late in periods.
//
// All modelling constants live at the top of this file. They were tuned so
// that league-wide averages land on modern NBA numbers (see the calibration
// test in tests/test_game.cpp).

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "commentary.hpp"
#include "sim_team.hpp"
#include "street/hoops/game.hpp"

namespace street::hoops {
namespace {

using detail::SimPlayer;
using detail::SimTeam;

// ── clock ────────────────────────────────────────────────────────────────────
constexpr double kShotClock = 24.0;
constexpr double kResetClock = 14.0;      // shot clock after an offensive rebound
constexpr double kOvertimeSeconds = 300.0;
constexpr double kSecondsPerTrip = 16.5;  // mean time to the first shot at pace 100
constexpr double kResetTripMean = 6.5;    // after an offensive rebound
constexpr double kRotationInterval = 45.0; // coaches look at the bench about once a minute

// ── shot selection & shooting ────────────────────────────────────────────────
constexpr std::array<double, 3> kZoneMix = {0.35, 0.24, 0.41};       // rim, mid, three
constexpr std::array<double, 3> kBaseLogit = {0.54, -0.40, -0.80};   // ~63%, ~40%, ~31% before skill
constexpr std::array<double, 3> kAssistRate = {0.55, 0.45, 0.85};   // shots created by a pass
constexpr std::array<double, 3> kBlockRate = {0.080, 0.032, 0.008};
constexpr std::array<double, 3> kShootingFoulRate = {0.180, 0.050, 0.014};
constexpr double kAssistedBonus = 0.12;   // open looks off the pass
constexpr double kFastBreakBonus = 0.35;
constexpr double kLateClockPenalty = 0.25;
constexpr double kFouledPenalty = 1.10;   // finishing through contact is hard
constexpr double kFatiguePenalty = 0.70;
constexpr double kHomeEdge = 0.06;
constexpr double kFireBoost = 0.22;       // NBA Jam rules
constexpr double kFireUsage = 1.40;
constexpr double kSpacing = 0.014;     // per point of lineup three-point rating above 68
constexpr double kScoreEffect = 0.010; // per point of lead, capped at 15
constexpr double kPaintPacking = 1.2;  // per unit of rim-attempt share above the league norm

// ── possessions ──────────────────────────────────────────────────────────────
constexpr double kTurnoverRate = 0.113;
constexpr double kStealShare = 0.52;
constexpr double kReachFoulRate = 0.068;
constexpr int kBonusFouls = 5; // the fifth team foul of a period puts the opponent in the bonus
constexpr double kTeamReboundRate = 0.07; // misses that go out of bounds / to no one
constexpr double kDefensiveBoxOut = 3.1;  // defenders' positional edge on the glass
constexpr double kFreeThrowBoxOut = 6.5;
constexpr double kRecoveryPerSecond = 0.0015;

double logistic(double x) { return 1.0 / (1.0 + std::exp(-x)); }

std::size_t zi(ShotZone zone) { return static_cast<std::size_t>(zone); }

class GameEngine {
public:
    GameEngine(const Team& home, const Team& away, Rng& rng, const GameOptions& options)
        : rng_(rng), options_(options) {
        // One draw from the main stream seeds the cosmetic streams, so commentary and
        // shot coordinates never perturb the game itself.
        const std::uint64_t flavor_seed = rng_.next_u64();
        narration_ = Rng(flavor_seed, 1);
        geometry_ = Rng(flavor_seed, 2);
        teams_[kHome] = detail::make_sim_team(home, rng_);
        teams_[kAway] = detail::make_sim_team(away, rng_);
    }

    GameResult run();

private:
    struct Trip {
        bool fast_break = false;
        bool late_clock = false;
        bool buzzer = false; // the shot goes up as the period ends
        bool heave = false;
        bool putback = false;
        int putback_player = -1;
    };
    enum class Outcome { Over, Retained, Missed };
    struct AttemptResult {
        Outcome outcome = Outcome::Over;
        ShotZone zone = ShotZone::Rim;
        bool blocked = false;
    };

    // flow
    void start_period();
    void end_period();
    void possession(int off);
    void shot_sequence(int off, Trip trip);
    Trip continue_trip(int off, bool putback_possible);
    AttemptResult attempt_shot(int off, const Trip& trip);
    bool free_throws(int off, int shooter, int count, detail::FreeThrowTrip kind);
    bool rebound(int off, ShotZone zone, bool blocked, bool free_throw);
    void turnover(int off, bool shot_clock_violation);
    bool reach_in_foul(int off);
    void intentional_fouls(int off);
    void jump_ball();

    // decisions
    double trip_time(int off);
    double gamma4();
    double turnover_probability(int off, bool fast_break) const;
    int choose_shooter(int off, bool clutch);
    int choose_passer(int off, int shooter);
    ShotZone choose_zone(const SimPlayer& shooter, const PlayStyle& style, const Trip& trip,
                         bool need_three);
    double shot_distance(ShotZone zone, bool& corner);
    template <class Weight>
    int pick_from_lineup(int side, Weight weight);
    bool should_foul_intentionally(int off) const;
    double spacing(int off) const;
    double paint_packing(int off) const;

    // bookkeeping
    void advance(double seconds);
    void add_points(int side, int player, int points);
    void commit_foul(int side, int player, bool team_foul);
    void narrate_milestones(int side, int player, int points_before);
    void rotate();
    bool clutch_time() const;
    int lead(int side) const { return teams_[side].score - teams_[1 - side].score; }
    PlayerLine& line(int side, int player) {
        return result_.box[static_cast<std::size_t>(side)][static_cast<std::size_t>(player)];
    }
    SimPlayer& sim(int side, int player) {
        return teams_[side].players[static_cast<std::size_t>(player)];
    }
    int slot_of(int side, int player) const;

    // narration (only when options_.record_plays)
    bool narrating() const { return options_.record_plays; }
    void log(int side, PlayKind kind, std::string text, bool highlight = false, int points = 0);
    void record_shot(int side, int player, ShotZone zone, double distance, bool corner, bool made);

    Rng& rng_;
    Rng narration_;
    Rng geometry_;
    GameOptions options_;
    std::array<SimTeam, 2> teams_;
    GameResult result_;
    int period_ = 0;
    double clock_ = 0.0;
    double elapsed_ = 0.0;
    double last_rotation_ = -1e9;
    bool force_rotation_ = true;
    int possession_ = kHome;
    int tip_winner_ = kHome;
    int last_rebounder_ = -1;
    bool fast_break_ = false;
};

// ── game flow ────────────────────────────────────────────────────────────────

GameResult GameEngine::run() {
    result_.home_id = teams_[kHome].team->id;
    result_.away_id = teams_[kAway].team->id;
    for (int side = 0; side < 2; ++side) {
        const auto& roster = teams_[side].team->roster;
        auto& box = result_.box[static_cast<std::size_t>(side)];
        box.resize(roster.size());
        for (std::size_t i = 0; i < roster.size(); ++i) {
            box[i].player_id = roster[i].id;
            box[i].starter = i < 5;
        }
    }

    for (period_ = 1;; ++period_) {
        start_period();
        while (clock_ > 0.0) {
            rotate();
            possession(possession_);
        }
        end_period();
        if (period_ >= 4 && teams_[kHome].score != teams_[kAway].score) break;
    }

    result_.home_score = teams_[kHome].score;
    result_.away_score = teams_[kAway].score;
    result_.overtimes = period_ - 4;
    for (int side = 0; side < 2; ++side)
        for (std::size_t i = 0; i < teams_[side].players.size(); ++i)
            result_.box[static_cast<std::size_t>(side)][i].minutes =
                teams_[side].players[i].played / 60.0;
    return std::move(result_);
}

void GameEngine::start_period() {
    clock_ = period_ <= 4 ? options_.period_minutes * 60.0 : kOvertimeSeconds;
    result_.period_scores[0].push_back(0);
    result_.period_scores[1].push_back(0);
    fast_break_ = false;
    force_rotation_ = true;
    for (auto& team : teams_) {
        team.period_fouls = 0;
        if (period_ > 1) {
            const double rest = period_ == 3 ? 0.35 : 0.12; // halftime vs. a quarter break
            for (auto& p : team.players) p.energy = std::min(1.0, p.energy + rest);
        }
    }
    // Opening tip decides Q1; its loser starts Q2 and Q3, its winner Q4. Overtimes tip again.
    if (period_ == 1 || period_ > 4)
        jump_ball();
    else
        possession_ = (period_ == 4) ? tip_winner_ : 1 - tip_winner_;
}

void GameEngine::end_period() {
    if (!narrating()) return;
    const bool over = period_ >= 4 && teams_[kHome].score != teams_[kAway].score;
    log(-1, PlayKind::Info,
        detail::call_period_end(period_, *teams_[kHome].team, *teams_[kAway].team,
                                teams_[kHome].score, teams_[kAway].score, over),
        true);
}

void GameEngine::jump_ball() {
    // The biggest man on the floor (last slot of the position-sorted lineup) jumps.
    const int home_c = teams_[kHome].lineup[4];
    const int away_c = teams_[kAway].lineup[4];
    const auto reach = [](const SimPlayer& p) {
        return p.player->ratings.athleticism + p.player->ratings.rebounding;
    };
    const double edge = 0.03 * (reach(sim(kHome, home_c)) - reach(sim(kAway, away_c)));
    const int winner = rng_.chance(logistic(edge)) ? kHome : kAway;
    if (period_ == 1) tip_winner_ = winner;
    possession_ = winner;
    if (narrating()) {
        const SimPlayer& jumper = sim(winner, winner == kHome ? home_c : away_c);
        log(winner, PlayKind::Info,
            detail::call_tipoff(*jumper.player, *teams_[winner].team, narration_), true);
    }
}

void GameEngine::possession(int off) {
    possession_ = 1 - off; // every path below ends with the ball changing hands
    const bool fast_break = std::exchange(fast_break_, false);
    const double start_clock = clock_;
    const bool final_period = period_ >= 4;
    const int lead_now = lead(off);

    if (should_foul_intentionally(off) && rng_.chance(0.85)) {
        intentional_fouls(off);
        return;
    }

    Trip trip;
    trip.fast_break = fast_break;
    double t = fast_break ? rng_.uniform(2.5, 7.0) : trip_time(off);

    if (!fast_break) {
        if (clock_ <= kShotClock + 1.0) {
            // The shot clock no longer matters: trailing late means hurry, otherwise
            // hold for the last shot of the period.
            if (final_period && lead_now < 0)
                t = std::min(t, rng_.uniform(4.0, 9.0));
            else
                t = std::max(t, clock_ - rng_.uniform(0.3, 2.5));
        } else if (final_period && clock_ <= 150.0 && lead_now > 0) {
            t = std::max(t, rng_.uniform(14.0, 22.0)); // milk the clock with a lead
        }
    }
    if (t >= kShotClock - 0.5 && clock_ > kShotClock) {
        trip.late_clock = true;
        if (rng_.chance(0.10)) {
            advance(kShotClock);
            turnover(off, true);
            return;
        }
        t = kShotClock - rng_.uniform(0.2, 1.5);
    }
    if (t >= clock_) {
        t = clock_;
        trip.buzzer = true;
        trip.heave = start_clock < 2.0;
        trip.late_clock = trip.late_clock || start_clock < 4.0;
    }

    if (!trip.heave && rng_.chance(turnover_probability(off, fast_break))) {
        advance(t * rng_.uniform(0.3, 0.95));
        turnover(off, false);
        return;
    }
    if (!trip.heave && rng_.chance(kReachFoulRate)) {
        const double before = t * rng_.uniform(0.2, 0.8);
        advance(before);
        if (reach_in_foul(off)) return;
        t -= before;
    }
    advance(t);
    shot_sequence(off, trip);
}

void GameEngine::shot_sequence(int off, Trip trip) {
    for (;;) {
        const AttemptResult r = attempt_shot(off, trip);
        if (r.outcome == Outcome::Over || clock_ <= 0.0) return;
        bool putback_possible = false;
        if (r.outcome == Outcome::Missed) {
            if (!rebound(off, r.zone, r.blocked, false)) return;
            putback_possible = true;
        }
        if (clock_ <= 0.0) return;
        trip = continue_trip(off, putback_possible);
    }
}

GameEngine::Trip GameEngine::continue_trip(int off, bool putback_possible) {
    Trip trip;
    if (putback_possible && last_rebounder_ >= 0) {
        const SimPlayer& p = sim(off, last_rebounder_);
        if (rng_.chance(std::clamp(0.30 * p.rim_tend, 0.08, 0.60))) {
            trip.putback = true;
            trip.putback_player = last_rebounder_;
        }
    }
    double t = trip.putback ? rng_.uniform(0.5, 2.5) : 3.0 + (kResetTripMean - 3.0) * gamma4();
    t = std::min(t, kResetClock - 0.3);
    if (t >= clock_) {
        t = clock_;
        trip.buzzer = true;
        trip.heave = !trip.putback && clock_ < 1.5;
    }
    advance(t);
    return trip;
}

GameEngine::AttemptResult GameEngine::attempt_shot(int off, const Trip& trip) {
    SimTeam& o = teams_[off];
    SimTeam& d = teams_[1 - off];
    const int def = 1 - off;
    const bool clutch = clutch_time();
    const int shooter = trip.putback ? trip.putback_player : choose_shooter(off, clutch);
    SimPlayer& sh = sim(off, shooter);
    const int defender = d.lineup[static_cast<std::size_t>(slot_of(off, shooter))];
    const SimPlayer& matched = sim(def, defender);
    const int lead_before = lead(off);

    // Where is the shot coming from?
    ShotZone zone = ShotZone::Rim;
    double distance = 0.0;
    bool corner = false;
    if (trip.heave) {
        zone = ShotZone::Three;
        distance = rng_.uniform(28.0, 41.0);
    } else if (trip.putback) {
        distance = rng_.uniform(0.5, 3.0);
    } else {
        const bool need_three = period_ >= 4 && lead_before == -3 && clock_ <= 15.0;
        zone = choose_zone(sh, *o.style, trip, need_three);
        distance = shot_distance(zone, corner);
    }
    const int value = zone == ShotZone::Three ? 3 : 2;
    ++o.shot_attempts;
    if (zone == ShotZone::Rim) ++o.rim_attempts;

    // Was it created by a pass? (credited as an assist only if it goes in)
    int passer = -1;
    if (!trip.putback && !trip.heave) {
        const double p_pass = kAssistRate[zi(zone)] * o.style->assist * (trip.fast_break ? 0.8 : 1.0);
        if (rng_.chance(std::min(p_pass, 0.95))) passer = choose_passer(off, shooter);
    }

    // Help defense at the rim, the matched defender elsewhere.
    const int contester =
        zone == ShotZone::Rim
            ? pick_from_lineup(def, [](const SimPlayer& p) { return p.block_weight; })
            : defender;

    detail::ShotCall call;
    if (narrating()) {
        call.shooter = sh.player;
        call.defender = matched.player;
        call.team = o.team;
        call.zone = zone;
        call.distance = distance;
        call.putback = trip.putback;
        call.fast_break = trip.fast_break;
        call.heave = trip.heave;
        call.running_on_fumes = sh.player->star && sh.energy < 0.5;
        call.dunk = zone == ShotZone::Rim && distance < 2.6 &&
                    narration_.chance(std::clamp((sh.player->ratings.athleticism - 55) / 35.0, 0.05, 0.9));
    }

    // Block?
    if (!trip.heave) {
        const SimPlayer& blocker = sim(def, contester);
        const double p_block = kBlockRate[zi(zone)] * blocker.block_power * sh.finish_factor *
                               (trip.fast_break ? 0.6 : 1.0);
        if (rng_.chance(p_block)) {
            ++line(off, shooter).fga;
            if (value == 3) ++line(off, shooter).tpa;
            ++line(def, contester).blk;
            const bool was_on_fire = sh.on_fire;
            sh.streak = 0;
            sh.on_fire = false;
            record_shot(off, shooter, zone, distance, corner, false);
            if (narrating()) {
                call.blocker = blocker.player;
                log(def, PlayKind::Block, detail::call_block(call, narration_),
                    clutch || (zone == ShotZone::Rim && narration_.chance(0.35)));
                if (was_on_fire) log(off, PlayKind::Streak, detail::call_fire_out(*sh.player, narration_));
            }
            return {Outcome::Missed, zone, true};
        }
    }

    // Shooting foul?
    const double foul_draw = zone == ShotZone::Three ? 1.0 : sh.foul_draw;
    const bool fouled =
        !trip.heave && rng_.chance(kShootingFoulRate[zi(zone)] * o.style->draw_foul * foul_draw);

    // Make probability: shooter vs. defense, distance, fatigue, situation.
    double p_make = 0.0;
    if (trip.heave) {
        p_make = std::clamp(0.20 - 0.012 * (distance - 28.0), 0.02, 0.2);
    } else {
        double logit = kBaseLogit[zi(zone)] + d.style->defense;
        // Leaders relax and trailers press: pulls margins back toward realistic spreads.
        logit -= kScoreEffect * std::clamp(lead_before, -15, 15);
        switch (zone) {
        case ShotZone::Rim: {
            double best_rim_d = -1.0;
            for (int idx : d.lineup) best_rim_d = std::max(best_rim_d, sim(def, idx).interior_d);
            logit += sh.rim_skill - (0.6 * best_rim_d + 0.4 * matched.interior_d);
            logit -= 0.10 * (distance - 2.0);
            logit += spacing(off) - paint_packing(off);
            break;
        }
        case ShotZone::Mid:
            logit += sh.mid_skill - matched.perimeter_d - 0.02 * (distance - 12.0);
            logit += 0.5 * spacing(off);
            break;
        case ShotZone::Three:
            logit += sh.three_skill - 0.8 * matched.perimeter_d;
            logit += corner ? 0.06 : -0.10 * (distance - 24.5);
            break;
        }
        logit -= kFatiguePenalty * (1.0 - sh.energy);
        if (clutch) logit += sh.clutch;
        if (off == kHome) logit += kHomeEdge;
        if (sh.on_fire) logit += kFireBoost;
        if (passer >= 0) logit += kAssistedBonus;
        if (trip.fast_break && zone == ShotZone::Rim) logit += kFastBreakBonus;
        if (trip.putback) logit += 0.10;
        if (trip.late_clock) logit -= kLateClockPenalty;
        if (fouled) logit -= kFouledPenalty;
        p_make = logistic(logit);
    }
    const bool made = rng_.chance(p_make);

    if (fouled && !made) {
        // A missed shot on a shooting foul is not a field-goal attempt.
        commit_foul(def, contester, true);
        if (narrating()) {
            call.defender = sim(def, contester).player;
            call.fouled = true;
            log(off, PlayKind::Foul, detail::call_shot(call, narration_));
        }
        const bool keep = free_throws(off, shooter, value, detail::FreeThrowTrip::Shooting);
        return {keep ? Outcome::Retained : Outcome::Over, zone, false};
    }

    record_shot(off, shooter, zone, distance, corner, made); // field-goal attempts only
    PlayerLine& sl = line(off, shooter);
    ++sl.fga;
    if (value == 3) ++sl.tpa;
    if (!made) {
        const bool was_on_fire = sh.on_fire;
        sh.streak = 0;
        sh.on_fire = false;
        if (narrating()) {
            const bool big_miss = period_ >= 4 && clock_ <= 10.0 && std::abs(lead_before) <= 3;
            log(off, PlayKind::Miss, detail::call_shot(call, narration_), big_miss);
            if (was_on_fire) log(off, PlayKind::Streak, detail::call_fire_out(*sh.player, narration_));
        }
        return {Outcome::Missed, zone, false};
    }

    ++sl.fgm;
    if (value == 3) ++sl.tpm;
    const int points_before = line(off, shooter).pts;
    add_points(off, shooter, value);
    if (passer >= 0) ++line(off, passer).ast;
    ++sh.streak;
    const bool caught_fire = sh.streak == 3;
    if (caught_fire) sh.on_fire = true;

    if (narrating()) {
        call.made = true;
        call.fouled = fouled;
        if (fouled) call.defender = sim(def, contester).player;
        if (passer >= 0) call.passer = sim(off, passer).player;
        detail::Situation situation;
        situation.period = period_;
        situation.clock = clock_;
        situation.lead_before = lead_before;
        situation.lead_after = lead(off);
        situation.period_buzzer = clock_ <= 0.0;
        situation.game_winner = clock_ <= 0.0 && period_ >= 4 && lead_before <= 0 && lead(off) > 0;
        situation.zone = zone;
        std::string text = detail::call_shot(call, narration_);
        const std::string tag = detail::call_situation(situation, *o.team, narration_);
        if (!tag.empty()) text += " " + tag;
        const bool late_and_close = period_ >= 4 && clock_ <= 120.0 && std::abs(lead(off)) <= 4;
        // Keep the highlight reel to roughly 30-40 moments a game.
        const bool showtime = (call.dunk && narration_.chance(0.35)) ||
                              (fouled && (value == 3 || narration_.chance(0.5)));
        const bool highlight = showtime || trip.heave || !tag.empty() || late_and_close ||
                               (zone == ShotZone::Three && distance >= 27.0) ||
                               situation.period_buzzer;
        log(off, PlayKind::Score, std::move(text), highlight, value);
        if (sh.streak == 2) log(off, PlayKind::Streak, detail::call_heating_up(*sh.player, narration_));
        if (caught_fire) log(off, PlayKind::Streak, detail::call_on_fire(*sh.player, narration_), true);
        narrate_milestones(off, shooter, points_before);
        if (value == 3 && line(off, shooter).tpm == 6 && narration_.chance(0.2))
            log(off, PlayKind::Streak, detail::call_shrug(*sh.player, narration_), true);
    }

    if (fouled) {
        commit_foul(def, contester, true);
        const bool keep = free_throws(off, shooter, 1, detail::FreeThrowTrip::AndOne);
        return {keep ? Outcome::Retained : Outcome::Over, zone, false};
    }
    return {Outcome::Over, zone, false};
}

bool GameEngine::free_throws(int off, int shooter, int count, detail::FreeThrowTrip kind) {
    SimPlayer& p = sim(off, shooter);
    double prob = p.ft_prob - 0.04 * (1.0 - p.energy);
    if (clutch_time()) prob += 0.25 * p.clutch;
    if (off == kHome) prob += 0.005;
    prob = std::clamp(prob, 0.25, 0.97);

    const int points_before = line(off, shooter).pts;
    int made = 0;
    bool last_made = false;
    for (int i = 0; i < count; ++i) {
        last_made = rng_.chance(prob);
        ++line(off, shooter).fta;
        if (last_made) {
            ++line(off, shooter).ftm;
            ++made;
            add_points(off, shooter, 1);
        }
    }
    if (narrating()) {
        const bool pressure = period_ >= 4 && clock_ <= 30.0 && std::abs(lead(off)) <= 4;
        log(off, PlayKind::FreeThrows,
            detail::call_free_throws(*p.player, made, count, kind, narration_), pressure, made);
        narrate_milestones(off, shooter, points_before);
    }
    if (last_made) return false;
    return rebound(off, ShotZone::Rim, false, true);
}

bool GameEngine::rebound(int off, ShotZone zone, bool blocked, bool free_throw) {
    last_rebounder_ = -1;
    if (blocked && rng_.chance(0.40)) return true; // swatted out of bounds, offense keeps it
    if (!free_throw && rng_.chance(kTeamReboundRate)) return rng_.chance(0.30);

    const int def = 1 - off;
    double offense = 0.0, defense = 0.0;
    for (int idx : teams_[off].lineup) offense += sim(off, idx).rebound_weight;
    for (int idx : teams_[def].lineup) defense += sim(def, idx).rebound_weight;
    offense *= teams_[off].style->crash * (zone == ShotZone::Rim ? 1.15 : 1.0);
    const double box_out = free_throw ? kFreeThrowBoxOut : kDefensiveBoxOut;
    const bool offensive = rng_.chance(offense / (offense + box_out * defense));

    const int side = offensive ? off : def;
    const int who = pick_from_lineup(side, [](const SimPlayer& p) { return p.rebound_weight; });
    PlayerLine& l = line(side, who);
    if (offensive) {
        ++l.oreb;
        last_rebounder_ = who;
    } else {
        ++l.dreb;
        fast_break_ = rng_.chance(0.10 * teams_[def].style->transition);
    }
    if (narrating())
        log(side, PlayKind::Rebound, detail::call_rebound(*sim(side, who).player, offensive, narration_));
    return offensive;
}

void GameEngine::turnover(int off, bool shot_clock_violation) {
    const int def = 1 - off;
    const int culprit =
        pick_from_lineup(off, [](const SimPlayer& p) { return p.usage * p.ball_security; });
    ++line(off, culprit).tov;
    const SimPlayer& victim = sim(off, culprit);
    const int matched = teams_[def].lineup[static_cast<std::size_t>(slot_of(off, culprit))];

    if (shot_clock_violation) {
        if (narrating())
            log(off, PlayKind::Turnover,
                detail::call_turnover(detail::TurnoverKind::ShotClock, *victim.player,
                                      *sim(def, matched).player, *teams_[off].team, narration_));
        return;
    }
    const double steal_share = std::min(0.8, kStealShare * teams_[def].style->pressure);
    if (rng_.chance(steal_share)) {
        const int thief = pick_from_lineup(def, [](const SimPlayer& p) { return p.steal_weight; });
        ++line(def, thief).stl;
        fast_break_ = rng_.chance(std::min(0.9, 0.55 * teams_[def].style->transition));
        if (narrating())
            log(def, PlayKind::Steal,
                detail::call_steal(*sim(def, thief).player, *victim.player, narration_),
                fast_break_ && narration_.chance(0.5));
        return;
    }
    const double u = rng_.uniform();
    detail::TurnoverKind kind = detail::TurnoverKind::ThreeSeconds;
    if (u < 0.22)
        kind = detail::TurnoverKind::OffensiveFoul;
    else if (u < 0.50)
        kind = detail::TurnoverKind::BadPass;
    else if (u < 0.72)
        kind = detail::TurnoverKind::Travel;
    else if (u < 0.93)
        kind = detail::TurnoverKind::OutOfBounds;
    if (kind == detail::TurnoverKind::OffensiveFoul) commit_foul(off, culprit, false);
    if (narrating())
        log(off, PlayKind::Turnover,
            detail::call_turnover(kind, *victim.player, *sim(def, matched).player,
                                  *teams_[off].team, narration_));
}

bool GameEngine::reach_in_foul(int off) {
    const int def = 1 - off;
    const int fouler = pick_from_lineup(def, [](const SimPlayer& p) { return p.foul_weight; });
    const int victim = pick_from_lineup(off, [](const SimPlayer& p) { return p.usage; });
    commit_foul(def, fouler, true);
    if (narrating())
        log(def, PlayKind::Foul,
            detail::call_foul(*sim(def, fouler).player, *sim(off, victim).player, false, narration_));
    if (teams_[def].period_fouls < kBonusFouls) return false;
    if (free_throws(off, victim, 2, detail::FreeThrowTrip::Bonus) && clock_ > 0.0)
        shot_sequence(off, continue_trip(off, false));
    return true;
}

bool GameEngine::should_foul_intentionally(int off) const {
    if (period_ < 4 || clock_ < 1.0) return false;
    const int ahead = lead(off);
    if (ahead >= 1 && ahead <= 3) return clock_ <= 24.0;
    if (ahead >= 4 && ahead <= 8) return clock_ <= 50.0;
    return false;
}

void GameEngine::intentional_fouls(int off) {
    const int def = 1 - off;
    // The leader inbounds to its best free-throw shooters; the trailer grabs whoever
    // catches it, preferably with a player it can afford to put in foul trouble.
    for (;;) {
        advance(rng_.uniform(1.0, 3.5));
        const int victim = pick_from_lineup(off, [](const SimPlayer& p) {
            const double ft = p.ft_prob * p.ft_prob;
            return p.usage * ft * ft * ft;
        });
        const int fouler =
            pick_from_lineup(def, [](const SimPlayer& p) { return 1.25 - p.closer; });
        commit_foul(def, fouler, true);
        if (narrating())
            log(def, PlayKind::Foul,
                detail::call_foul(*sim(def, fouler).player, *sim(off, victim).player, true,
                                  narration_));
        if (teams_[def].period_fouls >= kBonusFouls || clock_ <= 0.0) {
            if (free_throws(off, victim, 2, detail::FreeThrowTrip::Intentional) && clock_ > 0.0)
                shot_sequence(off, continue_trip(off, false));
            return;
        }
    }
}

// ── decisions ────────────────────────────────────────────────────────────────

double GameEngine::gamma4() {
    // Gamma(shape 4, mean 1): sum of four exponentials with a single log.
    const double u = (1.0 - rng_.uniform()) * (1.0 - rng_.uniform()) * (1.0 - rng_.uniform()) *
                     (1.0 - rng_.uniform());
    return -std::log(u) / 4.0;
}

double GameEngine::spacing(int off) const {
    // Shooters drag defenders out of the paint; a lineup of non-shooters invites a packed lane.
    int total = 0;
    for (int idx : teams_[off].lineup)
        total += teams_[off].players[static_cast<std::size_t>(idx)].player->ratings.three;
    return kSpacing * (total / 5.0 - 68.0);
}

double GameEngine::paint_packing(int off) const {
    // Defenses adjust to what an offense does most: the more of its shots a team
    // takes at the rim tonight, the more bodies it finds there. A prior of 20
    // league-average attempts keeps the first minutes from swinging wildly.
    const SimTeam& t = teams_[off];
    constexpr double kPrior = 20.0;
    const double share = (t.rim_attempts + kPrior * kZoneMix[0]) / (t.shot_attempts + kPrior);
    return kPaintPacking * std::max(0.0, share - kZoneMix[0]);
}

double GameEngine::trip_time(int off) {
    // The offense sets most of the tempo, the defense some of it.
    const double pace = 0.6 * teams_[off].style->pace + 0.4 * teams_[1 - off].style->pace;
    const double mean = kSecondsPerTrip * 100.0 / pace;
    return 4.0 + (mean - 4.0) * gamma4();
}

double GameEngine::turnover_probability(int off, bool fast_break) const {
    const SimTeam& o = teams_[off];
    const SimTeam& d = teams_[1 - off];
    double usage = 0.0, security = 0.0, pressure = 1.0;
    for (int idx : o.lineup) {
        const SimPlayer& p = o.players[static_cast<std::size_t>(idx)];
        usage += p.usage;
        security += p.usage * p.ball_security * (1.0 + 0.6 * (1.0 - p.energy));
    }
    // Product of per-defender factors == exp(0.3 * sum of perimeter-defense shifts).
    for (int idx : d.lineup) pressure *= d.players[static_cast<std::size_t>(idx)].pressure_factor;
    double p = kTurnoverRate * (security / usage) * pressure * d.style->pressure;
    if (off == kHome) p *= 0.97;
    if (fast_break) p *= 0.8;
    return p;
}

template <class Weight>
int GameEngine::pick_from_lineup(int side, Weight weight) {
    const SimTeam& t = teams_[side];
    std::array<double, 5> w{};
    for (std::size_t k = 0; k < 5; ++k)
        w[k] = weight(t.players[static_cast<std::size_t>(t.lineup[k])]);
    return t.lineup[rng_.weighted(w)];
}

int GameEngine::choose_shooter(int off, bool clutch) {
    return pick_from_lineup(off, [clutch](const SimPlayer& p) {
        double w = p.usage * (0.55 + 0.45 * p.energy);
        if (p.on_fire) w *= kFireUsage;
        if (clutch) w *= std::exp(4.0 * p.clutch); // the ball finds the closers
        return w;
    });
}

int GameEngine::choose_passer(int off, int shooter) {
    return pick_from_lineup(off, [&](const SimPlayer& p) {
        return p.player == sim(off, shooter).player ? 0.0 : p.pass_weight;
    });
}

ShotZone GameEngine::choose_zone(const SimPlayer& shooter, const PlayStyle& style,
                                 const Trip& trip, bool need_three) {
    double rim = shooter.rim_tend * style.rim * kZoneMix[0];
    double mid = shooter.mid_tend * style.mid * kZoneMix[1];
    double three = shooter.three_tend * style.three * kZoneMix[2];
    if (trip.fast_break) rim *= 3.0;
    if (trip.late_clock) {
        rim *= 0.7;
        mid *= 1.3;
    }
    if (need_three) three *= 6.0;
    const double u = rng_.uniform() * (rim + mid + three);
    if (u < rim) return ShotZone::Rim;
    return u < rim + mid ? ShotZone::Mid : ShotZone::Three;
}

double GameEngine::shot_distance(ShotZone zone, bool& corner) {
    const double u = rng_.uniform();
    switch (zone) {
    case ShotZone::Rim: return 0.5 + 5.5 * std::pow(u, 1.7); // most attempts right at the cup
    case ShotZone::Mid: return 6.0 + 15.5 * u;
    case ShotZone::Three:
        corner = u < 0.24;
        if (corner) return rng_.uniform(22.0, 23.7);
        return kThreeRadius + 4.5 * std::pow(rng_.uniform(), 2.5); // the odd logo shot
    }
    return 1.0;
}

// ── bookkeeping ──────────────────────────────────────────────────────────────

void GameEngine::advance(double seconds) {
    const double dt = std::clamp(seconds, 0.0, clock_);
    clock_ = seconds >= clock_ ? 0.0 : clock_ - dt;
    elapsed_ += dt;
    for (auto& team : teams_) {
        for (auto& p : team.players) {
            if (p.on_court) {
                p.played += dt;
                p.energy = std::max(0.3, p.energy - p.drain * dt);
            } else {
                p.energy = std::min(1.0, p.energy + kRecoveryPerSecond * dt);
            }
        }
    }
}

void GameEngine::add_points(int side, int player, int points) {
    teams_[side].score += points;
    line(side, player).pts += points;
    result_.period_scores[static_cast<std::size_t>(side)].back() += points;
    for (int idx : teams_[side].lineup) line(side, idx).plus_minus += points;
    for (int idx : teams_[1 - side].lineup) line(1 - side, idx).plus_minus -= points;
}

void GameEngine::commit_foul(int side, int player, bool team_foul) {
    SimPlayer& p = sim(side, player);
    ++p.fouls;
    ++line(side, player).pf;
    if (team_foul) ++teams_[side].period_fouls;
    if (p.fouls == detail::kFoulOut) {
        force_rotation_ = true;
        if (narrating()) log(side, PlayKind::Foul, detail::call_foul_out(*p.player), true);
    }
}

void GameEngine::narrate_milestones(int side, int player, int points_before) {
    // A 40- or 50-point night earns the booth's Kobe comparison.
    const int points = line(side, player).pts;
    for (int mark : {40, 50})
        if (points_before < mark && points >= mark && (mark == 50 || narration_.chance(0.5)))
            log(side, PlayKind::Streak, detail::call_milestone(*sim(side, player).player, points, narration_),
                true);
}

void GameEngine::rotate() {
    if (!force_rotation_ && elapsed_ - last_rotation_ < kRotationInterval) return;
    force_rotation_ = false;
    last_rotation_ = elapsed_;
    for (int side = 0; side < 2; ++side) {
        detail::RotationContext context;
        context.period = period_;
        context.clock = clock_;
        context.elapsed = elapsed_;
        context.margin = lead(side);
        const detail::Substitutions subs = detail::manage_rotation(teams_[side], context);
        if (!narrating()) continue;
        for (int i = 0; i < subs.count; ++i) {
            const auto [out, in] = subs.list[static_cast<std::size_t>(i)];
            log(side, PlayKind::Substitution,
                detail::call_substitution(*sim(side, in).player, *sim(side, out).player));
        }
    }
}

bool GameEngine::clutch_time() const {
    return period_ >= 4 && clock_ <= 300.0 && std::abs(lead(kHome)) <= 5;
}

int GameEngine::slot_of(int side, int player) const {
    const auto& lineup = teams_[side].lineup;
    for (std::size_t k = 0; k < lineup.size(); ++k)
        if (lineup[k] == player) return static_cast<int>(k);
    return 0;
}

void GameEngine::log(int side, PlayKind kind, std::string text, bool highlight, int points) {
    Play play;
    play.period = period_;
    play.clock = clock_;
    play.side = side;
    play.text = std::move(text);
    play.home_score = teams_[kHome].score;
    play.away_score = teams_[kAway].score;
    play.highlight = highlight;
    play.kind = kind;
    play.points = points;
    result_.plays.push_back(std::move(play));
}

void GameEngine::record_shot(int side, int player, ShotZone zone, double distance, bool corner,
                             bool made) {
    if (!options_.record_shots) return;
    // Place the attempt on the half court (cosmetic stream): the distance is the
    // modelled one, the angle is free within the zone's geometry.
    double x = 0.0, y = 0.0;
    if (zone == ShotZone::Three && corner) {
        const double side_sign = geometry_.chance(0.5) ? 1.0 : -1.0;
        x = side_sign * distance;
        y = geometry_.uniform(0.8, 13.8);
    } else {
        double max_angle = std::numbers::pi / 2.0;
        if (zone == ShotZone::Three) {
            max_angle = std::asin(std::min(1.0, kCornerThree / std::max(distance, kCornerThree)));
        } else {
            const double floor_y = zone == ShotZone::Rim ? 4.0 : 0.5; // stay in front of the board
            max_angle = std::acos(std::clamp((floor_y - kRimY) / distance, -1.0, 1.0));
        }
        const double angle = geometry_.uniform(-max_angle, max_angle);
        x = distance * std::sin(angle);
        y = std::min(kRimY + distance * std::cos(angle), 46.5);
    }
    const int value = zone == ShotZone::Three ? 3 : 2;
    result_.shots.push_back(Shot{sim(side, player).player->id, side, x, y, value, made, zone, period_});
}

} // namespace

GameResult simulate_game(const Team& home, const Team& away, Rng& rng, const GameOptions& options) {
    if (home.roster.size() < 5 || away.roster.size() < 5)
        throw std::invalid_argument("simulate_game: every roster needs at least five players");
    if (options.period_minutes <= 0)
        throw std::invalid_argument("simulate_game: period_minutes must be positive");
    GameEngine engine(home, away, rng, options);
    return engine.run();
}

} // namespace street::hoops
