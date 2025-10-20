#include <SFML/Graphics.hpp>;
#include <iostream>;
#include <Math.h>;
#include <map>
#include "Player.h"

using namespace sf;
using namespace std;

#define FILE_PATH "Map.txt"

const static int MAP_WIDTH = 500;
const static int MAP_HEIGHT = 20;

char MAP[MAP_HEIGHT][MAP_WIDTH + 1] = { };

float TileSize = 40.f;
float Margin = 0.f;

View view1({ 0.f, 0.f }, { 1200, 800 });
Vector2f view1POS = { 0.f, 0.f };

void drawMap(RenderWindow& window, map<char, Sprite> spriteSheet) {
	for (int y = 0; y < MAP_HEIGHT; y++)
		for (int x = 0; x < MAP_WIDTH; x++) {
			if (x * TileSize - view1POS.x * 2 < 1200 && y * TileSize < 800 - view1POS.y) {
				if (MAP[y][x] == ' ') {
					RectangleShape tile({ TileSize, TileSize });
					tile.setFillColor(Color::White);
					tile.setPosition({ x * TileSize - view1POS.x, y * TileSize - view1POS.y });
					window.draw(tile);
				}
				else {
					auto sprite = spriteSheet.find(MAP[y][x]);
					if (sprite != spriteSheet.end()) {

					}
					else {
						sprite = spriteSheet.find('U');
					}
					sprite->second.setPosition({ x * TileSize - view1POS.x, y * TileSize - view1POS.y });
					window.draw(sprite->second);
				}
			}
		}
}


int main()
{
	FILE* file;
	view1.setCenter({ 600.f, 400.f });
	if (errno_t err_n = fopen_s(&file, FILE_PATH, "r")) {
		cout << "NOT Correct!!!!" << endl;
		return err_n;
	}
	fread(&MAP, sizeof(MAP), 1, file);
	fclose(file);

	Texture tx_BlackDirt;
	tx_BlackDirt.loadFromFile("Sprites/BlackDirt.png");
	Texture tx_Dirt;
	tx_Dirt.loadFromFile("Sprites/Dirt.png");
	Texture tx_Grass;
	tx_Grass.loadFromFile("Sprites/Grass.png");
	Texture tx_Undefined;
	tx_Undefined.loadFromFile("Sprites/Undefined.png");


	map<char, Sprite> spriteSheet = {
		{'D', Sprite(tx_Dirt)},
		{'B', Sprite(tx_BlackDirt)},
		{'G', Sprite(tx_Grass)},
		{'U', Sprite(tx_Undefined)},
	};


	RenderWindow window(VideoMode({ 1200, 800 }), "Hello World!");

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
		player.update(dt, MAP, MAP_WIDTH, MAP_HEIGHT, TileSize, view1, view1POS,window);
		window.clear(Color::Black);
		drawMap(window, spriteSheet);
		player.draw(window, view1);
		window.display();
	}
	
}