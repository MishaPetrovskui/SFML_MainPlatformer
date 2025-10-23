#pragma once
#include <SFML/Graphics.hpp>

enum class EnemyState {
    Idle,
    Chasing,
    Returning,
    Dead
};

class Enemy {
private:
    sf::Sprite sprite;
    sf::Vector2f spawnPos;
    float speed;
    float detectionRange; // pixels
    float verticalDetectRange; // pixels
    int hp;
    int maxHp;
    EnemyState state;
    float contactCooldown; // seconds between damage ticks to player
    float contactTimer;
    int contactDamage;
    float patrolRange;

public:
    Enemy(float x, float y, sf::Texture& texture, float tileSize);
    void update(float dt, const sf::Vector2f& playerPos);
    void draw(sf::RenderWindow& window);
    void takeDamage(int dmg);
    bool isAlive() const { return state != EnemyState::Dead; }
    sf::FloatRect getBounds() const { return sprite.getGlobalBounds(); }

    // returns damage to apply to player this frame (0 if none)
    int checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt);
};