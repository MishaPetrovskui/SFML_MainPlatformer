#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <string>

enum class EnemyState {
    Idle,
    Chasing,
    Returning,
    Dead
};

class Enemy {
private:
    sf::RectangleShape hitbox;
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

    int   lastMapWidth = 0;
    int   lastMapHeight = 0;
    float lastTileSize = 32.f;

    struct Animation {
        sf::Texture texture;
        std::vector<sf::IntRect> frames;
    };
    std::map<std::string, Animation> animations;
    std::string currentAnimation;
    int   animFrame = 0;
    float animTimer = 0.f;
    float animSpeed = 0.12f;
    bool  attackPlaying = false;
    bool  attackHitDealt = false;
    int   pendingDamage = 0;

    void setAnimation(const std::string& name);
    void updateAnimation(float dt);

public:
    Enemy(float x, float y, sf::Texture& fallbackTexture, float tileSize);

    void loadAnimations(const std::string& walkPath,
        const std::string& attackPath);

    void update(float dt, const sf::Vector2f& playerPos,
        int map[][501], int mapWidth, int mapHeight, float tileSize);
    void draw(sf::RenderWindow& window, bool debugMode = false);
    void takeDamage(int dmg);

    bool isAlive() const { return state != EnemyState::Dead; }
    sf::FloatRect getBounds() const { return hitbox.getGlobalBounds(); }
    int  checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt);
    sf::Vector2f getPosition() const { return hitbox.getPosition(); }
    bool hasLineOfSight(const sf::Vector2f& playerPos,
        int map[][501], int mapWidth, int mapHeight, float tileSize) const;
};