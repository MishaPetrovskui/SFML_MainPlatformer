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
private:
    sf::Sprite sprite;
    sf::Vector2f spawnPos;
    float speed;
    float detectionRange;
    float verticalDetectRange;
    float closeDetectRadius;
    int hp;
    int maxHp;
    EnemyState state;
    float contactCooldown;
    float contactTimer;
    int contactDamage;
    float patrolRange;
    float gravity;
    float velocityY;
    int facingDir = 1;
    float visionAngleCos;
    float patrolTimer;
    float patrolWaitTime;
    float patrolWalkTime;
    float patrolPhaseTimer;
    bool  patrolWalking;
    float patrolDir;
    float lookTimer;
    float lookInterval;
    bool  isLooking;
    float lookDuration;

    bool onGround;
public:
    Enemy(float x, float y, sf::Texture& texture, float tileSize);
    void update(float dt, const sf::Vector2f& playerPos, int map[][501], int mapWidth, int mapHeight, float tileSize);
    void draw(sf::RenderWindow& window, bool debugMode = false);
    void takeDamage(int dmg);
    bool isAlive() const { return state != EnemyState::Dead; }
    sf::FloatRect getBounds() const { return sprite.getGlobalBounds(); }
    int checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt);
    sf::Vector2f getPosition() const { return sprite.getPosition(); }
    bool hasLineOfSight(const sf::Vector2f& playerPos, int map[][501], int mapWidth, int mapHeight, float tileSize) const;
};