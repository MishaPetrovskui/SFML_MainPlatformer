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
    bool isSliding;
    int wallDirection;
    float stamina;
    float maxStamina;
    float staminaConsumption;
    float staminaRegenRate;
    sf::Vector2f spawnPoint;
    bool isDashing;
    float dashSpeed;
    float dashDuration;
    float dashTimer;
    float dashCooldown;
    float dashCooldownTimer;
    sf::Vector2f dashDirection;
    int lives = 1;
    bool checkWallContact(char map[][501], int mapWidth, int mapHeight, float tileSize);
public:
    Player(float startX = 50.f, float startY = 50.f);

    void update(float dt, char map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window);
    void draw(sf::RenderWindow& window, sf::View& view1);
    sf::Vector2f getPosition() { return shape.getPosition(); }
    void reset();
    bool isAlive() { return lives > 0; }
};