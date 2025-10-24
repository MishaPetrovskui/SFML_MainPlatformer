#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <map>
#include <vector>

class Enemy;

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
    int hp;
    int maxHp;
    int coins;
    float attackCooldown;
    float attackTimer;
    int attackDamage;
    float lavaDamageAccum;
    float spikeInvulTimer;
    float spikeInvulDuration;
    int spikeDamage;
    bool wasOnSpike;
    sf::Keyboard::Key attackKey;

    bool checkWallContact(int map[][501], int mapWidth, int mapHeight, float tileSize);

public:
    Player(float startX = 50.f, float startY = 50.f);
    void update(float dt, int map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window,
        int mobMap[][501], int interestingMap[][501], int backgroundMap[][501], std::vector<Enemy>& enemies);
    void draw(sf::RenderWindow& window, sf::View& view1, sf::Font& font);
    sf::Vector2f getPosition() { return shape.getPosition(); }
    void reset();
    bool isAlive() { return hp > 0; }
    void applyDamage(int dmg) { hp -= dmg; if (hp < 0) hp = 0; }
    int getHP() const { return hp; }
    int getCoins() const { return coins; }
    float getStamina() const { return stamina; }
    float getMaxStamina() const { return maxStamina; }
    void setAttackKey(sf::Keyboard::Key k) { attackKey = k; }
    sf::Keyboard::Key getAttackKey() const { return attackKey; }
};