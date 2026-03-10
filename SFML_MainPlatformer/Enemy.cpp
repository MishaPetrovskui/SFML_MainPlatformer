#include "Enemy.h"
#include <cmath>
#include <cstdlib>

static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    return (a.position.x < b.position.x + b.size.x) && (a.position.x + a.size.x > b.position.x) &&
        (a.position.y < b.position.y + b.size.y) && (a.position.y + a.size.y > b.position.y);
}

static bool isSolidTile(int tile) {
    if (tile == -1 || tile == 0 || tile == 5 || tile == 7) return false;
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
    closeDetectRadius = tileSize * 1.8f;
    patrolRange = tileSize * 3.f;
    speed = 60.f + (std::rand() % 30);
    maxHp = 80; hp = maxHp;
    state = EnemyState::Idle;
    contactCooldown = 0.8f; contactTimer = 0.f;
    contactDamage = 10;
    gravity = 900.f; velocityY = 0.f;
    onGround = false;
    facingDir = 1;
    visionAngleCos = std::cos(55.f * 3.14159265f / 180.f);

    // Патруль
    patrolTimer = 0.f;
    patrolWaitTime = 1.5f + (std::rand() % 100) / 50.f;  // 1.5–3.5s стоим
    patrolWalkTime = 1.0f + (std::rand() % 100) / 50.f;  // 1–3s идём
    patrolPhaseTimer = patrolWalkTime;
    patrolWalking = true;
    patrolDir = (std::rand() % 2 == 0) ? 1.f : -1.f;

    // Осмотр
    lookTimer = 2.f + (std::rand() % 100) / 33.f;
    lookInterval = 3.f + (std::rand() % 100) / 25.f;
    isLooking = false;
    lookDuration = 0.6f;
}

bool Enemy::hasLineOfSight(const sf::Vector2f& playerPos,
    int map[][501], int mapWidth, int mapHeight, float tileSize) const
{
    sf::Vector2f start = sprite.getPosition();
    start.x += sprite.getGlobalBounds().size.x * 0.5f;
    start.y += sprite.getGlobalBounds().size.y * 0.5f;

    sf::Vector2f dir = playerPos - start;
    float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (dist < 1.f) return true;

    int steps = static_cast<int>(dist / (tileSize * 0.45f)) + 2;
    sf::Vector2f step = dir / static_cast<float>(steps);

    for (int i = 1; i < steps; ++i) {
        sf::Vector2f p = start + step * static_cast<float>(i);
        int tx = static_cast<int>(p.x / tileSize);
        int ty = static_cast<int>(p.y / tileSize);
        if (tx < 0 || tx >= mapWidth || ty < 0 || ty >= mapHeight) continue;
        if (isSolidTile(map[ty][tx])) return false;
    }
    return true;
}

void Enemy::update(float dt, const sf::Vector2f& playerPos,
    int map[][501], int mapWidth, int mapHeight, float tileSize)
{
    if (state == EnemyState::Dead) return;

    sf::Vector2f pos = sprite.getPosition();
    float spriteW = sprite.getGlobalBounds().size.x;
    float spriteH = sprite.getGlobalBounds().size.y;

    // ── Гравитация ──────────────────────────────────────────────
    velocityY += gravity * dt;
    float tentativeY = pos.y + velocityY * dt;
    int leftTile = std::max(0, (int)std::floor(pos.x / tileSize));
    int rightTile = std::min(mapWidth - 1, (int)std::floor((pos.x + spriteW - 1.f) / tileSize));
    int footTile = (int)std::floor((tentativeY + spriteH) / tileSize);

    onGround = false;
    if (footTile >= 0 && footTile < mapHeight) {
        for (int tx = leftTile; tx <= rightTile; ++tx) {
            if (isSolidTile(map[footTile][tx])) {
                float tileTop = footTile * tileSize;
                if (tentativeY + spriteH >= tileTop) {
                    pos.y = tileTop - spriteH;
                    velocityY = 0.f;
                    onGround = true;
                    break;
                }
            }
        }
    }
    if (!onGround) pos.y = tentativeY;

    // Респавн при падении
    if (pos.y > spawnPos.y + tileSize * 6.f) {
        pos = spawnPos; velocityY = 0.f;
        hp = maxHp; state = EnemyState::Idle; contactTimer = 0.f;
        sprite.setPosition(pos); return;
    }

    // ── Определение видимости игрока ─────────────────────────────
    float dx = playerPos.x - (pos.x + spriteW * 0.5f);
    float dy = playerPos.y - (pos.y + spriteH * 0.5f);
    float dist = std::sqrt(dx * dx + dy * dy);

    bool playerDetected = false;

    // 1) Ближняя зона — всегда замечает
    if (dist <= closeDetectRadius) {
        playerDetected = true;
    }
    // 2) Конус зрения
    else if (dist <= detectionRange && std::abs(dy) <= verticalDetectRange) {
        float ndx = dx / dist;
        float dot = ndx * (float)facingDir;
        if (dot >= visionAngleCos)
            playerDetected = hasLineOfSight(playerPos, map, mapWidth, mapHeight, tileSize);
    }

    // ── Переходы состояний ───────────────────────────────────────
    if (playerDetected) {
        state = EnemyState::Chasing;
        isLooking = false;
    }
    else {
        if (state == EnemyState::Chasing)
            state = EnemyState::Returning;
    }

    // ── Поведение по состоянию ───────────────────────────────────
    float moveX = 0.f;

    if (state == EnemyState::Chasing) {
        if (dx < -2.f) { moveX = -speed; facingDir = -1; }
        else if (dx > 2.f) { moveX = speed; facingDir = 1; }
    }
    else if (state == EnemyState::Returning) {
        float sx = spawnPos.x;
        if (std::abs(pos.x - sx) > 3.f) {
            moveX = (pos.x < sx) ? speed : -speed;
            facingDir = (moveX > 0.f) ? 1 : -1;
        }
        else {
            pos.x = sx;
            state = EnemyState::Idle;
        }
    }
    else if (state == EnemyState::Idle) {
        // Осмотр по сторонам
        lookTimer -= dt;
        if (lookTimer <= 0.f && !isLooking) {
            isLooking = true;
            lookTimer = lookDuration;
            facingDir = -facingDir;   // поворот
        }
        if (isLooking) {
            lookTimer -= dt;
            if (lookTimer <= 0.f) {
                isLooking = false;
                lookTimer = lookInterval;
            }
        }

        // Патруль
        if (!isLooking) {
            patrolPhaseTimer -= dt;
            if (patrolPhaseTimer <= 0.f) {
                patrolWalking = !patrolWalking;
                patrolPhaseTimer = patrolWalking ? patrolWalkTime : patrolWaitTime;
                if (patrolWalking) {
                    patrolDir = -patrolDir;
                    facingDir = (patrolDir > 0.f) ? 1 : -1;
                }
            }

            if (patrolWalking) {
                // Не уходим дальше patrolRange от спавна
                float distFromSpawn = pos.x - spawnPos.x;
                if ((patrolDir > 0.f && distFromSpawn < patrolRange) ||
                    (patrolDir < 0.f && distFromSpawn > -patrolRange)) {
                    moveX = patrolDir * speed * 0.6f;
                    facingDir = (moveX > 0.f) ? 1 : -1;
                }
                else {
                    patrolDir = -patrolDir;
                    facingDir = (patrolDir > 0.f) ? 1 : -1;
                    patrolPhaseTimer = patrolWaitTime;
                    patrolWalking = false;
                }

                // Не шагаем в пропасть
                int nextTileX = (int)std::floor((pos.x + spriteW * 0.5f + moveX * dt + patrolDir * spriteW * 0.5f) / tileSize);
                int belowTile = (int)std::floor((pos.y + spriteH + 2.f) / tileSize);
                if (belowTile >= 0 && belowTile < mapHeight &&
                    nextTileX >= 0 && nextTileX < mapWidth) {
                    if (!isSolidTile(map[belowTile][nextTileX])) {
                        moveX = 0.f;
                        patrolDir = -patrolDir;
                        facingDir = (patrolDir > 0.f) ? 1 : -1;
                        patrolWalking = false;
                        patrolPhaseTimer = patrolWaitTime;
                    }
                }
            }
        }
    }

    // ── Горизонтальное движение + коллизия стен ──────────────────
    if (moveX != 0.f) {
        float nextX = pos.x + moveX * dt;
        int checkTileX = (moveX > 0.f)
            ? (int)std::floor((nextX + spriteW) / tileSize)
            : (int)std::floor(nextX / tileSize);
        int midTileY = (int)std::floor((pos.y + spriteH * 0.5f) / tileSize);

        bool wallHit = false;
        if (checkTileX >= 0 && checkTileX < mapWidth &&
            midTileY >= 0 && midTileY < mapHeight) {
            if (isSolidTile(map[midTileY][checkTileX])) wallHit = true;
        }

        if (!wallHit) pos.x = nextX;
        else if (state == EnemyState::Idle) {
            patrolDir = -patrolDir;
            facingDir = (patrolDir > 0.f) ? 1 : -1;
        }
    }

    sprite.setPosition(pos);
    if (hp <= 0) state = EnemyState::Dead;
    if (contactTimer > 0.f) contactTimer -= dt;
}

void Enemy::draw(sf::RenderWindow& window, bool debugMode) {
    if (state == EnemyState::Dead) return;

    sf::FloatRect b = sprite.getGlobalBounds();
    window.draw(sprite);

    // HP бар
    float barW = std::max(20.f, b.size.x);
    sf::RectangleShape bg({ barW, 6.f });
    bg.setFillColor(sf::Color(50, 50, 50, 200));
    bg.setPosition({ b.position.x + (b.size.x - barW) * 0.5f, b.position.y - 12.f });
    window.draw(bg);

    float perc = std::max(0.f, (float)hp / (float)maxHp);
    sf::RectangleShape fg({ barW * perc, 6.f });
    fg.setFillColor(sf::Color::Green);
    fg.setPosition(bg.getPosition());
    window.draw(fg);

    if (!debugMode) return;

    sf::Vector2f center = {
        b.position.x + b.size.x * 0.5f,
        b.position.y + b.size.y * 0.5f
    };

    // Конус зрения
    const int   SEG = 24;
    const float halfAngle = std::acos(visionAngleCos);
    float baseAngle = (facingDir > 0) ? 0.f : 3.14159265f;

    sf::VertexArray cone(sf::PrimitiveType::TriangleFan, SEG + 2);
    cone[0].position = center;
    cone[0].color = sf::Color(255, 255, 0, 50);
    for (int i = 0; i <= SEG; ++i) {
        float a = baseAngle - halfAngle + (2.f * halfAngle * i / SEG);
        cone[i + 1].position = { center.x + std::cos(a) * detectionRange,
                                  center.y + std::sin(a) * detectionRange };
        cone[i + 1].color = sf::Color(255, 255, 0, 10);
    }
    window.draw(cone);

    // Линии границ конуса
    sf::VertexArray coneLines(sf::PrimitiveType::Lines, 4);
    coneLines[0] = { center, sf::Color(255, 255, 0, 180) };
    coneLines[1] = { { center.x + std::cos(baseAngle - halfAngle) * detectionRange,
                        center.y + std::sin(baseAngle - halfAngle) * detectionRange },
                      sf::Color(255, 255, 0, 60) };
    coneLines[2] = { center, sf::Color(255, 255, 0, 180) };
    coneLines[3] = { { center.x + std::cos(baseAngle + halfAngle) * detectionRange,
                        center.y + std::sin(baseAngle + halfAngle) * detectionRange },
                      sf::Color(255, 255, 0, 60) };
    window.draw(coneLines);

    // Ближняя зона (круг)
    const int CR = 20;
    sf::VertexArray circle(sf::PrimitiveType::TriangleFan, CR + 2);
    circle[0].position = center;
    circle[0].color = sf::Color(255, 80, 80, 60);
    for (int i = 0; i <= CR; ++i) {
        float a = 2.f * 3.14159265f * i / CR;
        circle[i + 1].position = { center.x + std::cos(a) * closeDetectRadius,
                                    center.y + std::sin(a) * closeDetectRadius };
        circle[i + 1].color = sf::Color(255, 80, 80, 20);
    }
    window.draw(circle);

    // Хитбокс
    sf::RectangleShape dbgBox({ b.size.x, b.size.y });
    dbgBox.setPosition(b.position);
    dbgBox.setFillColor(sf::Color::Transparent);
    dbgBox.setOutlineColor(sf::Color::Red);
    dbgBox.setOutlineThickness(1.f);
    window.draw(dbgBox);

    // Точка спавна
    sf::CircleShape spawnDot(4.f);
    spawnDot.setFillColor(sf::Color(0, 200, 255, 180));
    spawnDot.setPosition({ spawnPos.x - 4.f, spawnPos.y - 4.f });
    window.draw(spawnDot);
}

void Enemy::takeDamage(int dmg) {
    hp -= dmg;
    if (hp <= 0) { hp = 0; state = EnemyState::Dead; }
}

int Enemy::checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt) {
    if (state == EnemyState::Dead || contactTimer > 0.f) return 0;
    if (rectsIntersect(playerBounds, sprite.getGlobalBounds())) {
        contactTimer = contactCooldown;
        return contactDamage;
    }
    return 0;
}