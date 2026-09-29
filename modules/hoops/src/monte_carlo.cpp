#include "street/hoops/monte_carlo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "street/core/rng.hpp"
#include "street/core/stopwatch.hpp"
#include "street/hoops/season.hpp"

namespace street::hoops {
namespace {

/// Integer tallies of one chunk. Integers make the merge order-independent too,
/// but we still merge in chunk order so the design stays obviously deterministic.
struct Tally {
    std::vector<long long> titles, finals, playoffs, wins;
    std::uint64_t games = 0;

    explicit Tally(std::size_t teams)
        : titles(teams, 0), finals(teams, 0), playoffs(teams, 0), wins(teams, 0) {}
};

Tally run_chunk(const std::vector<Team>& league,
                const std::unordered_map<std::uint32_t, std::size_t>& index, int chunk, int first,
                int count, std::uint64_t seed, const TitleOddsOptions& options) {
    Tally tally(league.size());
    // The chunk's generator depends only on (seed, chunk index) — never on which
    // thread happens to run it.
    const auto c = static_cast<std::uint64_t>(chunk);
    Rng chunk_rng(mix64(seed ^ mix64(c + 1)), c + 1);
    for (int s = first; s < first + count; ++s) {
        Season season(league, chunk_rng.next_u64(), options.rounds);
        season.play_regular_season();
        const PlayoffResult playoffs = season.play_playoffs(options.playoff_teams);
        for (const Standing& st : season.standings()) tally.wins[index.at(st.team_id)] += st.wins;
        for (const Series* series : playoffs.round(0)) {
            ++tally.playoffs[index.at(series->high_id)];
            ++tally.playoffs[index.at(series->low_id)];
        }
        const Series& finals = playoffs.finals();
        ++tally.finals[index.at(finals.high_id)];
        ++tally.finals[index.at(finals.low_id)];
        ++tally.titles[index.at(playoffs.champion_id)];
        tally.games += season.games_played();
    }
    return tally;
}

} // namespace

TitleOddsReport simulate_title_odds(const std::vector<Team>& league, int seasons,
                                    std::uint64_t seed, unsigned threads,
                                    const TitleOddsOptions& options) {
    if (seasons < 1) throw std::invalid_argument("title_odds: seasons must be at least 1");
    if (league.size() < 2) throw std::invalid_argument("title_odds: need at least two teams");
    const int field = options.playoff_teams;
    if (field < 2 || (field & (field - 1)) != 0 || field > static_cast<int>(league.size()))
        throw std::invalid_argument("title_odds: playoff field must be a power of two <= league size");

    std::unordered_map<std::uint32_t, std::size_t> index;
    for (std::size_t i = 0; i < league.size(); ++i) index.emplace(league[i].id, i);

    const Stopwatch clock;
    const int chunks = (seasons + kSeasonsPerChunk - 1) / kSeasonsPerChunk;
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workers = std::min<unsigned>(threads == 0 ? hardware : threads,
                                                static_cast<unsigned>(chunks));

    std::vector<Tally> tallies(static_cast<std::size_t>(chunks), Tally(league.size()));
    std::atomic<int> next_chunk{0};
    std::atomic<int> seasons_done{0};
    std::atomic<unsigned> workers_done{0};
    std::exception_ptr failure;
    std::mutex failure_mutex;

    const auto worker = [&] {
        try {
            for (int c = next_chunk.fetch_add(1); c < chunks; c = next_chunk.fetch_add(1)) {
                const int first = c * kSeasonsPerChunk;
                const int count = std::min(kSeasonsPerChunk, seasons - first);
                tallies[static_cast<std::size_t>(c)] =
                    run_chunk(league, index, c, first, count, seed, options);
                seasons_done.fetch_add(count);
            }
        } catch (...) {
            const std::lock_guard lock(failure_mutex);
            if (!failure) failure = std::current_exception();
            next_chunk.store(chunks); // stop handing out work
        }
        workers_done.fetch_add(1);
    };

    std::vector<std::thread> pool;
    pool.reserve(workers);
    for (unsigned w = 0; w < std::max(workers, 1u); ++w) pool.emplace_back(worker);
    if (options.progress) {
        while (workers_done.load() < pool.size()) {
            options.progress(seasons_done.load(), seasons);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    for (auto& t : pool) t.join();
    if (failure) std::rethrow_exception(failure);
    if (options.progress) options.progress(seasons, seasons);

    // Merge in chunk order.
    Tally total(league.size());
    for (const Tally& t : tallies) {
        for (std::size_t i = 0; i < league.size(); ++i) {
            total.titles[i] += t.titles[i];
            total.finals[i] += t.finals[i];
            total.playoffs[i] += t.playoffs[i];
            total.wins[i] += t.wins[i];
        }
        total.games += t.games;
    }

    TitleOddsReport report;
    report.seasons = seasons;
    report.games = total.games;
    report.threads = static_cast<unsigned>(pool.size());
    const double n = seasons;
    for (std::size_t i = 0; i < league.size(); ++i) {
        TitleOdds o;
        o.team_id = league[i].id;
        o.title_probability = static_cast<double>(total.titles[i]) / n;
        o.finals_probability = static_cast<double>(total.finals[i]) / n;
        o.playoff_probability = static_cast<double>(total.playoffs[i]) / n;
        o.average_wins = static_cast<double>(total.wins[i]) / n;
        report.odds.push_back(o);
    }
    std::sort(report.odds.begin(), report.odds.end(), [](const TitleOdds& a, const TitleOdds& b) {
        if (a.title_probability != b.title_probability)
            return a.title_probability > b.title_probability;
        if (a.average_wins != b.average_wins) return a.average_wins > b.average_wins;
        return a.team_id < b.team_id;
    });
    report.seconds = clock.seconds();
    return report;
}

std::vector<TitleOdds> title_odds(const std::vector<Team>& league, int seasons, std::uint64_t seed,
                                  unsigned threads) {
    return simulate_title_odds(league, seasons, seed, threads).odds;
}

} // namespace street::hoops
