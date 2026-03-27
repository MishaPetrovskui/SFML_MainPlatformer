#include "Enemy.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <algorithm>


static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    return (a.position.x < b.position.x + b.size.x) &&
        (a.position.x + a.size.x > b.position.x) &&
        (a.position.y < b.position.y + b.size.y) &&
        (a.position.y + a.size.y > b.position.y);
}

static bool isSolidTile(int tile) {
    if (tile == -1 || tile == 0 || tile == 5 || tile == 7) return false;
    if (tile >= 16 && tile <= 21) return false;
    return true;
}

Enemy::Enemy(float x, float y, sf::Texture& fallbackTexture, float tileSize)
    : sprite(fallbackTexture)
{
    hitbox.setSize({ tileSize, tileSize });
    hitbox.setPosition({ x, y });
    hitbox.setFillColor(sf::Color::Transparent);

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

    patrolWaitTime = 1.5f + (std::rand() % 100) / 50.f;
    patrolWalkTime = 1.0f + (std::rand() % 100) / 50.f;
    patrolPhaseTimer = patrolWalkTime;
    patrolWalking = true;
    patrolDir = (std::rand() % 2 == 0) ? 1.f : -1.f;

    lookTimer = 2.f + (std::rand() % 100) / 33.f;
    lookInterval = 3.f + (std::rand() % 100) / 25.f;
    isLooking = false;
    lookDuration = 0.6f;

    currentAnimation = "idle";
    animFrame = 0; animTimer = 0.f; animSpeed = 0.12f;
    attackPlaying = false;
}


void Enemy::loadAnimations(const std::string& walkPath,
    const std::string& attackPath)
{
    {
        Animation& a = animations["walkToRight"];
        if (!a.texture.loadFromFile(walkPath))
            std::cout << "[Enemy] WARN: cannot load " << walkPath << "\n";

        a.frames.clear();
        int fw = 64;
        int fh = (int)a.texture.getSize().y;
        int cnt = (fw > 0 && fh > 0) ? (int)a.texture.getSize().x / fw : 0;
        if (cnt < 1) cnt = 1;
        for (int i = 0; i < cnt; ++i)
            a.frames.push_back(sf::IntRect({ i * fw, 0 }, { fw, fh }));
        std::cout << "[Enemy] walk frames: " << cnt << " (" << fw << "x" << fh << ")\n";
    }

    {
        Animation& a = animations["attack"];
        if (!a.texture.loadFromFile(attackPath))
            std::cout << "[Enemy] WARN: cannot load " << attackPath << "\n";
        int fw = 64;
        int fh = (int)a.texture.getSize().y;
        int cnt = (fw > 0 && fh > 0) ? (int)a.texture.getSize().x / fw : 0;
        if (cnt < 1) cnt = 1;
        for (int i = 0; i < cnt; ++i)
            a.frames.push_back(sf::IntRect({ i * fw, 0 }, { fw, fh }));
        std::cout << "[Enemy] attack frames: " << cnt << " (" << fw << "x" << fh << ")\n";
    }

    {
        Animation& walkAnim = animations["walkToRight"];
        Animation& a = animations["idle"];
        a.texture = walkAnim.texture;
        int idleCnt = std::min((int)walkAnim.frames.size(), 2);
        for (int i = 0; i < idleCnt; ++i)
            a.frames.push_back(walkAnim.frames[i]);
    }

    setAnimation("idle");
}


void Enemy::setAnimation(const std::string& name) {
    if (currentAnimation == name) return;
    if (animations.find(name) == animations.end()) return;

    currentAnimation = name;
    animFrame = 0;
    animTimer = 0.f;
    if (name == "attack") { attackPlaying = true; attackHitDealt = false; }

    Animation& anim = animations[name];
    sprite.setTexture(anim.texture, true);
    if (!anim.frames.empty())
        sprite.setTextureRect(anim.frames[0]);
}


void Enemy::updateAnimation(float dt) {
    if (animations.empty()) return;
    auto it = animations.find(currentAnimation);
    if (it == animations.end() || it->second.frames.empty()) return;

    Animation& anim = it->second;

    float spd = (currentAnimation == "idle") ? 0.30f
        : (currentAnimation == "walkToRight") ? 0.10f
        : (currentAnimation == "attack") ? 0.08f
        : animSpeed;

    animTimer += dt;
    if (animTimer >= spd) {
        animTimer = 0.f;
        animFrame++;

        if (currentAnimation == "attack") {
            if (animFrame == 3 && !attackHitDealt) {
                attackHitDealt = true;
                pendingDamage = contactDamage;
            }
            if (animFrame >= (int)anim.frames.size()) {
                animFrame = (int)anim.frames.size() - 1;
                attackPlaying = false;
            }
        }
        else {
            if (animFrame >= (int)anim.frames.size())
                animFrame = 0;
        }

        sprite.setTexture(anim.texture, true);
        if (animFrame < (int)anim.frames.size())
            sprite.setTextureRect(anim.frames[animFrame]);
    }
}


bool Enemy::hasLineOfSight(const sf::Vector2f& playerPos,
    int map[][501], int mapWidth, int mapHeight, float tileSize) const
{
    sf::FloatRect hb = hitbox.getGlobalBounds();
    sf::Vector2f start = {
        hb.position.x + hb.size.x * 0.5f,
        hb.position.y + hb.size.y * 0.5f
    };
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

    lastMapWidth = mapWidth;
    lastMapHeight = mapHeight;
    lastTileSize = tileSize;

    sf::Vector2f pos = hitbox.getPosition();
    float hw = hitbox.getSize().x;
    float hh = hitbox.getSize().y;

    velocityY += gravity * dt;
    float nextY = pos.y + velocityY * dt;

    int leftTile = std::max(0, (int)std::floor(pos.x / tileSize));
    int rightTile = std::min(mapWidth - 1, (int)std::floor((pos.x + hw - 1.f) / tileSize));
    int footTile = (int)std::floor((nextY + hh) / tileSize);

    onGround = false;
    if (footTile >= 0 && footTile < mapHeight) {
        for (int tx = leftTile; tx <= rightTile; ++tx) {
            if (isSolidTile(map[footTile][tx])) {
                pos.y = footTile * tileSize - hh;
                velocityY = 0.f;
                onGround = true;
                break;
            }
        }
    }
    if (!onGround) pos.y = nextY;

    if (pos.y > spawnPos.y + tileSize * 6.f) {
        pos = spawnPos;
        velocityY = 0.f;
        hp = maxHp;
        state = EnemyState::Idle;
        contactTimer = 0.f;
        hitbox.setPosition(pos);
        return;
    }

    float cx = pos.x + hw * 0.5f;
    float cy = pos.y + hh * 0.5f;
    float dx = playerPos.x - cx;
    float dy = playerPos.y - cy;
    float dist = std::sqrt(dx * dx + dy * dy);

    bool playerDetected = false;
    if (dist <= closeDetectRadius) {
        playerDetected = true;
    }
    else if (dist <= detectionRange && std::abs(dy) <= verticalDetectRange) {
        float ndx = dx / dist;
        float dot = ndx * (float)facingDir;
        if (dot >= visionAngleCos)
            playerDetected = hasLineOfSight(playerPos, map, mapWidth, mapHeight, tileSize);
    }

    if (playerDetected) {
        state = EnemyState::Chasing;
        isLooking = false;
    }
    else if (state == EnemyState::Chasing) {
        state = EnemyState::Returning;
    }

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
        if (isLooking) {
            lookTimer -= dt;
            if (lookTimer <= 0.f) {
                isLooking = false;
                lookTimer = lookInterval;
            }
        }
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
                int nextTileX = (int)std::floor((pos.x + hw * 0.5f + moveX * dt + patrolDir * hw * 0.5f) / tileSize);
                int belowTile = (int)std::floor((pos.y + hh + 2.f) / tileSize);
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

    if (moveX != 0.f) {
        float nextX = pos.x + moveX * dt;
        int checkTileX = (moveX > 0.f)
            ? (int)std::floor((nextX + hw) / tileSize)
            : (int)std::floor(nextX / tileSize);
        int midTileY = (int)std::floor((pos.y + hh * 0.5f) / tileSize);

        bool wallHit = false;
        if (checkTileX >= 0 && checkTileX < mapWidth &&
            midTileY >= 0 && midTileY < mapHeight)
            if (isSolidTile(map[midTileY][checkTileX])) wallHit = true;

        if (!wallHit) pos.x = nextX;
        else if (state == EnemyState::Idle) {
            patrolDir = -patrolDir;
            facingDir = (patrolDir > 0.f) ? 1 : -1;
        }
    }

    if (hp <= 0) state = EnemyState::Dead;
    if (contactTimer > 0.f) contactTimer -= dt;

    hitbox.setPosition(pos);

    if (!attackPlaying) {
        bool moving = (std::abs(moveX) > 1.f);
        setAnimation(moving ? "walkToRight" : "idle");
    }

    updateAnimation(dt);

    float FRAME_W = 64.f;
    float FRAME_H = 64.f;

    float targetW = hw;
    float scale = targetW / FRAME_W;
    if (scale > 2.5f) scale = 2.5f;
    if (scale < 0.5f) scale = 0.5f;

    sprite.setOrigin({ FRAME_W * 0.5f, FRAME_H });
    sprite.setScale(facingDir > 0
        ? sf::Vector2f{ scale, scale }
    : sf::Vector2f{ -scale, scale });
    sprite.setPosition({ pos.x + hw * 0.5f, pos.y + hh + 4 });
}

void Enemy::draw(sf::RenderWindow& window, bool debugMode) {
    if (state == EnemyState::Dead) return;

    window.draw(sprite);

    sf::Vector2f hbPos = hitbox.getPosition();
    float hw = hitbox.getSize().x;
    float hh = hitbox.getSize().y;
    float barW = 36.f;
    sf::RectangleShape bg({ barW, 6.f });
    bg.setFillColor(sf::Color(50, 50, 50, 200));
    bg.setPosition({ hbPos.x + hw * 0.5f - barW * 0.5f, hbPos.y - 10.f });
    window.draw(bg);

    float perc = std::max(0.f, (float)hp / (float)maxHp);
    sf::RectangleShape fg({ barW * perc, 6.f });
    fg.setFillColor(sf::Color::Green);
    fg.setPosition(bg.getPosition());
    window.draw(fg);

    if (!debugMode) return;

    sf::RectangleShape dbgBox(hitbox.getSize());
    dbgBox.setPosition(hbPos);
    dbgBox.setFillColor(sf::Color::Transparent);
    dbgBox.setOutlineColor(sf::Color::Red);
    dbgBox.setOutlineThickness(1.f);
    window.draw(dbgBox);

    sf::Vector2f center = { hbPos.x + hw * 0.5f, hbPos.y + hh * 0.5f };
    const int SEG = 24;
    const float halfAngle = std::acos(visionAngleCos);
    float baseAngle = (facingDir > 0) ? 0.f : 3.14159265f;

    float worldW = (lastMapWidth > 0) ? lastMapWidth * lastTileSize : 1e9f;
    float worldH = (lastMapHeight > 0) ? lastMapHeight * lastTileSize : 1e9f;

    auto clipRay = [&](float angle) -> sf::Vector2f {
        float dx = std::cos(angle);
        float dy = std::sin(angle);
        float tMax = detectionRange;
        if (dx != 0.f) {
            if (dx > 0.f) tMax = std::min(tMax, (worldW - center.x) / dx);
            else           tMax = std::min(tMax, (0.f - center.x) / dx);
        }
        if (dy != 0.f) {
            if (dy > 0.f) tMax = std::min(tMax, (worldH - center.y) / dy);
            else           tMax = std::min(tMax, (0.f - center.y) / dy);
        }
        tMax = std::max(0.f, tMax);
        return { center.x + dx * tMax, center.y + dy * tMax };
        };

    sf::VertexArray cone(sf::PrimitiveType::TriangleFan, SEG + 2);
    cone[0].position = center;
    cone[0].color = sf::Color(255, 255, 0, 50);
    for (int i = 0; i <= SEG; ++i) {
        float a = baseAngle - halfAngle + (2.f * halfAngle * i / SEG);
        cone[i + 1].position = clipRay(a);
        cone[i + 1].color = sf::Color(255, 255, 0, 10);
    }
    window.draw(cone);

    sf::VertexArray coneLines(sf::PrimitiveType::Lines, 4);
    coneLines[0] = { center, sf::Color(255, 255, 0, 180) };
    coneLines[1] = { clipRay(baseAngle - halfAngle), sf::Color(255, 255, 0, 60) };
    coneLines[2] = { center, sf::Color(255, 255, 0, 180) };
    coneLines[3] = { clipRay(baseAngle + halfAngle), sf::Color(255, 255, 0, 60) };
    window.draw(coneLines);

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

    sf::CircleShape spawnDot(4.f);
    spawnDot.setFillColor(sf::Color(0, 200, 255, 180));
    spawnDot.setPosition({ spawnPos.x - 4.f, spawnPos.y - 4.f });
    window.draw(spawnDot);
}

void Enemy::takeDamage(int dmg) {
    hp -= dmg;
    if (hp <= 0) { hp = 0; state = EnemyState::Dead; return; }
    if (!attackPlaying) {
        setAnimation("attack");
        animFrame = 0; animTimer = 0.f;
        auto& anim = animations["attack"];
        if (!anim.frames.empty())
            sprite.setTextureRect(anim.frames[0]);
    }
}

int Enemy::checkAndGetContactDamage(const sf::FloatRect& playerBounds, float dt) {
    if (state == EnemyState::Dead) return 0;

    bool touching = rectsIntersect(playerBounds, hitbox.getGlobalBounds());

    if (touching && !attackPlaying && contactTimer <= 0.f) {
        setAnimation("attack");
        animFrame = 0; animTimer = 0.f;
        auto& anim = animations["attack"];
        if (!anim.frames.empty())
            sprite.setTextureRect(anim.frames[0]);
        contactTimer = contactCooldown;
    }

    if (pendingDamage > 0 && touching) {
        int dmg = pendingDamage;
        pendingDamage = 0;
        return dmg;
    }
    if (!touching) pendingDamage = 0;

    return 0;
}