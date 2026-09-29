#include <iostream>
#include <string>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/text.hpp"
#include "street/hoops/league.hpp"
#include "street/hoops/render.hpp"

namespace street::app {

int cmd_teams(Args args) {
    cli::Command cmd{"teams", "Browse the league's teams and rosters"};
    cmd.positional("TEAM", "Team abbreviation, name or city (omit for the whole league)")
        .example("street teams", "all twelve teams with identity, stars and overall")
        .example("street teams RIO", "Rio de Janeiro Tamborzão roster with every rating");
    const auto parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }
    if (parsed.positional().size() > 1) throw cli::UsageError("expected at most one team");

    const auto league = hoops::default_league();
    if (parsed.positional().empty()) {
        std::cout << hoops::render_league(league);
        return 0;
    }
    const std::string& key = parsed.positional().front();
    const hoops::Team* team = hoops::find_team(league, key);
    if (team == nullptr) {
        std::vector<std::string> abbrevs;
        for (const auto& t : league) abbrevs.push_back(t.abbrev);
        throw cli::UsageError("unknown team '" + key + "' (try one of: " + text::join(abbrevs, ", ") + ")");
    }
    std::cout << hoops::render_roster(*team);
    return 0;
}

} // namespace street::app
