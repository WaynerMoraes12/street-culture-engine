// The built-in rhyme lexicon: a few hundred words per language around the four pillars of
// The culture (basketball, sneakers, the streets, music) plus common words that rap leans on.

#include "street/bars/lexicon.hpp"

#include <algorithm>
#include <cstdlib>
#include <tuple>

#include "street/bars/phonetics.hpp"
#include "street/bars/tokenizer.hpp"

namespace street::bars {
namespace {

constexpr std::string_view kEnglish[] = {
    // basketball
    "ball", "hoop", "hoops", "court", "game", "games", "net", "rim", "dunk", "slam", "jam",
    "crossover", "dribble", "rebound", "assist", "three", "buzzer", "clutch", "baller", "point",
    "guard", "center", "forward", "block", "steal", "layup", "fadeaway", "jumper", "swish",
    "brick", "foul", "bench", "coach", "league", "season", "playoffs", "finals", "ring", "trophy",
    "rookie", "veteran", "legend", "champion", "championship", "arena", "crowd", "hardwood",
    "paint", "zone", "press", "fastbreak", "pass", "shot", "score", "scorer", "defense", "offense",
    "glass", "backboard", "lob", "alley-oop", "overtime", "timeout", "starter", "draft",
    "contract", "dynasty", "record", "title", "streetball", "playground", "blacktop", "handles",
    "ankles", "step-back", "shooter", "splash", "boards", "bucket", "buckets", "tipoff", "jersey",
    "team", "squad", "roster", "rival", "victory", "defeat", "win", "streak", "clock", "quarter",
    "halftime", "whistle", "referee", "triple", "double",
    // sneakers
    "kicks", "sneakers", "sneaker", "shoes", "shoe", "laces", "sole", "soles", "retro", "colorway",
    "suede", "leather", "mesh", "heat", "grail", "drop", "resale", "hype", "box", "pair", "fresh",
    "crisp", "clean", "flight", "air", "jordans", "collector", "release", "raffle", "restock",
    "deadstock", "heel", "toe", "tongue", "midsole", "stitch", "canvas", "vintage", "limited",
    "exclusive", "collab", "swoosh", "stripes", "size", "fit", "drip", "style", "flex", "fashion",
    "brand", "store", "sale",
    // streets
    "corner", "city", "concrete", "pavement", "alley", "hustle", "grind", "struggle", "crew",
    "fam", "homie", "street", "streets", "town", "borough", "subway", "night", "light", "lights",
    "siren", "neighborhood", "building", "rooftop", "avenue", "downtown", "uptown", "traffic",
    "graffiti", "wall", "gate", "fence", "porch", "stoop", "park", "bridge", "train", "hood",
    "dream", "dreams", "money", "cash", "paper", "bread", "dough", "gold", "chain", "chains",
    "ice", "diamond", "diamonds", "crown", "throne", "king", "queen", "boss", "empire", "legacy",
    "power", "respect", "loyalty", "pride", "hunger", "fire", "flame", "smoke", "rain", "storm",
    "thunder", "lightning", "sky", "star", "stars", "moon", "sun", "sunset", "sunrise", "midnight",
    "morning", "summer", "winter", "cold", "weather",
    // music
    "beat", "beats", "bars", "bar", "rhyme", "rhymes", "flow", "mic", "microphone", "verse",
    "verses", "hook", "track", "rap", "rapper", "rhythm", "bass", "drum", "drums", "snare",
    "vinyl", "studio", "stage", "show", "cypher", "freestyle", "lyric", "lyrics", "poet", "poetry",
    "story", "sample", "loop", "tempo", "melody", "chorus", "anthem", "album", "mixtape", "single",
    "headphones", "speaker", "speakers", "volume", "radio", "soul", "funk", "groove", "vibe",
    "vibes", "tune", "song", "sound", "noise", "encore", "spotlight", "classic", "banger", "remix",
    "producer", "turntable", "scratch",
    // everyday words
    "time", "mind", "life", "day", "way", "stay", "play", "pay", "say", "away", "today", "real",
    "feel", "deal", "meal", "steel", "wheel", "bold", "told", "hold", "goal", "roll", "control",
    "higher", "desire", "hour", "flower", "tower", "name", "fame", "same", "claim", "aim", "frame",
    "shame", "blame", "came", "go", "know", "slow", "glow", "grow", "below", "though", "pro",
    "low", "snow", "throw", "bright", "fight", "right", "tight", "sight", "height", "might",
    "write", "white", "tonight", "heart", "start", "art", "apart", "smart", "chart", "part",
    "dark", "spark", "mark", "sharp", "fast", "last", "past", "blast", "class", "mass", "top",
    "stop", "pop", "shop", "nonstop", "hop", "true", "blue", "through", "new", "view", "few",
    "move", "prove", "smooth", "truth", "youth", "proof", "roof", "loose", "juice", "choose",
    "lose", "news", "clues", "blues", "cruise", "better", "letter", "together", "forever", "never",
    "whatever", "clever", "pressure", "measure", "treasure", "pleasure", "ready", "steady",
    "heavy", "honey", "funny", "sunny", "easy", "crazy", "lazy", "amazing", "blazing", "praise",
    "days", "ways", "maze", "phase", "pain", "gain", "brain", "lane", "plane", "main", "remain",
    "explain", "insane", "champagne", "vision", "mission", "decision", "precision", "position",
    "ambition", "tradition", "nation", "station", "creation", "motivation", "dedication",
    "reputation", "generation", "celebration", "destination", "inspiration", "emotion", "motion",
    "ocean", "devotion", "promotion", "believe", "achieve", "receive", "leave", "sleeve", "love",
    "above", "glove", "enough", "tough", "rough", "stuff", "bluff",
    "necessary", "sanctuary", "visionary", "missionary", "dictionary", "january", "dreamers",
    "leaders", "believers", "features", "teachers", "creatures", "keepers", "streamers",
    // legends and eras
    "jordan", "kobe", "mamba", "lebron", "iverson", "answer", "eminem", "detroit", "bronx",
    "boombox", "cassette", "cassettes", "walkman", "turntables", "maiden", "trooper", "eddie",
    "gallop", "galloping", "metal", "iron", "eighties", "seventies", "nineties", "adidas",
    "mentality", "comeback", "hooper", "hoopers", "dancer", "rocks", "drama", "upon", "icon",
    "marathon", "lonely", "goalie", "only", "slowly", "super", "flu", "mile", "rings",
};

constexpr std::string_view kPortuguese[] = {
    // basquete
    "basquete", "bola", "quadra", "cesta", "aro", "rede", "tabela", "jogo", "jogada", "jogador",
    "time", "ponto", "pontos", "arremesso", "enterrada", "rebote", "garrafão", "drible", "passe",
    "toco", "bloqueio", "torcida", "campeão", "título", "final", "temporada", "craque", "lenda",
    "placar", "cronômetro", "tempo", "técnico", "treino", "banco", "reserva", "titular", "armador",
    "pivô", "lance", "falta", "bandeja", "contra-ataque", "marcação", "defesa", "ataque",
    "vitória", "derrota", "troféu", "medalha", "taça", "ginásio", "arquibancada", "apito", "juiz",
    "rival", "clássico", "decisão", "prorrogação", "virada", "cravada", "salto", "altura",
    // tênis
    "tênis", "pisante", "cadarço", "sola", "par", "caixa", "lançamento", "estilo", "couro",
    "camurça", "colecionador", "edição", "raro", "exclusivo", "estoque", "vitrine", "marca",
    "modelo", "cano", "bico", "calcanhar", "palmilha", "cor", "novo", "limpo", "branco", "preto",
    "vermelho", "dourado", "brilho", "moda",
    // rua
    "rua", "beco", "favela", "quebrada", "bairro", "esquina", "asfalto", "concreto", "cidade",
    "noite", "luz", "sirene", "corre", "trampo", "luta", "calçada", "viela", "praça", "ponte",
    "metrô", "ônibus", "laje", "muro", "grafite", "portão", "morro", "periferia", "comunidade",
    "parceiro", "irmão", "mano", "família", "respeito", "lealdade", "orgulho", "fome", "sonho",
    "sonhos", "dinheiro", "grana", "ouro", "corrente", "coroa", "trono", "rei", "rainha", "chefe",
    "império", "legado", "poder", "fogo", "chama", "fumaça", "chuva", "tempestade", "trovão",
    "céu", "estrela", "lua", "sol", "madrugada", "manhã", "verão", "inverno", "frio", "calor",
    "suor",
    // música
    "rima", "rimas", "batida", "verso", "rap", "microfone", "palco", "som", "grave", "caneta",
    "papel", "poesia", "poeta", "história", "memória", "glória", "refrão", "melodia", "ritmo",
    "disco", "vinil", "estúdio", "show", "plateia", "multidão", "letra", "samba", "baile",
    "groove", "música", "canção", "rádio", "fone", "volume", "festa", "dança", "cena", "arte",
    // palavras do dia a dia
    "vida", "lida", "saída", "ferida", "corrida", "subida", "partida", "comida", "bebida", "sorte",
    "forte", "norte", "morte", "porte", "coração", "razão", "pressão", "visão", "missão", "chão",
    "mão", "pão", "paixão", "emoção", "nação", "geração", "ambição", "tradição", "revolução",
    "solução", "direção", "atenção", "oração", "dono", "sono", "nome", "homem", "jovem", "gente",
    "frente", "quente", "mente", "sempre", "presente", "diferente", "semente", "valente", "amor",
    "dor", "flor", "valor", "melhor", "sonhador", "lutador", "vencedor", "clima", "cima", "prima",
    "estima", "autoestima", "logo", "povo", "ovo", "sua", "tua", "nua", "continua", "passado",
    "lado", "recado", "amarrado", "marcado", "gravado", "pesado", "parado", "suado", "caminhada",
    "estrada", "chegada", "largada", "pegada", "rapaziada", "mais", "rivais", "finais", "jamais",
    "iguais", "demais", "sinais", "reais", "ideais", "ninguém", "também", "além", "alguém", "bem",
    "trem", "vem", "tem", "cem", "refém", "voar", "parar", "ganhar", "sonhar", "brilhar", "chegar",
    "lutar", "jogar", "rimar", "cantar", "dançar", "olhar", "lugar", "mar", "luar", "fé", "pé",
    "até", "maré", "café", "axé", "você", "dia", "alegria", "magia", "energia", "sabedoria",
    "harmonia", "fantasia", "agonia", "correria", "trajetória", "escola", "cola", "hora", "agora",
    "embora", "fora", "aurora", "senhora", "demora", "mundo", "fundo", "segundo", "profundo",
    "vagabundo", "tudo", "mudo", "escudo", "veludo", "raça", "graça", "massa", "ameaça", "caça",
    "passa", "nada", "escada", "pesada", "jornada", "balada", "parada", "saudade", "verdade",
    "vontade", "liberdade", "humildade", "amizade", "realidade", "felicidade", "vaidade",
    // lendas e eras
    "mamba", "fita", "cassete", "oitenta", "setenta", "noventa", "cinquenta", "metal", "ferro",
    "donzela", "resposta", "anel", "anéis", "toca-discos", "lendário", "detroit",
};


struct Entry {
    std::string_view word;
    Pronunciation pron;
    std::string key;
    std::string skeleton;
};

template <std::size_t N>
std::vector<Entry> build(const std::string_view (&words)[N], Language lang) {
    std::vector<Entry> entries;
    entries.reserve(N);
    for (std::string_view w : words) {
        Entry e{w, pronounce(w, lang), {}, {}};
        e.key = e.pron.rhyme_key();
        e.skeleton = e.pron.vowel_skeleton();
        entries.push_back(std::move(e));
    }
    return entries;
}

/// Transcribed once per language on first use, then shared read-only.
const std::vector<Entry>& entries(Language lang) {
    if (lang == Language::Portuguese) {
        static const std::vector<Entry> portuguese = build(kPortuguese, Language::Portuguese);
        return portuguese;
    }
    static const std::vector<Entry> english = build(kEnglish, Language::English);
    return english;
}

/// Coarse class of the sound a rhyme key ends on; slant rhymes that share it (game/rain:
/// both nasal) sound closer than ones that don't (game/cake).
int coda_class(std::string_view key, Language lang) {
    if (key.empty()) return 0;
    const char c = key.back();
    if (is_vowel_phoneme(c, lang) || c == 'y' || c == 'w') return 0;
    switch (c) {
    case 'm': case 'n': case 'N': return 1;
    case 'p': case 't': case 'k': case 'b': case 'd': case 'g': return 2;
    case 'l': case 'r': case 'R': case 'L': return 3;
    default: return 4; // fricatives and affricates
    }
}

} // namespace

std::span<const std::string_view> lexicon(Language lang) {
    if (lang == Language::Portuguese) return kPortuguese;
    return kEnglish;
}

std::vector<RhymeMatch> find_rhymes(std::string_view word, Language lang, std::size_t limit) {
    if (lang == Language::Auto) lang = detect_language(word);
    const Pronunciation query = pronounce(word, lang);
    if (query.nuclei.empty()) return {};
    const std::string key = query.rhyme_key();
    const std::string skeleton = query.vowel_skeleton();
    const std::vector<std::string> query_vowels = query.vowels();
    const std::string self = normalize_word(word);
    const int query_coda = coda_class(key, lang);

    struct Scored {
        RhymeMatch match;
        bool same_coda;
        int syllable_gap;
    };
    std::vector<Scored> scored;
    for (const Entry& e : entries(lang)) {
        if (e.word == self) continue;
        RhymeKind kind = RhymeKind::None;
        if (e.key == key) kind = RhymeKind::Perfect;
        else if (e.skeleton == skeleton) kind = RhymeKind::Slant;
        if (kind == RhymeKind::None) continue;
        const std::vector<std::string> vowels = e.pron.vowels();
        const int matched = std::max(1, matching_tail(query_vowels, vowels, lang));
        scored.push_back({{std::string(e.word), kind, matched},
                          coda_class(e.key, lang) == query_coda,
                          std::abs(e.pron.syllables() - query.syllables())});
    }
    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        // Best first: kind, multisyllabic length, same final sound class, similar length, a-z.
        return std::tuple(-static_cast<int>(a.match.kind), -a.match.syllables_matched, !a.same_coda,
                          a.syllable_gap, a.match.word) <
               std::tuple(-static_cast<int>(b.match.kind), -b.match.syllables_matched, !b.same_coda,
                          b.syllable_gap, b.match.word);
    });
    std::vector<RhymeMatch> out;
    for (std::size_t i = 0; i < scored.size() && out.size() < limit; ++i)
        out.push_back(std::move(scored[i].match));
    return out;
}

std::vector<std::string> rhymes_for(std::string_view word, Language lang, std::size_t limit) {
    std::vector<std::string> words;
    for (RhymeMatch& m : find_rhymes(word, lang, limit)) words.push_back(std::move(m.word));
    return words;
}

} // namespace street::bars
