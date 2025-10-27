#include "Player.h"
#include "Enemy.h"
#include <algorithm>

static bool rectsIntersect(const sf::FloatRect& a, const sf::FloatRect& b) {
    return (a.position.x < b.position.x + b.size.x) && (a.position.x + a.size.x > b.position.x) &&
        (a.position.y < b.position.y + b.size.y) && (a.position.y + a.size.y > b.position.y);
}

static sf::FloatRect expandRect(const sf::FloatRect& r, float pad) {
    return sf::FloatRect({ r.position.x - pad, r.position.y - pad }, { r.size.x + pad * 2.f, r.size.y + pad * 2.f });
}

static sf::FloatRect getSpikeHitbox(float tx, float ty, float tileSize, int spikeType) {
    const float SPIKE_DEPTH = tileSize * 0.4f;

    switch (spikeType) {
    case 18:
        return sf::FloatRect({ tx, ty + tileSize - SPIKE_DEPTH }, { tileSize, SPIKE_DEPTH });

    case 19:
        return sf::FloatRect({ tx, ty }, { SPIKE_DEPTH, tileSize });

    case 20:
        return sf::FloatRect({ tx + tileSize - SPIKE_DEPTH, ty }, { SPIKE_DEPTH, tileSize });

    case 21:
        return sf::FloatRect({ tx, ty }, { tileSize, SPIKE_DEPTH });

    default:
        return sf::FloatRect({ tx, ty }, { tileSize, tileSize });
    }
}

Player::Player(sf::Texture _tx, float startX, float startY) : texture(_tx), sprite(_tx) {
    shape.setSize({ 30.f, 40.f });
    shape.setPosition({ startX, startY });
    spawnPoint = { startX, startY };

    sprite.setTexture(texture);
    sprite.setTextureRect(animation["idle"][0]);
    sprite.setScale({ 2.5f, 2.5f });
    sprite.setPosition({ startX, startY });

    velocity = { 0.f, 0.f };
    speed = 200.f;
    gravity = 900.f;
    onGround = false;
    isSliding = false;
    wallDirection = 0;
    maxStamina = 1000.f;
    stamina = maxStamina;
    staminaConsumption = 30.f;
    staminaRegenRate = 20.f;
    isDashing = false;
    dashSpeed = 600.f;
    dashDuration = 0.2f;
    dashTimer = 0.f;
    dashCooldown = 0.5f;
    dashCooldownTimer = 0.f;
    dashDirection = { 0.f, 0.f };
    maxHp = 100;
    hp = maxHp;
    coins = 0;
    attackCooldown = 0.4f;
    attackTimer = 0.f;
    attackDamage = 30;
    attackKey = sf::Keyboard::Key::J;
    lavaDamageAccum = 0.f;
    spikeInvulTimer = 0.f;
    spikeInvulDuration = 0.6f;
    spikeDamage = 10;
    wasOnSpike = false;

    currentAnimation = "idle";
    animationFrame = 0;
    animationTimer = 0.f;
    animationSpeed = 0.1f;
    facingRight = true;

    hasKey = false;
    deathAnimationFinished = false;

    // инициализация нового флага
    limitedDashMode = false;
    dashAvailable = true;
}

bool Player::checkWallContact(int map[][501], int mapWidth, int mapHeight, float tileSize) {
    sf::FloatRect playerBounds = shape.getGlobalBounds();
    float px = playerBounds.position.x;
    float py = playerBounds.position.y;
    float pw = playerBounds.size.x;
    float ph = playerBounds.size.y;

    wallDirection = 0;

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = map[y][x];
            if (tile == -1) continue;

            float tx = x * tileSize;
            float ty = y * tileSize;
            float tw = tileSize;
            float th = tileSize;

            if (py + ph > ty && py < ty + th) {
                if (std::abs(px - (tx + tw)) < 5.f) {
                    wallDirection = -1;
                    return true;
                }
                if (std::abs((px + pw) - tx) < 5.f) {
                    wallDirection = 1;
                    return true;
                }
            }
        }
    }

    return false;
}

void Player::updateAnimation(float dt) {
    float currentSpeed = (currentAnimation == "idle") ? 0.5f : animationSpeed;

    animationTimer += dt;

    if (animationTimer >= currentSpeed) {
        animationTimer = 0.f;

        if (animation.find(currentAnimation) != animation.end()) {
            animationFrame++;
            if (animationFrame >= animation[currentAnimation].size()) {
                if (currentAnimation == "Death") {
                    animationFrame = animation[currentAnimation].size() - 1;
                    deathAnimationFinished = true;
                }
                else {
                    animationFrame = 0;
                }
            }
            sprite.setTextureRect(animation[currentAnimation][animationFrame]);
        }
    }
}

void Player::setAnimation(const string& animName) {
    if (currentAnimation != animName) {
        currentAnimation = animName;
        animationFrame = 0;
        animationTimer = 0.f;
        if (animName == "Death") {
            deathAnimationFinished = false;
        }
        if (animation.find(animName) != animation.end()) {
            sprite.setTextureRect(animation[animName][0]);
        }
    }
}

void Player::update(float dt, int map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window,
    int mobMap[][501], int interestingMap[][501], int backgroundMap[][501], std::vector<Enemy>& enemies) {

    if (hp <= 0) {
        setAnimation("Death");
        updateAnimation(dt);
        sprite.setPosition(shape.getPosition());
        return;
    }

    if (dashCooldownTimer > 0.f) {
        dashCooldownTimer -= dt;
    }

    if (attackTimer > 0.f) attackTimer -= dt;

    if (spikeInvulTimer > 0.f) spikeInvulTimer -= dt;

    if (isDashing) {
        dashTimer -= dt;
        if (dashTimer <= 0.f) {
            isDashing = false;
            velocity = { 0.f, 0.f };
        }
        else {
            velocity = dashDirection * dashSpeed;
        }
    }
    else {
        // при limitedDashMode проверяем доступность даша
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) && dashCooldownTimer <= 0.f && stamina >= 50.f
            && (!limitedDashMode || dashAvailable)) {
            sf::Vector2f dashDir = { 0.f, 0.f };

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
                dashDir.x = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
                dashDir.x = 1.f;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
                dashDir.y = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
                dashDir.y = 1.f;

            float length = std::sqrt(dashDir.x * dashDir.x + dashDir.y * dashDir.y);
            if (length > 0.f) {
                dashDirection = dashDir / length;
                isDashing = true;
                dashTimer = dashDuration;
                dashCooldownTimer = dashCooldown;
                stamina -= 50.f;

                // если режим ограничен — пометим, что даш использован до следующего касания земли
                if (limitedDashMode) dashAvailable = false;
            }
        }

        bool touchingWall = checkWallContact(map, mapWidth, mapHeight, tileSize);

        if (touchingWall && !onGround && velocity.y > 0.f) {
            bool pressingIntoWall = false;
            if (wallDirection == -1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) {
                pressingIntoWall = true;
            }
            else if (wallDirection == 1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))) {
                pressingIntoWall = true;
            }

            if (pressingIntoWall && stamina > 0.f) {
                isSliding = true;
                velocity.y = 50.f;
                stamina -= staminaConsumption * dt;
                if (stamina < 0.f) stamina = 0.f;
            }
            else {
                isSliding = false;
            }
        }
        else {
            isSliding = false;

            if (onGround && stamina < maxStamina) {
                stamina += staminaRegenRate * dt;
                if (stamina > maxStamina) stamina = maxStamina;
            }
        }

        if (isSliding) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) {

                if (stamina >= 20.f) {
                    velocity.y = -420.f;
                    velocity.x = -wallDirection * speed * 1.5f;
                    stamina -= 20.f;
                    isSliding = false;
                }
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                velocity.y = 200.f;
                velocity.x = -wallDirection * speed * 1.2f;
                isSliding = false;
            }
        }

        if (!isSliding && !isDashing) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
                velocity.x = -speed;
                facingRight = false;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
                velocity.x = speed;
                facingRight = true;
            }
            else {
                velocity.x = 0.f;
            }

            if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) && onGround) {
                velocity.y = -420.f;
                onGround = false;
            }
        }

        if (!isDashing) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
        }
    }

    bool attackPressed = sf::Keyboard::isKeyPressed(attackKey);
    if (attackPressed && attackTimer <= 0.f) {
        setAnimation("Attack");

        sf::FloatRect pBounds = shape.getGlobalBounds();
        const float attackRadius = 60.f;
        sf::FloatRect attackRect = expandRect(pBounds, attackRadius);

        for (auto& e : enemies) {
            if (!e.isAlive()) continue;
            if (rectsIntersect(attackRect, e.getBounds())) {
                e.takeDamage(attackDamage);
            }
        }
        attackTimer = attackCooldown;
    }

    sf::Vector2f nextPos = shape.getPosition() + velocity * dt;
    float px = nextPos.x;
    float py = nextPos.y;
    float pw = shape.getSize().x;
    float ph = shape.getSize().y;

    onGround = false;

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = map[y][x];
            if (tile == -1) continue;

            float tx = x * tileSize;
            float ty = y * tileSize;
            float tw = tileSize;
            float th = tileSize;

            float overlapX = std::min(px + pw, tx + tw) - std::max(px, tx);
            float overlapY = std::min(py + ph, ty + th) - std::max(py, ty);

            if (overlapX > 0.f && overlapY > 0.f) {
                if (overlapX < overlapY) {
                    if (px < tx) {
                        nextPos.x -= overlapX;
                    }
                    else {
                        nextPos.x += overlapX;
                    }
                    if (!isDashing) velocity.x = 0.f;
                    px = nextPos.x;
                }
                else {
                    if (py < ty) {
                        nextPos.y -= overlapY;
                        velocity.y = 0.f;
                        onGround = true;
                    }
                    else {
                        nextPos.y += overlapY;
                        velocity.y = 0.f;
                    }
                    py = nextPos.y;
                }
            }
        }
    }

    shape.setPosition(nextPos);

    // если режим ограничен и игрок на земле — восстановить возможность даша
    if (limitedDashMode && onGround) {
        dashAvailable = true;
    }

    if (!onGround) {
        setAnimation("Jump");
    }
    else if (velocity.x != 0.f) {
        if (facingRight) {
            setAnimation("walkToRight");
        }
        else {
            setAnimation("walkToLeft");
        }
    }
    else {
        if (attackTimer > 0.f) {
            if (currentAnimation != "Attack") setAnimation("Attack");
        }
        else if (sf::Keyboard::isKeyPressed(attackKey) && attackTimer <= 0.f) {
            if (currentAnimation != "Attack") setAnimation("Attack");
        }
        else {
            setAnimation("idle");
        }
    }

    updateAnimation(dt);

    if (facingRight) {
        sprite.setScale({ 2.5f, 2.5f });
    }
    else {
        sprite.setScale({ -2.5f, 2.5f });
    }
    sf::Vector2f spritePos = shape.getPosition();

    sf::IntRect texRect = sprite.getTextureRect();
    float spriteHeight = texRect.size.y * 2.5f;
    float spriteWidth = texRect.size.x * 2.5f;

    spritePos.y += shape.getSize().y - spriteHeight;

    spritePos.x += (shape.getSize().x - spriteWidth) / 2.f;

    if (!facingRight) {
        spritePos.x += spriteWidth;
    }

    sprite.setPosition(spritePos);

    sf::FloatRect playerBounds = shape.getGlobalBounds();
    const float collectPadding = 8.f;
    sf::FloatRect collectRect = expandRect(playerBounds, collectPadding);

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = interestingMap[y][x];
            sf::FloatRect tileRect({ static_cast<float>(x) * tileSize, static_cast<float>(y) * tileSize }, { tileSize, tileSize });

            if (tile == 7) {
                if (rectsIntersect(collectRect, tileRect)) {
                    coins += 1;
                    interestingMap[y][x] = -1;
                }
            }

            if (tile == 22) {
                if (rectsIntersect(collectRect, tileRect)) {
                    hasKey = true;
                    interestingMap[y][x] = -1;
                    std::cout << "Key collected!" << std::endl;
                }
            }
        }
    }
    if (hasKey) {
        for (int y = 0; y < mapHeight; ++y) {
            for (int x = 0; x < mapWidth; ++x) {
                if (map[y][x] == 12) {
                    map[y][x] = 13;
                }
            }
        }
    }

    const float lavaDPS = 25.f;
    bool onLava = false;
    bool onSpike = false;

    float pLeft = playerBounds.position.x;
    float pRight = playerBounds.position.x + playerBounds.size.x;
    float pTop = playerBounds.position.y;
    float pBottom = playerBounds.position.y + playerBounds.size.y;

    int startX = std::max(0, (int)(pLeft / tileSize) - 1);
    int endX = std::min(mapWidth, (int)(pRight / tileSize) + 2);
    int startY = std::max(0, (int)(pTop / tileSize) - 1);
    int endY = std::min(mapHeight, (int)(pBottom / tileSize) + 2);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            int t1 = map[y][x];
            int t2 = backgroundMap[y][x];
            int t3 = interestingMap[y][x];

            sf::FloatRect tileRect({ static_cast<float>(x) * tileSize, static_cast<float>(y) * tileSize }, { tileSize, tileSize });

            if (t1 == 16 || t1 == 17 || t2 == 16 || t2 == 17 || t3 == 16 || t3 == 17) {
                if (rectsIntersect(playerBounds, tileRect)) {
                    onLava = true;
                }
            }

            if (t1 >= 18 && t1 <= 21) {
                sf::FloatRect spikeHitbox = getSpikeHitbox(x * tileSize, y * tileSize, tileSize, t1);
                if (rectsIntersect(playerBounds, spikeHitbox)) {
                    onSpike = true;
                }
            }
            if (t2 >= 18 && t2 <= 21) {
                sf::FloatRect spikeHitbox = getSpikeHitbox(x * tileSize, y * tileSize, tileSize, t2);
                if (rectsIntersect(playerBounds, spikeHitbox)) {
                    onSpike = true;
                }
            }
            if (t3 >= 18 && t3 <= 21) {
                sf::FloatRect spikeHitbox = getSpikeHitbox(x * tileSize, y * tileSize, tileSize, t3);
                if (rectsIntersect(playerBounds, spikeHitbox)) {
                    onSpike = true;
                }
            }
        }
    }

    if (onLava) {
        lavaDamageAccum += lavaDPS * dt;
        int dmg = static_cast<int>(lavaDamageAccum);
        if (dmg > 0) {
            applyDamage(dmg);
            lavaDamageAccum -= static_cast<float>(dmg);
        }
    }
    else {
        lavaDamageAccum = 0.f;
    }

    if (onSpike) {
        if (spikeInvulTimer <= 0.f) {
            applyDamage(spikeDamage);
            spikeInvulTimer = spikeInvulDuration;
        }
    }

    float mapHeightPx = mapHeight * tileSize;
    if (shape.getPosition().y > mapHeightPx + 100.f) {
        hp = 0;
        velocity = { 0.f, 0.f };
        isDashing = false;
        dashCooldownTimer = 0.f;
        return;
    }

    for (auto& e : enemies) {
        if (!e.isAlive()) continue;
        int dmg = e.checkAndGetContactDamage(playerBounds, dt);
        if (dmg > 0) {
            applyDamage(dmg);
        }
    }
}

void Player::draw(sf::RenderWindow& window, sf::View& view1, sf::Font& font) {
    window.draw(sprite);
    window.setView(view1);
}

void Player::reset() {
    shape.setPosition(spawnPoint);
    velocity = { 0.f, 0.f };
    stamina = maxStamina;
    hp = maxHp;
    coins = 0;
    isDashing = false;
    dashCooldownTimer = 0.f;
    attackTimer = 0.f;
    lavaDamageAccum = 0.f;
    spikeInvulTimer = 0.f;
    wasOnSpike = false;
    currentAnimation = "idle";
    animationFrame = 0;
    animationTimer = 0.f;
    facingRight = true;
    hasKey = false;
    deathAnimationFinished = false;
    sprite.setPosition(spawnPoint);
    sprite.setTextureRect(animation["idle"][0]);
    sprite.setScale({ 2.5f, 2.5f });

    // восстановим доступ к дашу при ресете
    dashAvailable = true;
}

void Player::setSpawnPoint(float x, float y) {
    spawnPoint = { x, y };
}