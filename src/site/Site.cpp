#include "Site.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "flash/Stage.h"
#include "game/Game.h"
#include "stb_image.h"

namespace site {

// ------------------------------------------------------------------ page texts
// The original page templates aren't in the archive; these are the visible labels, re-typed.

namespace {
struct Ui {
    const char* tabs[Site::TabCount];
    const char* strip[3];
    const char* help;
    const char* lost;
    const char* pilot;
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
     {"The sector map",
      "The galaxy map isn't ported yet: until it is, pick the next sector with the <strong>arrow keys</strong> "
      "and press <strong>ENTER</strong>. Sectors further than your engine allows can't be reached."}},
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
     {"La carte des secteurs",
      "La carte de la galaxie n'est pas encore port\xC3\xA9" "e : en attendant, choisissez le prochain secteur avec "
      "les <strong>fl\xC3\xA8" "ches</strong> et validez avec <strong>ENTR\xC3\x89" "E</strong>."}},
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
     {"Die Sektorkarte",
      "Die Galaxiekarte ist noch nicht portiert: bis dahin w\xC3\xA4hlst du den n\xC3\xA4" "chsten Sektor mit den "
      "<strong>Pfeiltasten</strong> und best\xC3\xA4tigst mit <strong>ENTER</strong>."}},
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
     {"El mapa de sectores",
      "El mapa de la galaxia a\xC3\xBAn no est\xC3\xA1 portado: mientras tanto, elige el siguiente sector con las "
      "<strong>flechas</strong> y pulsa <strong>INTRO</strong>."}},
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
    hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);

    TextStyle tabSt;
    tabSt.size = 8.5;
    tabSt.bold = true;
    tabSt.color = C_TAB;
    for (int i = 0; i < TabCount; i++) {
        tabLabel[i].setPlain(ui->tabs[i], tabSt);
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

void Site::buildMissions() {
    Card& info = addCard(load("small_post.gif"), "", ui->lost);
    info.info = true;
    const PlayerInfo* pi = Cs::pi;
    if (!pi) return;
    // newest first, like the site's news feed
    for (size_t k = pi->log.size(); k-- > 0;) {
        const MissionLogEntry& e = pi->log[k];
        std::string n = std::to_string(e.mission);
        std::string body = t((e.state == 0 ? "mission_desc_" : "mission_end_") + n);
        if (body.empty()) continue;
        addCard(load("small_mission.gif"), t("mission_name_" + n), fillPlaceholders(body), formatDate(e.time),
                vignette(e.mission));
    }
}

void Site::buildMyPage() {
    const PlayerInfo* pi = Cs::pi;
    if (!pi) return;
    auto row = [](const char* label, const std::string& v) {
        return std::string(label) + " : <strong>" + v + "</strong><br/>";
    };
    std::string where = "[" + std::to_string(pi->x) + "][" + std::to_string(pi->y) + "]";
    int planet = ZoneInfo::getPlanet(pi->x, pi->y);
    if (planet >= 0 && planet < (int)ZoneInfo::list.size()) where += " " + ZoneInfo::list[planet].name;
    int done = 0;
    for (int i = 0; i < pi->missions.size(); i++)
        if (pi->missions.get(i) == 1) done++;
    std::string html = row(ui->position, "<em>" + where + "</em>") + row(ui->minerals, numStr(pi->minerai)) +
                       row(ui->envelopes, std::to_string(pi->getLife())) +
                       row(ui->missiles, std::to_string(pi->missile) + " / " + std::to_string(pi->missileMax)) +
                       row(ui->engine, std::to_string(pi->engine)) + row(ui->radar, std::to_string(pi->radar)) +
                       (pi->drone > 0 ? row(ui->drones, std::to_string(pi->drone)) : std::string()) +
                       row(ui->missionsDone, std::to_string(done)) +
                       row(ui->fuel, std::to_string(pi->chl) + " + " + std::to_string(pi->chs)) +
                       "<p>" + ui->fuelFree + "</p>";
    addCard(load("small_pad.gif"), pilotName() + " - " + ui->myTitle, html, "", load("faq_pad.gif"));

    std::string items;
    for (int i = 0; i < pi->items.size(); i++) {
        if (!pi->gotItem(i)) continue;
        std::string name = Text::at("ITEM_NAMES", i);
        if (name.empty() || name.find_first_not_of("- ") == std::string::npos) continue;
        if (!items.empty()) items += ", ";
        items += name;
    }
    addCard(load("small_earth.gif"), ui->items, items.empty() ? std::string(ui->none) : items);

    std::string shop;
    for (int i = 0; i < pi->shopItems.size(); i++) {
        if (pi->shopItems.get(i) != 1) continue;
        std::string name = Text::at("SHOP_ITEM_NAMES", i);
        if (name.empty() || name.find_first_not_of("- ") == std::string::npos) continue;
        if (!shop.empty()) shop += ", ";
        shop += name;
    }
    addCard(load("small_money.gif"), ui->shop, shop.empty() ? std::string(ui->none) : shop);
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

std::string Site::signature() const {
    const PlayerInfo* pi = Cs::pi;
    std::string s = std::to_string(tab_);
    if (pi)
        s += "|" + std::to_string(pi->log.size()) + "|" + std::to_string(pi->minerai) + "|" + std::to_string(pi->x) +
             "," + std::to_string(pi->y) + "|" + std::to_string(pi->missile) + "|" + std::to_string(pi->stats.levels) +
             "|" + std::to_string(pi->items.size()) + "|" + std::to_string(pi->life);
    return s;
}

void Site::rebuild() {
    cards.clear();
    builtWidth = content.w;
    builtSig = signature();
    switch (tab_) {
        case Missions: buildMissions(); break;
        case MyPage: buildMyPage(); break;
        case Records: buildRecords(); break;
        default: buildHelp(); break;
    }
    double y = 0;
    for (auto& c : cards) {
        c.y = y;
        if (c.info) {
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
    if (tb != tab_) scrollY = 0;
    tab_ = tb;
    rebuild();
}

void Site::clampScroll() { scrollY = std::max(0.0, std::min(scrollY, std::max(0.0, contentH - content.h))); }

void Site::update() {
    if (signature() != builtSig) rebuild();
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
            if (dragThumb) {
                double range = scrollTrack.h - scrollThumb.h;
                if (range > 0) scrollY = (my - dragOffset - scrollTrack.y) / range * (contentH - content.h);
                clampScroll();
            }
            bool over = false;
            if (!st->pointerInGame) {
                for (auto& r : tabsR) over |= r.has(mx, my);
                over |= helpBtn.has(mx, my);
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
            if (panel.has(mx, my)) {
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
                    if (l == 0) {
                        setTab(Missions);
                        break;
                    }
                }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT) dragThumb = false;
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

    // feed
    clip(&content);
    for (auto& c : cards) {
        double y = content.y + c.y - scrollY;
        if (y > content.y + content.h || y + c.h < content.y) continue;
        R box{content.x, y, content.w, c.h};
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

void Site::render() {
    drawFrame();
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
