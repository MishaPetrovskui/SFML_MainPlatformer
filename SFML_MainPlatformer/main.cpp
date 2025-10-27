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
    GAME_OVER,
    BEST_TIMES_MENU
};

int currentLevel = 1;
bool limitedDashMode = false; // false = неограниченные деши, true = один деш с перезарядкой

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
    RectangleShape bg({ rect.size.x, rect.size.y });
    bg.setPosition({ rect.position.x, rect.position.y });
    bg.setFillColor(isSelected ? Color(60, 60, 80) : Color(40, 40, 60));
    bg.setOutlineColor(Color::White);
    bg.setOutlineThickness(2.f);
    window.draw(bg);

    Text txt(font, text, 20);
    txt.setFillColor(Color::White);
    FloatRect bounds = txt.getLocalBounds();
    txt.setPosition({
        rect.position.x + (rect.size.x - bounds.size.x) / 2.f,
        rect.position.y + (rect.size.y - bounds.size.y) / 2.f + 7.f
        });
    window.draw(txt);
}

void DrawMainMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    Text title(font, "PIXELRUN", 64);
    title.setFillColor(Color::White);
    title.setStyle(Text::Bold);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 120.f });
    window.draw(title);

    vector<pair<string, FloatRect>> buttons = {
        {"LEVELS", FloatRect({450.f, 300.f}, {300.f, 50.f})},
        {"CREATORS", FloatRect({450.f, 370.f}, {300.f, 50.f})},
        {"SETTINGS", FloatRect({450.f, 440.f}, {300.f, 50.f})},
        {"EXIT", FloatRect({450.f, 510.f}, {300.f, 50.f})}
    };

    for (const auto& [text, rect] : buttons) {
        bool isHovered = rect.contains(Vector2f(mousePos));
        DrawMenuButton(window, rect, text, font, isHovered);
    }
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
        "Yashchenko Denis (HoWL)"
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
    FloatRect remapRect({ 450.f-90.f, 250.f }, { 480.f, 50.f });
    DrawMenuButton(window, remapRect, keyText, font, remapRect.contains(Vector2f(mousePos)));

    string dashText = "DASH MODE: " + string(limitedDash ? "LIMITED" : "UNLIMITED");
    FloatRect dashRect({ 450.f-90.f , 320.f }, { 480.f, 50.f });
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

void DrawHUD(RenderWindow& window, Font& font, const Player& player, float gameTime, int levelNum) {
    View gameView = window.getView();
    window.setView(window.getDefaultView());

    RectangleShape hudBg({ 1160.f, 80.f });
    hudBg.setPosition({ 20.f, 20.f });
    hudBg.setFillColor(Color(0, 0, 0, 180));
    window.draw(hudBg);

    const float barWidth = 300.f;
    const float barHeight = 20.f;
    const float leftX = 40.f;

    RectangleShape hpBg({ barWidth, barHeight });
    hpBg.setPosition({ leftX, 30.f });
    hpBg.setFillColor(Color(60, 0, 0, 180));
    window.draw(hpBg);

    float hpRatio = static_cast<float>(player.getHP()) / 100.f;
    RectangleShape hpBar({ barWidth * hpRatio, barHeight });
    hpBar.setPosition({ leftX, 30.f });
    hpBar.setFillColor(Color::Red);
    window.draw(hpBar);

    Text hpText(font, "HP: " + to_string(player.getHP()), 18);
    hpText.setPosition({ leftX - 35.f, 30.f });
    hpText.setFillColor(Color::White);
    window.draw(hpText);

    RectangleShape staBg({ barWidth, barHeight });
    staBg.setPosition({ leftX, 60.f });
    staBg.setFillColor(Color(0, 60, 0, 180));
    window.draw(staBg);

    float staRatio = player.getStamina() / player.getMaxStamina();
    RectangleShape staBar({ barWidth * staRatio, barHeight });
    staBar.setPosition({ leftX, 60.f });
    staBar.setFillColor(Color::Green);
    window.draw(staBar);

    Text levelText(font, "Level: " + to_string(levelNum), 24);
    levelText.setPosition({ leftX, 85.f });
    levelText.setFillColor(Color::White);
    window.draw(levelText);

    string timeStr = FormatTime(gameTime);
    Text timeText(font, timeStr, 32);
    FloatRect timeBounds = timeText.getLocalBounds();
    timeText.setPosition({
        (1200.f - timeBounds.size.x) / 2.f,
        45.f
        });
    timeText.setFillColor(Color::White);
    window.draw(timeText);

    Text coinsText(font, "Coins: " + to_string(player.getCoins()), 24);
    FloatRect coinBounds = coinsText.getLocalBounds();
    coinsText.setPosition({ 1140.f - coinBounds.size.x, 45.f });
    coinsText.setFillColor(Color::Yellow);
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

int main()
{
    static int OriginalInterestingMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};

    std::vector<std::tuple<int, int, int>> mobTemplate;

    loadBestRecords();

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
    Texture tx_OrangePortal;

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
                        gameState = MAIN_MENU;
                        player.reset();
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
                    FloatRect levels({ 450.f, 300.f }, { 300.f, 50.f });
                    FloatRect creators({ 450.f, 370.f }, { 300.f, 50.f });
                    FloatRect settings({ 450.f, 440.f }, { 300.f, 50.f });
                    FloatRect exitBtn({ 450.f, 510.f }, { 300.f, 50.f });

                    if (levels.contains(mouse)) gameState = LEVELS_MENU;
                    else if (creators.contains(mouse)) gameState = CREATORS_MENU;
                    else if (settings.contains(mouse)) gameState = SETTINGS_MENU;
                    else if (exitBtn.contains(mouse)) window.close();
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
                    FloatRect remap({ 450.f, 250.f }, { 300.f, 50.f });
                    FloatRect dashToggle({ 450.f, 320.f }, { 300.f, 50.f });
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

                    gameState = GAME_OVER;
                }
            }
            time += dt;
        }

        window.clear(Color::Cyan);

        if (gameState == MAIN_MENU || gameState == LEVELS_MENU ||
            gameState == CREATORS_MENU || gameState == SETTINGS_MENU ||
            gameState == BEST_TIMES_MENU) {
            if (hasMenuBg) {
                Sprite menuBg(menuBgTexture);
                Vector2u texSize = menuBgTexture.getSize();
                Vector2u winSize = window.getSize();

                menuBgTexture.setRepeated(true);

                menuBgOffset += MENU_SCROLL_SPEED * dt;
                if (menuBgOffset > texSize.x)
                    menuBgOffset -= texSize.x;

                menuBg.setTextureRect(IntRect({ static_cast<int>(menuBgOffset), 0 }, {
                    static_cast<int>(winSize.x),
                    static_cast<int>(winSize.y) }));
                menuBg.setPosition({ 0.f, 0.f });
                window.draw(menuBg);

                menuBg.setTextureRect(IntRect({ 0, 0 }, {
                    static_cast<int>(winSize.x),
                    static_cast<int>(winSize.y) }));
                menuBg.setPosition({ static_cast<float>(winSize.x) - menuBgOffset, 0.f });
                window.draw(menuBg);
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

            for (auto& e : enemies) e.draw(window);

            player.draw(window, view1, font);
            DrawHUD(window, font, player, time, currentLevel);
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

        window.display();
    }

    return 0;
}