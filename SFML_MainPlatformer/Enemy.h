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

    // ── Свет (динамический, зависит от мап-криейтора) ─────────────────────────
    // lightEmit > 0 означает, что враг излучает свет (например, слизь)
    float lightEmit = 0.f;

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

    // ── Инициализация из данных мап-криейтора ─────────────────────────────────
    // Вызывать сразу после конструктора, когда спауниш врага из MapEntity.
    //   hp_val    — MapEntity::health  (0 → оставить дефолт 80)
    //   dmg_val   — MapEntity::damage  (0 → оставить дефолт 10)
    //   spd_val   — MapEntity::speed   (0 → оставить дефолт)
    //   light_val — MapEntity::light   (0..1, 0 → без свечения)
    void initFromMapEntity(int hp_val, int dmg_val, float spd_val, float light_val);

    void loadAnimations(const std::string& walkPath,
        const std::string& attackPath);

    void update(float dt, const sf::Vector2f& playerPos,
        int map[][501], int mapWidth, int mapHeight, float tileSize);
    void draw(sf::RenderWindow& window, bool debugMode = false, float lightLevel = 1.f);
    void takeDamage(int dmg);

    bool isAlive() const { return state != EnemyState::Dead; }
    sf::FloatRect getBounds() const { return hitbox.getGlobalBounds(); }
    int  checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt);
    sf::Vector2f getPosition() const { return hitbox.getPosition(); }

    // Центр хитбокса (удобно для расчёта позиции источника света)
    sf::Vector2f getCenter() const {
        sf::Vector2f p = hitbox.getPosition();
        sf::Vector2f s = hitbox.getSize();
        return { p.x + s.x * 0.5f, p.y + s.y * 0.5f };
    }

    // 0 = не светит; значение из мап-криейтора (0..1)
    float getLightEmit() const { return lightEmit; }

    // Радиус источника в пикселях мира (масштабируется из lightEmit)
    float getLightRadius() const { return lightEmit * 320.f; }

    // Цвет свечения слизи — зеленоватый
    sf::Color getLightColor() const { return sf::Color(120, 230, 100, 255); }

    bool hasLineOfSight(const sf::Vector2f& playerPos,
        int map[][501], int mapWidth, int mapHeight, float tileSize) const;
};