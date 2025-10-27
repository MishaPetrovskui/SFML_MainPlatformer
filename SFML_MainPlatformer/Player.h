#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <map>
#include <vector>

using namespace sf;
using namespace std;
class Enemy;

class Player {
private:
    sf::RectangleShape shape;
    sf::Sprite sprite;
    sf::Texture texture;
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
    string currentAnimation;
    int animationFrame;
    float animationTimer;
    float animationSpeed;
    bool facingRight;
    bool hasKey;
    bool deathAnimationFinished;

    // Новый функционал для ограничения даша
    bool limitedDashMode; // если true — только один даш в воздухе, восстанавливается при касании земли
    bool dashAvailable;   // доступен ли даш (для limitedDashMode)

    map<string, vector<IntRect>> animation = {
        {
            "idle", {
                IntRect({7, 4}, {16, 28}),
                IntRect({39, 4}, {16, 28}),
            }
        },
        {
            "walkToRight", {
                IntRect({6, 103}, {17, 25}),
                IntRect({39, 102}, {16, 26}),
                IntRect({71, 100}, {16, 28}),
                IntRect({103, 101}, {16, 27}),
                IntRect({103, 101}, {16, 27}),
                IntRect({167, 102}, {16, 26}),
                IntRect({199, 100}, {16, 28}),
                IntRect({231, 101}, {16, 27}),
            }
        },
        {
            "walkToLeft", {
                IntRect({6, 103}, {17, 25}),
                IntRect({39, 102}, {16, 26}),
                IntRect({71, 100}, {16, 28}),
                IntRect({103, 101}, {16, 27}),
                IntRect({103, 101}, {16, 27}),
                IntRect({167, 102}, {16, 26}),
                IntRect({199, 100}, {16, 28}),
                IntRect({231, 101}, {16, 27}),
            }
        },
        {
            "Death",{
                IntRect({7, 228}, {16, 28}),
                IntRect({37, 229}, {18, 27}),
                IntRect({70, 231}, {17, 25}),
                IntRect({103, 233}, {17, 24}),
                IntRect({135, 239}, {21, 17}),
                IntRect({162, 244}, {29, 12}),
                IntRect({194, 245}, {29, 11}),
                IntRect({226, 245}, {29, 11}),
            }
        },
        {
            "Attack",{
                IntRect({102, 257}, {19, 31}),
                IntRect({134, 257}, {16, 31}),
                IntRect({166, 264}, {19, 24}),
                IntRect({197, 262}, {18, 26}),
                IntRect({231, 260}, {16, 28}),
            }
        },
        {
             "Jump", {
                  IntRect({7, 164}, {16, 28}),
                  IntRect({38, 164}, {18, 28}),
                  IntRect({70, 162}, {18, 28}),
                  IntRect({102, 161}, {18, 28}),
                  IntRect({134, 161}, {19, 28}),
                  IntRect({166, 164}, {19, 28}),
                  IntRect({197, 164}, {19, 28}),
                  IntRect({230, 164}, {18, 28}),
             }
        },
    };

    bool checkWallContact(int map[][501], int mapWidth, int mapHeight, float tileSize);
    void updateAnimation(float dt);
    void setAnimation(const string& animName);

public:
    Player(Texture, float startX = 50.f, float startY = 50.f);
    void update(float dt, int map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window,
        int mobMap[][501], int interestingMap[][501], int backgroundMap[][501], std::vector<Enemy>& enemies);
    void draw(sf::RenderWindow& window, sf::View& view1, sf::Font& font);
    sf::Vector2f getPosition() const { return shape.getPosition(); }
    int getPositionX() const { return static_cast<int>(shape.getPosition().x); }
    int getPositionY() const { return static_cast<int>(shape.getPosition().y); }
    void reset();
    bool isAlive() const { return hp > 0; }
    void applyDamage(int dmg) { hp -= dmg; if (hp < 0) hp = 0; }
    int getHP() const { return hp; }
    int getCoins() const { return coins; }
    float getStamina() const { return stamina; }
    float getMaxStamina() const { return maxStamina; }
    void setAttackKey(sf::Keyboard::Key k) { attackKey = k; }
    sf::Keyboard::Key getAttackKey() const { return attackKey; }
    bool hasFinishedDeathAnimation() const { return deathAnimationFinished; }
    bool getHasKey() const { return hasKey; }
    void setSpawnPoint(float x, float y);

    // управление режимом даша
    void setLimitedDashMode(bool v) { limitedDashMode = v; if (!limitedDashMode) dashAvailable = true; }
    bool getLimitedDashMode() const { return limitedDashMode; }
};