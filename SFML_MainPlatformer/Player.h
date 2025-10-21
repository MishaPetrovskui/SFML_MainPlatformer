#pragma once
#include <SFML/Graphics.hpp>;
#include <iostream>;
#include <Math.h>;
#include <map>
class Player {
private:
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    float speed;
    float gravity;
    bool onGround;

public:
    Player(float startX = 50.f, float startY = 50.f);

    void update(float dt, char map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, sf::View& view1);

    sf::Vector2f getPosition() { return shape.getPosition(); }
};