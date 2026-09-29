#include "street/bars/ghostwriter.hpp"

#include <algorithm>
#include <span>
#include <vector>

#include "street/bars/phonetics.hpp"
#include "street/core/rng.hpp"
#include "street/core/text.hpp"

namespace street::bars {
namespace {

/// A rhyme family for the ghostwriter: line endings whose final words rhyme perfectly.
struct Family {
    std::span<const std::string_view> endings;
};

// --- English --------------------------------------------------------------------------

constexpr std::string_view kEnOpeners[] = {
    "{subject} came through and dropped {points}",
    "Laced up in {shoe}",
    "Crossover, step-back",
    "{team} on my chest",
    "Fresh out the box",
    "Nothing but net",
    "{subject} on the break",
    "Four quarters deep",
    "Box score don't lie",
    "Hardwood shining",
    "Heat in the building",
    "The whole arena rocking",
    "Put up {points} for {team}",
    "Lights on the court",
    "Soles never creasing",
    "Ankles on the floor",
    "Clock running down",
    "Buzzer in the air",
    "Ice in my veins",
    "Rep {team} for life",
    "{shoe} on my feet",
    "{subject} with the handle",
    "Pull up from the logo",
    "Cold-blooded from deep",
};

constexpr std::string_view kEnAme[] = {"they know the name", "hotter than a flame",
                                       "running up the game", "never been the same",
                                       "straight to the hall of fame", "nobody else to blame"};
constexpr std::string_view kEnOw[] = {"watch the city glow", "stealing the show",
                                      "going with the flow", "letting everybody know",
                                      "moving like a pro", "courtside, front row"};
constexpr std::string_view kEnIght[] = {"shining in the light", "every single night",
                                        "taking off in flight", "doing it right",
                                        "the future looking bright", "ready for the fight"};
constexpr std::string_view kEnEat[] = {"you can feel the heat", "rocking every beat",
                                       "straight up off the street", "nobody can compete",
                                       "never taste defeat", "everybody off their feet"};
constexpr std::string_view kEnAy[] = {"every single day", "get up out my way",
                                      "the crowd came out to play", "that's the price you pay",
                                      "and I'm here to stay"};
constexpr std::string_view kEnOld[] = {"a heart made of gold", "a jumper ice cold",
                                       "the story being told", "breaking the mold",
                                       "the rookie playing bold"};
constexpr std::string_view kEnOng[] = {"all night long", "the crowd sing along",
                                       "proving 'em wrong", "the bench going strong",
                                       "the anthem and the song"};
constexpr std::string_view kEnIre[] = {"setting the net on fire", "the stakes getting higher",
                                       "all heart and desire", "here to inspire"};
constexpr std::string_view kEnEam[] = {"living out the dream", "we the starting team",
                                       "the pressure extreme", "the flavor supreme"};
constexpr std::string_view kEnIck[] = {"the handle too quick", "every move slick",
                                       "the jumper gonna stick", "a magic trick",
                                       "a first-round pick"};
constexpr std::string_view kEnAce[] = {"right up in your face", "picking up the pace",
                                       "we finish first place", "the court is my space",
                                       "running the race"};
constexpr std::string_view kEnOp[] = {"straight to the top", "they never gonna stop",
                                      "fresh up out the shop", "we living hip-hop",
                                      "watch the defense drop"};
constexpr std::string_view kEnAll[] = {"standing ten feet tall", "the writing on the wall",
                                       "giving it my all", "answer to the call",
                                       "never gonna fall"};
constexpr std::string_view kEnOwn[] = {"the king of the town", "wearing the crown",
                                       "we shut it down"};

constexpr Family kEnFamilies[] = {{kEnAme}, {kEnOw},  {kEnIght}, {kEnEat}, {kEnAy},
                                  {kEnOld}, {kEnOng}, {kEnIre},  {kEnEam}, {kEnIck},
                                  {kEnAce}, {kEnOp},  {kEnAll},  {kEnOwn}};

// --- Português ------------------------------------------------------------------------

constexpr std::string_view kPtOpeners[] = {
    "{subject} fez {points} pontos",
    "De {shoe} no pé",
    "Cruzado e arremesso",
    "É {team} até o fim",
    "Tênis novo na caixa",
    "Só cesta de três",
    "Contra-ataque rápido",
    "Quatro quartos de guerra",
    "A torcida grita",
    "Luz no ginásio",
    "Drible desconcertante",
    "{points} no placar",
    "Pisante impecável",
    "O relógio correndo",
    "Tabela e rede",
    "Da quebrada pro mundo",
    "{subject} no garrafão",
    "Camisa de {team}",
    "Cadarço amarrado",
    "Chute do meio da rua",
};

constexpr std::string_view kPtAo[] = {"com a bola na mão", "coração de leão",
                                      "sem perder a razão", "sentindo a pressão",
                                      "calando a multidão", "o rei do garrafão"};
constexpr std::string_view kPtOgo[] = {"a quadra pegando fogo", "virando o jogo",
                                       "e a festa começa logo"};
constexpr std::string_view kPtIma[] = {"o flow lá em cima", "esquentando o clima",
                                       "cada verso uma rima", "subindo a autoestima",
                                       "é a obra-prima"};
constexpr std::string_view kPtAda[] = {"na quadra lotada", "virando a madrugada",
                                       "cravando a enterrada", "é a nossa jogada",
                                       "de volta pra quebrada", "vencendo de virada"};
constexpr std::string_view kPtEnte[] = {"sempre pra frente", "o jogo tá quente",
                                        "é o sonho da gente", "treinando a mente",
                                        "brilhando de repente"};
constexpr std::string_view kPtAis[] = {"cada vez mais", "deixando os rivais",
                                       "chegando nas finais", "voltar atrás jamais",
                                       "entre os imortais"};
constexpr std::string_view kPtOr[] = {"com muito amor", "no auge do calor",
                                      "o jogo tem valor", "pingando de suor",
                                      "eu sou o melhor", "o maior jogador"};
constexpr std::string_view kPtIda[] = {"o jogo da vida", "ganhando a partida",
                                       "no ritmo da batida", "sem medo da subida",
                                       "na hora da saída"};
constexpr std::string_view kPtAdo[] = {"o nome gravado", "o tênis amarrado",
                                       "deixando o recado", "o time do meu lado",
                                       "apagando o passado"};
constexpr std::string_view kPtOrte[] = {"com fé e com sorte", "o braço mais forte",
                                        "a quadra é meu norte", "com estilo e porte"};
constexpr std::string_view kPtUa[] = {"é o som da rua", "brilhando igual lua",
                                      "a vitória é sua", "e a luta continua"};
constexpr std::string_view kPtAr[] = {"ninguém vai me parar", "nasci pra ganhar",
                                      "aprendi a voar", "sem medo de sonhar"};
constexpr std::string_view kPtEm[] = {"correndo mais que um trem", "jogando como ninguém",
                                      "e o time também", "eu vou além"};

constexpr Family kPtFamilies[] = {{kPtAo},  {kPtOgo}, {kPtIma}, {kPtAda},  {kPtEnte},
                                  {kPtAis}, {kPtOr},  {kPtIda}, {kPtAdo},  {kPtOrte},
                                  {kPtUa},  {kPtAr},  {kPtEm}};

// --- assembly -------------------------------------------------------------------------

std::string last_word(std::string_view ending) {
    const std::size_t space = ending.rfind(' ');
    return std::string(space == std::string_view::npos ? ending : ending.substr(space + 1));
}

std::string replace_all(std::string text, std::string_view slot, std::string_view value) {
    for (std::size_t pos = text.find(slot); pos != std::string::npos;
         pos = text.find(slot, pos + value.size()))
        text.replace(pos, slot.size(), value);
    return text;
}

/// Uppercases the first code point (works for "é" as well as "e").
std::string capitalize(std::string line) {
    if (line.empty()) return line;
    const std::size_t len = (static_cast<unsigned char>(line[0]) & 0x80) ? 2 : 1;
    return text::to_upper(line.substr(0, len)) + line.substr(len);
}

/// Chooses one family per couplet. Couplets closer than three apart (within four bars)
/// must differ in vowel skeleton, otherwise the analyzer would hear them as slant rhymes
/// of each other; unused families are preferred so a verse repeats as little as possible.
std::vector<std::size_t> choose_families(std::span<const Family> families, Language lang,
                                         int couplets, Rng& rng) {
    std::vector<std::string> skeletons;
    for (const Family& f : families)
        skeletons.push_back(pronounce(last_word(f.endings.front()), lang).vowel_skeleton());

    std::vector<std::size_t> order(families.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    rng.shuffle(order.begin(), order.end());

    std::vector<std::size_t> chosen;
    std::vector<int> uses(families.size(), 0);
    for (int c = 0; c < couplets; ++c) {
        const auto clashes = [&](std::size_t f) {
            for (int back = 1; back <= 2 && back <= c; ++back)
                if (skeletons[chosen[static_cast<std::size_t>(c - back)]] == skeletons[f]) return true;
            return false;
        };
        std::size_t best = order.front();
        int best_uses = -1;
        for (std::size_t f : order) {
            if (clashes(f)) continue;
            if (best_uses < 0 || uses[f] < best_uses) {
                best = f;
                best_uses = uses[f];
            }
        }
        ++uses[best];
        chosen.push_back(best);
    }
    return chosen;
}

} // namespace

std::string ghostwrite(const VerseRequest& request) {
    const bool pt = request.language == Language::Portuguese;
    const Language lang = pt ? Language::Portuguese : Language::English;
    const std::span<const Family> families = pt ? std::span<const Family>(kPtFamilies)
                                                : std::span<const Family>(kEnFamilies);
    const std::span<const std::string_view> openers =
        pt ? std::span<const std::string_view>(kPtOpeners) : std::span<const std::string_view>(kEnOpeners);

    // A dedicated stream keeps verses stable even if other seeded systems change.
    Rng rng(request.seed, hash_seed("street.bars.ghostwriter"));
    const int lines = std::clamp(request.lines, 2, 32);
    const int couplets = lines / 2;
    const int points = request.points > 0 ? request.points : rng.range(30, 61);

    const std::string subject = !request.subject.empty() ? request.subject : pt ? "o novato" : "the rookie";
    const std::string team = !request.team.empty() ? request.team : pt ? "a quebrada" : "Uptown";
    const std::string shoe = !request.shoe.empty() ? request.shoe : pt ? "Jordan 1" : "Jordan 1s";

    const std::vector<std::size_t> chosen = choose_families(families, lang, couplets, rng);
    std::vector<std::size_t> opener_order(openers.size());
    for (std::size_t i = 0; i < opener_order.size(); ++i) opener_order[i] = i;
    rng.shuffle(opener_order.begin(), opener_order.end());

    std::vector<std::string> verse;
    for (int c = 0; c < couplets; ++c) {
        const Family& family = families[chosen[static_cast<std::size_t>(c)]];
        // An odd line count turns the last couplet into a triplet.
        const int size = (c == couplets - 1 && lines % 2 == 1) ? 3 : 2;
        std::vector<std::size_t> picks(family.endings.size());
        for (std::size_t i = 0; i < picks.size(); ++i) picks[i] = i;
        rng.shuffle(picks.begin(), picks.end());
        for (int k = 0; k < size; ++k) {
            const std::string_view ending = family.endings[picks[static_cast<std::size_t>(k)]];
            const std::string_view opener = openers[opener_order[verse.size() % openers.size()]];
            std::string line = replace_all(std::string(opener), "{subject}", subject);
            line = replace_all(std::move(line), "{team}", team);
            line = replace_all(std::move(line), "{shoe}", shoe);
            line = replace_all(std::move(line), "{points}", std::to_string(points));
            verse.push_back(capitalize(line + ", " + std::string(ending)));
        }
    }

    std::string out;
    for (std::size_t i = 0; i < verse.size(); ++i) {
        if (i > 0 && i % 4 == 0 && verse.size() - i >= 2) out += '\n'; // 4-bar stanzas
        out += verse[i];
        out += '\n';
    }
    return out;
}

} // namespace street::bars
