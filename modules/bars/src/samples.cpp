// Original verses written for the Street Culture Engine (no third-party lyrics). They double
// as a demo of what the analyzer hears: end rhymes, internal rhymes and multisyllabic chains.

#include "street/bars/ghostwriter.hpp"

#include "street/core/text.hpp"

namespace street::bars {
namespace {

constexpr std::string_view kEnglishSample = R"(Laced up tight for the late-night fight,
Hardwood gleaming in the stadium light,
Crossover crazy, got the ankles breaking,
Whole crowd shaking, it's a legend in the making.

Fresh out the box with the heat on my feet,
Retro on the heel and the bass on repeat,
Suede and leather, never folded, I was born to be bold,
Ice on the rim and my jumper stone cold.

Microphone check, now I'm running the show,
Rhythm in the bounce and the bars in the flow,
Buzzer-beater swishing in the championship game,
Now the whole city screaming out my name.

From the blacktop to the stage, yeah, the story's legendary,
Every verse a victory, every rhyme is necessary,
Kicks up on the shelf in my sneaker sanctuary,
Been a problem on the block since the first of January.
)";

constexpr std::string_view kPortugueseSample = R"(Tênis novo no pé, cadarço amarrado,
Entro na quadra com o nome bordado,
Cruzado seco, o marcador foi pro chão,
A rede balança e explode a multidão.

Da quebrada pra quadra, na raça e na fé,
Treino de madrugada com o tênis no pé,
A rima é o meu drible, o clima esquentou,
Na virada do jogo o ginásio gritou.

Enterrada no aro, a torcida levanta,
O grave da batida no peito que canta,
É fogo no jogo, é luz no telão,
Cesta de três, coração de campeão.

Hoje a história é minha, amanhã é memória,
Cada ponto é suor, cada cesta é vitória,
O pisante é raro, meu verso é de ouro,
Coroa na cabeça, meu nome é tesouro.
)";


// A roll call of legends. Names, titles and facts only; every line is original.
constexpr std::string_view kTributeSample = R"(Seventy-three in the Bronx, block party heat on the street,
Two turntables and a boombox, the concrete kept the beat,
Run-DMC in My Adidas, every lace in its place,
Cassette on rewind in the case, the eighties in the bass.

Jordan hit The Shot, then the Flu Game, flew through and shut it down,
Six rings on his fingers, still the king of every town,
Iverson, the Answer, with a crossover handle,
Crossed Jordan in ninety-seven, lit the league up like a candle.

Kobe dropped eighty-one, the Mamba never missed a day,
Mamba mentality, forever in the gym to stay,
LeBron was down three to one, came back and won, and took the throne,
The chase-down block on Iguodala, carved in history's stone.

Eminem up out of Detroit, spitting fire on 8 Mile,
Every line on time, every rhyme with a style,
Iron Maiden on the tape deck, The Trooper, Eddie on the sleeve,
Galloping bassline, mic check, that's why I still believe.
)";

} // namespace

std::string_view sample_verse(Language lang) {
    return lang == Language::Portuguese ? kPortugueseSample : kEnglishSample;
}

std::string_view sample_verse(Sample sample) {
    switch (sample) {
    case Sample::Portuguese: return kPortugueseSample;
    case Sample::Tribute: return kTributeSample;
    case Sample::English: break;
    }
    return kEnglishSample;
}

std::optional<Sample> parse_sample(std::string_view name) {
    if (text::iequals(name, "tribute")) return Sample::Tribute;
    const std::optional<Language> lang = parse_language(name);
    if (lang == Language::English) return Sample::English;
    if (lang == Language::Portuguese) return Sample::Portuguese;
    return std::nullopt;
}

Language sample_language(Sample sample) noexcept {
    return sample == Sample::Portuguese ? Language::Portuguese : Language::English;
}

} // namespace street::bars
