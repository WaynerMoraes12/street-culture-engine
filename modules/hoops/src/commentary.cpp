#include "commentary.hpp"

#include <format>
#include <string_view>

namespace street::hoops::detail {
namespace {

// Placeholders: {s} shooter / actor, {a} passer, {d} defender, {b} blocker,
// {t} team name, {f} distance in feet.

constexpr std::string_view kDunk[] = {
    "{s} throws it DOWN!",
    "{s} rises up and hammers it home!",
    "Posterized! {s} dunks all over {d}!",
    "{s} with the two-handed slam. The building is shaking!",
    "Tomahawk jam by {s}!",
    "{s} takes flight... and finishes with authority!",
    "{s} goes baseline and throws down the reverse jam!",
    "Windmill! {s} is putting on a show!",
    "{s} catches {d} ball-watching and slams it home.",
};
constexpr std::string_view kAlleyOop[] = {
    "Alley-oop! {a} lobs it, {s} slams it!",
    "{a} throws it up, {s} throws it down!",
    "{s} catches the lob from {a} and flushes it!",
    "Lob city: {a} to {s} for the jam!",
};
constexpr std::string_view kLayup[] = {
    "{s} glides in for the layup.",
    "{s} slices through the lane and lays it in.",
    "Euro-step from {s}, and the finish!",
    "{s} spins past {d} and kisses it off the glass.",
    "Reverse layup, {s}. Silky smooth.",
    "{s} scoops it up and in over {d}.",
    "{s} floats one in over the big man.",
    "Teardrop from {s}... good.",
    "{s} bullies his way to the rim and scores.",
    "Up-and-under by {s}. {d} bit on the fake.",
};
constexpr std::string_view kLayupAssisted[] = {
    "Nice feed from {a}, easy two for {s}.",
    "{a} threads the needle to {s} for the layup.",
    "Backdoor cut! {a} finds {s} for the easy bucket.",
    "{a} drives and dishes, {s} finishes.",
};
constexpr std::string_view kPutback[] = {
    "{s} cleans up the miss. Putback!",
    "Second-chance points: {s} tips it in.",
    "{s} crashes the glass and slams back the miss!",
    "{s} with the offensive board and the quick stick-back.",
};
constexpr std::string_view kFastBreak[] = {
    "{s} leaks out and finishes the fast break.",
    "Coast to coast for {s}!",
    "{s} runs the floor and gets the easy two in transition.",
    "Nobody back! {s} all alone for the layup.",
};
constexpr std::string_view kMid[] = {
    "{s} pulls up from {f}... cash.",
    "Fadeaway jumper from {s}. Money!",
    "{s} rises over {d} for the {f}-footer.",
    "Turnaround J from {s}, all net.",
    "{s} hits the elbow jumper.",
    "{s} with the step-through in the post, and it drops.",
    "Mid-range maestro {s} knocks down another one.",
    "{s} jab-steps {d} into next week and hits the jumper.",
    "{s} freezes {d} with a hesitation dribble. Pull-up, good.",
};
constexpr std::string_view kMidAssisted[] = {
    "{a} finds {s} at the free-throw line. Nothing but net.",
    "Pick-and-pop: {a} to {s} from {f} feet, good.",
    "{s} curls off the screen, catches from {a}, buries it.",
};
constexpr std::string_view kThree[] = {
    "{s} from downtown... BANG!",
    "Step-back three by {s}. {d} is on skates!",
    "{s} pulls up from {f} feet. Got it!",
    "{s} crosses over {d} and drains the three!",
    "Wet! {s} connects from beyond the arc.",
    "{s} with the side-step three. Splash!",
    "{s} walks into a transition three and nails it.",
    "{s} fakes the drive, steps back, three-ball is good.",
};
constexpr std::string_view kThreeAssisted[] = {
    "{s} buries the three off the kick-out from {a}.",
    "Catch and shoot, {s}... splash!",
    "{a} swings it to {s}, three-ball is good.",
    "Drive and kick: {a} to {s} for three. Money!",
    "{s} spots up in the corner, {a} finds him. Good!",
};
constexpr std::string_view kDeepThree[] = {
    "From way downtown! {s} from {f} feet!",
    "{s} lets it fly from the logo... SPLASH!",
    "{s} pulls up from the parking lot. Are you kidding me?!",
    "Limitless range: {s} drills one from {f} feet!",
};
constexpr std::string_view kHeaveMade[] = {
    "{s} heaves it from {f} feet... IT'S GOOD!",
    "Half-court prayer from {s}... ANSWERED!",
};
constexpr std::string_view kHeaveMiss[] = {
    "{s} heaves it from {f} feet. No good.",
    "Desperation heave by {s} falls short.",
};
constexpr std::string_view kMissRim[] = {
    "{s} drives but can't finish.",
    "{s}'s layup rolls off the rim.",
    "{s} goes up strong but {d} contests. No good.",
    "{s} misses the bunny.",
    "{s} tries the reverse, off the iron.",
    "{s} forces it inside. No good.",
};
constexpr std::string_view kMissDunk[] = {
    "{s} tries to throw it down... clanks it off the back iron!",
    "Rim-rattler but no dunk: {s} misses the slam!",
};
constexpr std::string_view kMissMid[] = {
    "{s} pulls up from {f}... off the mark.",
    "{s}'s fadeaway rims out.",
    "Short from {s}.",
    "{s} rattles it in and out!",
    "{s} misfires from the elbow.",
    "{s} fires from {f}... long.",
    "{d} gets a hand up and {s} misses.",
};
constexpr std::string_view kMissThree[] = {
    "{s} fires from deep. No good.",
    "{s} bricks the three.",
    "{s}'s three hits the front rim.",
    "Corner three from {s} rims out.",
    "{s} launches from {f} feet... off the back iron.",
    "Airball! {s} comes up empty from {f} feet.",
    "{s} can't connect from beyond the arc.",
};
constexpr std::string_view kAndOne[] = {
    "AND ONE! {s} scores through the contact from {d}!",
    "{s} finishes through {d}, and the foul!",
    "Bucket and the foul! {s} flexes to the crowd.",
    "{s} muscles it in over {d} and gets the whistle!",
};
constexpr std::string_view kAndOneThree[] = {
    "{s} hits the three AND gets fouled by {d}! Four-point play chance!",
    "Three-ball, and {d} fouled him! {s} to the line for a four-point play.",
};
constexpr std::string_view kFouledMiss[] = {
    "{d} fouls {s} on the shot.",
    "{s} gets hacked by {d} on the way up.",
    "{d} bails {s} out with a foul on the shot.",
};
constexpr std::string_view kFouledMissThree[] = {
    "{d} fouls {s} on the three-point attempt. Three shots!",
    "{s} draws contact from {d} behind the arc. Three free throws.",
};
constexpr std::string_view kBlock[] = {
    "REJECTED! {b} swats {s}'s shot into the third row!",
    "{b} says NOT IN MY HOUSE!",
    "Get that weak stuff outta here! {b} blocks {s}.",
    "{b} meets {s} at the summit. Blocked!",
    "{b} pins it on the glass!",
    "Chase-down block by {b}!",
    "{b} erases {s}'s shot.",
    "Swat! {b} sends it back.",
};
constexpr std::string_view kSteal[] = {
    "{d} picks {s}'s pocket!",
    "Stolen! {d} jumps the passing lane.",
    "{d} strips {s} clean.",
    "{d} reads it all the way. Steal!",
    "Ball hawk {d} with the steal.",
    "{d} pokes it loose from {s} and takes off.",
};
constexpr std::string_view kBadPass[] = {
    "Bad pass from {s}, sails out of bounds.",
    "{s} throws it right to nobody. Turnover.",
    "{s}'s cross-court pass is off target.",
};
constexpr std::string_view kTravel[] = {
    "{s} travels.",
    "Walk! {s} shuffled his feet.",
    "{s} gets whistled for traveling.",
};
constexpr std::string_view kOutOfBounds[] = {
    "{s} steps on the baseline. Turnover.",
    "{s} loses the handle out of bounds.",
};
constexpr std::string_view kCharge[] = {
    "Offensive foul on {s}. {d} took the charge!",
    "{d} plants his feet and draws the charge on {s}.",
};
constexpr std::string_view kShotClock[] = {
    "Shot clock violation on the {t}.",
    "The {t} can't get a shot off. Shot clock violation.",
};
constexpr std::string_view kThreeSeconds[] = {
    "Three seconds in the key on {s}.",
};
constexpr std::string_view kOffRebound[] = {
    "{s} grabs the offensive board.",
    "{s} keeps it alive!",
    "{s} crashes the glass for the offensive rebound.",
    "{s} out-hustles everybody for the board.",
};
constexpr std::string_view kDefRebound[] = {
    "{s} rebounds.",
    "{s} pulls down the defensive board.",
    "{s} secures the rebound.",
    "Rebound, {s}.",
};
constexpr std::string_view kHeatingUp[] = {
    "{s} is heating up!",
    "{s} has the hot hand. That's two straight.",
    "Somebody check on {s}. He's heating up!",
};
constexpr std::string_view kOnFire[] = {
    "{s} is ON FIRE!",
    "Somebody call the fire department: {s} is ON FIRE!",
    "{s} can't miss right now. He's ON FIRE!",
    "Three straight for {s}. The net is burning!",
};
constexpr std::string_view kFireOut[] = {
    "{s} finally misses. The fire's out.",
    "{s} cools off with a miss.",
};
constexpr std::string_view kFoul[] = {
    "Reach-in foul on {d}.",
    "{d} fouls {s} on the drive.",
    "Loose-ball foul on {d}.",
    "{d} bumps {s} off the ball. Foul.",
};
constexpr std::string_view kIntentional[] = {
    "{d} fouls {s} on purpose to stop the clock.",
    "Intentional foul: {d} grabs {s} to send him to the line.",
};
constexpr std::string_view kTipoff[] = {
    "{s} wins the opening tip for the {t}.",
    "Tip goes to the {t}: {s} controls it.",
};
constexpr std::string_view kGameWinner[] = {
    "GAME WINNER AT THE BUZZER!",
    "HE WINS IT AT THE HORN!",
    "BUZZER BEATER! The {t} win it!",
};
constexpr std::string_view kPeriodBuzzer[] = {"Beat the buzzer!", "At the horn!"};
constexpr std::string_view kTie[] = {"Tie game!", "All square!", "We're even!"};
constexpr std::string_view kGoAhead[] = {
    "The {t} take the lead!",
    "Go-ahead bucket!",
    "The {t} are back in front!",
};
constexpr std::string_view kDagger[] = {"Dagger!", "That might be the dagger."};

// ── the legends in the booth ─────────────────────────────────────────────────
// Homages to Michael Jordan, Kobe Bryant, LeBron James and Allen Iverson. Every
// line is a comparison to a real, well-documented moment; none claims a
// fictional player is the legend. Used sparingly (see the chances below).
constexpr std::string_view kLegendCrossover[] = {
    "Shades of Iverson crossing over Jordan back in '97!",
    "That handle is pure Allen Iverson. The Answer would approve.",
    "A little Iverson-over-Lue swagger on the way back down the floor.",
};
constexpr std::string_view kLegendFadeaway[] = {
    "A Jordan-esque fadeaway.",
    "That's the fadeaway MJ made famous.",
    "A fadeaway straight out of the Kobe playbook.",
};
constexpr std::string_view kLegendDunk[] = {
    "Taking off from free-throw-line territory: 1988 dunk-contest vibes!",
};
constexpr std::string_view kLegendAlleyOop[] = {
    "Kobe to Shaq, Game 7 in 2000: that lob had the same energy.",
};
constexpr std::string_view kLegendFreightTrain[] = {
    "A freight train in transition. Very LeBron.",
};
constexpr std::string_view kLegendChaseDown[] = {
    "LeBron's chase-down on Iguodala in Game 7 of the 2016 Finals comes to mind!",
    "Straight out of LeBron's 2016 Finals playbook.",
};
constexpr std::string_view kLegendGameWinner[] = {
    "That's The Shot: Jordan over Ehlo in '89, all over again!",
    "Mamba mentality!",
};
constexpr std::string_view kLegendLastShot[] = {
    "Jordan over Russell in '98: The Last Shot vibes!",
    "Mamba mentality!",
};
constexpr std::string_view kLegendOnFire[] = {
    "Shades of LeBron scoring 25 straight against Detroit in 2007.",
    "Pound for pound, that's Iverson-level scoring.",
    "He's got that Kobe look in his eye.",
};
constexpr std::string_view kLegendShrug[] = {
    "That's six threes for {s}. Cue the Jordan shrug from the '92 Finals!",
};
constexpr std::string_view kLegendFumes[] = {
    "Running on fumes and still scoring: Flu Game energy.",
    "Jordan had the Flu Game in '97; {s} has dead legs and a hot hand.",
};
constexpr std::string_view kLegendForty[] = {
    "{s} has {n}. Nobody's saying Kobe's 81... but nobody's stopping him either.",
    "{n} points for {s}, and the Kobe's-81 watch is officially on.",
};
constexpr std::string_view kLegendFifty[] = {
    "{n} points! Kobe dropped 60 in his farewell game, and {s} is closing in.",
    "{s} hits {n}. Somebody cue up the tape of Kobe's 81.",
};

struct Names {
    std::string s, a, d, b, t, f, n;
};

/// Announcers mix last names and street nicknames.
std::string who(const Player* player, Rng& flavor) {
    if (player == nullptr) return {};
    if (!player->nickname.empty() && flavor.chance(0.28)) return "\"" + player->nickname + "\"";
    return player->last_name();
}

std::string expand(std::string_view pattern, const Names& names) {
    std::string out;
    out.reserve(pattern.size() + 24);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}') {
            switch (pattern[i + 1]) {
            case 's': out += names.s; break;
            case 'a': out += names.a; break;
            case 'd': out += names.d; break;
            case 'b': out += names.b; break;
            case 't': out += names.t; break;
            case 'f': out += names.f; break;
            case 'n': out += names.n; break;
            default: out.append(pattern.substr(i, 3)); break;
            }
            i += 2;
            continue;
        }
        out += pattern[i];
    }
    return out;
}

template <std::size_t N>
std::string_view pick(Rng& flavor, const std::string_view (&options)[N]) {
    return flavor.pick(options);
}

Names shot_names(const ShotCall& call, Rng& flavor) {
    Names n;
    n.s = who(call.shooter, flavor);
    n.a = who(call.passer, flavor);
    n.d = who(call.defender, flavor);
    n.b = who(call.blocker, flavor);
    n.t = call.team != nullptr ? call.team->name : std::string{};
    n.f = std::to_string(static_cast<int>(call.distance + 0.5));
    return n;
}

std::string_view made_template(const ShotCall& call, Rng& flavor) {
    const bool assisted = call.passer != nullptr;
    if (call.putback) return pick(flavor, kPutback);
    switch (call.zone) {
    case ShotZone::Rim:
        if (call.dunk) return assisted ? pick(flavor, kAlleyOop) : pick(flavor, kDunk);
        if (call.fast_break) return pick(flavor, kFastBreak);
        return assisted && flavor.chance(0.6) ? pick(flavor, kLayupAssisted) : pick(flavor, kLayup);
    case ShotZone::Mid:
        return assisted && flavor.chance(0.5) ? pick(flavor, kMidAssisted) : pick(flavor, kMid);
    case ShotZone::Three:
        if (call.distance >= 27.0) return pick(flavor, kDeepThree);
        return assisted && flavor.chance(0.6) ? pick(flavor, kThreeAssisted)
                                              : pick(flavor, kThree);
    }
    return pick(flavor, kLayup);
}


/// A rare nod to a legend, keyed to the move the template just described.
/// Returns "" most of the time; draws only from the flavor stream.
std::string legend_for_shot(std::string_view pattern, const ShotCall& call, const Names& names,
                            Rng& flavor) {
    const auto has = [&](std::string_view word) { return pattern.find(word) != std::string_view::npos; };
    const auto tag = [&](std::string_view line) { return " " + expand(line, names); };
    if (call.running_on_fumes && flavor.chance(0.15)) return tag(pick(flavor, kLegendFumes));
    if ((has("cross") || has("skates") || has("hesitation")) && flavor.chance(0.04))
        return tag(pick(flavor, kLegendCrossover));
    if (has("adeaway") && flavor.chance(0.06)) return tag(pick(flavor, kLegendFadeaway));
    if (call.dunk && (has("lob") || has("Lob") || has("Alley")) && flavor.chance(0.02))
        return tag(pick(flavor, kLegendAlleyOop));
    if (call.dunk && call.fast_break && flavor.chance(0.06)) return tag(pick(flavor, kLegendFreightTrain));
    if (call.dunk && call.passer == nullptr && flavor.chance(0.008)) return tag(pick(flavor, kLegendDunk));
    return {};
}
} // namespace

std::string call_shot(const ShotCall& call, Rng& flavor) {
    const Names names = shot_names(call, flavor);
    if (call.heave) return expand(call.made ? pick(flavor, kHeaveMade) : pick(flavor, kHeaveMiss), names);

    const bool three = call.zone == ShotZone::Three;
    if (!call.made) {
        if (call.fouled)
            return expand(three ? pick(flavor, kFouledMissThree) : pick(flavor, kFouledMiss), names);
        switch (call.zone) {
        case ShotZone::Rim:
            return expand(call.dunk ? pick(flavor, kMissDunk) : pick(flavor, kMissRim), names);
        case ShotZone::Mid: return expand(pick(flavor, kMissMid), names);
        case ShotZone::Three: return expand(pick(flavor, kMissThree), names);
        }
    }
    if (call.fouled) {
        std::string text = expand(three ? pick(flavor, kAndOneThree) : pick(flavor, kAndOne), names);
        if (call.passer != nullptr) text += " (" + names.a + " assist)";
        return text;
    }
    const std::string_view pattern = made_template(call, flavor);
    std::string text = expand(pattern, names);
    if (call.passer != nullptr && pattern.find("{a}") == std::string_view::npos)
        text += " (" + names.a + " assist)";
    text += legend_for_shot(pattern, call, names, flavor);
    return text;
}

std::string call_block(const ShotCall& call, Rng& flavor) {
    const Names names = shot_names(call, flavor);
    const std::string_view pattern = pick(flavor, kBlock);
    std::string text = expand(pattern, names);
    const bool chase_down = pattern.find("Chase-down") != std::string_view::npos;
    if ((chase_down && flavor.chance(0.15)) || (call.fast_break && flavor.chance(0.04)))
        text += " " + std::string(pick(flavor, kLegendChaseDown));
    return text;
}

std::string call_free_throws(const Player& shooter, int made, int attempts, FreeThrowTrip trip,
                             Rng& flavor) {
    const std::string s = who(&shooter, flavor);
    if (trip == FreeThrowTrip::AndOne) {
        if (made == 1)
            return flavor.chance(0.5) ? s + " completes the and-one."
                                      : s + " converts the freebie. Old-school.";
        return s + " misses the and-one free throw.";
    }
    std::string text;
    if (attempts == 2) {
        if (made == 2)
            text = flavor.chance(0.5) ? s + " hits both free throws." : s + " knocks down a pair.";
        else if (made == 1)
            text = flavor.chance(0.5) ? s + " splits the pair at the line." : s + " makes one of two.";
        else
            text = flavor.chance(0.5) ? s + " misses both free throws!"
                                      : s + " comes up empty at the line, 0-for-2.";
    } else {
        text = std::format("{} goes {}-for-{} at the line.", s, made, attempts);
    }
    if (trip == FreeThrowTrip::Bonus) text += " (penalty)";
    return text;
}

std::string call_steal(const Player& thief, const Player& victim, Rng& flavor) {
    Names n;
    n.d = who(&thief, flavor);
    n.s = who(&victim, flavor);
    return expand(pick(flavor, kSteal), n);
}

std::string call_turnover(TurnoverKind kind, const Player& player, const Player& defender,
                          const Team& team, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    n.d = who(&defender, flavor);
    n.t = team.name;
    switch (kind) {
    case TurnoverKind::BadPass: return expand(pick(flavor, kBadPass), n);
    case TurnoverKind::Travel: return expand(pick(flavor, kTravel), n);
    case TurnoverKind::OutOfBounds: return expand(pick(flavor, kOutOfBounds), n);
    case TurnoverKind::OffensiveFoul: return expand(pick(flavor, kCharge), n);
    case TurnoverKind::ShotClock: return expand(pick(flavor, kShotClock), n);
    case TurnoverKind::ThreeSeconds: return expand(pick(flavor, kThreeSeconds), n);
    }
    return expand(pick(flavor, kBadPass), n);
}

std::string call_rebound(const Player& player, bool offensive, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    return expand(offensive ? pick(flavor, kOffRebound) : pick(flavor, kDefRebound), n);
}

std::string call_heating_up(const Player& player, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    return expand(pick(flavor, kHeatingUp), n);
}

std::string call_on_fire(const Player& player, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    std::string text = expand(pick(flavor, kOnFire), n);
    if (flavor.chance(0.025)) text += " " + std::string(pick(flavor, kLegendOnFire));
    return text;
}

std::string call_milestone(const Player& player, int points, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    n.n = std::to_string(points);
    return expand(points >= 50 ? pick(flavor, kLegendFifty) : pick(flavor, kLegendForty), n);
}

std::string call_shrug(const Player& player, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    return expand(pick(flavor, kLegendShrug), n);
}

std::string call_fire_out(const Player& player, Rng& flavor) {
    Names n;
    n.s = who(&player, flavor);
    return expand(pick(flavor, kFireOut), n);
}

std::string call_foul(const Player& fouler, const Player& victim, bool intentional, Rng& flavor) {
    Names n;
    n.d = who(&fouler, flavor);
    n.s = who(&victim, flavor);
    return expand(intentional ? pick(flavor, kIntentional) : pick(flavor, kFoul), n);
}

std::string call_foul_out(const Player& player) {
    return player.short_name() + " has fouled out of the game.";
}

std::string call_substitution(const Player& in, const Player& out) {
    return in.short_name() + " checks in for " + out.short_name() + ".";
}

std::string call_tipoff(const Player& winner, const Team& team, Rng& flavor) {
    Names n;
    n.s = who(&winner, flavor);
    n.t = team.name;
    return expand(pick(flavor, kTipoff), n);
}

std::string call_situation(const Situation& s, const Team& team, Rng& flavor) {
    Names n;
    n.t = team.name;
    if (s.game_winner) {
        std::string call = expand(pick(flavor, kGameWinner), n);
        if (flavor.chance(0.5)) call += " " + std::string(pick(flavor, kLegendGameWinner));
        return call;
    }
    std::string tag;
    if (s.period_buzzer) tag = std::string(pick(flavor, kPeriodBuzzer));
    const bool late = s.period >= 4 && s.clock <= 120.0;
    if (late) {
        std::string_view extra;
        if (s.lead_after == 0)
            extra = pick(flavor, kTie);
        else if (s.lead_before <= 0 && s.lead_after > 0)
            extra = pick(flavor, kGoAhead);
        else if (s.lead_before >= 1 && s.lead_after >= 5 && s.clock <= 40.0)
            extra = pick(flavor, kDagger);
        if (!extra.empty()) tag += (tag.empty() ? "" : " ") + expand(extra, n);
        const bool jumper_for_the_lead = s.lead_before <= 0 && s.lead_after > 0 && s.clock <= 30.0 &&
                                         s.zone != ShotZone::Rim;
        if (jumper_for_the_lead && flavor.chance(0.15))
            tag += " " + std::string(pick(flavor, kLegendLastShot));
    }
    return tag;
}

std::string call_period_end(int period, const Team& home, const Team& away, int home_score,
                            int away_score, bool game_over) {
    const std::string score =
        std::format("{} {}, {} {}", home.name, home_score, away.name, away_score);
    if (game_over) {
        const int ot = period - 4;
        const std::string tag = ot <= 0 ? "FINAL" : ot == 1 ? "FINAL/OT" : std::format("FINAL/{}OT", ot);
        return std::format("{}: {}.", tag, score);
    }
    if (home_score == away_score && period >= 4) {
        if (period == 4)
            return std::format("End of regulation, tied at {}. We're going to overtime!", home_score);
        return std::format("End of overtime {}, still tied at {}. Five more minutes!", period - 4,
                           home_score);
    }
    switch (period) {
    case 1: return "End of the 1st quarter: " + score + ".";
    case 2: return "Halftime: " + score + ".";
    case 3: return "End of the 3rd quarter: " + score + ".";
    default: return std::format("End of period {}: {}.", period, score);
    }
}

} // namespace street::hoops::detail
