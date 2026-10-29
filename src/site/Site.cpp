#include "Site.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <set>

#include "flash/Stage.h"
#include "game/Game.h"
#include "game/Map.h"
#include "game/Shell.h"
#include "stb_image.h"

namespace site {

// ------------------------------------------------------------------ page texts
// The original page templates aren't in the archive; these are the visible labels, re-typed.

namespace {
struct Ui {
    const char* tabs[4];
    const char* strip[3];
    const char* help;
    const char* lost;
    const char* pilot;
    const char* prisonerFor;  // %d days
    const char* exploration;
    // My Page
    const char* myTitle;
    const char* position;
    const char* minerals;
    const char* envelopes;
    const char* missiles;
    const char* engine;
    const char* radar;
    const char* drones;
    const char* fuel;
    const char* fuelFree;
    const char* missionsDone;
    const char* items;
    const char* shop;
    const char* none;
    // Records
    const char* recTitle;
    const char* recNote;
    const char* levels;
    const char* cleared;
    const char* lostLevels;
    const char* minTotal;
    const char* bestMin;
    const char* fired;
    const char* playTime;
    const char* since;
    // Help
    const char* helpCards[4][2];
    const char* days[7];
    const char* daysShort[7];
    const char* months[12];
};

const Ui UI_EN = {
    {"MISSIONS", "MY PAGE", "RECORDS", "HELP"},
    {"USE AN OPTION DURING THE GAME", "MISSILE", "PAUSE\n(+ OPTIONS)"},
    "HELP",
    "If you are lost, you can consult <a href=\"1\">THE GAME HELP</a>.",
    "Pilot",
    "a prisoner for %d days",
    "Exploration",
    "Envelope status",
    "Position",
    "Minerals",
    "Emergency envelopes",
    "Missiles",
    "Engine power",
    "Radar range",
    "Drones",
    "Hydrogen capsules (liquid + solid)",
    "Fuel is free in this offline version.",
    "Missions completed",
    "Equipment collected",
    "Bought at the astroports",
    "nothing yet",
    "Personal records",
    "The online rankings are gone; these are your own figures, kept on this computer.",
    "Sectors played",
    "Sectors cleared",
    "Sectors lost",
    "Minerals extracted",
    "Best haul in one sector",
    "Missiles fired",
    "Time spent drilling",
    "Prisoner since",
    {{"The game",
      "You are a prisoner of the <strong>ESCorp</strong>, sentenced to mine the conglomerates of an infinite "
      "universe with your <strong>envelope</strong> (the pad) and a drilling ball. Clear a sector to extract its "
      "minerals and to travel further; new orders arrive in the <a href=\"0\">MISSIONS</a> tab as you progress."},
     {"Controls",
      "<strong>Mouse</strong>: move the envelope; click to launch the ball and to use an option you have caught.<br/>"
      "<strong>SPACE</strong>: fire a missile, once you own one.<br/>"
      "<strong>P</strong> or <strong>ESC</strong>: pause the game (and pick an option).<br/>"
      "<strong>F11</strong> or <strong>Alt+Enter</strong>: full screen."},
     {"Envelopes",
      "Losing every ball costs one <strong>emergency envelope</strong>. Extra envelopes, missiles and "
      "engine upgrades are found through missions or bought at the astroports."},
     {"The galaxy map",
      "Click one of the highlighted squares around your envelope to fly there and drill it; a better "
      "<strong>engine</strong> reaches further. Alien icons are <strong>astroports</strong>: stop on one and "
      "open the shop with the button at the bottom of the map. The spanner button holds your preferences.<br/>"
      "Sectors you have cleared stay lit (their minerals are gone), skulls mark the ones where you lost, and "
      "your <strong>radar</strong> decides how far into the dark you can fly."}},
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
     "December"},
};

const Ui UI_FR = {
    {"MISSIONS", "MA PAGE", "RECORDS", "AIDE"},
    {"UTILISER UNE OPTION PENDANT LA PARTIE", "MISSILE", "PAUSE\n(+ OPTIONS)"},
    "AIDE",
    "Si vous \xC3\xAAtes perdu, vous pouvez consulter <a href=\"1\">L'AIDE DU JEU</a>.",
    "Pilote",
    "prisonnier depuis %d jours",
    "Exploration",
    "\xC3\x89tat de l'enveloppe",
    "Position",
    "Minerai",
    "Enveloppes de secours",
    "Missiles",
    "Puissance du moteur",
    "Port\xC3\xA9" "e du radar",
    "Drones",
    "Capsules d'hydrog\xC3\xA8ne (liquide + solide)",
    "Le carburant est gratuit dans cette version hors ligne.",
    "Missions accomplies",
    "\xC3\x89quipement r\xC3\xA9" "cup\xC3\xA9r\xC3\xA9",
    "Achet\xC3\xA9 dans les astroports",
    "rien pour l'instant",
    "Records personnels",
    "Les classements en ligne ont disparu ; voici vos propres chiffres, gard\xC3\xA9s sur cet ordinateur.",
    "Secteurs jou\xC3\xA9s",
    "Secteurs nettoy\xC3\xA9s",
    "Secteurs perdus",
    "Minerai extrait",
    "Meilleure r\xC3\xA9" "colte en un secteur",
    "Missiles tir\xC3\xA9s",
    "Temps de forage",
    "Prisonnier depuis le",
    {{"Le jeu",
      "Vous \xC3\xAAtes un prisonnier de l'<strong>ESCorp</strong>, condamn\xC3\xA9 \xC3\xA0 forer les conglom\xC3\xA9rats "
      "d'un univers infini avec votre <strong>enveloppe</strong> et une bille de forage. Nettoyez un secteur pour "
      "en extraire le minerai et aller plus loin ; les nouveaux ordres arrivent dans l'onglet "
      "<a href=\"0\">MISSIONS</a>."},
     {"Commandes",
      "<strong>Souris</strong> : d\xC3\xA9placer l'enveloppe ; cliquer pour lancer la bille et utiliser une option "
      "attrap\xC3\xA9" "e.<br/><strong>ESPACE</strong> : tirer un missile, quand vous en avez un.<br/>"
      "<strong>P</strong> ou <strong>ECHAP</strong> : pause (et choix d'une option).<br/>"
      "<strong>F11</strong> ou <strong>Alt+Entr\xC3\xA9" "e</strong> : plein \xC3\xA9" "cran."},
     {"Enveloppes",
      "Perdre toutes ses billes co\xC3\xBBte une <strong>enveloppe de secours</strong>. Enveloppes, missiles et "
      "moteurs s'obtiennent par les missions ou dans les astroports."},
     {"La carte de la galaxie",
      "Cliquez sur une des cases en surbrillance autour de votre enveloppe pour y aller et la forer ; un meilleur "
      "<strong>moteur</strong> va plus loin. Les ic\xC3\xB4nes extraterrestres sont des <strong>astroports</strong> : "
      "arr\xC3\xAAtez-vous dessus et ouvrez la boutique avec le bouton en bas de la carte. La cl\xC3\xA9 donne acc\xC3\xA8s "
      "aux pr\xC3\xA9" "f\xC3\xA9rences.<br/>Les secteurs nettoy\xC3\xA9s restent \xC3\xA9" "clair\xC3\xA9s (leur minerai est \xC3\xA9puis\xC3\xA9), "
      "les t\xC3\xAAtes de mort marquent vos \xC3\xA9" "checs, et votre <strong>radar</strong> fixe jusqu'o\xC3\xB9 vous pouvez "
      "vous aventurer dans l'inconnu."}},
    {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"},
    {"dim", "lun", "mar", "mer", "jeu", "ven", "sam"},
    {"janvier", "f\xC3\xA9vrier", "mars", "avril", "mai", "juin", "juillet", "ao\xC3\xBBt", "septembre", "octobre",
     "novembre", "d\xC3\xA9" "cembre"},
};

const Ui UI_DE = {
    {"MISSIONEN", "MEINE SEITE", "REKORDE", "HILFE"},
    {"EINE OPTION IM SPIEL BENUTZEN", "RAKETE", "PAUSE\n(+ OPTIONEN)"},
    "HILFE",
    "Wenn du nicht weiterwei\xC3\x9Ft, lies die <a href=\"1\">SPIELHILFE</a>.",
    "Pilot",
    "seit %d Tagen H\xC3\xA4" "ftling",
    "Erkundung",
    "Zustand der H\xC3\xBClle",
    "Position",
    "Mineralien",
    "Notfallh\xC3\xBCllen",
    "Raketen",
    "Antriebsleistung",
    "Radarreichweite",
    "Drohnen",
    "Wasserstoffkapseln (fl\xC3\xBCssig + fest)",
    "Treibstoff ist in dieser Offline-Version kostenlos.",
    "Erf\xC3\xBCllte Missionen",
    "Gesammelte Ausr\xC3\xBCstung",
    "In den Astroports gekauft",
    "noch nichts",
    "Pers\xC3\xB6nliche Rekorde",
    "Die Online-Ranglisten gibt es nicht mehr; hier stehen deine eigenen Werte, auf diesem Computer gespeichert.",
    "Gespielte Sektoren",
    "Ger\xC3\xA4umte Sektoren",
    "Verlorene Sektoren",
    "Abgebaute Mineralien",
    "Beste Ausbeute in einem Sektor",
    "Abgefeuerte Raketen",
    "Bohrzeit",
    "H\xC3\xA4" "ftling seit",
    {{"Das Spiel",
      "Du bist ein H\xC3\xA4" "ftling der <strong>ESCorp</strong> und musst die Konglomerate eines unendlichen "
      "Universums mit deiner <strong>H\xC3\xBClle</strong> und einer Bohrkugel abbauen. R\xC3\xA4ume einen Sektor, "
      "um seine Mineralien zu f\xC3\xB6rdern und weiterzufliegen; neue Befehle erscheinen unter "
      "<a href=\"0\">MISSIONEN</a>."},
     {"Steuerung",
      "<strong>Maus</strong>: H\xC3\xBClle bewegen; klicken, um die Kugel abzuschie\xC3\x9F" "en und eine gefangene Option "
      "zu benutzen.<br/><strong>LEERTASTE</strong>: Rakete abfeuern, sobald du eine hast.<br/>"
      "<strong>P</strong> oder <strong>ESC</strong>: Pause (und Option w\xC3\xA4hlen).<br/>"
      "<strong>F11</strong> oder <strong>Alt+Enter</strong>: Vollbild."},
     {"H\xC3\xBCllen",
      "Wer alle Kugeln verliert, verbraucht eine <strong>Notfallh\xC3\xBClle</strong>. H\xC3\xBCllen, Raketen und "
      "Antriebe gibt es durch Missionen oder in den Astroports."},
     {"Die Galaxiekarte",
      "Klicke auf eines der hervorgehobenen Felder um deine H\xC3\xBClle, um dorthin zu fliegen und es abzubauen; "
      "ein besserer <strong>Antrieb</strong> reicht weiter. Alien-Symbole sind <strong>Astroports</strong>: halte "
      "dort an und \xC3\xB6" "ffne den Laden mit dem Knopf unten auf der Karte. Der Schraubenschl\xC3\xBCssel \xC3\xB6" "ffnet "
      "die Einstellungen.<br/>Ger\xC3\xA4umte Sektoren bleiben hell (ihre Mineralien sind abgebaut), Totenk\xC3\xB6pfe "
      "markieren verlorene, und dein <strong>Radar</strong> bestimmt, wie weit du ins Dunkle fliegen kannst."}},
    {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"},
    {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},
    {"Januar", "Februar", "M\xC3\xA4rz", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober",
     "November", "Dezember"},
};

const Ui UI_ES = {
    {"MISIONES", "MI P\xC3\x81GINA", "R\xC3\x89" "CORDS", "AYUDA"},
    {"USAR UNA OPCI\xC3\x93N DURANTE LA PARTIDA", "MISIL", "PAUSA\n(+ OPCIONES)"},
    "AYUDA",
    "Si est\xC3\xA1s perdido, puedes consultar <a href=\"1\">LA AYUDA DEL JUEGO</a>.",
    "Piloto",
    "prisionero desde hace %d d\xC3\xAD" "as",
    "Exploraci\xC3\xB3n",
    "Estado de la envoltura",
    "Posici\xC3\xB3n",
    "Minerales",
    "Envolturas de emergencia",
    "Misiles",
    "Potencia del motor",
    "Alcance del radar",
    "Drones",
    "C\xC3\xA1psulas de hidr\xC3\xB3geno (l\xC3\xADquido + s\xC3\xB3lido)",
    "El combustible es gratis en esta versi\xC3\xB3n sin conexi\xC3\xB3n.",
    "Misiones cumplidas",
    "Equipo recogido",
    "Comprado en los astropuertos",
    "nada todav\xC3\xAD" "a",
    "R\xC3\xA9" "cords personales",
    "Las clasificaciones en l\xC3\xAD" "nea ya no existen; estas son tus propias cifras, guardadas en este ordenador.",
    "Sectores jugados",
    "Sectores limpiados",
    "Sectores perdidos",
    "Minerales extra\xC3\xAD" "dos",
    "Mejor botin en un sector",
    "Misiles disparados",
    "Tiempo de perforaci\xC3\xB3n",
    "Prisionero desde",
    {{"El juego",
      "Eres un prisionero de la <strong>ESCorp</strong>, condenado a perforar los conglomerados de un universo "
      "infinito con tu <strong>envoltura</strong> y una bola perforadora. Limpia un sector para extraer sus "
      "minerales y seguir viajando; las nuevas \xC3\xB3rdenes llegan a la pesta\xC3\xB1" "a <a href=\"0\">MISIONES</a>."},
     {"Controles",
      "<strong>Rat\xC3\xB3n</strong>: mover la envoltura; clic para lanzar la bola y usar una opci\xC3\xB3n atrapada.<br/>"
      "<strong>ESPACIO</strong>: disparar un misil, cuando tengas uno.<br/>"
      "<strong>P</strong> o <strong>ESC</strong>: pausa (y elegir una opci\xC3\xB3n).<br/>"
      "<strong>F11</strong> o <strong>Alt+Intro</strong>: pantalla completa."},
     {"Envolturas",
      "Perder todas las bolas cuesta una <strong>envoltura de emergencia</strong>. Envolturas, misiles y motores "
      "se consiguen con las misiones o en los astropuertos."},
     {"El mapa de la galaxia",
      "Haz clic en una de las casillas resaltadas alrededor de tu envoltura para viajar all\xC3\xAD y perforarla; "
      "un <strong>motor</strong> mejor llega m\xC3\xA1s lejos. Los iconos alien\xC3\xAD" "genas son <strong>astropuertos</strong>: "
      "detente en uno y abre la tienda con el bot\xC3\xB3n de abajo del mapa. La llave abre las preferencias.<br/>"
      "Los sectores limpiados quedan iluminados (sus minerales se agotaron), las calaveras marcan los perdidos y "
      "tu <strong>radar</strong> decide hasta d\xC3\xB3nde puedes volar en la oscuridad."}},
    {"domingo", "lunes", "martes", "mi\xC3\xA9rcoles", "jueves", "viernes", "s\xC3\xA1" "bado"},
    {"dom", "lun", "mar", "mi\xC3\xA9", "jue", "vie", "s\xC3\xA1" "b"},
    {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre",
     "diciembre"},
};

const Ui* ui = &UI_EN;

// colours sampled from the original page
constexpr unsigned C_PANEL_ITEM = 0x3C4A80, C_INFO = 0x44528A, C_TEXT = 0xE6EAF8, C_STRONG = 0xFFE060,
                   C_EM = 0xB4FF50, C_TITLE = 0xFFD030, C_DATE = 0xB4BCD8, C_LINK = 0xFFA040, C_TAB = 0xFFFFFF,
                   C_STRIP = 0xC8D0F0, C_VIG_BORDER = 0x9AA6D0;

std::string pilotName() {
    const char* n = std::getenv("USERNAME");
    if (!n || !*n) n = std::getenv("USER");
    std::string s = (n && *n) ? n : ui->pilot;
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] -= 32;
    return s;
}

std::string numStr(long long v) {
    // 12345 -> "12 345"
    std::string s = std::to_string(std::llabs(v)), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i > 0 && (s.size() - i) % 3 == 0) out += ' ';
        out += s[i];
    }
    return v < 0 ? "-" + out : out;
}
}  // namespace

// ------------------------------------------------------------------ setup

Site::~Site() {
    for (auto& [k, v] : tex)
        if (v.t) SDL_DestroyTexture(v.t);
    if (hand) SDL_DestroyCursor(hand);
    if (arrow) SDL_DestroyCursor(arrow);
}

Tex* Site::load(const std::string& file, bool optional) {
    auto it = tex.find(file);
    if (it != tex.end()) return it->second.t ? &it->second : nullptr;
    Tex& tx = tex[file];
    int w, h, n;
    unsigned char* px = stbi_load((dir + "/" + file).c_str(), &w, &h, &n, 4);
    if (!px) {
        if (!optional) SDL_Log("site: can't load %s", file.c_str());
        return nullptr;
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, px, w * 4);
    tx.t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    stbi_image_free(px);
    if (!tx.t) return nullptr;
    SDL_SetTextureBlendMode(tx.t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tx.t, SDL_SCALEMODE_LINEAR);
    tx.w = w;
    tx.h = h;
    return &tx;
}

bool Site::init(SDL_Renderer* r, const std::string& siteDir, const std::string& dataDir, const std::string& lng) {
    ren = r;
    dir = siteDir;
    lang = lng;
    ui = lang == "fr" ? &UI_FR : lang == "de" ? &UI_DE : lang == "es" ? &UI_ES : &UI_EN;
    size_t sz = 0;
    void* raw = SDL_LoadFile((dataDir + "/site_" + lang + ".json").c_str(), &sz);
    if (!raw) raw = SDL_LoadFile((dataDir + "/site_en.json").c_str(), &sz);
    if (raw) {
        try {
            texts = nlohmann::json::parse((const char*)raw, (const char*)raw + sz);
        } catch (...) {
        }
        SDL_free(raw);
    }
    RichText::setLibrary(flash::Stage::get()->mainLib);
    std::string frame = lang == "fr" ? "core_center.png" : "core_center_" + lang + ".png";
    if (!load(frame, true)) frame = "core_center_en.png";
    tex["frame"] = tex[frame];
    tex.erase(frame);
    if (!tex["frame"].t) {
        SDL_Log("site: page graphics missing in %s", dir.c_str());
        return false;
    }
    {
        std::string assets = siteDir;
        size_t cut = assets.find_last_of("/\\");
        assets = cut == std::string::npos ? "." : assets.substr(0, cut);
        if (!inventory.init(assets, dataDir, lang)) SDL_Log("site: inventory art (assets/inv.json) missing");
    }
    hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);

    TextStyle tabSt;
    tabSt.size = 8.5;
    tabSt.bold = true;
    tabSt.color = C_TAB;
    tabStyle = tabSt;
    for (int i = 0; i < TabCount; i++) {
        tabText[i] = i < 4 ? ui->tabs[i]
                           : i == Universe ? tr("UNIVERSE MAP", "UNIVERS", "UNIVERSUM", "UNIVERSO") : std::string("DEBUG");
        tabLabel[i].setPlain(tabText[i], tabSt);
        tabLabel[i].layout(0);
    }
    TextStyle stripSt;
    stripSt.size = 7.5;
    stripSt.bold = true;
    stripSt.color = C_STRIP;
    for (int i = 0; i < 3; i++) stripText[i].setPlain(ui->strip[i], stripSt);
    stripText[0].layout(112);
    stripText[1].layout(0);
    stripText[2].layout(0);  // explicit line break
    TextStyle helpSt = tabSt;
    helpSt.color = 0xFFFFFF;
    helpLabel.setPlain(ui->help, helpSt);
    helpLabel.layout(0);
    TextStyle footSt;
    footSt.size = 8.5;
    footSt.color = 0xB4BCD8;
    footer.setPlain(tr("Unofficial fan-made port of Alphabounce (Motion Twin, 2007), not affiliated with Motion Twin "
                       "- game licensed CC BY-NC-SA 4.0",
                       "Portage non officiel d'Alphabounce (Motion Twin, 2007) r\xC3\xA9" "alis\xC3\xA9 par des fans, "
                       "sans lien avec Motion Twin - jeu sous licence CC BY-NC-SA 4.0",
                       "Inoffizielle Fan-Portierung von Alphabounce (Motion Twin, 2007), nicht mit Motion Twin "
                       "verbunden - Lizenz CC BY-NC-SA 4.0",
                       "Adaptaci\xC3\xB3n no oficial de Alphabounce (Motion Twin, 2007) hecha por fans, sin relaci\xC3\xB3n "
                       "con Motion Twin - licencia CC BY-NC-SA 4.0"),
                    footSt);
    loadFeedPrefs();
    return true;
}

std::string Site::t(const std::string& key) const {
    auto it = texts.find(key);
    return (it != texts.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

Tex* Site::vignette(int n) {
    std::string name = t("vig" + std::to_string(n));
    if (name.empty()) name = "vig" + std::to_string(n);
    return load("vigs/" + name + ".png", true);
}

// ------------------------------------------------------------------ layout

void Site::layout() {
    auto* st = flash::Stage::get();
    int ow, oh;
    SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
    // the original page is 900x500; it scales with the window, and any extra room goes to the panels
    u = std::max(0.5, std::min(ow / 900.0, oh / 500.0));
    PW = ow / u;
    PH = oh / u;
    double hg = PH - 140;                 // game panel height (360 on the original page)
    double maxGameW = PW - 17 - 8 - 300;  // keep at least 300 px for the right-hand panel
    if (hg * 400 / 360 > maxGameW) hg = maxGameW * 360 / 400;
    hg = std::max(hg, 100.0);
    double gw = hg * 400 / 360;
    game = {9, 75, gw, hg};
    strip = {8, 75 + hg + 7, gw + 2, 31};
    panel = {gw + 17, 54, PW - 8 - (gw + 17), strip.y + strip.h - 54};
    content = {panel.x + 6, panel.y + 30, panel.w - 12 - 14, panel.h - 36};
    double tw = std::min(79.0, (panel.w - 14) / TabCount - 2);
    for (int i = 0; i < TabCount; i++) tabsR[i] = {panel.x + 7 + i * (tw + 2), panel.y - 4, tw, 22};
    // a label too long for its tab ("UNIVERSE MAP") is set smaller
    if (tw != tabFitW) {
        tabFitW = tw;
        for (int i = 0; i < TabCount; i++) {
            TextStyle st = tabStyle;
            for (int k = 0; k < 8; k++) {
                tabLabel[i].setPlain(tabText[i], st);
                tabLabel[i].layout(0);
                if (tabLabel[i].width() <= tw - 6) break;
                st.size -= 0.5;
            }
        }
    }
    helpBtn = {strip.x + strip.w - 57, strip.y + 2, 53, 27};
    scrollTrack = {content.x + content.w + 3, content.y, 10, content.h};
    st->gameRect = {(float)(game.x * u), (float)(game.y * u), (float)(game.w * u), (float)(game.h * u)};
    st->updateView();
    if (builtWidth != content.w) rebuild();
}

// ------------------------------------------------------------------ panel content

Site::Card& Site::addCard(Tex* icon, const std::string& title, const std::string& html, const std::string& date,
                          Tex* vig) {
    cards.emplace_back();
    Card& c = cards.back();
    c.icon = icon;
    c.vig = vig;
    TextStyle ts;
    ts.size = 11.5;
    ts.bold = true;
    ts.smallCaps = true;
    ts.color = C_TITLE;
    c.title.setPlain(title, ts);
    TextStyle ds;
    ds.size = 9;
    ds.color = C_DATE;
    c.date.setPlain(date, ds);
    c.date.layout(0);
    HtmlTheme th;
    th.base.size = 10.5;
    th.base.color = C_TEXT;
    th.strong.color = C_STRONG;
    th.strong.bold = true;
    th.em.color = C_EM;
    th.em.bold = true;
    th.link.color = C_LINK;
    th.link.bold = true;
    th.link.underline = true;
    th.paraGap = 5;
    c.body.set(parseHtml(html, th));
    return c;
}

std::string Site::fillPlaceholders(std::string s) const {
    const std::string key = "::name::", name = pilotName();
    for (size_t p; (p = s.find(key)) != std::string::npos;) s.replace(p, key.size(), name);
    return s;
}

std::string Site::formatDate(long long tt) const {
    if (tt <= 0) return "";
    time_t tv = (time_t)tt;
    struct tm lt {};
#ifdef _WIN32
    localtime_s(&lt, &tv);
#else
    localtime_r(&tv, &lt);
#endif
    std::string f = t("date_fmt"), out;
    if (f.empty()) f = "%a, %B %d at %I:%M %p";
    char buf[16];
    for (size_t i = 0; i < f.size(); i++) {
        if (f[i] != '%' || i + 1 >= f.size()) {
            out += f[i];
            continue;
        }
        char c = f[++i];
        switch (c) {
            case 'a': out += ui->daysShort[lt.tm_wday]; break;
            case 'A': out += ui->days[lt.tm_wday]; break;
            case 'B': out += ui->months[lt.tm_mon]; break;
            case 'd': SDL_snprintf(buf, sizeof buf, "%02d", lt.tm_mday); out += buf; break;
            case 'Y': out += std::to_string(lt.tm_year + 1900); break;
            case 'H': SDL_snprintf(buf, sizeof buf, "%02d", lt.tm_hour); out += buf; break;
            case 'I': SDL_snprintf(buf, sizeof buf, "%02d", (lt.tm_hour + 11) % 12 + 1); out += buf; break;
            case 'M': SDL_snprintf(buf, sizeof buf, "%02d", lt.tm_min); out += buf; break;
            case 'p': out += lt.tm_hour < 12 ? "AM" : "PM"; break;
            default: out += c;
        }
    }
    return out;
}

std::string Site::tr(const char* en, const char* fr, const char* de, const char* es) const {
    return lang == "fr" ? fr : lang == "de" ? de : lang == "es" ? es : en;
}

void Site::loadFeedPrefs() {
    auto& so = flash::Stage::get()->sharedObject("pref");
    hideCompleted = so.value("hideCompleted", false);
    dismissed.clear();
    if (so["dismissed"].is_array())
        for (auto& n : so["dismissed"])
            if (n.is_number()) dismissed.insert((int)n);
}

void Site::saveFeedPrefs() {
    auto* st = flash::Stage::get();
    auto& so = st->sharedObject("pref");
    so["hideCompleted"] = hideCompleted;
    so["dismissed"] = nlohmann::json(std::vector<int>(dismissed.begin(), dismissed.end()));
    st->flushSharedObject("pref");
}

void Site::buildMissions() {
    const PlayerInfo* pi = Cs::pi;
    // the help line, plus the feed's filters (a convenience of the port; the site had no such switches)
    std::string html = ui->lost;
    html += std::string("<br/><a href=\"3\">") +
            (hideCompleted ? tr("Show completed missions", "Afficher les missions accomplies",
                                "Erf\xC3\xBCllte Missionen zeigen", "Mostrar las misiones cumplidas")
                           : tr("Hide completed missions", "Masquer les missions accomplies",
                                "Erf\xC3\xBCllte Missionen ausblenden", "Ocultar las misiones cumplidas")) +
            "</a>";
    if (!dismissed.empty())
        html += std::string(" &nbsp; <a href=\"4\">") +
                tr("Bring back dismissed missions", "R\xC3\xA9" "afficher les missions retir\xC3\xA9" "es",
                   "Entfernte Missionen zur\xC3\xBC" "ckholen", "Recuperar las misiones descartadas") +
                " (" + std::to_string(dismissed.size()) + ")</a>";
    Card& info = addCard(load("small_post.gif"), "", html);
    info.info = true;
    if (!pi) return;
    // newest first, like the site's news feed; each mission appears once: its briefing while it's under
    // way, replaced by its outcome once completed
    std::set<int> shown;
    for (size_t k = pi->log.size(); k-- > 0;) {
        const MissionLogEntry& e = pi->log[k];
        if (!shown.insert(e.mission).second) continue;
        bool completed = e.state == 1;
        if (dismissed.count(e.mission) || (completed && hideCompleted)) continue;
        std::string n = std::to_string(e.mission);
        std::string body = t((completed ? "mission_end_" : "mission_desc_") + n);
        if (body.empty()) continue;
        body = fillPlaceholders(body);
        if (completed)
            body += "<p><a href=\"" + std::to_string(100 + e.mission) + "\">" +
                    tr("Dismiss", "Retirer", "Entfernen", "Descartar") + "</a></p>";
        addCard(load("small_mission.gif"), t("mission_name_" + n), body, formatDate(e.time), vignette(e.mission));
    }
}

void Site::buildMyPage() {
    const PlayerInfo* pi = Cs::pi;
    if (!pi) return;
    // header line of the page, then the inventory widget (inv.swf)
    long long days = pi->stats.firstPlayed > 0 ? ((long long)time(nullptr) - pi->stats.firstPlayed) / 86400 : 0;
    char daysBuf[128];
    SDL_snprintf(daysBuf, sizeof daysBuf, ui->prisonerFor, (int)days);
    std::string where = "[" + std::to_string(pi->x) + "][" + std::to_string(pi->y) + "]";
    std::string html = std::string(ui->position) + " : <em>" + where + "</em> &nbsp; " + daysBuf + " &nbsp; " +
                       ui->exploration + " : <strong>" + numStr(pi->clearedCount()) + "</strong>";
    Card& head = addCard(load("small_pad.gif"), pilotName(), html);
    head.compact = true;
    if (inventory.ready()) {
        inventory.build();
        cards.emplace_back();
        cards.back().widget = true;
    }
}

void Site::buildRecords() {
    const PlayerInfo* pi = Cs::pi;
    if (!pi) return;
    const PlayerStats& s = pi->stats;
    auto row = [](const char* label, const std::string& v) {
        return std::string(label) + " : <strong>" + v + "</strong><br/>";
    };
    long long secs = s.playSeconds;
    char tbuf[32];
    SDL_snprintf(tbuf, sizeof tbuf, "%lldh %02lldm", secs / 3600, (secs / 60) % 60);
    // rank badge grows with completed missions (13 badges on the site)
    int done = 0;
    for (int i = 0; i < pi->missions.size(); i++)
        if (pi->missions.get(i) == 1) done++;
    int rank = std::min(12, done / 3);
    std::string html = std::string("<p>") + ui->recNote + "</p>" + row(ui->levels, numStr(s.levels)) +
                       row(ui->cleared, numStr(s.cleared)) + row(ui->lostLevels, numStr(s.lost)) +
                       row(ui->minTotal, numStr(s.mineralsTotal)) + row(ui->bestMin, numStr(s.bestMinerals)) +
                       row(ui->fired, numStr(s.missilesFired)) + row(ui->playTime, tbuf) +
                       (s.firstPlayed > 0 ? row(ui->since, formatDate(s.firstPlayed)) : std::string());
    addCard(load("rank_" + std::to_string(rank) + ".gif"), ui->recTitle, html, "", load("large_planet.gif"));
}

void Site::buildHelp() {
    const char* icons[4] = {"small_earth.gif", "small_pad.gif", "faq_death.gif", "small_mission.gif"};
    for (int i = 0; i < 4; i++) addCard(load(icons[i]), ui->helpCards[i][0], ui->helpCards[i][1]);
}

void Site::buildDebug() {
    auto* st = flash::Stage::get();
    const char* rname = SDL_GetRendererName(ren);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
    std::string html = std::string("<a href=\"2\">") + (st->debug.showFps ? "Hide" : "Show") +
                       " the frame rate</a> (or press <strong>F3</strong>)<br/>"
                       "The overlay sits over the logo: frames drawn per second, the time spent "
                       "updating and drawing each frame, how many glowing or blurred objects were drawn, "
                       "how many of them needed a slow GPU read-back, and the scratch textures in use.";
    addCard(load("small_options.gif", true), "Frame rate", html);
    std::string travel = std::string("<a href=\"5\">") + (st->debug.freeTravel ? "Turn off" : "Turn on") +
                         " free travel</a> : <strong>" + (st->debug.freeTravel ? "ON" : "off") +
                         "</strong><br/>Click any square on the galaxy map to fly there and play it, ignoring "
                         "your engine's range, the radar, fuel and the ESCorp's zone. Levels and saves work as "
                         "usual, so missions can be completed this way.";
    addCard(load("small_earth.gif", true), "Free travel", travel);
    std::string land = std::string("<a href=\"6\">") + (st->debug.lander ? "Turn off" : "Turn on") +
                       " planet landing</a> : <strong>" + (st->debug.lander ? "ON" : "off") +
                       "</strong><br/>Adds the lander button to the galaxy map whenever you are on a planet, with "
                       "landing pods and the space suit, even without the Lander reactor. Land on the "
                       "ground, send the drones for crystals, walk out to visit the inhabitants, then fly off "
                       "the top of the screen to take off.";
    addCard(load("small_earth.gif", true), "Planet landing", land);
    std::string keys = "<strong>F3</strong> frame rate overlay<br/><strong>F4</strong> pause / resume the game<br/>"
                       "<strong>F5</strong> pause and advance one frame (hold to keep stepping)<br/>"
                       "<strong>1</strong> magnet, <strong>2</strong> glue, <strong>3</strong> bigger pad, "
                       "<strong>4</strong> fire ball, <strong>5</strong> laser, <strong>6</strong> multi-ball, "
                       "<strong>7</strong> extra life, <strong>8</strong> missile refill, <strong>9</strong> volt "
                       "ball, <strong>0</strong> halo ball<br/><strong>Shift</strong> + <strong>1</strong> to "
                       "<strong>9</strong>: kamikaze, charge, opening, quasar, generator, time, ultraviolet, yoyo, "
                       "+1 missile<br/>The capsules drop under the pointer, so you still have to catch them.";
    addCard(load("small_options.gif", true), "Debug keys", keys);
    std::string info = std::string("Renderer : <strong>") + (rname ? rname : "?") + "</strong><br/>Window : <strong>" +
                       std::to_string(ow) + " x " + std::to_string(oh) + "</strong><br/>Game scale : <strong>" +
                       std::to_string((int)std::lround(st->viewScale * 100)) + "%</strong><br/>Colour effects : <strong>" +
                       (st->customBlend ? "exact (GPU blend modes)" : "approximate (no custom blend modes)") +
                       "</strong>";
    addCard(load("small_mode.gif", true), "Display", info);
}

std::string Site::signature() const {
    const PlayerInfo* pi = Cs::pi;
    std::string s = std::to_string(tab_) + (flash::Stage::get()->debug.showFps ? "f" : "") +
                    (flash::Stage::get()->debug.freeTravel ? "t" : "") + (flash::Stage::get()->debug.lander ? "l" : "");
    if (pi)
        s += "|" + std::to_string(pi->log.size()) + "|" + std::to_string(pi->minerai) + "|" + std::to_string(pi->sectors.size()) + "|" + std::to_string(pi->x) +
             "," + std::to_string(pi->y) + "|" + std::to_string(pi->missile) + "|" + std::to_string(pi->stats.levels) +
             "|" + std::to_string(pi->items.size()) + "|" + std::to_string(pi->life);
    return s;
}

void Site::rebuild() {
    cards.clear();
    widgetR = R();
    builtWidth = content.w;
    builtSig = signature();
    switch (tab_) {
        case Missions: buildMissions(); break;
        case MyPage: buildMyPage(); break;
        case Records: buildRecords(); break;
        case Debug: buildDebug(); break;
        case Universe: break;  // drawn straight into the panel
        default: buildHelp(); break;
    }
    double y = 0;
    for (auto& c : cards) {
        c.y = y;
        if (c.widget) {
            c.h = Inventory::H * std::min(content.w / Inventory::W, 2.5);
        } else if (c.info) {
            c.body.layout(content.w - 34);
            c.h = std::max(18.0, c.body.height()) + 12;
        } else {
            double textX = c.vig ? 64 : 8;
            c.title.layout(0);
            c.head = (24 + c.title.width() + 16 + c.date.width() > content.w && c.date.width() > 0) ? 34 : 22;
            c.body.layout(content.w - textX - 8);
            double bodyH = std::max(c.body.height(), c.vig ? (double)c.vig->h + 2 : 0.0);
            c.h = c.head + 4 + bodyH + 10;
        }
        y += c.h + 6;
    }
    contentH = y;
    clampScroll();
}

void Site::setTab(int tb) {
    if (tb < 0 || tb >= TabCount) return;
    if (tb == Universe && tab_ != Universe) centreUniverse();
    if (tb != tab_) scrollY = 0;
    tab_ = tb;
    rebuild();
}

void Site::clampScroll() { scrollY = std::max(0.0, std::min(scrollY, std::max(0.0, contentH - content.h))); }

void Site::update() {
    if (signature() != builtSig) rebuild();
    if (tab_ == MyPage) inventory.update();
}

// ------------------------------------------------------------------ input

void Site::handleEvent(const SDL_Event& e) {
    auto* st = flash::Stage::get();
    auto toPage = [&](float wx, float wy) {
        SDL_RenderCoordinatesFromWindow(ren, wx, wy, &wx, &wy);
        mx = wx / u;
        my = wy / u;
    };
    switch (e.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            toPage(e.motion.x, e.motion.y);
            if (tab_ == MyPage && widgetR.w > 0) {
                bool in = !st->pointerInGame && content.has(mx, my) && widgetR.has(mx, my);
                double s = widgetR.w / Inventory::W;
                inventory.pointer((mx - widgetR.x) / s, (my - widgetR.y) / s, in);
            }
            if (uniDrag) {
                uniCX = uniDragCX - (mx - uniDragX) / uniZoom;
                uniCY = uniDragCY - (my - uniDragY) / uniZoom;
            }
            if (dragThumb) {
                double range = scrollTrack.h - scrollThumb.h;
                if (range > 0) scrollY = (my - dragOffset - scrollTrack.y) / range * (contentH - content.h);
                clampScroll();
            }
            bool over = false;
            if (!st->pointerInGame) {
                for (auto& r : tabsR) over |= r.has(mx, my);
                over |= helpBtn.has(mx, my);
                if (tab_ == Universe) over |= uniBtn.has(mx, my);
                if (content.has(mx, my))
                    for (auto& c : cards) {
                        double tx = content.x + (c.info ? 28 : (c.vig ? 64 : 8));
                        double ty = content.y + c.y - scrollY + (c.info ? 6 : c.head + 6);
                        over |= c.body.linkAt(mx - tx, my - ty) >= 0;
                    }
            }
            if (over != handShown && !(st->mouseHidden && st->pointerInGame)) {
                SDL_SetCursor(over ? hand : arrow);
                handShown = over;
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            float wx, wy;
            SDL_GetMouseState(&wx, &wy);
            toPage(wx, wy);
            if (tab_ == Universe && content.has(mx, my)) {
                zoomUniverse(std::pow(1.25, e.wheel.y), mx, my);
            } else if (panel.has(mx, my)) {
                scrollY -= e.wheel.y * 48;
                clampScroll();
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (e.button.button != SDL_BUTTON_LEFT || st->gameOwnsButton) break;
            toPage(e.button.x, e.button.y);
            for (int i = 0; i < TabCount; i++)
                if (tabsR[i].has(mx, my)) setTab(i);
            if (helpBtn.has(mx, my)) setTab(Help);
            if (tab_ == Universe && content.has(mx, my) && !st->pointerInGame) {
                if (uniBtn.has(mx, my)) {
                    centreUniverse();
                } else {
                    uniDrag = true;
                    uniDragX = mx;
                    uniDragY = my;
                    uniDragCX = uniCX;
                    uniDragCY = uniCY;
                }
            }
            if (hasThumb && scrollThumb.has(mx, my)) {
                dragThumb = true;
                dragOffset = my - scrollThumb.y;
            } else if (scrollTrack.has(mx, my)) {
                scrollY += (my < scrollThumb.y ? -1 : 1) * content.h * 0.9;
                clampScroll();
            }
            if (content.has(mx, my))
                for (auto& c : cards) {
                    double tx = content.x + (c.info ? 28 : (c.vig ? 64 : 8));
                    double ty = content.y + c.y - scrollY + (c.info ? 6 : c.head + 6);
                    int l = c.body.linkAt(mx - tx, my - ty);
                    if (l == 1) {
                        setTab(Help);
                        break;
                    }
                    if (l == 2 || l == 5 || l == 6) {
                        auto* stg = flash::Stage::get();
                        if (l == 2) stg->debug.showFps = !stg->debug.showFps;
                        else if (l == 5) stg->debug.freeTravel = !stg->debug.freeTravel;
                        else stg->debug.lander = !stg->debug.lander;
                        stg->saveDebugPrefs();
                        // the map builds its buttons when it opens: reopen it to show or hide the lander
                        if (l == 6 && Shell::get() && Shell::get()->getMap() && Shell::get()->getMap()->isMoving())
                            Shell::get()->requestInfos();
                        rebuild();
                        break;
                    }
                    if (l == 3 || l == 4 || l >= 100) {
                        if (l == 3) hideCompleted = !hideCompleted;
                        else if (l == 4) dismissed.clear();
                        else dismissed.insert(l - 100);
                        saveFeedPrefs();
                        rebuild();
                        break;
                    }
                    if (l == 0) {
                        setTab(Missions);
                        break;
                    }
                }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT) dragThumb = uniDrag = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_PAGEUP || e.key.key == SDLK_PAGEDOWN) {
                scrollY += (e.key.key == SDLK_PAGEUP ? -1 : 1) * content.h * 0.9;
                clampScroll();
            }
            break;
        default: break;
    }
}

// ------------------------------------------------------------------ drawing

void Site::blit(Tex* tx, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh,
                double alpha) {
    if (!tx) return;
    SDL_FRect s{(float)sx, (float)sy, (float)sw, (float)sh};
    // snap to whole window pixels so stretched slices meet without seams
    float x0 = std::round(dx * u), y0 = std::round(dy * u), x1 = std::round((dx + dw) * u), y1 = std::round((dy + dh) * u);
    SDL_FRect d{x0, y0, x1 - x0, y1 - y0};
    SDL_SetTextureAlphaModFloat(tx->t, (float)alpha);
    SDL_RenderTexture(ren, tx->t, &s, &d);
    SDL_SetTextureAlphaModFloat(tx->t, 1);
}

void Site::nine(Tex* tx, double sx, double sy, double sw, double sh, double l, double r, double tp, double b,
                const R& d) {
    if (!tx) return;
    // corners keep their size, edges stretch along one axis, the centre stretches both ways
    double xs[4] = {sx, sx + l, sx + sw - r, sx + sw}, ys[4] = {sy, sy + tp, sy + sh - b, sy + sh};
    double xd[4] = {d.x, d.x + l, d.x + d.w - r, d.x + d.w}, yd[4] = {d.y, d.y + tp, d.y + d.h - b, d.y + d.h};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (xs[i + 1] > xs[i] && ys[j + 1] > ys[j] && xd[i + 1] > xd[i] && yd[j + 1] > yd[j])
                blit(tx, xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], xd[i], yd[j], xd[i + 1] - xd[i],
                     yd[j + 1] - yd[j]);
}

void Site::fill(const R& r, unsigned rgb, double alpha) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, (Uint8)std::lround(alpha * 255));
    float x0 = std::round(r.x * u), y0 = std::round(r.y * u), x1 = std::round((r.x + r.w) * u),
          y1 = std::round((r.y + r.h) * u);
    SDL_FRect f{x0, y0, x1 - x0, y1 - y0};
    SDL_RenderFillRect(ren, &f);
}

void Site::clip(const R* r) {
    if (!r) {
        SDL_SetRenderClipRect(ren, nullptr);
        return;
    }
    SDL_Rect c{(int)std::round(r->x * u), (int)std::round(r->y * u), (int)std::round(r->w * u),
               (int)std::round(r->h * u)};
    SDL_SetRenderClipRect(ren, &c);
}

void Site::drawFrame() {
    Tex* fr = &tex["frame"];
    // starry background behind the translucent frame
    if (Tex* bg = load("core_planet.jpg")) {
        double s = std::max(PW / bg->w, PH / bg->h);
        blit(bg, 0, 0, bg->w, bg->h, (PW - bg->w * s) / 2, (PH - bg->h * s) / 2, bg->w * s, bg->h * s);
    }
    double midH = PH - 54 - 27;
    // top row: logo, stretched bar, right edge
    blit(fr, 0, 0, 366, 54, 0, 0, 366, 54);
    blit(fr, 600, 0, 1, 54, 366, 0, PW - 366 - 3, 54);
    blit(fr, 897, 0, 3, 54, PW - 3, 0, 3, 54);
    // middle: borders and the gap colour between the panels
    blit(fr, 0, 54, 8, 419, 0, 54, 8, midH);
    blit(fr, 413, 54, 1, 419, 8, 54, PW - 16, midH);
    blit(fr, 892, 54, 8, 419, PW - 8, 54, 8, midH);
    // bottom row
    blit(fr, 0, 473, 20, 27, 0, PH - 27, 20, 27);
    blit(fr, 450, 473, 1, 27, 20, PH - 27, PW - 40, 27);
    blit(fr, 880, 473, 20, 27, PW - 20, PH - 27, 20, 27);
    // panels
    nine(fr, 8, 74, 402, 362, 3, 3, 3, 3, {game.x - 1, game.y - 1, game.w + 2, game.h + 2});
    nine(fr, 8, 442, 402, 31, 6, 6, 6, 6, strip);
    nine(fr, 417, 54, 475, 419, 3, 3, 26, 3, panel);
}

void Site::drawPanel() {
    // tabs
    Tex* on = load("tab_on.gif");
    Tex* off = load("tab_off.gif");
    for (int i = 0; i < TabCount; i++) {
        const R& r = tabsR[i];
        bool hot = i == tab_ || (r.has(mx, my) && !flash::Stage::get()->pointerInGame);
        Tex* img = hot ? on : off;
        if (img) nine(img, 0, 0, img->w, img->h, 8, 8, 8, 8, {r.x, r.y + 3, r.w, (double)img->h});
        RichText& l = tabLabel[i];
        l.draw(r.x + (r.w - l.width()) / 2, r.y + 3 + (19 - l.height()) / 2 + 1, u);
    }

    if (tab_ == Universe) {
        clip(&content);
        drawUniverse();
        clip(nullptr);
        hasThumb = false;
        fill(scrollTrack, 0x1E2548, 0.8);
        return;
    }

    // feed
    clip(&content);
    for (auto& c : cards) {
        double y = content.y + c.y - scrollY;
        if (y > content.y + content.h || y + c.h < content.y) continue;
        R box{content.x, y, content.w, c.h};
        if (c.widget) {
            double sc = std::min(content.w / Inventory::W, 2.5);
            double wx = content.x + (content.w - Inventory::W * sc) / 2;
            widgetR = {wx, y, Inventory::W * sc, Inventory::H * sc};
            inventory.render(wx * u, y * u, sc * u);
            continue;
        }
        if (c.info) {
            fill(box, C_INFO, 0.85);
            if (c.icon) blit(c.icon, box.x + 6, y + (c.h - c.icon->h) / 2);
            c.body.draw(box.x + 28, y + 6, u);
            continue;
        }
        fill(box, C_PANEL_ITEM, 0.9);
        fill({box.x, y + c.head, box.w, 1}, 0x2A3460, 0.6);
        if (c.icon) blit(c.icon, box.x + 4, y + 3);
        c.title.draw(box.x + 24, y + 4, u);
        c.date.draw(box.x + box.w - 6 - c.date.width(), y + (c.head > 22 ? 20 : 6), u);
        double ty = y + c.head + 6;
        if (c.vig) {
            fill({box.x + 6, ty - 1, c.vig->w + 2.0, c.vig->h + 2.0}, C_VIG_BORDER, 1);
            blit(c.vig, box.x + 7, ty);
        }
        c.body.draw(box.x + (c.vig ? 64 : 8), ty, u);
    }
    clip(nullptr);

    // scrollbar
    hasThumb = contentH > content.h + 0.5;
    fill(scrollTrack, 0x1E2548, 0.8);
    if (hasThumb) {
        double th = std::max(24.0, scrollTrack.h * content.h / contentH);
        double ty = scrollTrack.y + (scrollTrack.h - th) * (scrollY / (contentH - content.h));
        scrollThumb = {scrollTrack.x + 1, ty, scrollTrack.w - 2, th};
        fill(scrollThumb, dragThumb ? 0xE0E4F0 : 0xB8C0D8, 1);
    }
}

// ------------------------------------------------------------------ universe map

void Site::centreUniverse() {
    if (!Cs::pi) return;
    uniCX = Cs::pi->x + 0.5;
    uniCY = Cs::pi->y + 0.5;
    uniCentred = true;
}

void Site::zoomUniverse(double factor, double px, double py) {
    // keep the sector under the pointer where it is
    double cx = content.x + content.w / 2, cy = content.y + content.h / 2;
    double sx = uniCX + (px - cx) / uniZoom, sy = uniCY + (py - cy) / uniZoom;
    double minZ = std::min(content.w / 1186.0, content.h / 1199.0) * 0.9;
    uniZoom = std::clamp(uniZoom * factor, minZ, 16.0);
    uniCX = sx - (px - cx) / uniZoom;
    uniCY = sy - (py - cy) / uniZoom;
}

void Site::drawUniverse() {
    const PlayerInfo* pi = Cs::pi;
    if (!uniCentred) centreUniverse();
    fill(content, 0x000028, 1);
    double z = uniZoom;
    double cx = content.x + content.w / 2, cy = content.y + content.h / 2;
    auto toPage = [&](double sx, double sy) { return std::pair<double, double>{cx + (sx - uniCX) * z, cy + (sy - uniCY) * z}; };
    // keep some of the map in view
    uniCX = std::clamp(uniCX, -(double)UNI_OX - 50, 1186.0 - UNI_OX + 50);
    uniCY = std::clamp(uniCY, -(double)UNI_OY - 50, 1199.0 - UNI_OY + 50);

    if (Tex* img = load("universe.png", true)) {
        SDL_SetTextureScaleMode(img->t, z * u >= 3 ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
        auto [x0, y0] = toPage(-UNI_OX, -UNI_OY);
        SDL_FRect d{(float)(x0 * u), (float)(y0 * u), (float)(img->w * z * u), (float)(img->h * z * u)};
        SDL_RenderTexture(ren, img->t, nullptr, &d);
    }
    // the sectors played: cleared ones in green, lost ones in red
    if (pi)
        for (auto& [k, v] : pi->sectors) {
            auto [x, y] = toPage(k.first, k.second);
            double s = std::max(z, 1.5);
            fill({x + (z - s) / 2, y + (z - s) / 2, s, s}, v == 1 ? 0x40E060 : 0xE04040, 0.7);
        }
    // the player
    if (pi) {
        auto [px, py] = toPage(pi->x + 0.5, pi->y + 0.5);
        double ix = std::clamp(px, content.x + 6, content.x + content.w - 6);
        double iy = std::clamp(py, content.y + 30, content.y + content.h - 6);
        double t = SDL_GetTicks() / 1000.0;
        double r = 5 + 3 * (0.5 + 0.5 * std::sin(t * 5));
        fill({ix - r, iy - 0.5, r * 2 + 1, 1}, 0xFFE060, 1);
        fill({ix - 0.5, iy - r, 1, r * 2 + 1}, 0xFFE060, 1);
        fill({ix - 2, iy - 2, 4, 4}, 0xFFFFFF, 1);
        TextStyle ps;
        ps.size = 9;
        ps.bold = true;
        ps.color = 0xFFE060;
        RichText me;
        me.setPlain("[" + std::to_string(pi->x) + "][" + std::to_string(pi->y) + "]", ps);
        me.layout(0);
        me.draw(std::min(ix + 8, content.x + content.w - me.width() - 4), iy - me.height() - 2, u);
    }
    // header: where the pointer is, and how to move around
    fill({content.x, content.y, content.w, 24}, 0x1E2548, 0.85);
    TextStyle st;
    st.size = 9;
    st.color = C_TEXT;
    TextStyle hi = st;
    hi.bold = true;
    hi.color = C_STRONG;
    std::vector<Span> spans;
    if (content.has(mx, my) && my > content.y + 24) {
        int sx = (int)std::floor(uniCX + (mx - cx) / z), sy = (int)std::floor(uniCY + (my - cy) / z);
        spans.push_back({"[" + std::to_string(sx) + "][" + std::to_string(sy) + "]", hi});
        int planet = ZoneInfo::getPlanet(sx, sy);
        if (planet >= 0 && planet < (int)ZoneInfo::list.size()) spans.push_back({"  " + ZoneInfo::list[planet].name, st});
        if (pi) {
            int stt = pi->sectorState(sx, sy);
            if (stt == 1) spans.push_back({tr("  - cleared", "  - nettoy\xC3\xA9", "  - ger\xC3\xA4umt", "  - limpiado"), st});
            else if (stt == 0) spans.push_back({tr("  - lost", "  - perdu", "  - verloren", "  - perdido"), st});
        }
    } else {
        spans.push_back({tr("Drag to move, wheel to zoom", "Glisser pour d\xC3\xA9placer, molette pour zoomer",
                            "Ziehen zum Verschieben, Mausrad zum Zoomen", "Arrastra para mover, rueda para ampliar"),
                         st});
    }
    RichText info;
    info.set(spans);
    info.layout(0);
    info.draw(content.x + 6, content.y + (24 - info.height()) / 2, u);
    TextStyle bs = st;
    bs.bold = true;
    bs.color = 0xFFFFFF;
    RichText btn;
    btn.setPlain(tr("Centre on me", "Me centrer", "Auf mich zentrieren", "Centrar en m\xC3\xAD"), bs);
    btn.layout(0);
    uniBtn = {content.x + content.w - btn.width() - 16, content.y + 3, btn.width() + 10, 18};
    fill(uniBtn, uniBtn.has(mx, my) ? 0x6A7CC8 : 0x4A5CA8, 1);
    btn.draw(uniBtn.x + 5, uniBtn.y + (18 - btn.height()) / 2, u);
}

void Site::render() {
    drawFrame();
    // where the debug overlay goes: over the logo, clear of the game
    {
        auto& dbg = flash::Stage::get()->debug;
        dbg.anchorX = 72 * u;
        dbg.anchorY = 5 * u;
        dbg.anchorScale = u;
    }
    // fan-game notice, bottom right
    footer.layout(0);
    footer.draw(PW - 14 - footer.width(), PH - 27 + (27 - footer.height()) / 2, u);
    // fuel capsules and minerals above the game, as on the site
    if (const PlayerInfo* pi = Cs::pi) {
        Tex* fuel = load("small_fuel.gif");
        Tex* fuelFree = load("small_fuel_free.gif");
        double x = 12;
        int n = 0;
        for (int i = 0; i < pi->chl && n < 30; i++, n++, x += 15) blit(fuel, x, 54);
        for (int i = 0; i < pi->chs && n < 30; i++, n++, x += 15) blit(fuelFree, x, 54);
        TextStyle ms;
        ms.size = 11;
        ms.bold = true;
        ms.color = 0xFFFFFF;
        minerals.setPlain(numStr(pi->minerai), ms);
        minerals.layout(0);
        double right = game.x + game.w;
        blit(load("small_money.gif"), right - 16, 54);
        minerals.draw(right - 20 - minerals.width(), 56, u);
    }
    // control strip
    Tex* h = load(lang == "fr" ? "help.gif" : "help_" + lang + ".gif", true);
    if (!h) h = load("help_en.gif");
    if (h) {
            double hx = strip.x + 5, hy = strip.y;
        blit(h, hx, hy);
        stripText[0].draw(hx + 20, hy + (31 - stripText[0].height()) / 2, u);
        stripText[1].draw(hx + 178, hy + (31 - stripText[1].height()) / 2, u);
        stripText[2].draw(hx + 284, hy + (31 - stripText[2].height()) / 2, u);
    }
    bool hot = helpBtn.has(mx, my) && !flash::Stage::get()->pointerInGame;
    blit(load(hot ? "button_small_hover.gif" : "button_small.gif"), helpBtn.x, helpBtn.y);
    helpLabel.draw(helpBtn.x + (53 - helpLabel.width()) / 2, helpBtn.y + (25 - helpLabel.height()) / 2, u);
    drawPanel();
}

}  // namespace site
