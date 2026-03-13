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
    sf::Texture texture;       // legacy / fallback single-sheet
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
    bool attackHitDealt = false;  // true after hit frame processed
    float lavaDamageAccum;
    float spikeInvulTimer;
    float spikeInvulDuration;
    int spikeDamage;
    bool wasOnSpike;
    float damageFlashTimer = 0.f;
    float hitFreezeTimer = 0.f;
    static constexpr float DAMAGE_FLASH_DURATION = 0.25f;
    static constexpr float HIT_FREEZE_DURATION = 0.15f;
    sf::Keyboard::Key attackKey;
    sf::Vector2f attackDir = { 1.f, 0.f };
    string currentAnimation;
    int animationFrame;
    float animationTimer;
    float animationSpeed;
    bool facingRight;
    bool hasKey;
    bool deathAnimationFinished;

    bool limitedDashMode;
    bool dashAvailable;
    bool  jumpHeld;
    float coyoteTimer;
    float jumpBufferTimer;
    float wallJumpLockTimer;
    static constexpr float COYOTE_TIME = 0.12f;
    static constexpr float JUMP_BUFFER_TIME = 0.15f;
    static constexpr float WALL_JUMP_LOCK_TIME = 0.22f;
    static constexpr float JUMP_VELOCITY = 420.f;
    static constexpr float JUMP_CUT_VELOCITY = 170.f;
    static constexpr float LEDGE_FORGIVENESS = 5.f;
    // Новая система анимаций — каждая анимация хранит свою текстуру и фреймы
    struct Animation {
        sf::Texture texture;
        std::vector<sf::IntRect> frames;
    };
    map<string, Animation> animations;

    bool checkWallContact(int map[][501], int mapWidth, int mapHeight, float tileSize);
    void updateAnimation(float dt);
    void setAnimation(const string& animName);
    sf::FloatRect getInnerBounds() const;

public:
    Player(Texture, float startX = 50.f, float startY = 50.f);
    void loadAnimationSheets(const std::string& walkPath,
        const std::string& attackPath,
        const std::string& idlePath);
    void update(float dt, int map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window,
        int mobMap[][501], int interestingMap[][501], int backgroundMap[][501], std::vector<Enemy>& enemies);
    void draw(sf::RenderWindow& window, sf::View& view1, sf::Font& font, bool debugMode = false, bool pauseMode = false);
    sf::Vector2f getPosition() const { return shape.getPosition(); }
    int getPositionX() const { return static_cast<int>(shape.getPosition().x); }
    int getPositionY() const { return static_cast<int>(shape.getPosition().y); }
    void reset();
    bool isAlive() const { return hp > 0; }
    void applyDamage(int dmg) { hp -= dmg; if (hp < 0) hp = 0; damageFlashTimer = DAMAGE_FLASH_DURATION; }
    int getHP() const { return hp; }
    int getCoins() const { return coins; }
    float getStamina() const { return stamina; }
    float getMaxStamina() const { return maxStamina; }
    void setAttackKey(sf::Keyboard::Key k) { attackKey = k; }
    sf::Keyboard::Key getAttackKey() const { return attackKey; }
    bool hasFinishedDeathAnimation() const { return deathAnimationFinished; }
    bool getHasKey() const { return hasKey; }
    void setSpawnPoint(float x, float y);
    void setLimitedDashMode(bool v) { limitedDashMode = v; if (!limitedDashMode) dashAvailable = true; }
    bool getLimitedDashMode() const { return limitedDashMode; }
};