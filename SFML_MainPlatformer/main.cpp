#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <map>
#include "Player.h"

using namespace sf;
using namespace std;

#define FILE_PATH "Map.txt"

const static int MAP_WIDTH = 500;
const static int MAP_HEIGHT = 20;
char MAP[MAP_HEIGHT][MAP_WIDTH + 1] = {};

float TileSize = 40.f;

View view1(FloatRect({ 0, 0 }, { 1200, 800 }));

enum GameState {
    MENU,
    PLAYING,
    GAME_OVER
};

void drawMap(RenderWindow& window, map<char, Sprite>& spriteSheet)
{
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            char tile = MAP[y][x];
            if (tile == ' ' || tile == '\0') continue;

            auto sprite = spriteSheet.find(tile);
            if (sprite == spriteSheet.end())
                sprite = spriteSheet.find('U');

            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void drawBg(RenderWindow& window, map<char, Sprite>& spriteSheet)
{
    auto sprite = spriteSheet.find('O');
    if (sprite == spriteSheet.end()) return;

    // Покриваємо всю область, яку бачить камера
    Vector2f topLeft = view1.getCenter() - view1.getSize() / 2.f;
    Vector2f bottomRight = view1.getCenter() + view1.getSize() / 2.f;

    int startX = max(0, (int)(topLeft.x / TileSize) - 1);
    int startY = max(0, (int)(topLeft.y / TileSize) - 1);
    int endX = min(MAP_WIDTH, (int)(bottomRight.x / TileSize) + 1);
    int endY = min(MAP_HEIGHT, (int)(bottomRight.y / TileSize) + 1);

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            sprite->second.setPosition({ x * TileSize, y * TileSize });
            window.draw(sprite->second);
        }
    }
}

void drawMenu(RenderWindow& window, Font& font) 
{
    View menuView = window.getDefaultView();
    window.setView(menuView);

    RectangleShape background({ 1200.f, 800.f });
    background.setFillColor(Color(20, 20, 40));
    window.draw(background);

    Text title(font, "PLATFORMER GAME", 60);
    title.setFillColor(Color::White);
    title.setOutlineColor(Color(100, 150, 255));
    title.setOutlineThickness(3.f);
    title.setPosition({ 75.f, 100.f });
    window.draw(title);

    Text authorsTitle(font, "Created by:", 27);
    authorsTitle.setFillColor(Color(200, 200, 200));
    authorsTitle.setPosition({ 450.f, 250.f });
    window.draw(authorsTitle);

    Text author1(font, "Petrovsky Mikhail (Dram)", 23);
    author1.setFillColor(Color(150, 200, 255));
    author1.setPosition({ 380.f, 310.f });
    window.draw(author1);

    Text author2(font, "Yashchenko Denis (HoWL)", 23);
    author2.setFillColor(Color(150, 200, 255));
    author2.setPosition({ 380.f, 350.f });
    window.draw(author2);

    RectangleShape buttonBg({ 300.f, 60.f });
    buttonBg.setPosition({ 450.f, 480.f });
    buttonBg.setFillColor(Color(50, 100, 200));
    buttonBg.setOutlineColor(Color::White);
    buttonBg.setOutlineThickness(3.f);
    window.draw(buttonBg);

    Text startText(font, "PRESS ENTER TO START", 9);
    startText.setFillColor(Color::White);
    startText.setPosition({ 480.f, 495.f });
    window.draw(startText);

    Text controls(font, "Controls:\nWASD/Arrows - Move\nSpace - Jump\nShift - Dash", 15);
    controls.setFillColor(Color(180, 180, 180));
    controls.setPosition({ 480.f, 600.f });
    window.draw(controls);
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
    fread(&MAP, sizeof(MAP), 1, file);
    fclose(file);

    Texture tx_Stone, tx_BlueSky, tx_Dirt, tx_Grass, tx_Undefined;
    tx_Stone.loadFromFile("Sprites/Stone.png");
    tx_BlueSky.loadFromFile("Sprites/Blue Sky.png");
    tx_Dirt.loadFromFile("Sprites/Dirt.png");
    tx_Grass.loadFromFile("Sprites/Grass.png");
    tx_Undefined.loadFromFile("Sprites/Undefined.png");

    map<char, Sprite> spriteSheet = {
        {'D', Sprite(tx_Dirt)},
        {'S', Sprite(tx_Stone)},
        {'G', Sprite(tx_Grass)},
        {'U', Sprite(tx_Undefined)},
        {'O', Sprite(tx_BlueSky)},
    };

    Font font;
    if (!font.openFromFile("Fonts/DigitalPixelV100-Regular.ttf")) {
        cout << "Font not found!" << endl;
        return -1;
    }

    RenderWindow window(VideoMode({ 1200, 800 }), "Hello world!");

    Player player(100.f, 100.f);
    Clock clock;
    GameState gameState = MENU;
    while (window.isOpen()) {

        while (const optional event = window.pollEvent())
        {

            if (event->is<Event::Closed>())
                window.close();
            if (event->is<Event::KeyPressed>()) {
                auto keyEvent = event->getIf<Event::KeyPressed>();

                if (keyEvent->code == Keyboard::Key::Escape) {
                    if (gameState == PLAYING) {
                        gameState = MENU;
                        player.reset();
                    }
                    else if (gameState == GAME_OVER) {
                        gameState = MENU;
                        player.reset();
                    }
                    else {
                        window.close();
                    }
                }

                if (keyEvent->code == Keyboard::Key::Enter) {
                    if (gameState == MENU) {
                        gameState = PLAYING;
                        player.reset();
                    }
                    else if (gameState == GAME_OVER) {
                        gameState = PLAYING;
                        player.reset();
                    }
                }
            }
        }

        float dt = clock.restart().asSeconds();
        if (gameState == PLAYING) {
            player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, window);
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
        if (gameState == MENU) {
            drawMenu(window, font);
        }
        else if (gameState == PLAYING) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            player.draw(window, view1);
        }
        else if (gameState == GAME_OVER) {
            window.setView(view1);
            drawBg(window, spriteSheet);
            drawMap(window, spriteSheet);
            player.draw(window, view1);
            drawGameOver(window, font);
        }
        window.display();
    }
}
