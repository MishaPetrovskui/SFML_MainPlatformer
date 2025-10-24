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
    float detectionRange;
    float verticalDetectRange;
    int hp;
    int maxHp;
    EnemyState state;
    float contactCooldown;
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
    int checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt);
};