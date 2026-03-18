#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <map>
#include <cstring>
#include <set>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "Player.h"
#include "Enemy.h"
#pragma comment(lib, "winhttp.lib")
#include "ApiClient.h"

using namespace sf;
using namespace std;

const static int MAP_WIDTH = 500;
const static int MAP_HEIGHT = 20;
std::string FormatTime(float seconds);
int BackgroundMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};
int MAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};
int MobMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};
int InterestingMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};

float TileSize = 40.f;

View view1(FloatRect({ 0, 0 }, { 1200, 800 }));

enum GameState {
    MAIN_MENU,
    LEVELS_MENU,
    CREATORS_MENU,
    SETTINGS_MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    BEST_TIMES_MENU,
    LOGIN_MENU, REGISTER_MENU, LEADERBOARD_MENU, QUESTS_MENU, SHOP_MENU
};

int currentLevel = 1;
bool debugMode = false;
bool limitedDashMode = false;

struct LevelRecord {
    int level;
    float time;
    int coins;
    int kills;
    bool completed;

    LevelRecord() : level(0), time(999999.f), coins(0), kills(0), completed(false) {}
    LevelRecord(int l, float t, int c, int k, bool comp)
        : level(l), time(t), coins(c), kills(k), completed(comp) {
    }
};

std::map<int, LevelRecord> bestRecords;

void saveBestRecord(const LevelRecord& record) {
    std::map<int, LevelRecord> allRecords;

    std::ifstream inFile("best_records.txt");
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            LevelRecord rec;
            int completed_int;

            if (iss >> rec.level >> rec.time >> rec.coins >> rec.kills >> completed_int) {
                rec.completed = (completed_int == 1);
                allRecords[rec.level] = rec;
            }
        }
        inFile.close();
    }

    if (allRecords.find(record.level) == allRecords.end() ||
        record.time < allRecords[record.level].time) {
        allRecords[record.level] = record;
    }

    std::ofstream outFile("best_records.txt");
    if (outFile.is_open()) {
        outFile << "# Level Time Coins Kills Completed\n";
        for (const auto& pair : allRecords) {
            outFile << pair.second.level << " "
                << pair.second.time << " "
                << pair.second.coins << " "
                << pair.second.kills << " "
                << (pair.second.completed ? 1 : 0) << "\n";
        }
        outFile.close();
    }
}

void loadBestRecords() {
    bestRecords.clear();
    std::ifstream file("best_records.txt");

    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        LevelRecord rec;
        int completed_int;

        if (iss >> rec.level >> rec.time >> rec.coins >> rec.kills >> completed_int) {
            rec.completed = (completed_int == 1);
            bestRecords[rec.level] = rec;
        }
    }

    file.close();
}

bool loadLevel(int levelNumber, std::vector<std::tuple<int, int, int>>& mobTemplate) {
    string filename = "Map" + to_string(levelNumber) + ".txt";
    FILE* file;
    if (errno_t err_n = fopen_s(&file, filename.c_str(), "r")) {
        cout << "Map file " << filename << " not found!" << endl;
        return false;
    }

    fread(&BackgroundMAP, sizeof(BackgroundMAP), 1, file);
    fread(&MAP, sizeof(MAP), 1, file);
    fread(&MobMAP, sizeof(MobMAP), 1, file);
    fread(&InterestingMAP, sizeof(InterestingMAP), 1, file);
    fclose(file);

    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (BackgroundMAP[y][x] == 0 && MAP[y][x] == 0)
                BackgroundMAP[y][x] = 5;
        }

    mobTemplate.clear();
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            int tile = MobMAP[y][x];
            if (tile == 8 || tile == 9) {
                mobTemplate.emplace_back(x, y, tile);
                MobMAP[y][x] = -1;
            }
        }
    }

    cout << "Level " << levelNumber << " loaded successfully!" << endl;
    return true;
}

void analyzeMaps() {
    std::set<int> bgTiles, mainTiles, mobTiles, interestTiles;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (BackgroundMAP[y][x] != -1 && BackgroundMAP[y][x] != 0)
                bgTiles.insert(BackgroundMAP[y][x]);
            if (MAP[y][x] != -1 && MAP[y][x] != 0)
                mainTiles.insert(MAP[y][x]);
            if (MobMAP[y][x] != -1 && MobMAP[y][x] != 0)
                mobTiles.insert(MobMAP[y][x]);
            if (InterestingMAP[y][x] != -1 && InterestingMAP[y][x] != 0)
                interestTiles.insert(InterestingMAP[y][x]);
        }
    }

    cout << "\n=== MAP ANALYSIS ===" << endl;
    cout << "Background tiles: ";
    for (int t : bgTiles) cout << t << " ";
    cout << endl;

    cout << "Main MAP tiles: ";
    for (int t : mainTiles) cout << t << " ";
    cout << endl;

    cout << "Mob tiles: ";
    for (int t : mobTiles) cout << t << " ";
    cout << endl;

    cout << "Interesting tiles: ";
    for (int t : interestTiles) cout << t << " ";
    cout << endl;

    cout << "\n=== HAZARD DETECTION ===" << endl;
    int lavaCount = 0, spikeCount = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int t1 = MAP[y][x];
            int t2 = BackgroundMAP[y][x];
            int t3 = MobMAP[y][x];
            int t4 = InterestingMAP[y][x];

            if (t1 == 16 || t1 == 17 || t2 == 16 || t2 == 17 ||
                t3 == 16 || t3 == 17 || t4 == 16 || t4 == 17) {
                lavaCount++;
            }

            if ((t1 >= 18 && t1 <= 21) || (t2 >= 18 && t2 <= 21) ||
                (t3 >= 18 && t3 <= 21) || (t4 >= 18 && t4 <= 21)) {
                spikeCount++;
            }
        }
    }

    cout << "Total lava tiles: " << lavaCount << endl;
    cout << "Total spike tiles: " << spikeCount << endl;
    cout << "==================\n" << endl;
}

static string keyToString(Keyboard::Key k) {
    int ki = static_cast<int>(k);
    int a = static_cast<int>(Keyboard::Key::A);
    int z = static_cast<int>(Keyboard::Key::Z);
    if (ki >= a && ki <= z) {
        char c = 'A' + (ki - a);
        return string(1, c);
    }
    int n0 = static_cast<int>(Keyboard::Key::Num0);
    int n9 = static_cast<int>(Keyboard::Key::Num9);
    if (ki >= n0 && ki <= n9) {
        char c = '0' + (ki - n0);
        return string(1, c);
    }
    switch (k) {
    case Keyboard::Key::Space: return "Space";
    case Keyboard::Key::LShift: return "LShift";
    case Keyboard::Key::RShift: return "RShift";
    case Keyboard::Key::LControl: return "LCtrl";
    case Keyboard::Key::RControl: return "RCtrl";
    case Keyboard::Key::LAlt: return "LAlt";
    case Keyboard::Key::RAlt: return "RAlt";
    case Keyboard::Key::Escape: return "Esc";
    case Keyboard::Key::Enter: return "Enter";
    case Keyboard::Key::Up: return "Up";
    case Keyboard::Key::Down: return "Down";
    case Keyboard::Key::Left: return "Left";
    case Keyboard::Key::Right: return "Right";
    case Keyboard::Key::Tab: return "Tab";
    case Keyboard::Key::Backspace: return "Back";
    default:
        return "Key" + to_string(static_cast<int>(k));
    }
}

void drawMap(RenderWindow& window, map<int, Sprite>& spriteSheet)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            int tile = MAP[y][x];
            if (tile == -1) continue;

            auto sprite = spriteSheet.find(tile);
            if (sprite == spriteSheet.end())
                sprite = spriteSheet.find(0);

            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void drawBg(RenderWindow& window, map<int, Sprite>& spriteSheet)
{
    Vector2f topLeft = view1.getCenter() - view1.getSize() / 2.f;
    Vector2f bottomRight = view1.getCenter() + view1.getSize() / 2.f;

    int startX = max(0, (int)(topLeft.x / TileSize) - 1);
    int startY = max(0, (int)(topLeft.y / TileSize) - 1);
    int endX = min(MAP_WIDTH, (int)(bottomRight.x / TileSize) + 2);
    int endY = min(MAP_HEIGHT, (int)(bottomRight.y / TileSize) + 2);

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int tile = BackgroundMAP[y][x];
            auto sprite = spriteSheet.find(tile);
            if (sprite == spriteSheet.end())
                sprite = spriteSheet.find(5);

            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void drawMob(RenderWindow& window, map<int, Sprite>& spriteSheet)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            int tile = MobMAP[y][x];
            if (tile == -1) continue;

            auto sprite = spriteSheet.find(tile);
            if (sprite == spriteSheet.end())
                sprite = spriteSheet.find(0);

            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void drawInteresting(RenderWindow& window, map<int, Sprite>& spriteSheet)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            int tile = InterestingMAP[y][x];
            if (tile == -1) continue;

            auto sprite = spriteSheet.find(tile);
            if (sprite == spriteSheet.end())
                sprite = spriteSheet.find(0);

            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void DrawMenuButton(RenderWindow& window, const FloatRect& rect, const string& text, const Font& font, bool isSelected = false) {
    const float x = rect.position.x, y = rect.position.y;
    const float w = rect.size.x, h = rect.size.y;
    const float px = 4.f;

    Color cBase = isSelected ? Color(160, 148, 120) : Color(130, 118, 95);
    Color cLight = isSelected ? Color(200, 190, 160) : Color(175, 163, 132);
    Color cDark = isSelected ? Color(80, 72, 55) : Color(70, 62, 45);
    Color cShadow = Color(40, 35, 25, 180);
    Color cCorner = Color(90, 82, 62);

    RectangleShape shadow({ w, h });
    shadow.setPosition({ x + px, y + px });
    shadow.setFillColor(cShadow);
    window.draw(shadow);

    RectangleShape base({ w, h });
    base.setPosition({ x, y });
    base.setFillColor(cBase);
    window.draw(base);

    RectangleShape etop({ w - px * 2, px });
    etop.setPosition({ x + px, y });
    etop.setFillColor(cLight);
    window.draw(etop);

    RectangleShape eleft({ px, h - px * 2 });
    eleft.setPosition({ x, y + px });
    eleft.setFillColor(cLight);
    window.draw(eleft);

    RectangleShape ebot({ w - px * 2, px });
    ebot.setPosition({ x + px, y + h - px });
    ebot.setFillColor(cDark);
    window.draw(ebot);

    RectangleShape eright({ px, h - px * 2 });
    eright.setPosition({ x + w - px, y + px });
    eright.setFillColor(cDark);
    window.draw(eright);

    auto drawPx = [&](float px_x, float px_y, Color c) {
        RectangleShape p({ px, px });
        p.setPosition({ px_x, px_y });
        p.setFillColor(c);
        window.draw(p);
        };
    drawPx(x, y, cCorner);
    drawPx(x + w - px, y, cCorner);
    drawPx(x, y + h - px, cCorner);
    drawPx(x + w - px, y + h - px, cCorner);

    if (!isSelected) {
        RectangleShape crack1({ px * 3, px });
        crack1.setPosition({ x + w * 0.22f, y + h * 0.35f });
        crack1.setFillColor(Color(75, 68, 50, 130));
        window.draw(crack1);
        RectangleShape crack2({ px * 2, px });
        crack2.setPosition({ x + w * 0.65f, y + h * 0.62f });
        crack2.setFillColor(Color(75, 68, 50, 130));
        window.draw(crack2);
    }

    Text txt(font, text, 19);
    txt.setFillColor(isSelected ? Color(255, 248, 200) : Color(230, 220, 185));
    txt.setOutlineColor(Color(50, 42, 28, 220));
    txt.setOutlineThickness(2.f);
    FloatRect bounds = txt.getLocalBounds();
    txt.setPosition({
        x + (w - bounds.size.x) / 2.f,
        y + (h - bounds.size.y) / 2.f + 7.f
        });
    window.draw(txt);
}

void DrawMainMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    RectangleShape titleBg({ 560.f, 90.f });
    titleBg.setPosition({ 320.f, 100.f });
    titleBg.setFillColor(Color(0, 0, 0, 100));
    window.draw(titleBg);

    Text title(font, "PIXELRUN", 72);
    title.setFillColor(Color::White);
    title.setStyle(Text::Bold);
    title.setOutlineColor(Color(60, 80, 200));
    title.setOutlineThickness(3.f);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 108.f });
    window.draw(title);

    const float btnW = 240.f, btnH = 52.f, gap = 14.f;
    const float col1X = 270.f, col2X = 690.f;
    const float startY = 250.f;

    struct Btn { string label; float x, y; };
    vector<Btn> btns = {
        { "LEVELS",   col1X, startY },
        { "SHOP",     col1X, startY + (btnH + gap) },
        { "LOGIN",    col1X, startY + (btnH + gap) * 2 },
        { "CREATORS", col2X, startY },
        { "SETTINGS", col2X, startY + (btnH + gap) },
        { "QUESTS",   col2X, startY + (btnH + gap) * 2 },
    };

    for (auto& b : btns) {
        FloatRect r({ b.x, b.y }, { btnW, btnH });
        DrawMenuButton(window, r, b.label, font, r.contains(Vector2f(mousePos)));
    }

    FloatRect exitRect({ (1200.f - 200.f) / 2.f, startY + (btnH + gap) * 3 + 10.f }, { 200.f, 44.f });
    DrawMenuButton(window, exitRect, "EXIT", font, exitRect.contains(Vector2f(mousePos)));
}

void DrawLevelsMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    Text title(font, "LEVELS", 48);
    title.setFillColor(Color::White);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 120.f });
    window.draw(title);

    FloatRect level1Rect({ 450.f, 250.f }, { 300.f, 50.f });
    DrawMenuButton(window, level1Rect, "LEVEL 1", font, level1Rect.contains(Vector2f(mousePos)));

    FloatRect level2Rect({ 450.f, 320.f }, { 300.f, 50.f });
    DrawMenuButton(window, level2Rect, "LEVEL 2", font, level2Rect.contains(Vector2f(mousePos)));

    FloatRect bestTimesRect({ 450.f, 390.f }, { 300.f, 50.f });
    DrawMenuButton(window, bestTimesRect, "BEST TIMES", font, bestTimesRect.contains(Vector2f(mousePos)));

    FloatRect backRect({ 450.f, 510.f }, { 300.f, 50.f });
    DrawMenuButton(window, backRect, "BACK", font, backRect.contains(Vector2f(mousePos)));
}

void DrawBestTimesMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    Text title(font, "BEST TIMES", 48);
    title.setFillColor(Color::White);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 80.f });
    window.draw(title);

    float yPos = 200.f;

    for (int lvl = 1; lvl <= 2; ++lvl) {
        string levelText = "LEVEL " + to_string(lvl);
        Text lvlTitle(font, levelText, 28);
        lvlTitle.setFillColor(Color::Yellow);
        FloatRect lvlBounds = lvlTitle.getLocalBounds();
        lvlTitle.setPosition({ (1200.f - lvlBounds.size.x) / 2.f, yPos });
        window.draw(lvlTitle);
        yPos += 40.f;

        if (bestRecords.find(lvl) != bestRecords.end() && bestRecords[lvl].completed) {
            const LevelRecord& rec = bestRecords[lvl];

            string timeStr = "Time: " + FormatTime(rec.time);
            Text timeText(font, timeStr, 20);
            timeText.setFillColor(Color::White);
            FloatRect timeBounds = timeText.getLocalBounds();
            timeText.setPosition({ (1200.f - timeBounds.size.x) / 2.f, yPos });
            window.draw(timeText);
            yPos += 30.f;

            string coinsStr = "Coins: " + to_string(rec.coins);
            Text coinsText(font, coinsStr, 20);
            coinsText.setFillColor(Color::White);
            FloatRect coinsBounds = coinsText.getLocalBounds();
            coinsText.setPosition({ (1200.f - coinsBounds.size.x) / 2.f, yPos });
            window.draw(coinsText);
            yPos += 30.f;

            string killsStr = "Kills: " + to_string(rec.kills);
            Text killsText(font, killsStr, 20);
            killsText.setFillColor(Color::White);
            FloatRect killsBounds = killsText.getLocalBounds();
            killsText.setPosition({ (1200.f - killsBounds.size.x) / 2.f, yPos });
            window.draw(killsText);
            yPos += 50.f;
        }
        else {
            Text noRecord(font, "No record yet", 20);
            noRecord.setFillColor(Color(150, 150, 150));
            FloatRect noBounds = noRecord.getLocalBounds();
            noRecord.setPosition({ (1200.f - noBounds.size.x) / 2.f, yPos });
            window.draw(noRecord);
            yPos += 60.f;
        }
    }

    FloatRect backRect({ 450.f, 650.f }, { 300.f, 50.f });
    DrawMenuButton(window, backRect, "BACK", font, backRect.contains(Vector2f(mousePos)));
}

void DrawCreatorsMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    Text title(font, "CREATORS", 48);
    title.setFillColor(Color::White);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 120.f });
    window.draw(title);

    vector<string> creators = {
        "Petrovskiy Mikhailo (Dram)",
        "Yashchenko Denis (HoWL)",
        "Kulik Svyatoslav (ezx)",
    };

    float y = 300.f;
    for (const auto& creator : creators) {
        Text txt(font, creator, 24);
        txt.setFillColor(Color::White);
        FloatRect bounds = txt.getLocalBounds();
        txt.setPosition({ (1200.f - bounds.size.x) / 2.f, y });
        window.draw(txt);
        y += 50.f;
    }

    FloatRect backRect({ 450.f, 510.f }, { 300.f, 50.f });
    DrawMenuButton(window, backRect, "BACK", font, backRect.contains(Vector2f(mousePos)));
}

void DrawSettingsMenu(RenderWindow& window, Font& font, Vector2i mousePos, Keyboard::Key attackKey, bool waitingForRemap, bool limitedDash) {
    window.setView(window.getDefaultView());

    Text title(font, "SETTINGS", 48);
    title.setFillColor(Color::White);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 120.f });
    window.draw(title);

    string keyText = "KEY OF ATTACK: " + keyToString(attackKey);
    FloatRect remapRect({ 450.f - 90.f, 250.f }, { 480.f, 50.f });
    DrawMenuButton(window, remapRect, keyText, font, remapRect.contains(Vector2f(mousePos)));

    string dashText = "DASH MODE: " + string(limitedDash ? "LIMITED" : "UNLIMITED");
    FloatRect dashRect({ 450.f - 90.f , 320.f }, { 480.f, 50.f });
    DrawMenuButton(window, dashRect, dashText, font, dashRect.contains(Vector2f(mousePos)));

    FloatRect backRect({ 450.f, 510.f }, { 300.f, 50.f });
    DrawMenuButton(window, backRect, "BACK", font, backRect.contains(Vector2f(mousePos)));

    if (waitingForRemap) {
        RectangleShape overlay({ 600.f, 200.f });
        overlay.setPosition({ 300.f, 300.f });
        overlay.setFillColor(Color(0, 0, 0, 200));
        window.draw(overlay);

        Text prompt(font, "ENTER ANY KEY...", 24);
        prompt.setFillColor(Color::White);
        FloatRect bounds = prompt.getLocalBounds();
        prompt.setPosition({
            300.f + (600.f - bounds.size.x) / 2.f,
            300.f + (200.f - bounds.size.y) / 2.f
            });
        window.draw(prompt);
    }
}

void DrawHUD(RenderWindow& window, Font& font, const Player& player,
    float gameTime, int levelNum, Texture& tx_HPBar, bool hasHPBar,
    Texture& tx_StaminaBar, bool hasStaminaBar)
{
    View gameView = window.getView();
    window.setView(window.getDefaultView());

    Text timeText(font, FormatTime(gameTime), 28);
    FloatRect timeBounds = timeText.getLocalBounds();
    timeText.setPosition({ (1200.f - timeBounds.size.x) / 2.f, 10.f });
    timeText.setFillColor(Color::White);
    timeText.setOutlineColor(Color::Black);
    timeText.setOutlineThickness(2.f);
    window.draw(timeText);

    Text levelText(font, "Level: " + to_string(levelNum), 20);
    levelText.setFillColor(Color::White);
    levelText.setOutlineColor(Color::Black);
    levelText.setOutlineThickness(1.f);
    levelText.setPosition({ 20.f, 10.f });
    window.draw(levelText);
    const float BAR_W = 380.f;
    const float BAR_H = BAR_W * (200.f / 1000.f);   // 76px
    const float BAR_X = 10.f;
    const float STA_BAR_Y = 800.f - BAR_H - 6.f;    // stamina bar — near bottom
    const float BAR_Y = STA_BAR_Y - BAR_H - 4.f;    // HP bar — directly above stamina

    float scaleX = BAR_W / 1000.f;
    float scaleY = BAR_H / 200.f;

    const float IMG_FILL_X = 150.f;
    const float IMG_FILL_W = 700.f;
    const float IMG_FILL_Y = 63.f;
    const float IMG_FILL_H = 74.f;

    float fillX = BAR_X + IMG_FILL_X * scaleX;
    float fillY = BAR_Y + IMG_FILL_Y * scaleY;
    float fillW = IMG_FILL_W * scaleX;
    float fillH = IMG_FILL_H * scaleY;

    float hpRatio = static_cast<float>(player.getHP()) / 100.f;
    hpRatio = std::max(0.f, std::min(1.f, hpRatio));

    RectangleShape emptyFill({ fillW, fillH });
    emptyFill.setPosition({ fillX, fillY });
    emptyFill.setFillColor(Color(15, 0, 0, 255));
    window.draw(emptyFill);

    const float PAD = 2.f;
    float barW = (fillW - PAD * 2) * hpRatio;

    {
        float emptyStartX = fillX + PAD + barW;
        float emptyPartW = (fillW - PAD * 2) * (1.f - hpRatio);
        if (emptyPartW > 0.f) {
            Color maroon = (hpRatio > 0.3f) ? Color(50, 0, 8, 255) : Color(80, 0, 12, 255);
            RectangleShape ep({ emptyPartW, fillH - PAD * 2 });
            ep.setPosition({ emptyStartX, fillY + PAD });
            ep.setFillColor(maroon);
            window.draw(ep);
        }
    }

    if (hpRatio > 0.f) {
        Color fillColor;
        if (hpRatio > 0.6f) {
            fillColor = Color(180, 20, 20, 255);
        }
        else if (hpRatio > 0.3f) {
            float t = (hpRatio - 0.3f) / 0.3f;
            fillColor = Color((uint8_t)(180 + (1.f - t) * 30), (uint8_t)(20 + (1.f - t) * 60), 10, 255);
        }
        else {
            float t = hpRatio / 0.3f;
            fillColor = Color((uint8_t)(90 + t * 120), (uint8_t)(t * 80), 10, 255);
        }
        RectangleShape fill({ barW, fillH - PAD * 2 });
        fill.setPosition({ fillX + PAD, fillY + PAD });
        fill.setFillColor(fillColor);
        window.draw(fill);

        RectangleShape shine({ barW, (fillH - PAD * 2) * 0.25f });
        shine.setPosition({ fillX + PAD, fillY + PAD });
        shine.setFillColor(Color(255, 100, 100, 60));
        window.draw(shine);
    }

    if (hasHPBar) {
        Sprite hpSprite(tx_HPBar);
        hpSprite.setPosition({ BAR_X, BAR_Y });
        hpSprite.setScale({ scaleX, scaleY });
        window.draw(hpSprite);
    }

    //Text hpText(font, to_string(player.getHP()) + " / 100", 14);
    //FloatRect hb = hpText.getLocalBounds();
    //hpText.setPosition({ fillX + (fillW - hb.size.x) / 2.f, fillY + (fillH - hb.size.y) / 2.f - 2.f });
    //hpText.setFillColor(Color::White);
    //hpText.setOutlineColor(Color::Black);
    //hpText.setOutlineThickness(1.f);
    //window.draw(hpText);

    // --- Stamina bar — same size as HP bar, directly below it ---
    // mama 2.png inner fill: x=98..967 (w=869), y=47..149 (h=102)
    const float MAMA_INNER_X = 98.f, MAMA_INNER_W = 869.f;
    const float MAMA_INNER_Y = 47.f, MAMA_INNER_H = 102.f;

    // mama 2.png drawn at SAME size as hp_bar (BAR_W x BAR_H, same scaleX/scaleY)
    // Fill area inside mama2 at same relative position as hp_bar fill
    float staFillX = BAR_X + IMG_FILL_X * scaleX;
    float staFillY = STA_BAR_Y + IMG_FILL_Y * scaleY;

    float staRatio = player.getStamina() / player.getMaxStamina();
    staRatio = std::max(0.f, std::min(1.f, staRatio));

    // 1) Dark background inside frame
    RectangleShape staBg({ fillW, fillH });
    staBg.setPosition({ staFillX, staFillY });
    staBg.setFillColor(Color(0, 20, 0, 255));
    window.draw(staBg);

    // 2) Green fill
    if (staRatio > 0.f) {
        Color staColor = (staRatio > 0.5f) ? Color(40, 210, 60) :
            (staRatio > 0.25f) ? Color(150, 210, 30) :
            Color(210, 170, 15);
        RectangleShape staFill({ (fillW - PAD * 2) * staRatio, fillH - PAD * 2 });
        staFill.setPosition({ staFillX + PAD, staFillY + PAD });
        staFill.setFillColor(staColor);
        window.draw(staFill);

        RectangleShape staShine({ (fillW - PAD * 2) * staRatio, (fillH - PAD * 2) * 0.25f });
        staShine.setPosition({ staFillX + PAD, staFillY + PAD });
        staShine.setFillColor(Color(180, 255, 180, 60));
        window.draw(staShine);
    }

    // 3) mama 2.png — same size as hp_bar (BAR_W x BAR_H, scaleX x scaleY)
    if (hasStaminaBar) {
        Sprite staSprite(tx_StaminaBar);
        staSprite.setPosition({ BAR_X, STA_BAR_Y });
        staSprite.setScale({ scaleX, scaleY });
        window.draw(staSprite);
    }

    Text coinsText(font, "Coins: " + to_string(player.getCoins()), 20);
    FloatRect coinBounds = coinsText.getLocalBounds();
    coinsText.setPosition({ 1180.f - coinBounds.size.x, 800.f - 35.f });
    coinsText.setFillColor(Color::Yellow);
    coinsText.setOutlineColor(Color::Black);
    coinsText.setOutlineThickness(1.f);
    window.draw(coinsText);

    window.setView(gameView);
}

string FormatTime(float seconds) {
    int totalSecs = static_cast<int>(seconds);
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    stringstream ss;
    ss << setfill('0') << setw(2) << mins << ":"
        << setfill('0') << setw(2) << secs;
    return ss.str();
}

void drawGameOver(RenderWindow& window, Font& font) {
    View menuView = window.getDefaultView();
    window.setView(menuView);

    RectangleShape overlay({ 1200.f, 800.f });
    overlay.setFillColor(Color(0, 0, 0, 200));
    window.draw(overlay);

    Text gameOverText(font, "GAME OVER", 75);
    gameOverText.setFillColor(Color::Red);
    gameOverText.setOutlineColor(Color::White);
    gameOverText.setOutlineThickness(3.f);
    gameOverText.setPosition({ 350.f - 100, 250.f });
    window.draw(gameOverText);

    Text restartText(font, "Press ENTER to restart", 27);
    restartText.setFillColor(Color::White);
    restartText.setPosition({ 380.f, 400.f });
    window.draw(restartText);

    Text menuText(font, "Press ESC for menu", 23);
    menuText.setFillColor(Color(200, 200, 200));
    menuText.setPosition({ 420.f, 470.f });
    window.draw(menuText);
}

void DrawPauseMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    RectangleShape overlay({ 1200.f, 800.f });
    overlay.setFillColor(Color(0, 0, 0, 160));
    window.draw(overlay);

    Text title(font, "PAUSED", 72);
    title.setFillColor(Color::White);
    FloatRect tb = title.getLocalBounds();
    title.setPosition({ (1200.f - tb.size.x) / 2.f, 200.f });
    window.draw(title);

    FloatRect resumeRect({ 450.f, 350.f }, { 300.f, 50.f });
    DrawMenuButton(window, resumeRect, "RESUME", font, resumeRect.contains(Vector2f(mousePos)));

    FloatRect menuRect({ 450.f, 420.f }, { 300.f, 50.f });
    DrawMenuButton(window, menuRect, "MAIN MENU", font, menuRect.contains(Vector2f(mousePos)));

    Text hint(font, "F3 = debug mode", 18);
    hint.setFillColor(Color(150, 150, 150));
    FloatRect hb = hint.getLocalBounds();
    hint.setPosition({ (1200.f - hb.size.x) / 2.f, 510.f });
    window.draw(hint);
}

void drawEndInfo(RenderWindow& window, Font& font, float finalTime, int coins, int kills, bool completed, int levelNum) {
    View menuView = window.getDefaultView();
    window.setView(menuView);

    RectangleShape overlay({ 1200.f, 800.f });
    overlay.setFillColor(Color(0, 0, 0, 200));
    window.draw(overlay);

    string titleStr = completed ? "LEVEL " + to_string(levelNum) + " COMPLETE!" : "GAME OVER";
    Text title(font, titleStr, 59);
    title.setFillColor(completed ? Color(0, 200, 100) : Color::Red);
    title.setOutlineColor(Color::White);
    title.setOutlineThickness(3.f);
    FloatRect tBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - tBounds.size.x) / 2.f, 140.f });
    window.draw(title);

    Text timeLabel(font, string("Time: ") + FormatTime(finalTime), 28);
    timeLabel.setFillColor(Color::White);
    timeLabel.setPosition({ 420.f, 280.f });
    window.draw(timeLabel);

    Text coinsLabel(font, string("Coins: ") + to_string(coins), 28);
    coinsLabel.setFillColor(Color::Yellow);
    coinsLabel.setPosition({ 420.f, 330.f });
    window.draw(coinsLabel);

    Text killsLabel(font, string("Enemies killed: ") + to_string(kills), 28);
    killsLabel.setFillColor(Color::White);
    killsLabel.setPosition({ 420.f, 380.f });
    window.draw(killsLabel);

    if (completed) {
        bool isNewRecord = false;
        if (bestRecords.find(levelNum) == bestRecords.end() ||
            finalTime < bestRecords[levelNum].time) {
            isNewRecord = true;
        }

        if (isNewRecord) {
            Text newRecordText(font, "NEW BEST TIME!", 24);
            newRecordText.setFillColor(Color(255, 215, 0));
            FloatRect nrBounds = newRecordText.getLocalBounds();
            newRecordText.setPosition({ (1200.f - nrBounds.size.x) / 2.f, 430.f });
            window.draw(newRecordText);
        }
    }

    Text restartText(font, "Press ENTER to restart", 24);
    restartText.setFillColor(Color::White);
    restartText.setPosition({ 430.f, 500.f });
    window.draw(restartText);

    Text menuText(font, "Press ESC for menu", 20);
    menuText.setFillColor(Color(200, 200, 200));
    menuText.setPosition({ 470.f, 540.f });
    window.draw(menuText);
}

void initializeLevel(int levelNum, std::vector<std::tuple<int, int, int>>& mobTemplate,
    std::vector<Enemy>& enemies, Texture& tx_Slime, Texture& tx_SlimeMan,
    static int OriginalInterestingMAP[MAP_HEIGHT][MAP_WIDTH + 1], Player& player) {
    loadLevel(levelNum, mobTemplate);
    analyzeMaps();

    std::memcpy(OriginalInterestingMAP, InterestingMAP, sizeof(InterestingMAP));

    bool foundSpawn = false;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            if (MAP[y][x] == 12 || InterestingMAP[y][x] == 12 || BackgroundMAP[y][x] == 12) {
                float spawnX = x * TileSize;
                float spawnY = y * TileSize;
                player.setSpawnPoint(spawnX, spawnY);
                foundSpawn = true;
                std::cout << "Spawn point set at: " << spawnX << ", " << spawnY << std::endl;
                break;
            }
        }
        if (foundSpawn) break;
    }

    if (!foundSpawn) {
        std::cout << "Warning: Orange portal not found, using default spawn" << std::endl;
        player.setSpawnPoint(100.f, 100.f);
    }

    enemies.clear();
    for (auto& t : mobTemplate) {
        int x, y, tile;
        std::tie(x, y, tile) = t;
        if (tile == 8) enemies.emplace_back(x * TileSize, y * TileSize, tx_Slime, TileSize);
        else if (tile == 9) enemies.emplace_back(x * TileSize, y * TileSize, tx_SlimeMan, TileSize);
    }
    if (enemies.empty()) enemies.emplace_back(5.f * TileSize, 8.f * TileSize, tx_Slime, TileSize);
}

std::string FitTextToWidth(const Font& font, const std::string& str, unsigned charSize, float maxWidth) {
    if (str.empty()) return str;
    Text probe(font, str, charSize);
    if (probe.getLocalBounds().size.x <= maxWidth) return str;
    for (int i = 1; i < (int)str.size(); ++i) {
        std::string sub = str.substr(i);
        Text t(font, sub, charSize);
        if (t.getLocalBounds().size.x <= maxWidth)
            return sub;
    }
    return str.substr(str.size() - 1);
}

void DrawInputBox(RenderWindow& window, const Font& font,
    float bx, float by, float bw, float bh,
    const std::string& value, const std::string& placeholder,
    bool active, bool masked = false)
{
    const float px = 3.f;
    Color cBase = active ? Color(100, 90, 68) : Color(75, 67, 50);
    Color cLight = active ? Color(150, 138, 108) : Color(110, 100, 76);
    Color cDark = Color(45, 40, 28);
    Color cCorner = Color(55, 48, 34);

    RectangleShape shadow({ bw, bh });
    shadow.setPosition({ bx + px, by + px });
    shadow.setFillColor(Color(0, 0, 0, 120));
    window.draw(shadow);

    RectangleShape base({ bw, bh });
    base.setPosition({ bx, by });
    base.setFillColor(cBase);
    window.draw(base);

    RectangleShape etop({ bw - px * 2, px });
    etop.setPosition({ bx + px, by }); etop.setFillColor(cDark); window.draw(etop);
    RectangleShape eleft({ px, bh - px * 2 });
    eleft.setPosition({ bx, by + px }); eleft.setFillColor(cDark); window.draw(eleft);
    RectangleShape ebot({ bw - px * 2, px });
    ebot.setPosition({ bx + px, by + bh - px }); ebot.setFillColor(cLight); window.draw(ebot);
    RectangleShape eright({ px, bh - px * 2 });
    eright.setPosition({ bx + bw - px, by + px }); eright.setFillColor(cLight); window.draw(eright);

    auto drawPx = [&](float cx, float cy) {
        RectangleShape p({ px, px }); p.setPosition({ cx, cy });
        p.setFillColor(cCorner); window.draw(p);
        };
    drawPx(bx, by); drawPx(bx + bw - px, by);
    drawPx(bx, by + bh - px); drawPx(bx + bw - px, by + bh - px);

    const float padding = 10.f;
    const float maxW = bw - padding * 2;
    bool isEmpty = value.empty();
    std::string display;
    if (isEmpty) {
        display = placeholder;
    }
    else if (masked) {
        display = std::string(value.size(), '*');
        display = FitTextToWidth(font, display, 20, maxW);
    }
    else {
        display = FitTextToWidth(font, value, 20, maxW);
    }

    Text txt(font, display, 20);
    txt.setFillColor(isEmpty ? Color(150, 138, 108) : Color(240, 228, 190));
    if (!isEmpty) { txt.setOutlineColor(Color(40, 34, 20, 180)); txt.setOutlineThickness(1.f); }
    FloatRect tb = txt.getLocalBounds();
    txt.setPosition({ bx + padding, by + (bh - tb.size.y) / 2.f + 5.f });
    window.draw(txt);

    if (active) {
        float cursorX = bx + padding + (isEmpty ? 0.f : txt.getLocalBounds().size.x + 2.f);
        RectangleShape cursor({ 2.f, bh - px * 4 });
        cursor.setPosition({ cursorX, by + px * 2 });
        cursor.setFillColor(Color(240, 228, 190, 200));
        window.draw(cursor);
    }
}

void DrawLoginMenu(RenderWindow& window, Font& font, Vector2i mousePos,
    std::string& email, std::string& password,
    bool emailActive, bool passwordActive, const std::string& error)
{
    window.setView(window.getDefaultView());

    Text title(font, "LOGIN", 52);
    title.setFillColor(Color(230, 220, 185));
    title.setOutlineColor(Color(50, 42, 28, 220));
    title.setOutlineThickness(3.f);
    FloatRect tb = title.getLocalBounds();
    title.setPosition({ (1200.f - tb.size.x) / 2.f, 95.f });
    window.draw(title);

    const float boxW = 400.f, boxH = 46.f, boxX = 400.f;
    DrawInputBox(window, font, boxX, 210.f, boxW, boxH, email, "Email...", emailActive, false);
    DrawInputBox(window, font, boxX, 276.f, boxW, boxH, password, "Password...", passwordActive, true);

    FloatRect loginBtn({ 400.f, 350.f }, { 190.f, 48.f });
    DrawMenuButton(window, loginBtn, "LOGIN", font, loginBtn.contains(Vector2f(mousePos)));
    FloatRect regBtn({ 610.f, 350.f }, { 190.f, 48.f });
    DrawMenuButton(window, regBtn, "REGISTER", font, regBtn.contains(Vector2f(mousePos)));
    FloatRect backBtn({ 450.f, 490.f }, { 300.f, 48.f });
    DrawMenuButton(window, backBtn, "BACK", font, backBtn.contains(Vector2f(mousePos)));

    if (!error.empty()) {
        Text errTxt(font, error, 18);
        errTxt.setFillColor(Color(255, 80, 80));
        errTxt.setOutlineColor(Color(60, 0, 0, 180));
        errTxt.setOutlineThickness(1.f);
        FloatRect eb = errTxt.getLocalBounds();
        errTxt.setPosition({ (1200.f - eb.size.x) / 2.f, 430.f });
        window.draw(errTxt);
    }
}

void DrawQuestsMenu(RenderWindow& window, Font& font, Vector2i mousePos,
    std::vector<ApiQuest>& quests)
{
    window.setView(window.getDefaultView());
    Text title(font, "QUESTS", 48);
    title.setFillColor(Color::White);
    FloatRect tb = title.getLocalBounds();
    title.setPosition({ (1200.f - tb.size.x) / 2.f, 60.f });
    window.draw(title);

    float y = 150.f;
    for (auto& q : quests) {
        RectangleShape bg({ 800.f, 60.f });
        bg.setPosition({ 200.f, y });
        bg.setFillColor(q.completed ? Color(0, 60, 0, 180) : Color(40, 40, 60, 180));
        bg.setOutlineColor(Color(100, 100, 100)); bg.setOutlineThickness(1.f);
        window.draw(bg);

        Text name(font, q.title, 18);
        name.setFillColor(Color::White);
        name.setPosition({ 210.f, y + 5.f });
        window.draw(name);

        std::string prog = std::to_string(std::min(q.currentValue, q.targetValue))
            + "/" + std::to_string(q.targetValue);
        Text progTxt(font, prog, 16);
        progTxt.setFillColor(Color::Yellow);
        progTxt.setPosition({ 210.f, y + 30.f });
        window.draw(progTxt);

        Text rew(font, "+" + std::to_string(q.reward) + " coins", 16);
        rew.setFillColor(Color::Yellow);
        rew.setPosition({ 700.f, y + 20.f });
        window.draw(rew);

        if (q.completed && !q.claimed) {
            FloatRect claimBtn({ 870.f, y + 10.f }, { 100.f, 38.f });
            DrawMenuButton(window, claimBtn, "CLAIM", font, claimBtn.contains(Vector2f(mousePos)));
        }

        y += 70.f;
        if (y > 680.f) break;
    }

    FloatRect backBtn({ 450.f, 720.f }, { 300.f, 50.f });
    DrawMenuButton(window, backBtn, "BACK", font, backBtn.contains(Vector2f(mousePos)));
}

void DrawLeaderboard(RenderWindow& window, Font& font, Vector2i mousePos,
    std::vector<ApiLeaderboardEntry>& entries, int levelNum)
{
    window.setView(window.getDefaultView());
    Text title(font, "LEADERBOARD - LEVEL " + std::to_string(levelNum), 36);
    title.setFillColor(Color::White);
    FloatRect tb = title.getLocalBounds();
    title.setPosition({ (1200.f - tb.size.x) / 2.f, 60.f });
    window.draw(title);

    float y = 140.f;
    for (auto& e : entries) {
        std::string line = "#" + std::to_string(e.rank) + "  " + e.username
            + "   " + FormatTime(e.time)
            + "   kills:" + std::to_string(e.kills);
        Text row(font, line, 20);
        row.setFillColor(e.rank == 1 ? Color::Yellow : Color::White);
        FloatRect rb = row.getLocalBounds();
        row.setPosition({ (1200.f - rb.size.x) / 2.f, y });
        window.draw(row);
        y += 40.f;
    }

    FloatRect backBtn({ 450.f, 680.f }, { 300.f, 50.f });
    DrawMenuButton(window, backBtn, "BACK", font, backBtn.contains(Vector2f(mousePos)));
}

void DrawShopMenu(RenderWindow& window, Font& font, Vector2i mousePos,
    std::vector<ApiSkin>& skins, bool shopLoaded)
{
    window.setView(window.getDefaultView());

    Text title(font, "SHOP", 48);
    title.setFillColor(Color::White);
    FloatRect tb = title.getLocalBounds();
    title.setPosition({ (1200.f - tb.size.x) / 2.f, 30.f });
    window.draw(title);

    if (!shopLoaded) {
        Text ld(font, "Loading...", 28);
        ld.setFillColor(Color::White);
        FloatRect lb = ld.getLocalBounds();
        ld.setPosition({ (1200.f - lb.size.x) / 2.f, 380.f });
        window.draw(ld);
        FloatRect backBtn({ 450.f, 730.f }, { 300.f, 50.f });
        DrawMenuButton(window, backBtn, "BACK", font, backBtn.contains(Vector2f(mousePos)));
        return;
    }

    const std::vector<std::string> types = { "player", "bar", "slash" };
    const std::vector<std::string> typeLabels = { "CHARACTER", "HP BAR", "SLASH" };
    float colX[3] = { 40.f, 440.f, 840.f };
    float colW = 340.f;

    for (int t = 0; t < 3; ++t) {
        Text colTitle(font, typeLabels[t], 22);
        colTitle.setFillColor(Color::Yellow);
        colTitle.setPosition({ colX[t], 90.f });
        window.draw(colTitle);

        float y = 130.f;
        for (auto& s : skins) {
            if (s.type != types[t]) continue;

            RectangleShape card({ colW, 70.f });
            card.setPosition({ colX[t], y });
            sf::Color cardColor = s.equipped ? Color(0, 80, 0, 200)
                : s.owned ? Color(0, 40, 80, 200)
                : Color(40, 40, 60, 200);
            card.setFillColor(cardColor);
            card.setOutlineColor(s.equipped ? Color::Green : Color(100, 100, 100));
            card.setOutlineThickness(s.equipped ? 2.f : 1.f);
            window.draw(card);

            Text name(font, s.name, 18);
            name.setFillColor(Color::White);
            name.setPosition({ colX[t] + 8.f, y + 6.f });
            window.draw(name);

            if (s.equipped) {
                Text eq(font, "EQUIPPED", 14);
                eq.setFillColor(Color::Green);
                eq.setPosition({ colX[t] + 8.f, y + 32.f });
                window.draw(eq);
            }
            else if (s.owned) {
                FloatRect equipBtn({ colX[t] + colW - 100.f, y + 18.f }, { 88.f, 34.f });
                DrawMenuButton(window, equipBtn, "EQUIP", font, equipBtn.contains(Vector2f(mousePos)));
            }
            else {
                Text price(font, std::to_string(s.price) + " coins", 14);
                price.setFillColor(Color::Yellow);
                price.setPosition({ colX[t] + 8.f, y + 32.f });
                window.draw(price);
                FloatRect buyBtn({ colX[t] + colW - 100.f, y + 18.f }, { 88.f, 34.f });
                DrawMenuButton(window, buyBtn, "BUY", font, buyBtn.contains(Vector2f(mousePos)));
            }

            y += 78.f;
            if (y > 680.f) break;
        }
    }

    FloatRect backBtn({ 450.f, 730.f }, { 300.f, 50.f });
    DrawMenuButton(window, backBtn, "BACK", font, backBtn.contains(Vector2f(mousePos)));
}

int main()
{
    static int OriginalInterestingMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};

    std::vector<std::tuple<int, int, int>> mobTemplate;

    loadBestRecords();

    bool apiLoggedIn = false;
    std::string loginEmail = "", loginPassword = "", loginError = "";
    std::string regUsername = "", regEmail = "", regPassword = "", regError = "";
    bool loginEmailActive = false, loginPasswordActive = false;
    std::vector<ApiLeaderboardEntry> leaderboard;
    std::vector<ApiQuest> quests;
    bool questsLoaded = false;
    std::vector<ApiSkin> shopSkins;
    bool shopLoaded = false;
    std::string shopMessage = "";


    if (!loadLevel(1, mobTemplate)) {
        return -1;
    }

    analyzeMaps();

    std::memcpy(OriginalInterestingMAP, InterestingMAP, sizeof(InterestingMAP));

    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (BackgroundMAP[y][x] == 0 && MAP[y][x] == 0)
                BackgroundMAP[y][x] = 5;
        }

    Texture tx_Undefined, tx_Dirt, tx_Stone, tx_Grass, tx_Ladder;
    Texture tx_BlueSky, tx_DirtBG, tx_StoneBG, tx_Coin, tx_Slime;
    Texture tx_SlimeMan, tx_ClosedDoor, tx_OpenedDoor;
    Texture tx_GreenBricks, tx_GreenBricksBG, tx_GreenGrass;
    Texture tx_Lava, tx_LavaTop, tx_Spikes, tx_SpikesLeft;
    Texture tx_SpikesRight, tx_SpikesTop, tx_Key, tx_BluePortal;
    Texture tx_OrangePortal, tx_HPBar, tx_StaminaBar;

    tx_Undefined.loadFromFile("Sprites/Undefined.png");
    tx_Dirt.loadFromFile("Sprites/Dirt.png");
    tx_Stone.loadFromFile("Sprites/Stone.png");
    tx_Grass.loadFromFile("Sprites/Grass.png");
    tx_Ladder.loadFromFile("Sprites/Ladder.png");
    tx_BlueSky.loadFromFile("Sprites/Blue Sky.png");
    tx_DirtBG.loadFromFile("Sprites/DirtBG.png");
    tx_StoneBG.loadFromFile("Sprites/StoneBG.png");
    tx_Coin.loadFromFile("Sprites/Coin.png");
    tx_Slime.loadFromFile("Sprites/Slime.png");
    tx_SlimeMan.loadFromFile("Sprites/SlimeMan.png");
    tx_ClosedDoor.loadFromFile("Sprites/ClosedDoor.png");
    tx_OpenedDoor.loadFromFile("Sprites/OpenedDoor.png");
    tx_GreenBricks.loadFromFile("Sprites/GreenBricks.png");
    tx_GreenBricksBG.loadFromFile("Sprites/GreenBricksBG.png");
    tx_GreenGrass.loadFromFile("Sprites/GreenGrass.png");
    tx_Lava.loadFromFile("Sprites/Lava.png");
    tx_LavaTop.loadFromFile("Sprites/LavaTop.png");
    tx_Spikes.loadFromFile("Sprites/Spikes.png");
    tx_SpikesLeft.loadFromFile("Sprites/SpikesLeft.png");
    tx_SpikesRight.loadFromFile("Sprites/SpikesRight.png");
    tx_SpikesTop.loadFromFile("Sprites/SpikesTop.png");
    tx_Key.loadFromFile("Sprites/Key.png");
    tx_BluePortal.loadFromFile("Sprites/BluePortal.png");
    tx_OrangePortal.loadFromFile("Sprites/OrangePortal.png");

    map<int, Sprite> spriteSheet = {
        {0, Sprite(tx_Undefined)},
        {1, Sprite(tx_Dirt)},
        {2, Sprite(tx_Stone)},
        {3, Sprite(tx_Grass)},
        {4, Sprite(tx_Ladder)},
        {5, Sprite(tx_BlueSky)},
        {6, Sprite(tx_GreenBricksBG)},
        {7, Sprite(tx_Coin)},
        {8, Sprite(tx_Slime)},
        {9, Sprite(tx_SlimeMan)},
        {10, Sprite(tx_DirtBG)},
        {11, Sprite(tx_StoneBG)},
        {12, Sprite(tx_OrangePortal)},
        {13, Sprite(tx_BluePortal)},
        {14, Sprite(tx_GreenBricks)},
        {15, Sprite(tx_GreenGrass)},
        {16, Sprite(tx_Lava)},
        {17, Sprite(tx_LavaTop)},
        {18, Sprite(tx_Spikes)},
        {19, Sprite(tx_SpikesLeft)},
        {20, Sprite(tx_SpikesRight)},
        {21, Sprite(tx_SpikesTop)},
        {22, Sprite(tx_Key)},
    };

    bool hasHPBar = tx_HPBar.loadFromFile("Sprites/hp_bar.png");
    bool hasStaminaBar = tx_StaminaBar.loadFromFile("Sprites/mama 2.png");

    std::vector<Enemy> enemies;
    for (auto& t : mobTemplate) {
        int x, y, tile;
        std::tie(x, y, tile) = t;
        if (tile == 8) enemies.emplace_back(x * TileSize, y * TileSize, tx_Slime, TileSize);
        else if (tile == 9) enemies.emplace_back(x * TileSize, y * TileSize, tx_SlimeMan, TileSize);
    }
    if (enemies.empty()) enemies.emplace_back(5.f * TileSize, 8.f * TileSize, tx_Slime, TileSize);

    Font font;
    if (!font.openFromFile("Fonts/DigitalPixelV100-Regular.ttf")) {
        cout << "Font not found!" << endl;
        return -1;
    }

    RenderWindow window(VideoMode({ 1200, 800 }), "PIXELRUN Game");
    Texture tx_Player("Sprites/AnimationSheet_Character.png");
    Player player(tx_Player, 100.f, 100.f);
    player.loadAnimationSheets(
        "Sprites/player_walk.png",
        "Sprites/player_attack.png",
        "Sprites/player_idle.png",
        "Sprites/player_fall.png"
    );
    Clock clock;
    GameState gameState = MAIN_MENU;

    bool waitingForRemap = false;
    Texture menuBgTexture;
    bool hasMenuBg = menuBgTexture.loadFromFile("Sprites/MENU1.png");
    float menuBgOffset = 0.f;
    const float MENU_SCROLL_SPEED = 50.f;

    float time = 0.f;
    bool enteringDoor = false;
    float enterTimer = 0.f;
    const float ENTER_DURATION = 0.9f;
    Vector2f enterViewStartCenter;
    Vector2f enterViewTargetCenter;
    Vector2f enterViewStartSize;
    bool levelCompleted = false;
    float finalTime = 0.f;
    int finalCoins = 0;
    int finalKills = 0;
    Vector2f StartdoorPosition(3600.f, 0.f);
    Vector2f EnddoorPosition(3651.f, 0.f);

    while (window.isOpen()) {
        while (const optional event = window.pollEvent())
        {
            if (event->is<Event::Closed>())
                window.close();

            if (event->is<Event::KeyPressed>()) {
                auto keyEvent = event->getIf<Event::KeyPressed>();

                if (keyEvent->code == Keyboard::Key::Escape) {
                    if (gameState == PLAYING) {
                        //gameState = MAIN_MENU;
                        //player.reset();
                        gameState = PAUSED;
                    }
                    else if (gameState == PAUSED) {
                        gameState = PLAYING;
                    }
                    else if (gameState == GAME_OVER) {
                        levelCompleted = false;
                        finalTime = 0.f;
                        finalCoins = 0;
                        finalKills = 0;
                        gameState = MAIN_MENU;
                        player.reset();
                    }
                    else if (gameState == BEST_TIMES_MENU) {
                        gameState = LEVELS_MENU;
                    }
                    else {
                        window.close();
                    }
                }
                if (keyEvent->code == Keyboard::Key::F3)
                    debugMode = !debugMode;
                if (keyEvent->code == Keyboard::Key::Enter) {
                    if (gameState == GAME_OVER) {
                        gameState = PLAYING;
                        initializeLevel(currentLevel, mobTemplate, enemies, tx_Slime, tx_SlimeMan, OriginalInterestingMAP, player);
                        player.reset();
                        player.setLimitedDashMode(limitedDashMode);
                        time = 0.f;
                        levelCompleted = false;
                        finalTime = 0.f;
                        finalCoins = 0;
                        finalKills = 0;
                    }
                }

                if (waitingForRemap) {
                    player.setAttackKey(keyEvent->code);
                    waitingForRemap = false;
                }
            }

            if (event->is<Event::MouseButtonPressed>()) {
                auto mp = event->getIf<Event::MouseButtonPressed>()->position;
                Vector2f mouse = Vector2f(window.mapPixelToCoords(mp, window.getDefaultView()));

                if (gameState == MAIN_MENU) {
                    const float btnW = 240.f, btnH = 52.f, gap = 14.f;
                    const float col1X = 270.f, col2X = 690.f, startY = 250.f;

                    FloatRect levels({ col1X, startY }, { btnW, btnH });
                    FloatRect shop({ col1X, startY + (btnH + gap) }, { btnW, btnH });
                    FloatRect loginBtn({ col1X, startY + (btnH + gap) * 2 }, { btnW, btnH });
                    FloatRect creators({ col2X, startY }, { btnW, btnH });
                    FloatRect settings({ col2X, startY + (btnH + gap) }, { btnW, btnH });
                    FloatRect questsBtn({ col2X, startY + (btnH + gap) * 2 }, { btnW, btnH });
                    FloatRect exitBtn({ (1200.f - 200.f) / 2.f, startY + (btnH + gap) * 3 + 10.f }, { 200.f, 44.f });

                    if (levels.contains(mouse))    gameState = LEVELS_MENU;
                    else if (creators.contains(mouse))  gameState = CREATORS_MENU;
                    else if (settings.contains(mouse))  gameState = SETTINGS_MENU;
                    else if (exitBtn.contains(mouse))   window.close();
                    else if (loginBtn.contains(mouse)) {
                        loginEmail = ""; loginPassword = ""; loginError = "";
                        loginEmailActive = false; loginPasswordActive = false;
                        ApiClient::instance().status = ApiStatus::Idle;
                        gameState = LOGIN_MENU;
                    }
                    else if (shop.contains(mouse)) {
                        shopSkins.clear(); shopLoaded = false; shopMessage = "";
                        gameState = SHOP_MENU;
                        ApiClient::instance().getShopAsync([&shopSkins, &shopLoaded](std::vector<ApiSkin> s) {
                            shopSkins = s;
                            ApiClient::instance().cachedSkins = s;
                            shopLoaded = true;
                            });
                    }
                    else if (questsBtn.contains(mouse)) {
                        gameState = QUESTS_MENU;
                        if (ApiClient::instance().player.loggedIn) {
                            quests.clear(); questsLoaded = false;
                            ApiClient::instance().getQuestsAsync([&quests, &questsLoaded](std::vector<ApiQuest> q) {
                                quests = q; questsLoaded = true;
                                });
                        }
                    }
                }
                else if (gameState == LEVELS_MENU) {
                    FloatRect level1({ 450.f, 250.f }, { 300.f, 50.f });
                    FloatRect level2({ 450.f, 320.f }, { 300.f, 50.f });
                    FloatRect bestTimes({ 450.f, 390.f }, { 300.f, 50.f });
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });

                    if (level1.contains(mouse)) {
                        currentLevel = 1;
                        gameState = PLAYING;
                        initializeLevel(currentLevel, mobTemplate, enemies, tx_Slime, tx_SlimeMan, OriginalInterestingMAP, player);
                        player.reset();
                        player.setLimitedDashMode(limitedDashMode);
                        time = 0.f;
                        levelCompleted = false;
                        finalTime = 0.f;
                        finalCoins = 0;
                        finalKills = 0;
                        StartdoorPosition = Vector2f(3600.f, 0.f);
                        EnddoorPosition = Vector2f(3651.f, 0.f);
                    }
                    else if (level2.contains(mouse)) {
                        currentLevel = 2;
                        gameState = PLAYING;
                        initializeLevel(currentLevel, mobTemplate, enemies, tx_Slime, tx_SlimeMan, OriginalInterestingMAP, player);
                        player.reset();
                        player.setLimitedDashMode(limitedDashMode);
                        time = 0.f;
                        levelCompleted = false;
                        finalTime = 0.f;
                        finalCoins = 0;
                        finalKills = 0;
                        StartdoorPosition = Vector2f(4400.f, 0.f);
                        EnddoorPosition = Vector2f(4444.f, 0.f);
                    }
                    else if (bestTimes.contains(mouse)) {
                        gameState = BEST_TIMES_MENU;
                    }
                    else if (back.contains(mouse)) gameState = MAIN_MENU;
                }
                else if (gameState == BEST_TIMES_MENU) {
                    FloatRect back({ 450.f, 650.f }, { 300.f, 50.f });
                    if (back.contains(mouse)) gameState = LEVELS_MENU;
                }
                else if (gameState == CREATORS_MENU) {
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });
                    if (back.contains(mouse)) gameState = MAIN_MENU;
                }
                else if (gameState == SETTINGS_MENU) {
                    FloatRect remap({ 450.f - 90.f, 250.f }, { 480.f, 50.f });
                    FloatRect dashToggle({ 450.f - 90.f , 320.f }, { 480.f, 50.f });
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });

                    if (remap.contains(mouse)) {
                        waitingForRemap = true;
                    }
                    else if (dashToggle.contains(mouse)) {
                        limitedDashMode = !limitedDashMode;
                    }
                    else if (back.contains(mouse)) {
                        gameState = MAIN_MENU;
                    }
                }
                else if (gameState == PAUSED) {
                    FloatRect resume({ 450.f, 350.f }, { 300.f, 50.f });
                    FloatRect toMenu({ 450.f, 420.f }, { 300.f, 50.f });
                    if (resume.contains(mouse))
                        gameState = PLAYING;
                    else if (toMenu.contains(mouse)) {
                        gameState = MAIN_MENU;
                        player.reset();
                    }
                }
                else if (gameState == LOGIN_MENU) {
                    FloatRect emailBox({ 400.f, 220.f }, { 400.f, 45.f });
                    FloatRect passBox({ 400.f, 290.f }, { 400.f, 45.f });
                    FloatRect loginBtn({ 400.f, 360.f }, { 190.f, 45.f });
                    FloatRect regBtn({ 610.f, 360.f }, { 190.f, 45.f });
                    FloatRect backBtn({ 450.f, 500.f }, { 300.f, 50.f });

                    if (emailBox.contains(mouse)) {
                        loginEmailActive = true; loginPasswordActive = false;
                    }
                    else if (passBox.contains(mouse)) {
                        loginEmailActive = false; loginPasswordActive = true;
                    }
                    else if (loginBtn.contains(mouse)) {
                        loginError = "";
                        ApiClient::instance().loginAsync(loginEmail, loginPassword);
                    }
                    else if (regBtn.contains(mouse)) {
                        std::string username = loginEmail.substr(0, loginEmail.find('@'));
                        if (username.empty()) username = "player";
                        loginError = "";
                        ApiClient::instance().registerAsync(username, loginEmail, loginPassword);
                    }
                    else if (backBtn.contains(mouse)) {
                        if (ApiClient::instance().status != ApiStatus::Loading)
                            gameState = MAIN_MENU;
                    }
                }
                else if (gameState == QUESTS_MENU) {
                    FloatRect backBtn({ 450.f, 720.f }, { 300.f, 50.f });
                    if (backBtn.contains(mouse)) gameState = MAIN_MENU;

                    float y = 150.f;
                    for (auto& q : quests) {
                        if (q.completed && !q.claimed) {
                            FloatRect claimBtn({ 870.f, y + 10.f }, { 100.f, 38.f });
                            if (claimBtn.contains(mouse)) {
                                ApiClient::instance().claimQuestAsync(q.questId, [&q](bool ok, int reward) {
                                    if (ok) {
                                        q.claimed = true;
                                        ApiClient::instance().player.coins += reward;
                                    }
                                    });
                            }
                        }
                        y += 70.f;
                        if (y > 680.f) break;
                    }
                }
                else if (gameState == SHOP_MENU) {
                    FloatRect backBtn({ 450.f, 730.f }, { 300.f, 50.f });
                    if (backBtn.contains(mouse)) { gameState = MAIN_MENU; }
                    else if (shopLoaded) {
                        const std::vector<std::string> types = { "player", "bar", "slash" };
                        float colX[3] = { 40.f, 440.f, 840.f };
                        float colW = 340.f;
                        for (int t = 0; t < 3; ++t) {
                            float y = 130.f;
                            for (auto& s : shopSkins) {
                                if (s.type != types[t]) continue;
                                if (!s.owned) {
                                    FloatRect buyBtn({ colX[t] + colW - 100.f, y + 18.f }, { 88.f, 34.f });
                                    if (buyBtn.contains(mouse)) {
                                        ApiClient::instance().buySkinAsync(s.id, [&s, &shopMessage](bool ok, std::string) {
                                            if (ok) { s.owned = true; ApiClient::instance().player.coins -= s.price; shopMessage = "Bought: " + s.name; }
                                            else { shopMessage = "Not enough coins!"; }
                                            });
                                    }
                                }
                                else if (!s.equipped) {
                                    FloatRect equipBtn({ colX[t] + colW - 100.f, y + 18.f }, { 88.f, 34.f });
                                    if (equipBtn.contains(mouse)) {
                                        std::string sid = s.id, stype = s.type;
                                        ApiClient::instance().equipSkinAsync(sid, [&shopSkins, sid, stype, &shopMessage](bool ok) {
                                            if (ok) {
                                                for (auto& sk : shopSkins) if (sk.type == stype) sk.equipped = false;
                                                for (auto& sk : shopSkins) if (sk.id == sid)   sk.equipped = true;
                                                shopMessage = "Equipped!";
                                            }
                                            });
                                    }
                                }
                                y += 78.f;
                                if (y > 680.f) break;
                            }
                        }
                    }
                }
            }
            if (auto* te = event->getIf<Event::TextEntered>()) {
                if (gameState == LOGIN_MENU) {
                    if (te->unicode == 8) {
                        if (loginEmailActive && !loginEmail.empty())    loginEmail.pop_back();
                        if (loginPasswordActive && !loginPassword.empty()) loginPassword.pop_back();
                    }
                    else if (te->unicode >= 32 && te->unicode < 128) {
                        if (loginEmailActive)    loginEmail += (char)te->unicode;
                        if (loginPasswordActive) loginPassword += (char)te->unicode;
                    }
                }
            }
        }

        {
            auto apiStatus = ApiClient::instance().status.load();
            if (apiStatus == ApiStatus::Success) {
                loginError = ApiClient::instance().getStatusMessage();
                ApiClient::instance().status = ApiStatus::Idle;
                if (gameState == LOGIN_MENU && ApiClient::instance().player.loggedIn)
                    gameState = MAIN_MENU;
            }
            else if (apiStatus == ApiStatus::Error) {
                loginError = ApiClient::instance().getStatusMessage();
                ApiClient::instance().status = ApiStatus::Idle;
            }
        }

        float dt = clock.restart().asSeconds();

        if (gameState == PLAYING) {
            if (player.getHasKey() && player.getPosition().x > StartdoorPosition.x && player.getPosition().x < EnddoorPosition.x)
                enteringDoor = true;
            if (!enteringDoor) {
                for (auto& e : enemies) {
                    e.update(dt, player.getPosition(), MAP, MAP_WIDTH, MAP_HEIGHT, TileSize);
                }

                player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, window, MobMAP, InterestingMAP, BackgroundMAP, enemies);

                sf::Vector2f ppos = player.getPosition();
                sf::Vector2f pcenter = { ppos.x + 15.f, ppos.y + 20.f };
                int doorTx = static_cast<int>(pcenter.x / TileSize);
                int doorTy = static_cast<int>(pcenter.y / TileSize);
                if (doorTx >= 0 && doorTx < MAP_WIDTH && doorTy >= 0 && doorTy < MAP_HEIGHT) {
                    if (MAP[doorTy][doorTx] == 13) {
                        enteringDoor = true;
                        enterTimer = ENTER_DURATION;
                        enterViewStartCenter = view1.getCenter();
                        enterViewTargetCenter = Vector2f((doorTx + 0.5f) * TileSize, (doorTy + 0.5f) * TileSize);
                        enterViewStartSize = view1.getSize();
                    }
                }

                if (!player.isAlive() && player.hasFinishedDeathAnimation()) {
                    levelCompleted = false;
                    finalTime = time;
                    finalCoins = player.getCoins();
                    finalKills = 0;
                    for (auto& e : enemies) if (!e.isAlive()) finalKills++;
                    gameState = GAME_OVER;
                }

                if (!enteringDoor) {
                    Vector2f playerPos = player.getPosition();
                    Vector2f viewCenter = view1.getCenter();
                    viewCenter.x += (playerPos.x - viewCenter.x) * 0.1f;
                    viewCenter.y += (playerPos.y - viewCenter.y) * 0.1f;
                    float mapWidthPx = MAP_WIDTH * TileSize;
                    float mapHeightPx = MAP_HEIGHT * TileSize;
                    float halfWidth = view1.getSize().x / 2.f;
                    float halfHeight = view1.getSize().y / 2.f;
                    if (viewCenter.x < halfWidth) viewCenter.x = halfWidth;
                    if (viewCenter.y < halfHeight) viewCenter.y = halfHeight;
                    if (viewCenter.x > mapWidthPx - halfWidth) viewCenter.x = mapWidthPx - halfWidth;
                    if (viewCenter.y > mapHeightPx - halfHeight) viewCenter.y = mapHeightPx - halfHeight;
                    view1.setCenter(viewCenter);
                }
            }
            else if (gameState == PAUSED);
            else {
                enterTimer -= dt;
                float progress = 1.f - std::max(0.f, enterTimer) / ENTER_DURATION;

                auto lerp = [](const Vector2f& a, const Vector2f& b, float t) {
                    return Vector2f(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
                    };
                Vector2f newCenter = lerp(enterViewStartCenter, enterViewTargetCenter, progress);
                Vector2f startSize = enterViewStartSize;
                Vector2f targetSize = startSize * 0.6f;
                Vector2f newSize = lerp(startSize, targetSize, progress);
                view1.setCenter(newCenter);
                view1.setSize(newSize);

                if (enterTimer <= 0.f) {
                    enteringDoor = false;
                    view1.setSize({ 1200.f, 800.f });
                    view1.setCenter({ 600.f, 400.f });
                    levelCompleted = true;
                    finalTime = time;
                    finalCoins = player.getCoins();
                    finalKills = 0;
                    for (auto& e : enemies) if (!e.isAlive()) finalKills++;

                    LevelRecord newRecord(currentLevel, finalTime, finalCoins, finalKills, true);
                    saveBestRecord(newRecord);
                    loadBestRecords();
                    if (ApiClient::instance().player.loggedIn) {
                        ApiClient::instance().submitRecordAsync(currentLevel, finalTime, finalCoins, finalKills);
                    }
                    gameState = GAME_OVER;
                }
            }
            time += dt;
        }

        window.clear(Color::Cyan);

        if (gameState == MAIN_MENU || gameState == LEVELS_MENU ||
            gameState == CREATORS_MENU || gameState == SETTINGS_MENU ||
            gameState == BEST_TIMES_MENU || gameState == LOGIN_MENU ||
            gameState == QUESTS_MENU || gameState == SHOP_MENU) {
            if (hasMenuBg) {
                menuBgTexture.setRepeated(true);
                Vector2u texSize = menuBgTexture.getSize();
                Vector2u winSize = window.getSize();

                menuBgOffset += MENU_SCROLL_SPEED * dt;
                if (menuBgOffset >= (float)texSize.x)
                    menuBgOffset -= (float)texSize.x;

                int tilesY = (int)std::ceil((float)winSize.y / texSize.y) + 1;
                for (int ty = 0; ty < tilesY; ++ty) {
                    Sprite menuBg(menuBgTexture);
                    menuBg.setTextureRect(IntRect(
                        { (int)menuBgOffset, ty * (int)texSize.y },
                        { (int)winSize.x,    (int)texSize.y }));
                    menuBg.setPosition({ 0.f, ty * (float)texSize.y });
                    window.draw(menuBg);
                }
            }
            else {
                window.clear(Color(20, 20, 40));
            }
        }

        Vector2i mousePos = Mouse::getPosition(window);
        if (gameState == MAIN_MENU) {
            DrawMainMenu(window, font, mousePos);
        }
        else if (gameState == LEVELS_MENU) {
            DrawLevelsMenu(window, font, mousePos);
        }
        else if (gameState == BEST_TIMES_MENU) {
            DrawBestTimesMenu(window, font, mousePos);
        }
        else if (gameState == CREATORS_MENU) {
            DrawCreatorsMenu(window, font, mousePos);
        }
        else if (gameState == SETTINGS_MENU) {
            DrawSettingsMenu(window, font, mousePos, player.getAttackKey(), waitingForRemap, limitedDashMode);
        }
        else if (gameState == PLAYING) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            drawMob(window, spriteSheet);
            drawInteresting(window, spriteSheet);

            for (auto& e : enemies) e.draw(window, debugMode);

            player.draw(window, view1, font, debugMode);
            if (debugMode) {
                Vector2f tl = view1.getCenter() - view1.getSize() / 2.f;
                Vector2f br = view1.getCenter() + view1.getSize() / 2.f;
                int sx = max(0, (int)(tl.x / TileSize));
                int sy = max(0, (int)(tl.y / TileSize));
                int ex = min(MAP_WIDTH, (int)(br.x / TileSize) + 2);
                int ey = min(MAP_HEIGHT, (int)(br.y / TileSize) + 2);
                for (int y = sy; y < ey; ++y)
                    for (int x = sx; x < ex; ++x)
                        if (MAP[y][x] > 0 && MAP[y][x] != -1) {
                            RectangleShape dbgTile({ TileSize - 1.f, TileSize - 1.f });
                            dbgTile.setPosition({ x * TileSize, y * TileSize });
                            dbgTile.setFillColor(Color::Transparent);
                            dbgTile.setOutlineColor(Color(0, 255, 100, 80));
                            dbgTile.setOutlineThickness(1.f);
                            window.draw(dbgTile);
                        }
            }
            DrawHUD(window, font, player, time, currentLevel, tx_HPBar, hasHPBar, tx_StaminaBar, hasStaminaBar);
        }
        if (gameState == PAUSED) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            drawMob(window, spriteSheet);
            drawInteresting(window, spriteSheet);
            for (auto& e : enemies) e.draw(window, debugMode);
            player.draw(window, view1, font, debugMode, true);
            DrawPauseMenu(window, font, mousePos);
        }
        else if (gameState == GAME_OVER) {
            player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, window, MobMAP, InterestingMAP, BackgroundMAP, enemies);
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            drawMob(window, spriteSheet);
            drawInteresting(window, spriteSheet);

            for (auto& e : enemies) e.draw(window);

            player.draw(window, view1, font);
            if (levelCompleted) {
                drawEndInfo(window, font, finalTime, finalCoins, finalKills, true, currentLevel);
            }
            else {
                drawGameOver(window, font);
            }
        }
        else if (gameState == LOGIN_MENU) {
            DrawLoginMenu(window, font, mousePos, loginEmail, loginPassword,
                loginEmailActive, loginPasswordActive, loginError);

            window.setView(window.getDefaultView());

            if (ApiClient::instance().status.load() == ApiStatus::Loading) {
                RectangleShape overlay({ 1200.f, 800.f });
                overlay.setFillColor(Color(0, 0, 0, 130));
                window.draw(overlay);
                Text loadTxt(font, "Connecting...", 32);
                loadTxt.setFillColor(Color::White);
                FloatRect lb = loadTxt.getLocalBounds();
                loadTxt.setPosition({ (1200.f - lb.size.x) / 2.f, 370.f });
                window.draw(loadTxt);
            }
            if (ApiClient::instance().player.loggedIn) {
                Text st(font, "Logged in: " + ApiClient::instance().player.username, 16);
                st.setFillColor(Color(100, 255, 100));
                st.setPosition({ 10.f, 10.f });
                window.draw(st);
            }
        }
        else if (gameState == QUESTS_MENU) {
            DrawQuestsMenu(window, font, mousePos, quests);

            window.setView(window.getDefaultView());

            if (!questsLoaded && ApiClient::instance().player.loggedIn) {
                Text loadTxt(font, "Loading quests...", 24);
                loadTxt.setFillColor(Color::White);
                FloatRect lb = loadTxt.getLocalBounds();
                loadTxt.setPosition({ (1200.f - lb.size.x) / 2.f, 370.f });
                window.draw(loadTxt);
            }

            std::string st = ApiClient::instance().player.loggedIn
                ? ("Player: " + ApiClient::instance().player.username
                    + "   |   Coins: " + std::to_string(ApiClient::instance().player.coins))
                : "Not logged in — go to LOGIN first";
            Text stTxt(font, st, 16);
            stTxt.setFillColor(ApiClient::instance().player.loggedIn
                ? Color(100, 255, 100) : Color(255, 150, 100));
            stTxt.setPosition({ 10.f, 10.f });
            window.draw(stTxt);
        }
        else if (gameState == SHOP_MENU) {
            DrawShopMenu(window, font, mousePos, shopSkins, shopLoaded);

            window.setView(window.getDefaultView());
            std::string coinsStr = ApiClient::instance().player.loggedIn
                ? ("Coins: " + std::to_string(ApiClient::instance().player.coins))
                : "Login to buy skins";
            Text coinsTxt(font, coinsStr, 18);
            coinsTxt.setFillColor(Color::Yellow);
            coinsTxt.setPosition({ 10.f, 10.f });
            window.draw(coinsTxt);
            if (!shopMessage.empty()) {
                Text msg(font, shopMessage, 18);
                msg.setFillColor(Color(100, 255, 100));
                FloatRect mb = msg.getLocalBounds();
                msg.setPosition({ (1200.f - mb.size.x) / 2.f, 755.f });
                window.draw(msg);
            }
        }

        if (gameState == MAIN_MENU) {
            window.setView(window.getDefaultView());
            std::string authStr = ApiClient::instance().player.loggedIn
                ? ("[ " + ApiClient::instance().player.username + "  |  "
                    + std::to_string(ApiClient::instance().player.coins) + " coins ]")
                : "[ Not logged in ]";
            Text authTxt(font, authStr, 18);
            authTxt.setFillColor(ApiClient::instance().player.loggedIn
                ? Color(100, 255, 150) : Color(180, 180, 200));
            authTxt.setOutlineColor(Color(0, 0, 0, 180));
            authTxt.setOutlineThickness(1.f);
            FloatRect ab = authTxt.getLocalBounds();
            authTxt.setPosition({ (1200.f - ab.size.x) / 2.f, 210.f });
            window.draw(authTxt);
        }

        window.display();
    }

    return 0;
}