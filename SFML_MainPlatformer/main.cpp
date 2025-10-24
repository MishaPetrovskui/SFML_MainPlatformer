#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <map>
#include <cstring>
#include <set>
#include "Player.h"
#include "Enemy.h"

using namespace sf;
using namespace std;

#define FILE_PATH "Map.txt"

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
    GAME_OVER
};

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
                if (lavaCount <= 5) {
                    int tile = (t1 == 16 || t1 == 17) ? t1 :
                        (t2 == 16 || t2 == 17) ? t2 :
                        (t3 == 16 || t3 == 17) ? t3 : t4;
                    string layer = (t1 == 16 || t1 == 17) ? "MAP" :
                        (t2 == 16 || t2 == 17) ? "BG" :
                        (t3 == 16 || t3 == 17) ? "MOB" : "INTERESTING";
                    cout << "Lava at (" << x << "," << y << ") - tile " << tile
                        << " in layer " << layer << endl;
                }
            }

            if ((t1 >= 18 && t1 <= 21) || (t2 >= 18 && t2 <= 21) ||
                (t3 >= 18 && t3 <= 21) || (t4 >= 18 && t4 <= 21)) {
                spikeCount++;
                if (spikeCount <= 9) {
                    int tile = (t1 >= 18 && t1 <= 21) ? t1 :
                        (t2 >= 18 && t2 <= 21) ? t2 :
                        (t3 >= 18 && t3 <= 21) ? t3 : t4;
                    string layer = (t1 >= 18 && t1 <= 21) ? "MAP" :
                        (t2 >= 18 && t2 <= 21) ? "BG" :
                        (t3 >= 18 && t3 <= 21) ? "MOB" : "INTERESTING";
                    cout << "Spike at (" << x << "," << y << ") - tile " << tile
                        << " in layer " << layer << endl;
                }
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
        rect.position.y + (rect.size.y - bounds.size.y) / 2.f
    });
    window.draw(txt);
}

void DrawMainMenu(RenderWindow& window, Font& font, Vector2i mousePos) {
    window.setView(window.getDefaultView());

    Text title(font, "PLATFORMER", 64);
    title.setFillColor(Color::White);
    title.setStyle(Text::Bold);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({(1200.f - titleBounds.size.x) / 2.f, 120.f});
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

    FloatRect levelRect({ 450.f, 300.f }, { 300.f, 50.f });
    DrawMenuButton(window, levelRect, "LEVEL 1", font, levelRect.contains(Vector2f(mousePos)));

    FloatRect backRect({ 450.f, 510.f }, { 300.f, 50.f });
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
        "Yashenko Denis (HoWL)"
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

void DrawSettingsMenu(RenderWindow& window, Font& font, Vector2i mousePos, Keyboard::Key attackKey, bool waitingForRemap) {
    window.setView(window.getDefaultView());

    Text title(font, "SETTINGS", 48);
    title.setFillColor(Color::White);
    FloatRect titleBounds = title.getLocalBounds();
    title.setPosition({ (1200.f - titleBounds.size.x) / 2.f, 120.f });
    window.draw(title);

    string keyText = "KEY OF ATTACK: " + keyToString(attackKey);
    FloatRect remapRect({ 450.f, 300.f }, { 300.f, 50.f });
    DrawMenuButton(window, remapRect, keyText, font, remapRect.contains(Vector2f(mousePos)));

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

void DrawHUD(RenderWindow& window, Font& font, const Player& player, float gameTime) {
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
    gameOverText.setPosition({ 350.f, 250.f });
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

int main()
{
    FILE* file;
    if (errno_t err_n = fopen_s(&file, FILE_PATH, "r"))
    {
        cout << "Map file not found!" << endl;
        return err_n;
    }
    fread(&BackgroundMAP, sizeof(BackgroundMAP), 1, file);
    fread(&MAP, sizeof(MAP), 1, file);
    fread(&MobMAP, sizeof(MobMAP), 1, file);
    fread(&InterestingMAP, sizeof(InterestingMAP), 1, file);
    fclose(file);

    analyzeMaps();

    static int OriginalInterestingMAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};
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
    Texture tx_SpikesRight, tx_SpikesTop;

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
        {12, Sprite(tx_ClosedDoor)},
        {13, Sprite(tx_OpenedDoor)},
        {14, Sprite(tx_GreenBricks)},
        {15, Sprite(tx_GreenGrass)},
        {16, Sprite(tx_Lava)},
        {17, Sprite(tx_LavaTop)},
        {18, Sprite(tx_Spikes)},
        {19, Sprite(tx_SpikesLeft)},
        {20, Sprite(tx_SpikesRight)},
        {21, Sprite(tx_SpikesTop)},
    };

    std::vector<std::tuple<int, int, int>> mobTemplate;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            int tile = MobMAP[y][x];
            if (tile == 8 || tile == 9) {
                mobTemplate.emplace_back(x, y, tile);
                MobMAP[y][x] = -1;
            }
        }
    }

    std::vector<Enemy> enemies;
    for (auto& t : mobTemplate) {
        int x, y, tile;
        std::tie(x, y, tile) = t;
        if (tile == 8) enemies.emplace_back(x * TileSize, y * TileSize, tx_Slime, TileSize);
        else if (tile == 9) enemies.emplace_back(x * TileSize, y * TileSize, tx_SlimeMan, TileSize);
    }
    if (enemies.empty()) {
        enemies.emplace_back(5.f * TileSize, 8.f * TileSize, tx_Slime, TileSize);
    }

    Font font;
    if (!font.openFromFile("Fonts/DigitalPixelV100-Regular.ttf")) {
        cout << "Font not found!" << endl;
        return -1;
    }

    RenderWindow window(VideoMode({ 1200, 800 }), "Platformer Game");

    Player player(100.f, 100.f);
    Clock clock;
    GameState gameState = MAIN_MENU;

    bool waitingForRemap = false;
    Texture menuBgTexture;
    bool hasMenuBg = menuBgTexture.loadFromFile("Sprites/MENU1.png");
    float menuBgOffset = 0.f;
    const float MENU_SCROLL_SPEED = 50.f;
    std::cout << hasMenuBg << endl;
	float time = 0.f;
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
                        gameState = MAIN_MENU;
                        player.reset();
                    }
                    else {
                        window.close();
                    }
                }

                if (keyEvent->code == Keyboard::Key::Enter) {
                    if (gameState == MAIN_MENU) {
                        gameState = PLAYING;
                        std::memcpy(InterestingMAP, OriginalInterestingMAP, sizeof(InterestingMAP));
                        player.reset();
                        enemies.clear();
                        for (auto& t : mobTemplate) {
                            int x, y, tile;
                            std::tie(x, y, tile) = t;
                            if (tile == 8) enemies.emplace_back(x * TileSize, y * TileSize, tx_Slime, TileSize);
                            else if (tile == 9) enemies.emplace_back(x * TileSize, y * TileSize, tx_SlimeMan, TileSize);
                        }
                        if (enemies.empty()) enemies.emplace_back(5.f * TileSize, 8.f * TileSize, tx_Slime, TileSize);
                    }
                    else if (gameState == GAME_OVER) {
                        gameState = PLAYING;
                        std::memcpy(InterestingMAP, OriginalInterestingMAP, sizeof(InterestingMAP));
                        player.reset();
						time = 0.f;
                        enemies.clear();
                        for (auto& t : mobTemplate) {
                            int x, y, tile;
                            std::tie(x, y, tile) = t;
                            if (tile == 8) enemies.emplace_back(x * TileSize, y * TileSize, tx_Slime, TileSize);
                            else if (tile == 9) enemies.emplace_back(x * TileSize, y * TileSize, tx_SlimeMan, TileSize);
                        }
                        if (enemies.empty()) enemies.emplace_back(5.f * TileSize, 8.f * TileSize, tx_Slime, TileSize);
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
                    FloatRect level1({ 450.f, 300.f }, { 300.f, 50.f });
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });
                    if (level1.contains(mouse)) {
                        gameState = PLAYING;
                        std::memcpy(InterestingMAP, OriginalInterestingMAP, sizeof(InterestingMAP));
                        player.reset();
                    }
                    else if (back.contains(mouse)) gameState = MAIN_MENU;
                }
                else if (gameState == CREATORS_MENU || gameState == SETTINGS_MENU) {
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });
                    if (back.contains(mouse)) gameState = MAIN_MENU;
                }
                else if (gameState == SETTINGS_MENU) {
                    FloatRect remap({ 450.f, 300.f }, { 300.f, 50.f });
                    FloatRect back({ 450.f, 510.f }, { 300.f, 50.f });
                    if (remap.contains(mouse)) {
                        waitingForRemap = true;
                    }
                    else if (back.contains(mouse)) {
                        gameState = MAIN_MENU;
                    }
                }
            }
        }

        float dt = clock.restart().asSeconds();

        if (gameState == PLAYING) {
            for (auto& e : enemies) {
                e.update(dt, player.getPosition());
            }

            player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, window, MobMAP, InterestingMAP, BackgroundMAP, enemies);
            if (!player.isAlive()) {
                gameState = GAME_OVER;
            }

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

        window.clear(Color::Cyan);

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

        Vector2i mousePos = Mouse::getPosition(window);
        if (gameState == MAIN_MENU) {
            //drawMenu(window, font, player.getAttackKey(), waitingForRemap);
            DrawMainMenu(window, font, mousePos);
        }
        else if (gameState == LEVELS_MENU) {
            DrawLevelsMenu(window, font, mousePos);
        }
        else if (gameState == CREATORS_MENU) {
            DrawCreatorsMenu(window, font, mousePos);
        }
        else if (gameState == SETTINGS_MENU) {
            DrawSettingsMenu(window, font, mousePos, player.getAttackKey(), waitingForRemap);
        }
        else if (gameState == PLAYING) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            drawMob(window, spriteSheet);
            drawInteresting(window, spriteSheet);

            for (auto& e : enemies) e.draw(window);

            player.draw(window, view1, font);
            DrawHUD(window, font, player, time += clock.getElapsedTime().asSeconds());
			std::cout << time << std::endl;
        }
        else if (gameState == GAME_OVER) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            drawMob(window, spriteSheet);
            drawInteresting(window, spriteSheet);

            for (auto& e : enemies) e.draw(window);

            player.draw(window, view1, font);
            drawGameOver(window, font);
        }

        window.display();
    }

    return 0;
}