#include "Enemy.h"
#include <cmath>

static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    return (a.position.x < b.position.x + b.size.x) && (a.position.x + a.size.x > b.position.x) &&
        (a.position.y < b.position.y + b.size.y) && (a.position.y + a.size.y > b.position.y);
}

static bool isSolidTile(int tile) {
    if (tile == -1) return false;
    if (tile == 5 || tile == 7) return false;
    if (tile >= 16 && tile <= 21) return false;
    return true;
}

Enemy::Enemy(float x, float y, sf::Texture& texture, float tileSize)
    : sprite(texture)
{
    sprite.setPosition({ x, y });
    spawnPos = { x, y };
    detectionRange = tileSize * 8.f;
    verticalDetectRange = tileSize * 3.f;
    patrolRange = tileSize * 3.f;
    speed = 80.f + (std::rand() % 40);
    maxHp = 80;
    hp = maxHp;
    state = EnemyState::Idle;
    contactCooldown = 0.8f;
    contactTimer = 0.f;
    contactDamage = 10;

    gravity = 900.f; 
    velocityY = 0.f;
}

void Enemy::update(float dt, const sf::Vector2f& playerPos, int map[][501], int mapWidth, int mapHeight, float tileSize) {
    if (state == EnemyState::Dead) {
        return;
    }

    sf::Vector2f pos = sprite.getPosition();

    velocityY += gravity * dt;
    float spriteH = sprite.getGlobalBounds().size.y;
    float tentativeY = pos.y + velocityY * dt;
    int leftTile = std::max(0, (int)std::floor(pos.x / tileSize));
    int rightTile = std::min(mapWidth - 1, (int)std::floor((pos.x + sprite.getGlobalBounds().size.x - 1.f) / tileSize));
    int footTile = (int)std::floor((tentativeY + spriteH) / tileSize);

    bool landed = false;
    if (footTile >= 0 && footTile < mapHeight) {
        for (int tx = leftTile; tx <= rightTile; ++tx) {
            if (isSolidTile(map[footTile][tx])) {
                float tileTop = footTile * tileSize;
                if (tentativeY + spriteH >= tileTop) {
                    pos.y = tileTop - spriteH;
                    velocityY = 0.f;
                    landed = true;
                    break;
                }
            }
        }
    }

    if (!landed) {
        pos.y = tentativeY;
    }

    float mapBottomPx = mapHeight * tileSize;
    const float FALL_RESPAWN_THRESHOLD = 200.f;
    if (pos.y > mapBottomPx + FALL_RESPAWN_THRESHOLD || pos.y > spawnPos.y + FALL_RESPAWN_THRESHOLD) {
        pos = spawnPos;
        velocityY = 0.f;
        hp = maxHp;
        state = EnemyState::Idle;
        contactTimer = 0.f;
        sprite.setPosition(pos);
        return;
    }

    float dx = playerPos.x - pos.x;
    float dy = playerPos.y - pos.y;

    bool playerInRange = (std::abs(dx) <= detectionRange) && (std::abs(dy) <= verticalDetectRange);

    if (playerInRange) {
        state = EnemyState::Chasing;
    }
    else {
        if (state == EnemyState::Chasing)
            state = EnemyState::Returning;
    }

    if (state == EnemyState::Chasing) {
        if (dx < -2.f) pos.x -= speed * dt;
        else if (dx > 2.f) pos.x += speed * dt;
    }
    else if (state == EnemyState::Returning) {
        float sx = spawnPos.x;
        if (std::abs(pos.x - sx) > 2.f) {
            if (pos.x < sx) pos.x += speed * dt;
            else pos.x -= speed * dt;
        }
        else {
            pos.x = sx;
            state = EnemyState::Idle;
        }
    }

    sprite.setPosition(pos);

    if (hp <= 0) state = EnemyState::Dead;
    if (contactTimer > 0.f) contactTimer -= dt;
}

void Enemy::draw(sf::RenderWindow& window) {
    if (state == EnemyState::Dead) return;

    sf::FloatRect b = sprite.getGlobalBounds();
    sf::Vector2f center = { b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f };

    window.draw(sprite);

    float barW = std::max(20.f, b.size.x);
    float barH = 6.f;
    sf::RectangleShape bg({ barW, barH });
    bg.setFillColor(sf::Color(50, 50, 50, 200));
    bg.setPosition({ float(b.position.x + (b.size.x - barW) * 0.5f), float(b.position.y - 12.f) });
    window.draw(bg);

    float perc = (float)hp / (float)maxHp;
    if (perc < 0.f) perc = 0.f;
    sf::RectangleShape fg({ barW * perc, barH });
    fg.setFillColor(sf::Color::Green);
    fg.setPosition(bg.getPosition());
    window.draw(fg);
}

void Enemy::takeDamage(int dmg) {
    hp -= dmg;
    if (hp <= 0) {
        hp = 0;
        state = EnemyState::Dead;
    }
}

int Enemy::checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt) {
    if (state == EnemyState::Dead) return 0;
    if (contactTimer > 0.f) {
        contactTimer -= dt;
        return 0;
    }
    sf::FloatRect enemyBounds = sprite.getGlobalBounds();
    if (rectsIntersect(playerBounds, enemyBounds)) {
        contactTimer = contactCooldown;
        return contactDamage;
    }
    return 0;
}