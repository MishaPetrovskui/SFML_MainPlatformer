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

    RenderWindow window(VideoMode({ 1200, 800 }), "Hello world!");

    Player player(100.f, 100.f);
    Clock clock;

    while (window.isOpen()) {

        while (const optional event = window.pollEvent())
        {

            if (event->is<Event::Closed>())
                window.close();
            if (event->is<Event::KeyPressed>() && event->getIf<Event::KeyPressed>()->code == Keyboard::Key::Escape) {
                fclose(file);
                window.close();
            }
        }

        float dt = clock.restart().asSeconds();
        player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, window);

        // === Рух камери за гравцем ===
        Vector2f playerPos = player.getPosition();
        Vector2f viewCenter = view1.getCenter();

        // Плавне слідування (lerp)
        viewCenter.x += (playerPos.x - viewCenter.x) * 0.1f;
        viewCenter.y += (playerPos.y - viewCenter.y) * 0.1f;

        // Межі карти (щоб камера не вийшла за межі)
        float mapWidthPx = MAP_WIDTH * TileSize;
        float mapHeightPx = MAP_HEIGHT * TileSize;

        float halfWidth = view1.getSize().x / 2.f;
        float halfHeight = view1.getSize().y / 2.f;

        if (viewCenter.x < halfWidth) viewCenter.x = halfWidth;
        if (viewCenter.y < halfHeight) viewCenter.y = halfHeight;
        if (viewCenter.x > mapWidthPx - halfWidth) viewCenter.x = mapWidthPx - halfWidth;
        if (viewCenter.y > mapHeightPx - halfHeight) viewCenter.y = mapHeightPx - halfHeight;

        view1.setCenter(viewCenter);

        window.clear(Color::Cyan);
        window.setView(view1);
		drawBg(window, spriteSheet);
        drawMap(window, spriteSheet);
        player.draw(window, view1);
        window.display();
    }
}
