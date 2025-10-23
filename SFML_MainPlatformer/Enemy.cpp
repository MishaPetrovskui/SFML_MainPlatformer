#include "Enemy.h"
#include <cmath>

static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    // совместимо с текущей структурой sf::FloatRect (position/size)
    return (a.position.x < b.position.x + b.size.x) && (a.position.x + a.size.x > b.position.x) &&
        (a.position.y < b.position.y + b.size.y) && (a.position.y + a.size.y > b.position.y);
}

Enemy::Enemy(float x, float y, sf::Texture& texture, float tileSize)
    : sprite(texture)
{
    sprite.setPosition({ x, y });
    spawnPos = { x, y };
    // чуть больше радиус обнаружения
    detectionRange = tileSize * 8.f; // было 6, увеличено до 8
    verticalDetectRange = tileSize * 3.f;
    patrolRange = tileSize * 3.f;
    speed = 80.f + (std::rand() % 40);
    maxHp = 80; // чуть больше хп
    hp = maxHp;
    state = EnemyState::Idle;
    contactCooldown = 0.8f;
    contactTimer = 0.f;
    contactDamage = 10;
}

void Enemy::update(float dt, const sf::Vector2f& playerPos) {
    if (state == EnemyState::Dead) return;

    sf::Vector2f pos = sprite.getPosition();
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

    sf::CircleShape rangeCircle;
    rangeCircle.setRadius(detectionRange);
    rangeCircle.setOrigin({ (float)detectionRange, (float)detectionRange });
    rangeCircle.setPosition(center);
    rangeCircle.setFillColor(sf::Color(255, 0, 0, 40));
    rangeCircle.setOutlineColor(sf::Color(255, 0, 0, 90));
    rangeCircle.setOutlineThickness(1.f);
    window.draw(rangeCircle);

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