// =============================================================================
//  SIZZLE - COZY CAFE   (SFML 3)
//  Stove, plates, fryer, pizza oven and drinks machine + a real game loop:
//  customer orders, patience, timer, strikes, levels, upgrade shop, effects, sound.
//  No std::vector anywhere - plain arrays only.
//
//  Audio: set ENABLE_AUDIO to 0 below (and drop -lsfml-audio) to build without sound.
// =============================================================================
#define ENABLE_AUDIO 1

#include <SFML/Graphics.hpp>
#if ENABLE_AUDIO
#include <SFML/Audio.hpp>
#endif
#include <iostream>
#include <string>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <ctime>

using namespace std;

enum GameState { ST_TITLE, ST_INTRO, ST_PLAY, ST_PAUSE, ST_RESULT };
enum DragKind { DRAG_NONE, DRAG_TRAY, DRAG_PATTY, DRAG_FRIES, DRAG_BAKED_PIZZA, DRAG_RAW_PIZZA, DRAG_DRINK, DRAG_DISH };

// dish types (also used as order bits: 1 << type)
enum Dish { DISH_BURGER = 0, DISH_PIZZA = 1, DISH_FRIES = 2, DISH_DRINK = 3 };
int dishValue[4] = { 100, 150, 50, 30 };
string dishName[4] = { "Burger", "Pizza", "Fries", "Drink" };

const float PI_F = 3.14159265f;

// ----------------------------------------------------------------------------
// Textures / fonts
// ----------------------------------------------------------------------------
sf::Texture texKitchen;
sf::Texture foodTextures[10];
sf::Texture texPattyRaw, texPattyBurnt;
sf::Texture texDough, texPSauce, texPCheese, texPepperoni;
sf::Texture texPizzaRaw, texPizzaBaked, texPizzaBurnt;
sf::Texture texPotatoRaw, texFriesCooked, texFriesBurnt;
sf::Texture texIcon[4];          // burger, pizza, fries, drink icons for order cards
sf::Font font;
string foodFiles[10] = {"top_bun","bottom_bun","patty","cheese","lettuce","tomato","onion","sauce","mayo","pickle"};

// ----------------------------------------------------------------------------
// Game state
// ----------------------------------------------------------------------------
GameState state = ST_TITLE;
float globalTime = 0.0f;
int level = 1, score = 0, coins = 0;
int scoreAtLevelStart = 0, coinsAtLevelStart = 0;
int delivered = 0, goal = 4, strikes = 0;
const int MAX_STRIKES = 3;
float timeLeft = 0.0f;
bool levelWon = false;
int starsEarned = 0;
string resultReason;
int levelCoins = 0, levelScoreGain = 0;

// upgrades (0..3 each)
int upStove = 0, upFryer = 0, upOven = 0, upPatience = 0;
const int MAX_UP = 3;
string upName[4] = { "Faster Stove", "Faster Fryer", "Hotter Oven", "Patient Customers" };
int* upPtr[4] = { &upStove, &upFryer, &upOven, &upPatience };
int upCost(int lvl) { return 60 + 50 * lvl + 10 * lvl * lvl; }   // 60, 120, 200

float pattyCook() { return 6.0f * (1.0f - 0.15f * upStove); }
float pattyBurn() { return pattyCook() + 9.0f; }
float fryCook()   { return 5.0f * (1.0f - 0.15f * upFryer); }
float fryBurn()   { return fryCook() + 8.0f; }
float pizzaCook() { return 9.0f * (1.0f - 0.15f * upOven); }
float pizzaBurn() { return pizzaCook() + 10.0f; }
const float WARN_TIME = 3.0f;
const float FILL_TIME = 3.0f;

// ---- burger ----
const int ID_DOUGH = 10, ID_PSAUCE = 11, ID_PCHEESE = 12, ID_PEPPERONI = 13, ID_POTATO = 14;
int recipe[7] = {1,7,2,4,5,3,0};     // bottom bun, sauce(optional), COOKED patty, lettuce, tomato, cheese, top bun
int step = 0;
bool sauceSkipped = false, assembled = false;
// ---- pizza ----
int pizzaRecipe[4] = {ID_DOUGH, ID_PSAUCE, ID_PCHEESE, ID_PEPPERONI};
int pizzaStep = 0;
bool pizzaAssembled = false, pizzaReady = false;
// ---- fries / drinks ----
bool friesReady = false;
const int NUM_DRINKS = 3;
bool drinkReady = false;
int drinkFlavor = 0;
sf::Color drinkColor[NUM_DRINKS] = { sf::Color(96,30,24,235), sf::Color(255,165,35,235), sf::Color(255,105,175,235) };

// ---- cooking stations ----
struct Cooker { bool active; float t; int st; };
const int NUM_BURNERS = 4, NUM_BASKETS = 2;
Cooker burner[NUM_BURNERS], basket[NUM_BASKETS], oven, drinkFill[NUM_DRINKS];

// ---- dragging ----
DragKind dragKind = DRAG_NONE;
int dragId = -1, dragSrc = -1;
sf::Vector2f mousePos, hoverPos;

// ---- feedback message ----
string msg; float msgT = 0.0f;
void setMsg(const string& s) { msg = s; msgT = 3.0f; }

// ---- pop animations (0 burger layer, 1 pizza, 2 fries, 3 drink, 4 baked pizza on plate / burger done) ----
const float POP = 0.28f;
float popBurger = 0, popPizzaLayer = 0, popDish[4] = {0,0,0,0};
float popScale(float t) { return t > 0 ? 1.0f + 0.32f * sinf(PI_F * (1.0f - t / POP)) : 1.0f; }

// ---- orders ----
struct Order { bool active; int want; int got; float patience, maxPat; float flash; };
const int NUM_ORDERS = 4;
Order orders[NUM_ORDERS];
float spawnTimer = 0.0f;

// ----------------------------------------------------------------------------
// Coordinates (artwork pixels 1337x751 -> game window 960x640)
// ----------------------------------------------------------------------------
const float SX = 960.0f / 1337.0f, SY = 640.0f / 751.0f;
sf::Vector2f M(float x, float y) { return sf::Vector2f(x * SX, y * SY); }
sf::FloatRect R(float x, float y, float w, float h) { return sf::FloatRect(sf::Vector2f(x * SX, y * SY), sf::Vector2f(w * SX, h * SY)); }

struct TrayItem { sf::FloatRect rect; int id; };
const int NUM_TRAY = 15;
TrayItem tray[NUM_TRAY] = {
    { R( 30,592, 95,100),  0 }, { R(125,592, 90,100),  1 }, { R(225,592,140,100),  2 },
    { R(375,592,130,100),  4 }, { R(510,592, 80,100),  5 }, { R(592,592, 72,100),  6 },
    { R(668,592,108,100),  3 }, { R(800,590, 38,102),  7 }, { R(840,590, 38,102),  8 },
    { R(879,590, 36,102),  8 }, { R(925,592, 95,100), ID_DOUGH }, { R(1025,592,78,100), ID_PSAUCE },
    { R(1105,592,66,100), ID_PCHEESE }, { R(1180,592,82,100), ID_PEPPERONI }, { R(1258,560,79,130), ID_POTATO }
};
float burnerPos[NUM_BURNERS][2] = { {290,380},{412,380},{262,440},{390,440} };
float basketPos[NUM_BASKETS][2] = { {828,386},{906,384} };
sf::Vector2f burgerPlate(570,382), pizzaPlate(702,382), friesPlate(568,454), drinkPlate(702,454), ovenPos(1256,392);
float drinkX[NUM_DRINKS] = { 986, 1036, 1087 };
const float GLASS_BOTTOM = 427, NOZZLE_Y = 342;
sf::FloatRect burnerRect[NUM_BURNERS], basketRect[NUM_BASKETS], drinkRect[NUM_DRINKS], cardRect[NUM_ORDERS];
sf::FloatRect burgerRect = R(500,322,140,100), pizzaRect = R(640,322,125,100), friesRect = R(500,424,135,66);
sf::FloatRect ovenRect = R(1150,335,185,110), drinkPlateRect = R(640,424,135,66);
float cardTop[NUM_ORDERS] = { 102, 217, 332, 450 };
sf::Vector2f boardCenter(409,333);
sf::Vector2f coinHud(183,43);      // artwork pos of the HUD coin (coin particles fly here)

float layerRise[10] = {0,10,9,2,7,7,7,2,2,7};
float spriteBottom[10] = {98,95,103,103,103,103,103,64,64,103};
const float BURGER_SX = 0.65f, BURGER_SY = 0.42f;

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float frand() { return (float)rand() / (float)RAND_MAX; }
float frange(float a, float b) { return a + (b - a) * frand(); }

// ----------------------------------------------------------------------------
// Audio
// ----------------------------------------------------------------------------
enum Sfx { SFX_DING, SFX_CHIME, SFX_POP, SFX_PLACE, SFX_CLICK, SFX_COIN, SFX_ERROR, SFX_BURN, SFX_SUCCESS, SFX_FAIL, SFX_POUR, SFX_COUNT };
#if ENABLE_AUDIO
const char* sfxFile[SFX_COUNT] = { "ding","chime","pop","place","click","coin","error","burn","success","fail","pour" };
sf::SoundBuffer sfxBuf[SFX_COUNT];
bool sfxOk[SFX_COUNT];
optional<sf::Sound> sfxPool[16];
sf::SoundBuffer sizzleBuf, bubbleBuf;
optional<sf::Sound> sizzleSnd, bubbleSnd;
sf::Music music;
bool musicOk = false;
void playSound(int id, float vol = 80.0f, float pitch = 1.0f)
{
    if (id < 0 || id >= SFX_COUNT || !sfxOk[id]) return;
    for (int i = 0; i < 16; i++)
        if (!sfxPool[i] || sfxPool[i]->getStatus() != sf::SoundSource::Status::Playing)
        {
            sfxPool[i].emplace(sfxBuf[id]);
            sfxPool[i]->setVolume(vol); sfxPool[i]->setPitch(pitch); sfxPool[i]->play();
            return;
        }
}
void loadAudio()
{
    for (int i = 0; i < SFX_COUNT; i++)
    {
        sfxOk[i] = sfxBuf[i].loadFromFile(string("sounds/") + sfxFile[i] + ".wav");
        if (!sfxOk[i]) cout << "Missing sound: " << sfxFile[i] << ".wav\n";
    }
    if (sizzleBuf.loadFromFile("sounds/sizzle.wav")) { sizzleSnd.emplace(sizzleBuf); sizzleSnd->setLooping(true); sizzleSnd->setVolume(35); }
    if (bubbleBuf.loadFromFile("sounds/bubble.wav")) { bubbleSnd.emplace(bubbleBuf); bubbleSnd->setLooping(true); bubbleSnd->setVolume(30); }
    musicOk = music.openFromFile("sounds/music.wav");
    if (musicOk) { music.setLooping(true); music.setVolume(30); music.play(); }
}
void setLoop(optional<sf::Sound>& s, bool on)
{
    if (!s) return;
    bool playing = (s->getStatus() == sf::SoundSource::Status::Playing);
    if (on && !playing) s->play();
    if (!on && playing) s->pause();
}
#else
void playSound(int, float = 80.0f, float = 1.0f) {}
void loadAudio() {}
#endif

// ----------------------------------------------------------------------------
// Particles & floating text
// ----------------------------------------------------------------------------
struct Particle { bool on; float x,y,vx,vy,life,maxLife,size,grav,rot,vrot,tx,ty; sf::Color col; int type; };
const int MAX_PART = 600;
Particle parts[MAX_PART];
enum { P_SOFT = 0, P_STAR = 1, P_COIN = 2, P_BUBBLE = 3 };

void spawnPart(int type, sf::Vector2f p, sf::Vector2f v, float life, float size, sf::Color c, float grav = 0.0f, sf::Vector2f target = sf::Vector2f(0,0))
{
    for (int i = 0; i < MAX_PART; i++)
        if (!parts[i].on)
        {
            Particle& q = parts[i];
            q.on = true; q.x = p.x; q.y = p.y; q.vx = v.x; q.vy = v.y; q.life = q.maxLife = life; q.size = size;
            q.grav = grav; q.rot = frange(0, 6.28f); q.vrot = frange(-6, 6); q.tx = target.x; q.ty = target.y; q.col = c; q.type = type;
            return;
        }
}
void burstStars(sf::Vector2f p, sf::Color c, int n, float speed)
{
    for (int i = 0; i < n; i++)
    {
        float a = frange(0, 6.28f), s = frange(speed * 0.4f, speed);
        spawnPart(P_STAR, p, sf::Vector2f(cosf(a) * s, sinf(a) * s - 20), frange(0.5f, 0.95f), frange(3.5f, 6.0f), c, 160.0f);
    }
}
void sparkleAt(sf::Vector2f p) { burstStars(p, sf::Color(255,235,120), 12, 110); burstStars(p, sf::Color(255,255,255), 6, 70); }
void smokeAt(sf::Vector2f p, int n)
{
    for (int i = 0; i < n; i++)
        spawnPart(P_SOFT, p + sf::Vector2f(frange(-10,10), frange(-4,4)), sf::Vector2f(frange(-12,12), frange(-45,-20)), frange(0.9f,1.6f), frange(5,9), sf::Color(50,45,45,170), -10.0f);
}
void coinsFlyTo(sf::Vector2f from, int n)
{
    sf::Vector2f target = M(coinHud.x, coinHud.y);
    for (int i = 0; i < n; i++)
        spawnPart(P_COIN, from + sf::Vector2f(frange(-14,14), frange(-8,8)), sf::Vector2f(frange(-60,60), frange(-110,-30)), frange(0.8f, 1.15f), 7.0f, sf::Color(255,205,40), 0.0f, target);
}
void updateParticles(float dt)
{
    for (int i = 0; i < MAX_PART; i++)
    {
        Particle& q = parts[i];
        if (!q.on) continue;
        q.life -= dt;
        if (q.life <= 0) { q.on = false; continue; }
        if (q.type == P_COIN)
        {
            float prog = 1.0f - q.life / q.maxLife;           // fly up a little, then home in on the HUD coin
            if (prog < 0.25f) { q.x += q.vx * dt; q.y += q.vy * dt; q.vx *= 0.9f; q.vy *= 0.9f; }
            else { float k = clamp01((prog - 0.25f) / 0.75f); q.x += (q.tx - q.x) * (4.0f + 14.0f * k) * dt; q.y += (q.ty - q.y) * (4.0f + 14.0f * k) * dt; }
        }
        else { q.x += q.vx * dt; q.y += q.vy * dt; q.vy += q.grav * dt; }
        q.rot += q.vrot * dt;
    }
}
void drawParticles(sf::RenderWindow& w)
{
    sf::CircleShape circ(1.0f, 20);
    circ.setOrigin(sf::Vector2f(1.0f, 1.0f));
    sf::ConvexShape dia(4);
    for (int i = 0; i < MAX_PART; i++)
    {
        const Particle& q = parts[i];
        if (!q.on) continue;
        float a = clamp01(q.life / q.maxLife);
        sf::Color c = q.col; c.a = (unsigned char)(c.a * (q.type == P_SOFT ? a * 0.9f : (a < 0.35f ? a / 0.35f : 1.0f)));
        if (q.type == P_STAR)
        {
            float s = q.size;
            dia.setPoint(0, sf::Vector2f(0, -s * 1.5f)); dia.setPoint(1, sf::Vector2f(s * 0.55f, 0));
            dia.setPoint(2, sf::Vector2f(0, s * 1.5f));  dia.setPoint(3, sf::Vector2f(-s * 0.55f, 0));
            dia.setFillColor(c); dia.setPosition(sf::Vector2f(q.x, q.y)); dia.setRotation(sf::degrees(q.rot * 57.3f)); w.draw(dia);
        }
        else if (q.type == P_COIN)
        {
            circ.setScale(sf::Vector2f(q.size, q.size)); circ.setPosition(sf::Vector2f(q.x, q.y));
            circ.setFillColor(sf::Color(255,205,40,c.a)); circ.setOutlineColor(sf::Color(190,120,20,c.a)); circ.setOutlineThickness(0.18f); w.draw(circ); circ.setOutlineThickness(0);
        }
        else if (q.type == P_BUBBLE)
        {
            circ.setScale(sf::Vector2f(q.size, q.size)); circ.setPosition(sf::Vector2f(q.x, q.y));
            circ.setFillColor(sf::Color(255,255,255,(unsigned char)(c.a * 0.35f))); circ.setOutlineColor(sf::Color(255,255,255,c.a)); circ.setOutlineThickness(0.25f); w.draw(circ); circ.setOutlineThickness(0);
        }
        else
        {
            float grow = q.size * (1.0f + (1.0f - a) * 1.2f);
            circ.setScale(sf::Vector2f(grow, grow)); circ.setPosition(sf::Vector2f(q.x, q.y)); circ.setFillColor(c); w.draw(circ);
        }
    }
}

struct FText { bool on; string s; float x, y, life; sf::Color col; unsigned size; };
const int MAX_FT = 24;
FText fts[MAX_FT];
void floatText(const string& s, sf::Vector2f p, sf::Color c, unsigned size = 22)
{
    for (int i = 0; i < MAX_FT; i++)
        if (!fts[i].on) { fts[i].on = true; fts[i].s = s; fts[i].x = p.x; fts[i].y = p.y; fts[i].life = 1.4f; fts[i].col = c; fts[i].size = size; return; }
}

// ----------------------------------------------------------------------------
// Drawing helpers
// ----------------------------------------------------------------------------
sf::String U(const string& s) { return sf::String::fromUtf8(s.begin(), s.end()); }

void drawText(sf::RenderWindow& w, const string& s, float x, float y, unsigned size, sf::Color c = sf::Color(115,55,48))
{
    sf::Text t(font, U(s), size); t.setFillColor(c); t.setPosition(sf::Vector2f(x, y)); w.draw(t);
}
void drawCentred(sf::RenderWindow& w, const string& s, sf::Vector2f p, unsigned size, sf::Color fill, sf::Color outline = sf::Color(0,0,0,0), bool bold = false)
{
    sf::Text t(font, U(s), size);
    if (bold) t.setStyle(sf::Text::Bold);
    sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(b.position.x + b.size.x / 2.0f, b.position.y + b.size.y / 2.0f));
    t.setPosition(p); t.setFillColor(fill);
    if (outline.a > 0) { t.setOutlineColor(outline); t.setOutlineThickness(2.0f); }
    w.draw(t);
}
void roundedRect(sf::RenderWindow& w, sf::FloatRect r, float rad, sf::Color fill, sf::Color outline, float thick)
{
    const int SEG = 6;
    sf::ConvexShape s(4 * (SEG + 1));
    int k = 0;
    float cx[4] = { r.position.x + r.size.x - rad, r.position.x + r.size.x - rad, r.position.x + rad, r.position.x + rad };
    float cy[4] = { r.position.y + rad, r.position.y + r.size.y - rad, r.position.y + r.size.y - rad, r.position.y + rad };
    for (int c = 0; c < 4; c++)
        for (int i = 0; i <= SEG; i++)
        {
            float a = (-90.0f + 90.0f * c + 90.0f * i / SEG) * PI_F / 180.0f;
            s.setPoint(k++, sf::Vector2f(cx[c] + cosf(a) * rad, cy[c] + sinf(a) * rad));
        }
    s.setFillColor(fill); s.setOutlineColor(outline); s.setOutlineThickness(thick);
    w.draw(s);
}
void drawSpriteAt(sf::RenderWindow& w, const sf::Texture& tex, float ox, float oy, sf::Vector2f artPos, float k, int alpha = 255)
{
    sf::Sprite s(tex);
    s.setOrigin(sf::Vector2f(ox, oy)); s.setScale(sf::Vector2f(SX * k, SY * k)); s.setPosition(M(artPos.x, artPos.y));
    s.setColor(sf::Color(255,255,255,(unsigned char)alpha)); w.draw(s);
}
void shadowAt(sf::RenderWindow& w, sf::Vector2f p, float rx, float ry, int alpha = 70)
{
    sf::CircleShape c(1.0f, 24); c.setOrigin(sf::Vector2f(1,1)); c.setScale(sf::Vector2f(rx, ry)); c.setPosition(p);
    c.setFillColor(sf::Color(30,15,5,(unsigned char)alpha)); w.draw(c);
}
// pulsing glow around a valid drop zone
void glowRect(sf::RenderWindow& w, sf::FloatRect r, sf::Color base = sf::Color(255,240,120))
{
    float pulse = 0.5f + 0.5f * sinf(globalTime * 7.0f);
    sf::Color fill = base; fill.a = (unsigned char)(35 + 40 * pulse);
    sf::Color line = base; line.a = (unsigned char)(150 + 100 * pulse);
    roundedRect(w, r, 10, fill, line, 2.5f);
}
int cookState(float t, float cook, float burn) { return t < cook ? 0 : (t < burn ? 1 : 2); }

void drawCooking(sf::RenderWindow& w, const sf::Texture& raw, const sf::Texture& cooked, const sf::Texture& burnt,
                 float ox, float oy, sf::Vector2f pos, float k, float t, float cook, float burn)
{
    drawSpriteAt(w, raw, ox, oy, pos, k);
    drawSpriteAt(w, cooked, ox, oy, pos, k, (int)(255 * clamp01(t / cook)));
    drawSpriteAt(w, burnt, ox, oy, pos, k, (int)(255 * clamp01((t - (burn - WARN_TIME)) / WARN_TIME)));
}
void drawTimerBar(sf::RenderWindow& w, sf::Vector2f artPos, float t, float cook, float burn, float width = 56.0f, bool showLabel = true)
{
    int st = cookState(t, cook, burn);
    float frac; sf::Color col; string label;
    if (st == 0) { frac = t / cook; col = sf::Color(255,170,40); }
    else if (st == 1)
    {
        if (t < burn - WARN_TIME) { frac = 1; col = sf::Color(70,200,90); label = "READY"; }
        else { frac = (burn - t) / WARN_TIME; col = sf::Color(235,60,50); label = "HURRY!"; }
    }
    else { frac = 1; col = sf::Color(110,25,25); label = "BURNT - click"; }
    sf::Vector2f c = M(artPos.x, artPos.y);
    sf::RectangleShape bg(sf::Vector2f(width, 8)); bg.setOrigin(sf::Vector2f(width / 2, 4)); bg.setPosition(c);
    bg.setFillColor(sf::Color(40,25,20,220)); bg.setOutlineColor(sf::Color(255,240,220)); bg.setOutlineThickness(1); w.draw(bg);
    sf::RectangleShape fill(sf::Vector2f((width - 2) * clamp01(frac), 6)); fill.setPosition(sf::Vector2f(c.x - width / 2 + 1, c.y - 3));
    fill.setFillColor(col); w.draw(fill);
    if (showLabel && label != "") drawCentred(w, label, sf::Vector2f(c.x, c.y - 12), 11, sf::Color::White, sf::Color(60,30,20));
}

// ---- burger drawing ----
sf::Sprite foodSprite(int id)
{
    sf::Sprite s(foodTextures[id]);
    s.setOrigin(sf::Vector2f(64.0f, spriteBottom[id])); s.setScale(sf::Vector2f(BURGER_SX, BURGER_SY));
    return s;
}
void drawEllipse(sf::RenderWindow& w, float cx, float cy, float rx, float ry, sf::Color c)
{
    sf::CircleShape e(1.0f, 48); e.setOrigin(sf::Vector2f(1.0f, 1.0f)); e.setScale(sf::Vector2f(rx, ry)); e.setPosition(sf::Vector2f(cx, cy)); e.setFillColor(c); w.draw(e);
}
void drawKetchupPuddle(sf::RenderWindow& w, float cx, float cy)
{
    drawEllipse(w, cx, cy, 46, 9, sf::Color(150,15,28)); drawEllipse(w, cx, cy-1, 43, 7.5f, sf::Color(205,30,42));
    drawEllipse(w, cx-10, cy-3, 22, 2.2f, sf::Color(245,120,120)); drawEllipse(w, cx-30, cy+8, 3.5f, 5, sf::Color(190,25,38));
    drawEllipse(w, cx+8, cy+9, 3.0f, 6, sf::Color(190,25,38)); drawEllipse(w, cx+33, cy+7, 3.5f, 4.5f, sf::Color(190,25,38));
}
void drawKetchupBand(sf::RenderWindow& w, float cx, float cy)
{
    drawEllipse(w, cx, cy, 43, 4.5f, sf::Color(150,15,28)); drawEllipse(w, cx, cy-0.5f, 41, 3.4f, sf::Color(208,32,44));
    drawEllipse(w, cx-10, cy-1.5f, 16, 1.0f, sf::Color(245,125,125)); drawEllipse(w, cx-28, cy+4, 3.0f, 3.5f, sf::Color(190,25,38));
    drawEllipse(w, cx+22, cy+4, 2.5f, 4.5f, sf::Color(190,25,38));
}
// bottom-centre of the stack at 'base' (game coords)
void drawBurgerStack(sf::RenderWindow& w, sf::Vector2f base, int layers, bool skipped, float popLast, float sc = 1.0f)
{
    float y = base.y; bool sauceShown = false;
    for (int i = 0; i < layers; i++)
    {
        int id = recipe[i];
        if (id == 7)
        {
            if (skipped) continue;
            drawKetchupPuddle(w, base.x, y - 7.0f * sc); sauceShown = true; y -= layerRise[7] * sc; continue;
        }
        sf::Sprite layer = foodSprite(id);
        float s = (i == layers - 1) ? popScale(popLast) : 1.0f;
        layer.setScale(sf::Vector2f(BURGER_SX * s * sc, BURGER_SY * s * sc));
        layer.setPosition(sf::Vector2f(base.x, y)); w.draw(layer);
        if (id == 2 && sauceShown) drawKetchupBand(w, base.x, y - 1.0f);
        y -= layerRise[id] * sc;
    }
}
void drawGlass(sf::RenderWindow& w, sf::Vector2f bottom, float k, sf::Color liq, float frac)
{
    float H = 66 * k, wt = 40 * k, wb = 32 * k;
    auto pt = [&](float yf, float side) { float half = (wb + (wt - wb) * yf) / 2.0f; return M(bottom.x + side * half, bottom.y - H * yf); };
    if (frac > 0.0f)
    {
        float top = frac * 0.92f;
        sf::ConvexShape l(4); l.setPoint(0, pt(0,-1)); l.setPoint(1, pt(0,1)); l.setPoint(2, pt(top,1)); l.setPoint(3, pt(top,-1)); l.setFillColor(liq); w.draw(l);
        sf::ConvexShape s(4); s.setPoint(0, pt(top,-1)); s.setPoint(1, pt(top,1)); s.setPoint(2, pt(top-0.06f,1)); s.setPoint(3, pt(top-0.06f,-1)); s.setFillColor(sf::Color(255,255,255,70)); w.draw(s);
    }
    sf::ConvexShape g(4); g.setPoint(0, pt(0,-1)); g.setPoint(1, pt(0,1)); g.setPoint(2, pt(1,1)); g.setPoint(3, pt(1,-1));
    g.setFillColor(sf::Color(255,255,255,35)); g.setOutlineColor(sf::Color(235,245,255,210)); g.setOutlineThickness(1.5f); w.draw(g);
    sf::ConvexShape sh(4); sh.setPoint(0, pt(0.12f,-0.7f)); sh.setPoint(1, pt(0.12f,-0.5f)); sh.setPoint(2, pt(0.85f,-0.5f)); sh.setPoint(3, pt(0.85f,-0.7f)); sh.setFillColor(sf::Color(255,255,255,70)); w.draw(sh);
}

// ----------------------------------------------------------------------------
// Reset helpers
// ----------------------------------------------------------------------------
void resetBurger() { step = 0; sauceSkipped = false; assembled = false; }
void resetPizza() { pizzaStep = 0; pizzaAssembled = false; pizzaReady = false; }
void resetCookers()
{
    for (int i = 0; i < NUM_BURNERS; i++) { burner[i].active = false; burner[i].t = 0; burner[i].st = 0; }
    for (int i = 0; i < NUM_BASKETS; i++) { basket[i].active = false; basket[i].t = 0; basket[i].st = 0; }
    oven.active = false; oven.t = 0; oven.st = 0;
    for (int i = 0; i < NUM_DRINKS; i++) { drinkFill[i].active = false; drinkFill[i].t = 0; drinkFill[i].st = 0; }
}
void cancelDrag() { dragKind = DRAG_NONE; dragId = -1; dragSrc = -1; }
void resetKitchen() { resetBurger(); resetPizza(); friesReady = false; drinkReady = false; resetCookers(); cancelDrag(); }

// ----------------------------------------------------------------------------
// Orders & level flow
// ----------------------------------------------------------------------------
int levelGoal(int L) { return 3 + L; }
float levelTime(int L) { return 150.0f + 10.0f * (L - 1); }
float spawnInterval(int L) { float v = 11.0f - 0.8f * (L - 1); return v < 5.5f ? 5.5f : v; }
int itemCount(int mask) { int n = 0; for (int i = 0; i < 4; i++) if (mask & (1 << i)) n++; return n; }

void spawnOrder()
{
    int slot = -1;
    for (int i = 0; i < NUM_ORDERS; i++) if (!orders[i].active) { slot = i; break; }
    if (slot < 0) return;
    int maxItems = level >= 4 ? 4 : (level >= 2 ? 3 : 2);
    int n = 1 + rand() % maxItems;
    int mask = 0;
    while (itemCount(mask) < n) mask |= 1 << (rand() % 4);
    float pat = (34.0f + 16.0f * n) * (1.0f - 0.04f * (level - 1)) * (1.0f + 0.2f * upPatience);
    if (pat < 30.0f) pat = 30.0f;
    orders[slot].active = true; orders[slot].want = mask; orders[slot].got = 0;
    orders[slot].patience = orders[slot].maxPat = pat; orders[slot].flash = 0.4f;
    playSound(SFX_CHIME, 70);
}
void startLevel()
{
    resetKitchen();
    for (int i = 0; i < NUM_ORDERS; i++) orders[i].active = false;
    for (int i = 0; i < MAX_PART; i++) parts[i].on = false;
    for (int i = 0; i < MAX_FT; i++) fts[i].on = false;
    delivered = 0; strikes = 0; goal = levelGoal(level); timeLeft = levelTime(level);
    scoreAtLevelStart = score; coinsAtLevelStart = coins; spawnTimer = 3.0f; msgT = 0;
    spawnOrder(); spawnOrder();
}
void endLevel(bool won, const string& reason)
{
    levelWon = won; resultReason = reason; cancelDrag();
    if (won)
    {
        starsEarned = strikes == 0 ? 3 : (strikes == 1 ? 2 : 1);
        score += (int)timeLeft * 2;
        coins += starsEarned * 15;
        playSound(SFX_SUCCESS, 90);
    }
    else { starsEarned = 0; playSound(SFX_FAIL, 90); }
    levelCoins = coins - coinsAtLevelStart; levelScoreGain = score - scoreAtLevelStart;
    state = ST_RESULT;
}
void completeOrder(int i)
{
    Order& o = orders[i];
    int items = itemCount(o.want);
    int tip = (int)(20.0f * clamp01(o.patience / o.maxPat));
    int gain = 10 * items + tip;
    coins += gain; score += 50 + tip * 2; delivered++;
    sf::Vector2f c = M(100, cardTop[i] + 55);
    floatText("Order complete!", c + sf::Vector2f(60, -18), sf::Color(120,255,140), 22);
    floatText("+" + to_string(gain) + " coins", c + sf::Vector2f(60, 8), sf::Color(255,220,80), 20);
    sparkleAt(c); sparkleAt(c + sf::Vector2f(40, 0)); coinsFlyTo(c + sf::Vector2f(30, 0), 8);
    playSound(SFX_COIN, 80); playSound(SFX_CHIME, 60, 1.3f);
    o.active = false;
    if (spawnTimer > 2.5f) spawnTimer = 2.5f;
    if (delivered >= goal) endLevel(true, "All orders served!");
}
void deliverDish(int type, int i)
{
    Order& o = orders[i];
    o.got |= (1 << type);
    score += dishValue[type];
    sf::Vector2f c = M(100, cardTop[i] + 40);
    floatText("+" + to_string(dishValue[type]), c + sf::Vector2f(30, -10), sf::Color(255,255,255), 22);
    burstStars(c, sf::Color(255,230,120), 8, 90);
    playSound(SFX_POP, 80);
    if (type == DISH_BURGER) resetBurger();
    else if (type == DISH_PIZZA) pizzaReady = false;
    else if (type == DISH_FRIES) friesReady = false;
    else drinkReady = false;
    if (o.got == o.want) completeOrder(i);
}
void loseOrder(int i)
{
    orders[i].active = false; strikes++;
    score -= 50; if (score < 0) score = 0;
    sf::Vector2f c = M(100, cardTop[i] + 50);
    floatText("Customer left!", c + sf::Vector2f(50, 0), sf::Color(255,110,100), 22);
    smokeAt(c, 6);
    playSound(SFX_ERROR, 90);
    if (strikes >= MAX_STRIKES) endLevel(false, "Too many unhappy customers!");
}

// ----------------------------------------------------------------------------
// Per-frame simulation
// ----------------------------------------------------------------------------
float steamT = 0.0f;
void tickCooker(Cooker& c, float dt, float cook, float burn, sf::Vector2f artPos)
{
    if (!c.active) return;
    c.t += dt;
    int st = cookState(c.t, cook, burn);
    if (st != c.st)
    {
        c.st = st;
        sf::Vector2f p = M(artPos.x, artPos.y);
        if (st == 1) { sparkleAt(p); playSound(SFX_DING, 85); floatText("Ready!", p + sf::Vector2f(0, -24), sf::Color(140,255,150), 18); }
        if (st == 2) { smokeAt(p, 14); playSound(SFX_BURN, 85); floatText("Burnt!", p + sf::Vector2f(0, -24), sf::Color(255,110,90), 18); }
    }
}
bool anyCooking(Cooker* c, int n) { for (int i = 0; i < n; i++) if (c[i].active && c[i].st < 2) return true; return false; }

void updateGame(float dt)
{
    for (int i = 0; i < NUM_BURNERS; i++) tickCooker(burner[i], dt, pattyCook(), pattyBurn(), sf::Vector2f(burnerPos[i][0], burnerPos[i][1]));
    for (int i = 0; i < NUM_BASKETS; i++) tickCooker(basket[i], dt, fryCook(), fryBurn(), sf::Vector2f(basketPos[i][0], basketPos[i][1]));
    tickCooker(oven, dt, pizzaCook(), pizzaBurn(), ovenPos);
    for (int i = 0; i < NUM_DRINKS; i++)
        if (drinkFill[i].active && drinkFill[i].t < FILL_TIME)
        {
            drinkFill[i].t += dt;
            if (drinkFill[i].t >= FILL_TIME) { drinkFill[i].t = FILL_TIME; sparkleAt(M(drinkX[i], 395)); playSound(SFX_DING, 70, 1.4f); }
        }

    // ambient effects: steam over cooking patties, bubbles in the fryer, smoke from burnt food
    steamT += dt;
    if (steamT > 0.11f)
    {
        steamT = 0;
        for (int i = 0; i < NUM_BURNERS; i++)
            if (burner[i].active && !(dragKind == DRAG_PATTY && dragSrc == i))
            {
                sf::Vector2f p = M(burnerPos[i][0], burnerPos[i][1]);
                if (burner[i].st < 2) spawnPart(P_SOFT, p + sf::Vector2f(frange(-12,12), -6), sf::Vector2f(frange(-6,6), frange(-38,-22)), frange(0.7f,1.1f), frange(3,5), sf::Color(255,255,255,120), -6.0f);
                else smokeAt(p, 1);
            }
        for (int i = 0; i < NUM_BASKETS; i++)
            if (basket[i].active && !(dragKind == DRAG_FRIES && dragSrc == i))
            {
                sf::Vector2f p = M(basketPos[i][0], basketPos[i][1]);
                spawnPart(P_BUBBLE, p + sf::Vector2f(frange(-20,20), frange(-6,8)), sf::Vector2f(frange(-4,4), frange(-30,-14)), frange(0.5f,0.9f), frange(1.6f,2.8f), sf::Color(255,255,255,200));
                if (basket[i].st == 2) smokeAt(p, 1);
            }
        if (oven.active && oven.st == 2) smokeAt(M(ovenPos.x, ovenPos.y - 20), 1);
    }

    // orders
    for (int i = 0; i < NUM_ORDERS; i++)
    {
        if (!orders[i].active) continue;
        orders[i].patience -= dt;
        if (orders[i].flash > 0) orders[i].flash -= dt;
        if (orders[i].patience <= 0) { loseOrder(i); if (state != ST_PLAY) return; }
    }
    int active = 0;
    for (int i = 0; i < NUM_ORDERS; i++) if (orders[i].active) active++;
    spawnTimer -= dt;
    if (active == 0 || spawnTimer <= 0.0f) { if (active < NUM_ORDERS) spawnOrder(); spawnTimer = spawnInterval(level); }

    timeLeft -= dt;
    if (timeLeft <= 0.0f) { timeLeft = 0.0f; endLevel(false, "Time's up!"); }
}

// ----------------------------------------------------------------------------
// Kitchen drawing
// ----------------------------------------------------------------------------
string kitchenHint()
{
    if (!assembled)
    {
        if (step == 0) return "Burger: drag the bottom bun onto the first plate";
        if (step == 1) return "Add sauce, or press S to skip";
        if (step == 2) return "Next: patty - cook it on the stove, then drag it over";
        if (step == 3) return "Next: lettuce";
        if (step == 4) return "Next: tomato";
        if (step == 5) return "Next: cheese";
        return "Finish with the top bun";
    }
    return "Burger done! Drag dishes from the plates onto a customer order";
}
void drawIcon(sf::RenderWindow& w, int type, sf::Vector2f center, float targetH, int alpha)
{
    sf::Sprite s(texIcon[type]);
    sf::Vector2u sz = texIcon[type].getSize();
    float k = targetH / (float)(sz.x > sz.y ? sz.x : sz.y);
    s.setOrigin(sf::Vector2f(sz.x / 2.0f, sz.y / 2.0f)); s.setScale(sf::Vector2f(k, k)); s.setPosition(center);
    s.setColor(sf::Color(255,255,255,(unsigned char)alpha)); w.draw(s);
}
void drawCheck(sf::RenderWindow& w, sf::Vector2f c)
{
    sf::CircleShape bg(8.0f, 20); bg.setOrigin(sf::Vector2f(8,8)); bg.setPosition(c); bg.setFillColor(sf::Color(60,185,90)); bg.setOutlineColor(sf::Color::White); bg.setOutlineThickness(1.5f); w.draw(bg);
    sf::RectangleShape a(sf::Vector2f(6, 2.4f)); a.setFillColor(sf::Color::White); a.setPosition(c + sf::Vector2f(-4.5f, 0.5f)); a.setRotation(sf::degrees(45)); w.draw(a);
    sf::RectangleShape b(sf::Vector2f(10, 2.4f)); b.setFillColor(sf::Color::White); b.setPosition(c + sf::Vector2f(-1.0f, 4.2f)); b.setRotation(sf::degrees(-50)); w.draw(b);
}
void drawOrders(sf::RenderWindow& w)
{
    for (int i = 0; i < NUM_ORDERS; i++)
    {
        const Order& o = orders[i];
        sf::FloatRect cr = cardRect[i];
        if (!o.active)
        {
            drawCentred(w, "Waiting for\ncustomer...", sf::Vector2f(cr.position.x + cr.size.x / 2, cr.position.y + cr.size.y / 2), 12, sf::Color(170,130,110));
            continue;
        }
        float frac = clamp01(o.patience / o.maxPat);
        float shake = frac < 0.25f ? sinf(globalTime * 45.0f) * 1.6f : 0.0f;
        if (o.flash > 0) { sf::Color f(255,255,200, (unsigned char)(120 * (o.flash / 0.4f))); roundedRect(w, cr, 10, f, sf::Color(0,0,0,0), 0); }
        int n = itemCount(o.want), k = 0;
        float cxMid = cr.position.x + cr.size.x / 2;
        float midY = cr.position.y + (cr.size.y - 22) / 2.0f;
        for (int d = 0; d < 4; d++)
        {
            if (!(o.want & (1 << d))) continue;
            sf::Vector2f p; float sz;
            if (n <= 3) { float sp = (cr.size.x - 14) / n; if (sp > 54) sp = 54; sz = sp - 4 > 46 ? 46 : sp - 4; p = sf::Vector2f(cxMid + (k - (n - 1) / 2.0f) * sp + shake, midY); }
            else { sz = 34; p = sf::Vector2f(cxMid + ((k % 2) - 0.5f) * 46 + shake, midY + ((k / 2) - 0.5f) * 32); }
            bool got = (o.got & (1 << d)) != 0;
            drawIcon(w, d, p, sz, got ? 90 : 255);
            if (got) drawCheck(w, p + sf::Vector2f(sz * 0.3f, sz * 0.3f));
            k++;
        }
        // patience bar
        float bx = cr.position.x + 10, by = cr.position.y + cr.size.y - 20, bw = cr.size.x - 20;
        sf::RectangleShape bg(sf::Vector2f(bw, 11)); bg.setPosition(sf::Vector2f(bx, by)); bg.setFillColor(sf::Color(170,125,100)); bg.setOutlineColor(sf::Color(120,70,50)); bg.setOutlineThickness(1); w.draw(bg);
        sf::Color col = frac > 0.5f ? sf::Color(70,200,70) : (frac > 0.25f ? sf::Color(245,200,30) : sf::Color(230,55,45));
        sf::RectangleShape fill(sf::Vector2f(bw * frac, 11)); fill.setPosition(sf::Vector2f(bx, by)); fill.setFillColor(col); w.draw(fill);
        sf::RectangleShape shine(sf::Vector2f(bw * frac, 3)); shine.setPosition(sf::Vector2f(bx, by + 1)); shine.setFillColor(sf::Color(255,255,255,90)); w.draw(shine);
    }
}
void drawHud(sf::RenderWindow& w)
{
    sf::Color ink(255,255,255), edge(70,15,20);
    drawCentred(w, to_string(coins), M(268,43), 24, ink, edge, true);
    int secs = (int)ceilf(timeLeft); if (secs < 0) secs = 0;
    char buf[16]; snprintf(buf, sizeof buf, "%02d:%02d", secs / 60, secs % 60);
    bool low = timeLeft < 15.0f && state == ST_PLAY;
    sf::Color tc = (low && ((int)(globalTime * 3) % 2 == 0)) ? sf::Color(255,120,100) : ink;
    drawCentred(w, buf, M(580,43), 24, tc, edge, true);
    drawCentred(w, "Level " + to_string(level), M(813,43), 20, ink, edge, true);
    drawCentred(w, "Score: " + to_string(score), M(1078,43), 18, ink, edge, true);
    drawCentred(w, to_string(delivered) + " / " + to_string(goal), M(1218,43), 18, ink, edge, true);
    // strikes (hearts)
    for (int i = 0; i < MAX_STRIKES; i++)
        drawCentred(w, "\xE2\x99\xA5", M(902 + i * 27, 43), 22, i < MAX_STRIKES - strikes ? sf::Color(235,45,60) : sf::Color(110,90,90), edge);
}
void drawDragSprite(sf::RenderWindow& w)
{
    if (dragKind == DRAG_NONE) return;
    sf::Vector2f art(mousePos.x / SX, mousePos.y / SY);
    shadowAt(w, mousePos + sf::Vector2f(4, 26), 26, 7, 60);
    if (dragKind == DRAG_TRAY)
    {
        if (dragId == 2) drawSpriteAt(w, texPattyRaw, 64, 66, art, 0.62f);
        else if (dragId == ID_POTATO) drawSpriteAt(w, texPotatoRaw, 64, 62, art, 0.62f);
        else if (dragId == ID_DOUGH) drawSpriteAt(w, texDough, 88, 64, art, 0.50f);
        else if (dragId == ID_PSAUCE) drawSpriteAt(w, texPSauce, 88, 64, art, 0.50f);
        else if (dragId == ID_PCHEESE) drawSpriteAt(w, texPCheese, 88, 64, art, 0.50f);
        else if (dragId == ID_PEPPERONI) drawSpriteAt(w, texPepperoni, 88, 64, art, 0.50f);
        else { sf::Sprite s(foodTextures[dragId]); s.setOrigin(sf::Vector2f(64.0f, 52.0f)); s.setScale(sf::Vector2f(0.85f, 0.70f)); s.setPosition(mousePos); w.draw(s); }
    }
    else if (dragKind == DRAG_PATTY) drawSpriteAt(w, foodTextures[2], 64, 66, art, 0.62f);
    else if (dragKind == DRAG_FRIES) drawSpriteAt(w, texFriesCooked, 64, 62, art, 0.66f);
    else if (dragKind == DRAG_BAKED_PIZZA) drawSpriteAt(w, texPizzaBaked, 88, 64, art, 0.66f);
    else if (dragKind == DRAG_RAW_PIZZA) drawSpriteAt(w, texPizzaRaw, 88, 64, art, 0.66f);
    else if (dragKind == DRAG_DRINK) drawGlass(w, art + sf::Vector2f(0, 33), 1.0f, drinkColor[dragSrc], 1.0f);
    else if (dragKind == DRAG_DISH)
    {
        if (dragId == DISH_BURGER) drawBurgerStack(w, mousePos + sf::Vector2f(0, 18), 7, sauceSkipped, 0.0f, 0.85f);
        else if (dragId == DISH_PIZZA) drawSpriteAt(w, texPizzaBaked, 88, 64, art, 0.66f);
        else if (dragId == DISH_FRIES) drawSpriteAt(w, texFriesCooked, 64, 62, art, 0.78f);
        else drawGlass(w, art + sf::Vector2f(0, 33), 1.0f, drinkColor[drinkFlavor], 1.0f);
    }
}
void drawDropGlows(sf::RenderWindow& w)
{
    if (dragKind == DRAG_NONE) return;
    if (dragKind == DRAG_TRAY)
    {
        if (dragId == 2) { for (int i = 0; i < NUM_BURNERS; i++) if (!burner[i].active) glowRect(w, burnerRect[i]); }
        else if (dragId == ID_POTATO) { for (int i = 0; i < NUM_BASKETS; i++) if (!basket[i].active) glowRect(w, basketRect[i]); }
        else if (dragId >= ID_DOUGH && dragId <= ID_PEPPERONI) { if (!pizzaAssembled && !pizzaReady && pizzaStep < 4 && dragId == pizzaRecipe[pizzaStep]) glowRect(w, pizzaRect); }
        else if (!assembled && step < 7 && dragId == recipe[step]) glowRect(w, burgerRect);
    }
    else if (dragKind == DRAG_PATTY) { if (!assembled && step < 7 && recipe[step] == 2) glowRect(w, burgerRect); }
    else if (dragKind == DRAG_FRIES) { if (!friesReady) glowRect(w, friesRect); }
    else if (dragKind == DRAG_RAW_PIZZA) { if (!oven.active) glowRect(w, ovenRect); }
    else if (dragKind == DRAG_BAKED_PIZZA) glowRect(w, pizzaRect);
    else if (dragKind == DRAG_DRINK) { if (!drinkReady) glowRect(w, drinkPlateRect); }
    else if (dragKind == DRAG_DISH)
        for (int i = 0; i < NUM_ORDERS; i++)
            if (orders[i].active && (orders[i].want & (1 << dragId)) && !(orders[i].got & (1 << dragId))) glowRect(w, cardRect[i], sf::Color(120,255,150));
}

void drawKitchenWorld(sf::RenderWindow& w)
{
    for (int i = 0; i < NUM_BURNERS; i++)
    {
        if (!burner[i].active || (dragKind == DRAG_PATTY && dragSrc == i)) continue;
        sf::Vector2f p(burnerPos[i][0], burnerPos[i][1]);
        drawCooking(w, texPattyRaw, foodTextures[2], texPattyBurnt, 64, 66, p, 0.60f, burner[i].t, pattyCook(), pattyBurn());
        drawTimerBar(w, sf::Vector2f(p.x, p.y - 38), burner[i].t, pattyCook(), pattyBurn());
    }
    for (int i = 0; i < NUM_BASKETS; i++)
    {
        if (!basket[i].active || (dragKind == DRAG_FRIES && dragSrc == i)) continue;
        sf::Vector2f p(basketPos[i][0], basketPos[i][1]);
        drawCooking(w, texPotatoRaw, texFriesCooked, texFriesBurnt, 64, 62, p, 0.62f, basket[i].t, fryCook(), fryBurn());
        drawTimerBar(w, sf::Vector2f(p.x, p.y - 42), basket[i].t, fryCook(), fryBurn());
    }
    if (oven.active && dragKind != DRAG_BAKED_PIZZA)
    {
        drawCooking(w, texPizzaRaw, texPizzaBaked, texPizzaBurnt, 88, 64, ovenPos, 0.78f, oven.t, pizzaCook(), pizzaBurn());
        drawTimerBar(w, sf::Vector2f(ovenPos.x, ovenPos.y - 56), oven.t, pizzaCook(), pizzaBurn());
    }
    for (int i = 0; i < NUM_DRINKS; i++)
    {
        if (!drinkFill[i].active || (dragKind == DRAG_DRINK && dragSrc == i)) continue;
        float frac = clamp01(drinkFill[i].t / FILL_TIME);
        if (frac < 1.0f)
        {
            float surf = GLASS_BOTTOM - 66 * frac * 0.92f;
            sf::RectangleShape stream(sf::Vector2f(3 * SX, (surf - NOZZLE_Y) * SY)); stream.setPosition(M(drinkX[i] - 1.5f, NOZZLE_Y));
            stream.setFillColor(drinkColor[i]); w.draw(stream);
        }
        drawGlass(w, sf::Vector2f(drinkX[i], GLASS_BOTTOM), 1.0f, drinkColor[i], frac);
        drawTimerBar(w, sf::Vector2f(drinkX[i], 352), drinkFill[i].t, FILL_TIME, 99999.0f, 30.0f, false);
    }
    // burger
    bool draggingBurger = (dragKind == DRAG_DISH && dragId == DISH_BURGER);
    if (!draggingBurger)
    {
        shadowAt(w, boardCenter + sf::Vector2f(0, 2), 36, 8, 40);
        float sc = assembled ? popScale(popDish[DISH_BURGER]) : 1.0f;
        if (step > 0) drawBurgerStack(w, boardCenter, step, sauceSkipped, popBurger, sc);
    }
    // pizza plate
    sf::Vector2f pp = pizzaPlate + sf::Vector2f(0, 2);
    if (pizzaReady && !(dragKind == DRAG_DISH && dragId == DISH_PIZZA)) drawSpriteAt(w, texPizzaBaked, 88, 64, pp, 0.62f * popScale(popDish[DISH_PIZZA]));
    else if (!pizzaReady && dragKind != DRAG_RAW_PIZZA)
    {
        float last = popScale(popPizzaLayer);
        if (pizzaStep >= 1) drawSpriteAt(w, texDough, 88, 64, pp, 0.62f * (pizzaStep == 1 ? last : 1.0f));
        if (pizzaStep >= 2) drawSpriteAt(w, texPSauce, 88, 64, pp, 0.62f * (pizzaStep == 2 ? last : 1.0f));
        if (pizzaStep >= 3) drawSpriteAt(w, texPCheese, 88, 64, pp, 0.62f * (pizzaStep == 3 ? last : 1.0f));
        if (pizzaStep >= 4) drawSpriteAt(w, texPepperoni, 88, 64, pp, 0.62f * (pizzaStep == 4 ? last : 1.0f));
    }
    if (drinkReady && !(dragKind == DRAG_DISH && dragId == DISH_DRINK))
        drawGlass(w, drinkPlate + sf::Vector2f(0, 14), 0.9f * popScale(popDish[DISH_DRINK]), drinkColor[drinkFlavor], 1.0f);
    if (friesReady && !(dragKind == DRAG_DISH && dragId == DISH_FRIES))
        drawSpriteAt(w, texFriesCooked, 64, 62, friesPlate + sf::Vector2f(0, 8), 0.75f * popScale(popDish[DISH_FRIES]));
    // hover highlight on trays / ready dishes
    if (dragKind == DRAG_NONE && state == ST_PLAY)
        for (int i = 0; i < NUM_TRAY; i++)
            if (tray[i].rect.contains(hoverPos)) { roundedRect(w, tray[i].rect, 8, sf::Color(255,255,255,40), sf::Color(255,255,255,120), 1.5f); break; }
}

void drawPlayOverlay(sf::RenderWindow& w)
{
    drawOrders(w);
    drawParticles(w);
    drawHud(w);
    // hint strip
    sf::FloatRect sb(sf::Vector2f(200, 610), sf::Vector2f(560, 24));
    roundedRect(w, sb, 6, sf::Color(255,247,235,235), sf::Color(190,125,85), 1);
    string status = (msgT > 0.0f) ? msg : kitchenHint();
    drawCentred(w, status, sf::Vector2f(480, 622), 13, sf::Color(110,65,48));
    drawDropGlows(w);
    drawDragSprite(w);
    for (int i = 0; i < MAX_FT; i++)
        if (fts[i].on)
        {
            sf::Color c = fts[i].col; c.a = (unsigned char)(255 * clamp01(fts[i].life / 0.5f));
            drawCentred(w, fts[i].s, sf::Vector2f(fts[i].x, fts[i].y), fts[i].size, c, sf::Color(40,20,10, c.a), true);
        }
}

// ----------------------------------------------------------------------------
// Buttons & menu screens
// ----------------------------------------------------------------------------
struct Btn { sf::FloatRect r; int id; };
Btn btns[16]; int btnCount = 0;
void addBtn(sf::FloatRect r, int id) { btns[btnCount].r = r; btns[btnCount].id = id; btnCount++; }
void drawButton(sf::RenderWindow& w, sf::FloatRect r, const string& label, int id, unsigned size = 24, bool enabled = true, sf::Color base = sf::Color(235,120,50))
{
    if (enabled) addBtn(r, id);
    bool hover = enabled && r.contains(hoverPos);
    sf::Color fill = enabled ? (hover ? sf::Color((unsigned char)(base.r + 15 > 255 ? 255 : base.r + 15), (unsigned char)(base.g + 25 > 255 ? 255 : base.g + 25), (unsigned char)(base.b + 25 > 255 ? 255 : base.b + 25)) : base) : sf::Color(185,170,160);
    sf::FloatRect sh = r; sh.position.y += 4; roundedRect(w, sh, 12, sf::Color(100,50,30,150), sf::Color(0,0,0,0), 0);
    sf::FloatRect rr = r; if (hover) { rr.position.y -= 1; }
    roundedRect(w, rr, 12, fill, sf::Color(130,60,25), 2.5f);
    drawCentred(w, label, sf::Vector2f(rr.position.x + rr.size.x / 2, rr.position.y + rr.size.y / 2 - 1), size, sf::Color::White, sf::Color(110,50,20), true);
}
void dimScreen(sf::RenderWindow& w, int a)
{
    sf::RectangleShape d(sf::Vector2f(960, 640)); d.setFillColor(sf::Color(25,10,8,(unsigned char)a)); w.draw(d);
}
void panel(sf::RenderWindow& w, sf::FloatRect r)
{
    sf::FloatRect sh = r; sh.position.x += 5; sh.position.y += 7; roundedRect(w, sh, 22, sf::Color(0,0,0,90), sf::Color(0,0,0,0), 0);
    roundedRect(w, r, 22, sf::Color(255,244,226), sf::Color(170,100,65), 5);
}
void drawStar(sf::RenderWindow& w, sf::Vector2f c, float r, bool filled)
{
    sf::ConvexShape s(10);
    for (int i = 0; i < 10; i++)
    {
        float a = (-90.0f + 36.0f * i) * PI_F / 180.0f, rad = (i % 2 == 0) ? r : r * 0.45f;
        s.setPoint(i, sf::Vector2f(c.x + cosf(a) * rad, c.y + sinf(a) * rad));
    }
    s.setFillColor(filled ? sf::Color(255,205,40) : sf::Color(205,185,165)); s.setOutlineColor(filled ? sf::Color(200,120,20) : sf::Color(160,135,115)); s.setOutlineThickness(3);
    w.draw(s);
}
string fmtTime(float t) { int s = (int)t; char b[16]; snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); return b; }

void drawTitle(sf::RenderWindow& w)
{
    dimScreen(w, 150);
    // bobbing food icons
    for (int i = 0; i < 4; i++)
        drawIcon(w, i, sf::Vector2f(330 + i * 100, 150 + sinf(globalTime * 2.2f + i * 1.3f) * 9), 70, 255);
    drawCentred(w, "Sizzle", sf::Vector2f(480, 255), 96, sf::Color(255,205,70), sf::Color(120,45,20), true);
    drawCentred(w, "Cozy Cafe", sf::Vector2f(480, 330), 38, sf::Color(255,240,220), sf::Color(120,45,20), true);
    drawButton(w, sf::FloatRect(sf::Vector2f(370, 385), sf::Vector2f(220, 62)), "PLAY", 1, 34);
    sf::FloatRect hp(sf::Vector2f(190, 480), sf::Vector2f(580, 120));
    roundedRect(w, hp, 14, sf::Color(255,244,226,225), sf::Color(170,100,65), 3);
    drawCentred(w, "How to play", sf::Vector2f(480, 497), 17, sf::Color(150,70,40), sf::Color(0,0,0,0), true);
    drawCentred(w, "Customers order on the left. Cook patties, fries and pizza, fill drinks,", sf::Vector2f(480, 524), 14, sf::Color(110,65,48));
    drawCentred(w, "then drag each finished dish from its plate onto the order card.", sf::Vector2f(480, 545), 14, sf::Color(110,65,48));
    drawCentred(w, "Don't let patience run out (3 strikes) and don't burn the food!", sf::Vector2f(480, 566), 14, sf::Color(110,65,48));
    drawCentred(w, "Esc = pause    S = skip ketchup", sf::Vector2f(480, 587), 13, sf::Color(150,110,90));
}
void drawIntro(sf::RenderWindow& w)
{
    dimScreen(w, 140);
    sf::FloatRect p(sf::Vector2f(240, 150), sf::Vector2f(480, 340)); panel(w, p);
    drawCentred(w, "Level " + to_string(level), sf::Vector2f(480, 205), 48, sf::Color(235,120,50), sf::Color(120,45,20), true);
    drawCentred(w, "Serve " + to_string(levelGoal(level)) + " orders", sf::Vector2f(480, 270), 26, sf::Color(110,65,48));
    drawCentred(w, "Time: " + fmtTime(levelTime(level)), sf::Vector2f(480, 308), 22, sf::Color(110,65,48));
    drawCentred(w, "3 strikes (customers who leave) and you're out", sf::Vector2f(480, 342), 16, sf::Color(150,70,40));
    if (level == 1) drawCentred(w, "Tip: the patty must be cooked on the stove first!", sf::Vector2f(480, 376), 15, sf::Color(130,95,75));
    drawButton(w, sf::FloatRect(sf::Vector2f(370, 410), sf::Vector2f(220, 58)), "START", 2, 30);
}
void drawPause(sf::RenderWindow& w)
{
    dimScreen(w, 160);
    panel(w, sf::FloatRect(sf::Vector2f(300, 150), sf::Vector2f(360, 340)));
    drawCentred(w, "Paused", sf::Vector2f(480, 205), 46, sf::Color(235,120,50), sf::Color(120,45,20), true);
    drawButton(w, sf::FloatRect(sf::Vector2f(350, 255), sf::Vector2f(260, 54)), "Resume", 3, 26);
    drawButton(w, sf::FloatRect(sf::Vector2f(350, 325), sf::Vector2f(260, 54)), "Restart Level", 4, 24, true, sf::Color(90,150,210));
    drawButton(w, sf::FloatRect(sf::Vector2f(350, 395), sf::Vector2f(260, 54)), "Quit to Title", 5, 24, true, sf::Color(150,110,100));
}
void drawResult(sf::RenderWindow& w)
{
    dimScreen(w, 170);
    sf::FloatRect p(sf::Vector2f(150, 25), sf::Vector2f(660, 590)); panel(w, p);
    drawCentred(w, levelWon ? "Level Complete!" : "Level Failed", sf::Vector2f(480, 72), 44, levelWon ? sf::Color(70,175,80) : sf::Color(215,70,55), sf::Color(90,45,25), true);
    drawCentred(w, resultReason, sf::Vector2f(480, 112), 16, sf::Color(140,95,75));
    for (int i = 0; i < 3; i++) drawStar(w, sf::Vector2f(400 + i * 80, 170), 32, i < starsEarned);
    drawCentred(w, "Orders served: " + to_string(delivered) + " / " + to_string(goal) + "      Strikes: " + to_string(strikes), sf::Vector2f(480, 228), 18, sf::Color(110,65,48));
    drawCentred(w, "Score: " + to_string(score) + "  (+" + to_string(levelScoreGain) + ")      Coins: " + to_string(coins) + "  (+" + to_string(levelCoins) + ")", sf::Vector2f(480, 258), 18, sf::Color(110,65,48));
    drawCentred(w, "Upgrade shop", sf::Vector2f(480, 303), 22, sf::Color(150,70,40), sf::Color(0,0,0,0), true);
    for (int i = 0; i < 4; i++)
    {
        float y = 330 + i * 52;
        roundedRect(w, sf::FloatRect(sf::Vector2f(185, y), sf::Vector2f(590, 44)), 10, sf::Color(250,230,205), sf::Color(205,160,125), 2);
        drawText(w, upName[i], 200, y + 10, 20, sf::Color(110,65,48));
        int lv = *upPtr[i];
        for (int k = 0; k < MAX_UP; k++)
        {
            sf::CircleShape c(8.0f, 16); c.setPosition(sf::Vector2f(450 + k * 24, y + 14)); c.setFillColor(k < lv ? sf::Color(255,190,40) : sf::Color(215,195,175)); c.setOutlineColor(sf::Color(170,110,40)); c.setOutlineThickness(2); w.draw(c);
        }
        if (lv >= MAX_UP) drawButton(w, sf::FloatRect(sf::Vector2f(630, y + 4), sf::Vector2f(130, 36)), "MAX", 0, 18, false);
        else { int cost = upCost(lv); drawButton(w, sf::FloatRect(sf::Vector2f(630, y + 4), sf::Vector2f(130, 36)), "Buy " + to_string(cost), 10 + i, 18, coins >= cost, sf::Color(80,170,90)); }
    }
    drawButton(w, sf::FloatRect(sf::Vector2f(340, 548), sf::Vector2f(280, 56)), levelWon ? "Next Level" : "Try Again", 6, 28);
}

// ----------------------------------------------------------------------------
// Input handling in the kitchen
// ----------------------------------------------------------------------------
void startDish(int type, sf::Vector2f p) { dragKind = DRAG_DISH; dragId = type; mousePos = p; }

void kitchenPress(sf::Vector2f p)
{
    for (int i = 0; i < NUM_BURNERS; i++)
        if (burner[i].active && burnerRect[i].contains(p))
        {
            int st = cookState(burner[i].t, pattyCook(), pattyBurn());
            if (st == 2) { burner[i].active = false; setMsg("Burnt patty thrown away."); playSound(SFX_CLICK); }
            else if (st == 1) { dragKind = DRAG_PATTY; dragSrc = i; mousePos = p; }
            else setMsg("The patty is still cooking...");
            return;
        }
    for (int i = 0; i < NUM_BASKETS; i++)
        if (basket[i].active && basketRect[i].contains(p))
        {
            int st = cookState(basket[i].t, fryCook(), fryBurn());
            if (st == 2) { basket[i].active = false; setMsg("Burnt fries thrown away."); playSound(SFX_CLICK); }
            else if (st == 1) { dragKind = DRAG_FRIES; dragSrc = i; mousePos = p; }
            else setMsg("The fries are still frying...");
            return;
        }
    if (oven.active && ovenRect.contains(p))
    {
        int st = cookState(oven.t, pizzaCook(), pizzaBurn());
        if (st == 2) { oven.active = false; setMsg("Burnt pizza thrown away."); playSound(SFX_CLICK); }
        else if (st == 1) { dragKind = DRAG_BAKED_PIZZA; mousePos = p; }
        else setMsg("The pizza is still baking...");
        return;
    }
    for (int i = 0; i < NUM_DRINKS; i++)
        if (drinkRect[i].contains(p))
        {
            if (!drinkFill[i].active) { drinkFill[i].active = true; drinkFill[i].t = 0; playSound(SFX_POUR, 55); }
            else if (drinkFill[i].t < FILL_TIME) setMsg("Still filling...");
            else { dragKind = DRAG_DRINK; dragSrc = i; mousePos = p; }
            return;
        }
    // finished dishes -> drag onto an order card
    if (assembled && burgerRect.contains(p)) { startDish(DISH_BURGER, p); return; }
    if (pizzaReady && pizzaRect.contains(p)) { startDish(DISH_PIZZA, p); return; }
    if (friesReady && friesRect.contains(p)) { startDish(DISH_FRIES, p); return; }
    if (drinkReady && drinkPlateRect.contains(p)) { startDish(DISH_DRINK, p); return; }
    if (pizzaAssembled && !pizzaReady && pizzaRect.contains(p)) { dragKind = DRAG_RAW_PIZZA; mousePos = p; return; }
    for (int i = 0; i < NUM_TRAY; i++)
        if (tray[i].rect.contains(p)) { dragKind = DRAG_TRAY; dragId = tray[i].id; mousePos = p; return; }
}

void kitchenRelease(sf::Vector2f p)
{
    if (dragKind == DRAG_TRAY)
    {
        int id = dragId;
        if (id == 2)
        {
            for (int i = 0; i < NUM_BURNERS; i++)
                if (burnerRect[i].contains(p))
                {
                    if (burner[i].active) { setMsg("That burner is busy."); playSound(SFX_ERROR, 60); }
                    else { burner[i].active = true; burner[i].t = 0; burner[i].st = 0; playSound(SFX_PLACE, 90); sparkleAt(M(burnerPos[i][0], burnerPos[i][1])); }
                    cancelDrag(); return;
                }
            if (burgerRect.contains(p)) { setMsg("Cook the patty on the stove first!"); playSound(SFX_ERROR, 60); }
        }
        else if (id == ID_POTATO)
        {
            for (int i = 0; i < NUM_BASKETS; i++)
                if (basketRect[i].contains(p))
                {
                    if (basket[i].active) { setMsg("That basket is busy."); playSound(SFX_ERROR, 60); }
                    else { basket[i].active = true; basket[i].t = 0; basket[i].st = 0; playSound(SFX_PLACE, 90); burstStars(M(basketPos[i][0], basketPos[i][1]), sf::Color(255,255,255), 6, 60); }
                    cancelDrag(); return;
                }
        }
        else if (id >= ID_DOUGH && id <= ID_PEPPERONI)
        {
            if (pizzaRect.contains(p))
            {
                if (pizzaReady || pizzaAssembled) { setMsg("The pizza plate is full."); playSound(SFX_ERROR, 60); }
                else if (id == pizzaRecipe[pizzaStep])
                {
                    pizzaStep++; popPizzaLayer = POP; playSound(SFX_PLACE, 90);
                    if (pizzaStep >= 4) { pizzaAssembled = true; setMsg("Pizza built! Drag it into the oven."); sparkleAt(M(pizzaPlate.x, pizzaPlate.y)); }
                }
                else { setMsg("That is not the next pizza ingredient."); playSound(SFX_ERROR, 60); }
            }
        }
        else
        {
            if (burgerRect.contains(p) && !assembled && step < 7)
            {
                if (id == recipe[step])
                {
                    step++; popBurger = POP; playSound(SFX_PLACE, 90);
                    if (step >= 7) { assembled = true; popDish[DISH_BURGER] = POP; sparkleAt(M(boardCenter.x, boardCenter.y - 30) / 1.0f); playSound(SFX_DING, 60); }
                }
                else { setMsg("That ingredient is not next in the recipe."); playSound(SFX_ERROR, 60); }
            }
        }
    }
    else if (dragKind == DRAG_PATTY)
    {
        if (burgerRect.contains(p) && !assembled)
        {
            if (step < 7 && recipe[step] == 2)
            {
                step++; burner[dragSrc].active = false; popBurger = POP; playSound(SFX_PLACE, 90);
                if (step >= 7) { assembled = true; popDish[DISH_BURGER] = POP; }
            }
            else if (step == 1) { setMsg("Add the sauce (or press S to skip) before the patty."); playSound(SFX_ERROR, 60); }
            else { setMsg("The patty is not next yet."); playSound(SFX_ERROR, 60); }
        }
    }
    else if (dragKind == DRAG_FRIES)
    {
        if (friesRect.contains(p))
        {
            if (friesReady) { setMsg("The fries plate is already full."); playSound(SFX_ERROR, 60); }
            else { friesReady = true; basket[dragSrc].active = false; popDish[DISH_FRIES] = POP; playSound(SFX_PLACE, 90); sparkleAt(M(friesPlate.x, friesPlate.y)); }
        }
    }
    else if (dragKind == DRAG_DRINK)
    {
        if (drinkPlateRect.contains(p))
        {
            if (drinkReady) { setMsg("The drink plate is already full."); playSound(SFX_ERROR, 60); }
            else { drinkReady = true; drinkFlavor = dragSrc; drinkFill[dragSrc].active = false; drinkFill[dragSrc].t = 0; popDish[DISH_DRINK] = POP; playSound(SFX_PLACE, 90); sparkleAt(M(drinkPlate.x, drinkPlate.y)); }
        }
    }
    else if (dragKind == DRAG_RAW_PIZZA)
    {
        if (ovenRect.contains(p))
        {
            if (oven.active) { setMsg("The oven is busy."); playSound(SFX_ERROR, 60); }
            else { oven.active = true; oven.t = 0; oven.st = 0; pizzaAssembled = false; pizzaStep = 0; playSound(SFX_PLACE, 90); sparkleAt(M(ovenPos.x, ovenPos.y)); }
        }
    }
    else if (dragKind == DRAG_BAKED_PIZZA)
    {
        if (pizzaRect.contains(p))
        {
            if (pizzaStep == 0 && !pizzaAssembled && !pizzaReady) { pizzaReady = true; oven.active = false; popDish[DISH_PIZZA] = POP; playSound(SFX_PLACE, 90); sparkleAt(M(pizzaPlate.x, pizzaPlate.y)); }
            else { setMsg("Clear the pizza plate first."); playSound(SFX_ERROR, 60); }
        }
    }
    else if (dragKind == DRAG_DISH)
    {
        bool dropped = false;
        for (int i = 0; i < NUM_ORDERS; i++)
            if (cardRect[i].contains(p))
            {
                dropped = true;
                if (!orders[i].active) { setMsg("No customer there yet."); playSound(SFX_ERROR, 60); }
                else if (!(orders[i].want & (1 << dragId))) { setMsg("That customer didn't order a " + dishName[dragId] + "."); playSound(SFX_ERROR, 70); }
                else if (orders[i].got & (1 << dragId)) { setMsg("They already have their " + dishName[dragId] + "."); playSound(SFX_ERROR, 60); }
                else { int type = dragId; cancelDrag(); deliverDish(type, i); return; }
                break;
            }
        (void)dropped;
    }
    cancelDrag();
}

// ----------------------------------------------------------------------------
// Menu actions
// ----------------------------------------------------------------------------
void newGame() { level = 1; score = 0; coins = 0; upStove = upFryer = upOven = upPatience = 0; }
void showIntro() { state = ST_INTRO; startLevel(); timeLeft = levelTime(level); }

void handleButton(int id)
{
    playSound(SFX_CLICK, 80);
    if (id == 1) { newGame(); showIntro(); }
    else if (id == 2) { state = ST_PLAY; }
    else if (id == 3) { state = ST_PLAY; }
    else if (id == 4) { score = scoreAtLevelStart; coins = coinsAtLevelStart; showIntro(); }
    else if (id == 5) { state = ST_TITLE; }
    else if (id == 6)
    {
        if (levelWon) level++;
        else score = scoreAtLevelStart;
        showIntro();
    }
    else if (id >= 10 && id <= 13)
    {
        int i = id - 10, lv = *upPtr[i];
        if (lv < MAX_UP && coins >= upCost(lv)) { coins -= upCost(lv); (*upPtr[i])++; playSound(SFX_COIN, 80); }
    }
}

void drawFrame(sf::RenderWindow& w)
{
    sf::Sprite bg(texKitchen);
    sf::Vector2u size = texKitchen.getSize();
    bg.setScale(sf::Vector2f(960.0f / size.x, 640.0f / size.y));
    w.draw(bg);
    btnCount = 0;
    if (state == ST_TITLE) { drawTitle(w); return; }
    drawKitchenWorld(w);
    drawPlayOverlay(w);
    if (state == ST_INTRO) drawIntro(w);
    else if (state == ST_PAUSE) drawPause(w);
    else if (state == ST_RESULT) drawResult(w);
}

// ----------------------------------------------------------------------------
int main()
{
    srand((unsigned)time(nullptr));
    if (!font.openFromFile("assets/DejaVuSans.ttf")) { cout << "Font missing in assets folder.\n"; return 1; }
    if (!texKitchen.loadFromFile("assets/kitchen.png")) { cout << "Missing assets/kitchen.png\n"; return 1; }
    for (int i = 0; i < 10; i++)
        if (!foodTextures[i].loadFromFile("assets/" + foodFiles[i] + ".png")) { cout << "Missing ingredient image: " << foodFiles[i] << ".png\n"; return 1; }
    struct TexFile { sf::Texture* tex; const char* file; };
    TexFile extra[16] = {
        { &texPattyRaw, "patty_raw" }, { &texPattyBurnt, "patty_burnt" }, { &texDough, "pizza_dough" }, { &texPSauce, "pizza_sauce" },
        { &texPCheese, "pizza_cheese" }, { &texPepperoni, "pizza_pepperoni" }, { &texPizzaRaw, "pizza_raw" }, { &texPizzaBaked, "pizza_baked" },
        { &texPizzaBurnt, "pizza_burnt" }, { &texPotatoRaw, "potato_raw" }, { &texFriesCooked, "fries_cooked" }, { &texFriesBurnt, "fries_burnt" },
        { &texIcon[0], "icon_burger" }, { &texIcon[1], "icon_pizza" }, { &texIcon[2], "icon_fries" }, { &texIcon[3], "icon_drink" }
    };
    for (int i = 0; i < 16; i++)
        if (!extra[i].tex->loadFromFile(string("assets/") + extra[i].file + ".png")) { cout << "Missing image: " << extra[i].file << ".png\n"; return 1; }
    for (int i = 0; i < NUM_BURNERS; i++) burnerRect[i] = R(burnerPos[i][0] - 46, burnerPos[i][1] - 32, 92, 64);
    for (int i = 0; i < NUM_BASKETS; i++) basketRect[i] = R(basketPos[i][0] - 42, basketPos[i][1] - 36, 84, 72);
    for (int i = 0; i < NUM_DRINKS; i++) drinkRect[i] = R(drinkX[i] - 26, 275, 52, 158);
    for (int i = 0; i < NUM_ORDERS; i++) cardRect[i] = R(12, cardTop[i], 175, 108);
    resetKitchen();
    for (int i = 0; i < NUM_ORDERS; i++) orders[i].active = false;
    for (int i = 0; i < MAX_PART; i++) parts[i].on = false;
    for (int i = 0; i < MAX_FT; i++) fts[i].on = false;
    timeLeft = levelTime(1);
    loadAudio();

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(960u, 640u)), "Sizzle - Cozy Cafe");
    window.setFramerateLimit(60);
    sf::Clock clock;
    sf::Vector2f pauseBtnCentre = M(72, 45);

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;
        globalTime += dt;

        while (const optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->is<sf::Event::FocusLost>() && state == ST_PLAY) { cancelDrag(); state = ST_PAUSE; }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape || key->code == sf::Keyboard::Key::P)
                {
                    if (state == ST_PLAY) { cancelDrag(); state = ST_PAUSE; }
                    else if (state == ST_PAUSE) state = ST_PLAY;
                }
                if (state == ST_PLAY && key->code == sf::Keyboard::Key::S && step == 1 && !assembled) { sauceSkipped = true; step = 2; playSound(SFX_CLICK, 70); }
                if (state == ST_PLAY && key->code == sf::Keyboard::Key::R) { resetKitchen(); setMsg("Kitchen cleared."); }
            }
            if (const auto* press = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (press->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f p((float)press->position.x, (float)press->position.y);
                    bool menu = (state != ST_PLAY);
                    if (menu)
                    {
                        for (int i = btnCount - 1; i >= 0; i--)
                            if (btns[i].r.contains(p)) { handleButton(btns[i].id); break; }
                    }
                    else
                    {
                        float dx = p.x - pauseBtnCentre.x, dy = p.y - pauseBtnCentre.y;
                        if (dx * dx + dy * dy < 32.0f * 32.0f) { playSound(SFX_CLICK, 80); state = ST_PAUSE; }
                        else kitchenPress(p);
                    }
                }
            }
            if (const auto* move = event->getIf<sf::Event::MouseMoved>())
            {
                hoverPos = sf::Vector2f((float)move->position.x, (float)move->position.y);
                if (dragKind != DRAG_NONE) mousePos = hoverPos;
            }
            if (const auto* release = event->getIf<sf::Event::MouseButtonReleased>())
                if (release->button == sf::Mouse::Button::Left && dragKind != DRAG_NONE)
                    kitchenRelease(sf::Vector2f((float)release->position.x, (float)release->position.y));
        }

        // simulation (only while actually playing)
        if (state == ST_PLAY)
        {
            updateGame(dt);
            if (popBurger > 0) popBurger -= dt;
            if (popPizzaLayer > 0) popPizzaLayer -= dt;
            for (int i = 0; i < 4; i++) if (popDish[i] > 0) popDish[i] -= dt;
            if (msgT > 0.0f) msgT -= dt;
        }
        if (state != ST_TITLE || true)
        {
            updateParticles(dt);
            for (int i = 0; i < MAX_FT; i++)
                if (fts[i].on) { fts[i].life -= dt; fts[i].y -= 38.0f * dt; if (fts[i].life <= 0) fts[i].on = false; }
        }
#if ENABLE_AUDIO
        setLoop(sizzleSnd, state == ST_PLAY && anyCooking(burner, NUM_BURNERS));
        setLoop(bubbleSnd, state == ST_PLAY && anyCooking(basket, NUM_BASKETS));
        if (musicOk) music.setVolume(state == ST_PLAY ? 28.0f : 38.0f);
#endif
        window.clear(sf::Color(255,245,235));
        drawFrame(window);
        window.display();
    }
    return 0;
}
