#include "Player.h"
#include "Enemy.h"
#include "Gamemap.h"
#include "TrapSystem.h"
#include "ApiClient.h"
#include "MultiplayerClient.h"
#include <algorithm>

using namespace sf;
using namespace std;

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
    case 18: return sf::FloatRect({ tx, ty + tileSize - SPIKE_DEPTH }, { tileSize, SPIKE_DEPTH });
    case 19: return sf::FloatRect({ tx, ty }, { SPIKE_DEPTH, tileSize });
    case 20: return sf::FloatRect({ tx + tileSize - SPIKE_DEPTH, ty }, { SPIKE_DEPTH, tileSize });
    case 21: return sf::FloatRect({ tx, ty }, { tileSize, SPIKE_DEPTH });
    default: return sf::FloatRect({ tx, ty }, { tileSize, tileSize });
    }
}

Player::Player(sf::Texture _tx, float startX, float startY) : texture(_tx), sprite(_tx) {
    shape.setSize({ 30.f, 40.f });
    shape.setPosition({ startX, startY });
    spawnPoint = { startX, startY };
    velocity = { 0.f, 0.f };
    speed = 200.f;
    gravity = 900.f;
    onGround = false;
    isSliding = false;
    wallDirection = 0;
    maxStamina = 175.f;
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
    limitedDashMode = false;
    dashAvailable = true;
    jumpHeld = false;
    coyoteTimer = 0.f;
    jumpBufferTimer = 0.f;
    wallJumpLockTimer = 0.f;
}

void Player::loadAnimationSheets(const std::string& walkPath,
    const std::string& attackPath,
    const std::string& idlePath,
    const std::string& fallPath,
    const std::string& jumpPath)
{
    animations["walkToRight"].texture.loadFromFile(walkPath);
    animations["walkToRight"].frames = {
        IntRect({0,   0}, {128, 128}), IntRect({128,  0}, {128, 128}),
        IntRect({256, 0}, {128, 128}), IntRect({384,  0}, {128, 128}),
        IntRect({512, 0}, {128, 128}), IntRect({640,  0}, {128, 128}),
        IntRect({768, 0}, {128, 128}), IntRect({896,  0}, {128, 128}),
        IntRect({1024,0}, {128, 128}), IntRect({1152, 0}, {128, 128}),
        IntRect({1280,0}, {128, 128}), IntRect({1408, 0}, {128, 128}),
        IntRect({1536,0}, {128, 128}), IntRect({1664, 0}, {128, 128}),
        IntRect({1792,0}, {128, 128}),
    };
    animations["walkToLeft"].texture.loadFromFile(walkPath);
    animations["walkToLeft"].frames = animations["walkToRight"].frames;

    animations["Attack"].texture.loadFromFile(attackPath);
    animations["Attack"].frames = {
        IntRect({0,   0}, {128, 128}),
        //IntRect({128, 0}, {128, 128}),
        IntRect({256, 0}, {128, 128}),
        //IntRect({384, 0}, {128, 128}),
        IntRect({512, 0}, {128, 128}),
        IntRect({640, 0}, {128, 128}),
        //IntRect({768, 0}, {128, 128}),
        IntRect({896, 0}, {128, 128}),
        IntRect({1024,0}, {128, 128}),
    };

    animations["idle"].texture.loadFromFile(idlePath);
    animations["idle"].frames = {
        IntRect({0,   0}, {128, 128}), IntRect({128,  0}, {128, 128}),
        IntRect({256, 0}, {128, 128}), IntRect({384,  0}, {128, 128}),
        IntRect({512, 0}, {128, 128}), IntRect({640,  0}, {128, 128}),
        IntRect({768, 0}, {128, 128}), IntRect({896,  0}, {128, 128}),
        IntRect({1024,0}, {128, 128}), IntRect({1152, 0}, {128, 128}),
        IntRect({1280,0}, {128, 128}), IntRect({1408, 0}, {128, 128}),
        IntRect({1536,0}, {128, 128}), IntRect({1664, 0}, {128, 128}),
        IntRect({1792,0}, {128, 128}),
    };

    if (!jumpPath.empty()) {
        animations["Jump"].texture.loadFromFile(jumpPath);
        auto& tex = animations["Jump"].texture;
        int frameCount = (int)(tex.getSize().x / 128);
        if (frameCount < 1) frameCount = 1;
        for (int i = 0; i < frameCount; ++i)
            animations["Jump"].frames.push_back(IntRect({ i * 128, 0 }, { 128, 128 }));
    }
    else {
        animations["Jump"].texture.loadFromFile(idlePath);
        animations["Jump"].frames = {
            IntRect({0,   0}, {128, 128}),
            IntRect({128, 0}, {128, 128}),
            IntRect({256, 0}, {128, 128}),
            IntRect({384, 0}, {128, 128}),
            IntRect({512, 0}, {128, 128}),
            IntRect({640, 0}, {128, 128}),
        };
    }

    if (!fallPath.empty()) {
        animations["Fall"].texture.loadFromFile(fallPath);
        auto& fallTex = animations["Fall"].texture;
        int frameCount = (int)(fallTex.getSize().x / 128);
        if (frameCount < 1) frameCount = 1;
        for (int i = 0; i < frameCount; ++i)
            animations["Fall"].frames.push_back(IntRect({ i * 128, 0 }, { 128, 128 }));
    }
    else {
        animations["Fall"].texture.loadFromFile(idlePath);
        animations["Fall"].frames = {
            IntRect({0,   0}, {128, 128}),
            IntRect({128, 0}, {128, 128}),
            IntRect({256, 0}, {128, 128}),
            IntRect({384, 0}, {128, 128}),
        };
    }

    animations["Death"].texture.loadFromFile("Sprites/player_death.png");
    animations["Death"].frames = {
        IntRect({0,    0}, {128, 128}), IntRect({128,  0}, {128, 128}),
        IntRect({256,  0}, {128, 128}), IntRect({384,  0}, {128, 128}),
        IntRect({512,  0}, {128, 128}), IntRect({640,  0}, {128, 128}),
        IntRect({768,  0}, {128, 128}), IntRect({896,  0}, {128, 128}),
        IntRect({1024, 0}, {128, 128}), /*IntRect({1152, 0}, {128, 128}),*/
        IntRect({1280, 0}, {128, 128}), IntRect({1408, 0}, {128, 128}),
        /*IntRect({1536, 0}, {128, 128}),*/ IntRect({1664, 0}, {128, 128}),
        IntRect({1792, 0}, {128, 128}), IntRect({1920, 0}, {128, 128}),
        IntRect({2048, 0}, {128, 128}), IntRect({2176, 0}, {128, 128}),
        IntRect({2304, 0}, {128, 128}),
    };
    sprite.setTexture(animations["idle"].texture, true);
    sprite.setTextureRect(animations["idle"].frames[0]);
}

sf::FloatRect Player::getInnerBounds() const {
    sf::FloatRect b = shape.getGlobalBounds();
    const float hShrink = 4.f;
    const float vShrink = 3.f;
    return sf::FloatRect(
        { b.position.x + hShrink, b.position.y + vShrink },
        { b.size.x - hShrink * 2.f, b.size.y - vShrink * 2.f }
    );
}

bool Player::checkWallContact(int map[][501], int mapWidth, int mapHeight, float tileSize) {
    sf::FloatRect playerBounds = shape.getGlobalBounds();
    float px = playerBounds.position.x;
    float py = playerBounds.position.y;
    float pw = playerBounds.size.x;
    float ph = playerBounds.size.y;

    wallDirection = 0;
    bool leftWall = false, rightWall = false;

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = map[y][x];
            if (tile == -1) continue;
            float tx = x * tileSize;
            float ty = y * tileSize;
            float tw = tileSize;
            float th = tileSize;
            if (py + ph > ty && py < ty + th) {
                if (std::abs(px - (tx + tw)) < 5.f) leftWall = true;
                if (std::abs((px + pw) - tx) < 5.f) rightWall = true;
            }
        }
    }

    wallBothSides = leftWall && rightWall;

    if (leftWall) { wallDirection = -1; return true; }
    if (rightWall) { wallDirection = 1; return true; }
    return false;
}

void Player::updateAnimation(float dt) {
    if (animations.find(currentAnimation) == animations.end()) return;
    Animation& anim = animations[currentAnimation];
    if (anim.frames.empty()) return;

    float spd = (currentAnimation == "idle") ? 0.12f
        : (currentAnimation == "Jump") ? 0.06f
        : (currentAnimation == "Fall") ? 0.07f
        : (currentAnimation == "Death") ? 0.085f
        : animationSpeed;
    animationTimer += dt;
    if (animationTimer >= spd) {
        animationTimer = 0.f;
        animationFrame++;

        if (currentAnimation == "Death") {
            if (animationFrame >= (int)anim.frames.size()) {
                animationFrame = (int)anim.frames.size() - 1;
                deathAnimationFinished = true;
            }
        }
        else if (currentAnimation == "Jump") {
            if (animationFrame >= (int)anim.frames.size()) {
                animationFrame = (int)anim.frames.size() - 1;
                jumpAnimDone = true;
            }
        }
        else if (currentAnimation == "Fall") {
            int total = (int)anim.frames.size();
            if (animationFrame >= total) {
                int loopStart = std::max(0, total - 4);
                int loopLen = total - loopStart;
                animationFrame = loopStart + ((animationFrame - loopStart) % loopLen);
            }
        }
        else {
            if (animationFrame >= (int)anim.frames.size())
                animationFrame = 0;
        }

        sprite.setTexture(anim.texture, true);
        sprite.setTextureRect(anim.frames[animationFrame]);
    }
}

void Player::setAnimation(const string& animName) {
    if (currentAnimation == animName) return;
    if (animations.find(animName) == animations.end()) return;

    currentAnimation = animName;
    animationFrame = 0;
    animationTimer = 0.f;
    if (animName == "Death") deathAnimationFinished = false;
    if (animName == "Jump")  jumpAnimDone = false;

    Animation& anim = animations[animName];
    sprite.setTexture(anim.texture, true);
    if (!anim.frames.empty())
        sprite.setTextureRect(anim.frames[0]);
}

void Player::update(float dt, int map[][501], int mapWidth, int mapHeight, float tileSize,
    sf::View& view1, sf::RenderWindow& window,
    int mobMap[][501], int interestingMap[][501], int backgroundMap[][501],
    std::vector<Enemy>& enemies)
{
    if (hp <= 0) {
        if (!deathPosSet) {
            deathSpritePos = shape.getPosition();
            velocity.x *= 0.3f;
            deathPosSet = true;
        }

        if (!onGround) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
            velocity.x *= std::pow(0.85f, dt * 60.f);

            sf::Vector2f nextPos = deathSpritePos + velocity * dt;
            float px = nextPos.x;
            float py = nextPos.y;
            float pw = shape.getSize().x;
            float ph = shape.getSize().y;

            onGround = false;
            for (int y = 0; y < mapHeight; ++y) {
                for (int x = 0; x < mapWidth; ++x) {
                    if (map[y][x] == -1) continue;
                    float tx = x * tileSize, ty = y * tileSize;
                    float overlapX = std::min(px + pw, tx + tileSize) - std::max(px, tx);
                    float overlapY = std::min(py + ph, ty + tileSize) - std::max(py, ty);
                    if (overlapX > 0.f && overlapY > 0.f) {
                        if (overlapX >= overlapY) {
                            if (py < ty) {
                                nextPos.y -= overlapY;
                                velocity.y = 0.f;
                                velocity.x = 0.f;
                                onGround = true;
                                py = nextPos.y;
                            }
                        }
                        else {
                            if (px < tx) nextPos.x -= overlapX;
                            else         nextPos.x += overlapX;
                            velocity.x = 0.f;
                            px = nextPos.x;
                        }
                    }
                }
            }
            deathSpritePos = nextPos;
            shape.setPosition(deathSpritePos);
        }

        setAnimation("Death");
        updateAnimation(dt);

        const float sprScale = 0.9f;
        sprite.setOrigin({ 44.5f, 96.f });
        sprite.setScale(facingRight
            ? sf::Vector2f{ sprScale, sprScale }
        : sf::Vector2f{ -sprScale, sprScale });

        sf::Vector2f dp = deathSpritePos;
        dp.x += shape.getSize().x * 0.5f;
        dp.y += shape.getSize().y - 3.f;
        sprite.setPosition(dp);
        return;
    }

    if (dashCooldownTimer > 0.f) dashCooldownTimer -= dt;
    if (attackTimer > 0.f) attackTimer -= dt;
    if (hitFreezeTimer > 0.f) hitFreezeTimer -= dt;
    if (damageFlashTimer > 0.f) damageFlashTimer -= dt;
    if (spikeInvulTimer > 0.f) spikeInvulTimer -= dt;
    if (wallJumpLockTimer > 0.f) wallJumpLockTimer -= dt;

    questPollTimer += dt;
    if (questPollTimer >= QUEST_POLL_INTERVAL) {
        questPollTimer = 0.f;
        auto& api = ApiClient::instance();
        if (api.player.loggedIn) {
            api.getQuestsAsync([](std::vector<ApiQuest>) {});
            std::thread([&api]() {
                api.refreshPlayerAsync();
                }).detach();
        }
    }

    bool jumpKeyDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

    bool jumpJustPressed = jumpKeyDown && !jumpHeld;

    if (jumpJustPressed)
        jumpBufferTimer = JUMP_BUFFER_TIME;
    else if (jumpBufferTimer > 0.f)
        jumpBufferTimer -= dt;
    if (onGround)
        coyoteTimer = COYOTE_TIME;
    else if (coyoteTimer > 0.f)
        coyoteTimer -= dt;

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

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) &&
            dashCooldownTimer <= 0.f && stamina >= 50.f &&
            (!limitedDashMode || dashAvailable))
        {
            sf::Vector2f dDir = { 0.f, 0.f };
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dDir.x = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dDir.x = 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    dDir.y = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  dDir.y = 1.f;

            float len = std::sqrt(dDir.x * dDir.x + dDir.y * dDir.y);
            if (len > 0.f) {
                dashDirection = dDir / len;
                isDashing = true;
                dashTimer = dashDuration;
                dashCooldownTimer = dashCooldown;
                stamina -= 50.f;
                if (limitedDashMode) dashAvailable = false;
            }
        }

        bool touchingWall = checkWallContact(map, mapWidth, mapHeight, tileSize);

        if (touchingWall && !onGround && velocity.y > 0.f && wallJumpLockTimer <= 0.f) {
            bool holdingTowardWall = (wallDirection == -1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) ||
                (wallDirection == -1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) ||
                (wallDirection == 1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) ||
                (wallDirection == 1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right));
            if (holdingTowardWall) {
                isSliding = true;
                const float WALL_SLIDE_SPEED = 35.f;
                velocity.y = WALL_SLIDE_SPEED;
            }
            else {
                isSliding = false;
                const float WALL_FALL_SPEED = 120.f;
                if (velocity.y < WALL_FALL_SPEED) velocity.y = WALL_FALL_SPEED;
            }
            facingRight = (wallDirection == -1);

            bool wantWallJump = jumpJustPressed || jumpBufferTimer > 0.f;
            if (wantWallJump) {
                if (wallBothSides) {
                    velocity.y = -JUMP_VELOCITY;
                    velocity.x = 0.f;
                }
                else {
                    const float BOUNCE_X = speed * 2.0f;
                    const float BOUNCE_Y = JUMP_VELOCITY * 0.75f;
                    velocity.x = static_cast<float>(-wallDirection) * BOUNCE_X;
                    velocity.y = -BOUNCE_Y;
                    facingRight = (-wallDirection > 0);
                }
                isSliding = false;
                wallJumpLockTimer = WALL_JUMP_LOCK_TIME;
                coyoteTimer = 0.f;
                jumpBufferTimer = 0.f;
                jumpAnimDone = false;
                setAnimation("Jump");
            }
        }
        else if (!touchingWall || onGround || velocity.y <= 0.f) {
            isSliding = false;

            if (onGround && stamina < maxStamina) {
                stamina += staminaRegenRate * dt;
                if (stamina > maxStamina) stamina = maxStamina;
            }
        }

        if (!isSliding && !isDashing) {

            bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
            bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

            if (hitFreezeTimer > 0.f) {
                velocity.x = 0.f;
            }
            else if (wallJumpLockTimer > 0.f) {
                const float AIR_STEER = 0.5f;
                const float accel = speed * AIR_STEER * dt * 10.f;
                if (left && !right)       velocity.x = std::max(velocity.x - accel, -speed);
                else if (right && !left)  velocity.x = std::min(velocity.x + accel, speed);
            }
            else {
                const float airAccel = 1400.f;
                if (left && !right) {
                    if (onGround) velocity.x = -speed;
                    else velocity.x = std::max(velocity.x - airAccel * dt, -speed);
                    facingRight = false;
                }
                else if (right && !left) {
                    if (onGround) velocity.x = speed;
                    else velocity.x = std::min(velocity.x + airAccel * dt, speed);
                    facingRight = true;
                }
                else {
                    if (onGround) {
                        velocity.x = 0.f;
                    }
                    else {
                        const float airFriction = 800.f;
                        if (velocity.x > 0.f) velocity.x = std::max(0.f, velocity.x - airFriction * dt);
                        else                  velocity.x = std::min(0.f, velocity.x + airFriction * dt);
                    }
                }
            }

            if (!onGround) {
                if (left && !right)       facingRight = false;
                else if (right && !left)  facingRight = true;
            }

            bool canJump = (coyoteTimer > 0.f);
            bool shouldJump = canJump && (jumpJustPressed || jumpBufferTimer > 0.f);

            if (shouldJump) {
                const float DIAG_H_MULT = 1.1f;
                float jumpVx = 0.f;
                if (wallJumpLockTimer <= 0.f) {
                    if (left && !right)  jumpVx = -speed * DIAG_H_MULT;
                    else if (right && !left) jumpVx = speed * DIAG_H_MULT;
                }

                velocity.y = -JUMP_VELOCITY;
                if (!isDashing) velocity.x = jumpVx;
                onGround = false;
                coyoteTimer = 0.f;
                jumpBufferTimer = 0.f;

                if (jumpVx < 0.f)      facingRight = false;
                else if (jumpVx > 0.f) facingRight = true;
            }

            if (!jumpKeyDown && velocity.y < -JUMP_CUT_VELOCITY && !isDashing) {
                velocity.y = -JUMP_CUT_VELOCITY;
            }
        }

        if (!isDashing) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
        }

    }

    if (sf::Keyboard::isKeyPressed(attackKey) && attackTimer <= 0.f && currentAnimation != "Attack") {
        setAnimation("Attack");
        animationFrame = 0;
        animationTimer = 0.f;
        attackHitDealt = false;
        attackTimer = attackCooldown;
    }

    if (currentAnimation == "Attack" && !attackHitDealt) {
        bool atkUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
        bool atkDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

        if (atkUp && !atkDown)          attackDir = { 0.f, -1.f };
        else if (atkDown && !atkUp)          attackDir = { 0.f,  1.f };
        else if (facingRight)                attackDir = { 1.f,  0.f };
        else                                 attackDir = { -1.f, 0.f };
    }

    if (currentAnimation == "Attack" && animationFrame == 3 && !attackHitDealt) {
        attackHitDealt = true;
        hitFreezeTimer = HIT_FREEZE_DURATION;
        sf::FloatRect pBounds = shape.getGlobalBounds();
        const float sideRange = 75.f;
        const float vertRange = 60.f;
        float cx = pBounds.position.x + pBounds.size.x * 0.5f;
        float cy = pBounds.position.y + pBounds.size.y * 0.5f;
        float hw = pBounds.size.x * 0.5f;
        float hh = pBounds.size.y * 0.5f;

        sf::FloatRect attackRect;
        bool ar_ = attackDir.x > 0.f, al = attackDir.x < 0.f;
        bool au = attackDir.y < 0.f, ad = attackDir.y > 0.f;

        if (!ar_ && !al && au && !ad) attackRect = { { cx - hw, cy - hh - vertRange }, { pBounds.size.x, vertRange } };
        else if (!ar_ && !al && !au && ad) attackRect = { { cx - hw, cy + hh }, { pBounds.size.x, vertRange } };
        else if (ar_ && !al && au && !ad) attackRect = { { cx, cy - hh - vertRange }, { sideRange, vertRange + hh } };
        else if (!ar_ && al && au && !ad) attackRect = { { cx - sideRange, cy - hh - vertRange }, { sideRange, vertRange + hh } };
        else if (ar_ && !al && !au && ad) attackRect = { { cx, cy }, { sideRange, hh + vertRange } };
        else if (!ar_ && al && !au && ad) attackRect = { { cx - sideRange, cy }, { sideRange, hh + vertRange } };
        else if (ar_) attackRect = { { cx, cy - hh }, { sideRange, pBounds.size.y } };
        else          attackRect = { { cx - sideRange, cy - hh }, { sideRange, pBounds.size.y } };

        for (auto& e : enemies) {
            if (!e.isAlive()) continue;
            if (rectsIntersect(attackRect, e.getBounds())) {
                bool wasAlive = e.isAlive();
                e.takeDamage(attackDamage);
                if (wasAlive && !e.isAlive()) {
                    int idx = (int)(&e - &enemies[0]);
                    MultiplayerClient::instance().sendEnemyKill(idx, 0); // level 0 = current
                }
            }
        }
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
                    float distFromTop = (py + ph) - ty;
                    if (distFromTop > 0.f && distFromTop <= LEDGE_FORGIVENESS && velocity.y >= 0.f) {
                        nextPos.y -= distFromTop;
                        velocity.y = 0.f;
                        onGround = true;
                        py = nextPos.y;
                    }
                    else {
                        if (px < tx) nextPos.x -= overlapX;
                        else nextPos.x += overlapX;
                        if (!isDashing) velocity.x = 0.f;
                        px = nextPos.x;
                    }
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

    if (limitedDashMode && onGround) dashAvailable = true;

    jumpHeld = jumpKeyDown;

    if (currentAnimation == "Attack") {
        int total = animations.count("Attack") ? (int)animations["Attack"].frames.size() : 1;
        if (animationFrame >= total - 1 && attackTimer <= 0.f) {
            if (!onGround) {
                fallTimer = 0.f; jumpAnimDone = false;
                setAnimation("Jump");
            }
            else if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
            else setAnimation("idle");
        }
    }
    else if (onGround) {
        fallTimer = 0.f;
        jumpAnimDone = false;
        if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
        else                    setAnimation("idle");
    }
    else if (isSliding) {
        if (currentAnimation != "Jump")
            setAnimation("Jump");
        if (!jumpAnimDone) {
            int total = animations.count("Jump") ? (int)animations["Jump"].frames.size() : 1;
            animationFrame = total - 1;
            jumpAnimDone = true;
            auto& anim = animations["Jump"];
            sprite.setTexture(anim.texture, true);
            sprite.setTextureRect(anim.frames[animationFrame]);
        }
    }
    else {
        if (velocity.y < 0.f) {
            fallTimer = 0.f;
            if (currentAnimation != "Jump")
                setAnimation("Jump");
        }
        else {
            if (currentAnimation == "Jump") {
                if (jumpAnimDone)
                    setAnimation("Fall");
            }
            else if (currentAnimation != "Fall") {
                setAnimation("Fall");
            }
        }
    }

    updateAnimation(dt);

    const float sprScale = 0.9f;
    sprite.setOrigin({ 44.5f, 96.f });
    sprite.setScale(facingRight ? sf::Vector2f{ sprScale, sprScale }
    : sf::Vector2f{ -sprScale, sprScale });

    sf::Vector2f spritePos = shape.getPosition();
    spritePos.x += shape.getSize().x * 0.5f;
    spritePos.y += shape.getSize().y - 3.f;
    sprite.setPosition(spritePos);

    sf::FloatRect playerBounds = shape.getGlobalBounds();
    sf::FloatRect collectRect = expandRect(playerBounds, 8.f);

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = interestingMap[y][x];
            sf::FloatRect tileRect({ static_cast<float>(x) * tileSize, static_cast<float>(y) * tileSize }, { tileSize, tileSize });
            if (tile == 7 && rectsIntersect(collectRect, tileRect)) {
                coins++;
                interestingMap[y][x] = -1;
            }
            if (tile == 22 && rectsIntersect(collectRect, tileRect)) {
                hasKey = true;
                interestingMap[y][x] = -1;
                std::cout << "Key collected!" << std::endl;
            }
        }
    }

    if (hasKey) {
        for (int y = 0; y < mapHeight; ++y)
            for (int x = 0; x < mapWidth; ++x)
                if (map[y][x] == 12) map[y][x] = 13;
    }

    sf::FloatRect innerBounds = getInnerBounds();

    const float lavaDPS = 60.f;
    bool onLava = false;
    bool onSpike = false;

    int startX = std::max(0, (int)(innerBounds.position.x / tileSize) - 1);
    int endX = std::min(mapWidth, (int)((innerBounds.position.x + innerBounds.size.x) / tileSize) + 2);
    int startY = std::max(0, (int)(innerBounds.position.y / tileSize) - 1);
    int endY = std::min(mapHeight, (int)((innerBounds.position.y + innerBounds.size.y) / tileSize) + 2);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            int t1 = map[y][x];
            int t2 = backgroundMap[y][x];
            int t3 = interestingMap[y][x];
            sf::FloatRect tileRect({ static_cast<float>(x) * tileSize, static_cast<float>(y) * tileSize }, { tileSize, tileSize });

            if (t1 == 16 || t1 == 17 || t2 == 16 || t2 == 17 || t3 == 16 || t3 == 17)
                if (rectsIntersect(innerBounds, tileRect)) onLava = true;

            for (int ti : { t1, t2, t3 })
                if (ti >= 18 && ti <= 21)
                    if (rectsIntersect(innerBounds, getSpikeHitbox(x * tileSize, y * tileSize, tileSize, ti)))
                        onSpike = true;
        }
    }

    if (onLava) {
        lavaDamageAccum += lavaDPS * dt;
        int dmg = static_cast<int>(lavaDamageAccum);
        if (dmg > 0) { applyDamage(dmg); lavaDamageAccum -= static_cast<float>(dmg); }
    }
    else {
        lavaDamageAccum = 0.f;
    }

    if (onSpike && spikeInvulTimer <= 0.f) {
        applyDamage(spikeDamage);
        spikeInvulTimer = spikeInvulDuration;
    }

    if (shape.getPosition().y > mapHeight * tileSize + 100.f) {
        hp = 0; velocity = {}; isDashing = false; dashCooldownTimer = 0.f;
        return;
    }

    for (auto& e : enemies) {
        if (!e.isAlive()) continue;
        int dmg = e.checkAndGetContactDamage(innerBounds, dt);
        if (dmg > 0) applyDamage(dmg);
    }
}

void Player::draw(sf::RenderWindow& window, sf::View& view1, sf::Font& font, bool debugMode, bool pauseMode) {
    window.setView(view1);

    if (hp <= 0) {
        sprite.setColor(sf::Color::White);
        window.draw(sprite);
        window.setView(view1);
        return;
    }

    if (damageFlashTimer > 0.f) {
        float alpha = damageFlashTimer / DAMAGE_FLASH_DURATION;
        sprite.setColor(sf::Color(255, (uint8_t)(255 * (1.f - alpha)), (uint8_t)(255 * (1.f - alpha)), 255));
    }
    else {
        sprite.setColor(sf::Color::White);
    }

    window.draw(sprite);

    if (debugMode) {
        sf::RectangleShape dbgShape(shape.getSize());
        dbgShape.setPosition(shape.getPosition());
        dbgShape.setFillColor(sf::Color::Transparent);
        dbgShape.setOutlineColor(sf::Color::Cyan);
        dbgShape.setOutlineThickness(1.f);
        window.draw(dbgShape);

        sf::FloatRect ib = getInnerBounds();
        sf::RectangleShape dbgInner({ ib.size.x, ib.size.y });
        dbgInner.setPosition(ib.position);
        dbgInner.setFillColor(sf::Color::Transparent);
        dbgInner.setOutlineColor(sf::Color::Yellow);
        dbgInner.setOutlineThickness(1.f);
        window.draw(dbgInner);

        if (attackTimer > 0.f) {
            sf::FloatRect pBounds = shape.getGlobalBounds();
            const float sideRange = 75.f;
            const float vertRange = 60.f;
            float cx = pBounds.position.x + pBounds.size.x * 0.5f;
            float cy = pBounds.position.y + pBounds.size.y * 0.5f;
            float hw = pBounds.size.x * 0.5f;
            float hh = pBounds.size.y * 0.5f;

            sf::FloatRect ar;
            bool ar_ = attackDir.x > 0.f;
            bool al = attackDir.x < 0.f;
            bool au = attackDir.y < 0.f;
            bool ad = attackDir.y > 0.f;

            if (!ar_ && !al && au && !ad) ar = { { cx - hw, cy - hh - vertRange }, { pBounds.size.x, vertRange } };
            else if (!ar_ && !al && !au && ad) ar = { { cx - hw, cy + hh }, { pBounds.size.x, vertRange } };
            else if (ar_ && !al && au && !ad) ar = { { cx, cy - hh - vertRange }, { sideRange, vertRange + hh } };
            else if (!ar_ && al && au && !ad) ar = { { cx - sideRange, cy - hh - vertRange }, { sideRange, vertRange + hh } };
            else if (ar_ && !al && !au && ad) ar = { { cx, cy }, { sideRange, hh + vertRange } };
            else if (!ar_ && al && !au && ad) ar = { { cx - sideRange, cy }, { sideRange, hh + vertRange } };
            else if (ar_) ar = { { cx, cy - hh }, { sideRange, pBounds.size.y } };
            else ar = { { cx - sideRange, cy - hh }, { sideRange, pBounds.size.y  } };

            sf::RectangleShape dbgAtk({ ar.size.x, ar.size.y });
            dbgAtk.setPosition(ar.position);
            dbgAtk.setFillColor(sf::Color(255, 80, 0, 60));
            dbgAtk.setOutlineColor(sf::Color(255, 80, 0));
            dbgAtk.setOutlineThickness(2.f);
            window.draw(dbgAtk);
        }

        if (pauseMode) {
            sf::Text dbgTxt(font,
                "vel: " + std::to_string((int)velocity.x) + "," + std::to_string((int)velocity.y) +
                "\nhp:" + std::to_string(hp) + " sta:" + std::to_string((int)stamina) +
                "\n" + currentAnimation +
                (isDashing ? " [DASH]" : "") +
                (isSliding ? " [SLIDE]" : "") +
                (onGround ? " [GND]" : ""),
                14);
            dbgTxt.setFillColor(sf::Color::White);
            dbgTxt.setOutlineColor(sf::Color::Black);
            dbgTxt.setOutlineThickness(1.f);
            dbgTxt.setPosition({ shape.getPosition().x - 20.f, shape.getPosition().y - 65.f });
            window.draw(dbgTxt);
        }
    }

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
    attackHitDealt = false;
    lavaDamageAccum = 0.f;
    spikeInvulTimer = 0.f;
    wasOnSpike = false;
    currentAnimation = "idle";
    animationFrame = 0;
    animationTimer = 0.f;
    facingRight = true;
    hasKey = false;
    deathAnimationFinished = false;
    deathPosSet = false;
    deathSpritePos = {};
    if (animations.count("idle")) {
        Animation& anim = animations["idle"];
        sprite.setTexture(anim.texture, true);
        if (!anim.frames.empty()) sprite.setTextureRect(anim.frames[0]);
        sprite.setOrigin({ 44.5f, 96.f });
        sprite.setScale({ 0.9f, 0.9f });
    }
    dashAvailable = true;

    jumpHeld = false;
    coyoteTimer = 0.f;
    jumpBufferTimer = 0.f;
    wallJumpLockTimer = 0.f;
    fallTimer = 0.f;
    jumpAnimDone = false;
}

void Player::setSpawnPoint(float x, float y) {
    spawnPoint = { x, y };
}

bool Player::checkWallContactGMap(const GameMap& gmap) {
    sf::FloatRect pb = shape.getGlobalBounds();
    float px = pb.position.x, py = pb.position.y;
    float pw = pb.size.x, ph = pb.size.y;
    constexpr float probe = 5.f;

    wallDirection = 0;
    bool leftWall = false, rightWall = false;

    for (const auto& c : gmap.colliders) {
        float oy = std::min(py + ph, c.y + c.height) - std::max(py, c.y);
        if (oy <= 0.f) continue;
        if (std::abs(px - (c.x + c.width)) < probe) leftWall = true;
        if (std::abs((px + pw) - c.x) < probe) rightWall = true;
    }

    wallBothSides = leftWall && rightWall;
    if (leftWall) { wallDirection = -1; return true; }
    if (rightWall) { wallDirection = 1; return true; }
    return false;
}

void Player::update(float dt, const GameMap& gmap, float tileSize,
    sf::View& view1, sf::RenderWindow& window,
    std::vector<Enemy>& enemies)
{
    if (hp <= 0) {
        if (!deathPosSet) {
            deathSpritePos = shape.getPosition();
            velocity.x *= 0.3f;
            deathPosSet = true;
        }
        if (!onGround) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
            velocity.x *= std::pow(0.85f, dt * 60.f);

            sf::Vector2f nextPos = deathSpritePos + velocity * dt;
            float pw = shape.getSize().x, ph = shape.getSize().y;
            onGround = false;

            for (const auto& c : gmap.colliders) {
                float ox = std::min(nextPos.x + pw, c.x + c.width) - std::max(nextPos.x, c.x);
                float oy = std::min(nextPos.y + ph, c.y + c.height) - std::max(nextPos.y, c.y);
                if (ox <= 0.f || oy <= 0.f) continue;
                if (ox >= oy) {
                    if (nextPos.y < c.y) { nextPos.y -= oy; velocity.y = 0.f; velocity.x = 0.f; onGround = true; }
                }
                else {
                    if (nextPos.x < c.x) nextPos.x -= ox; else nextPos.x += ox;
                    velocity.x = 0.f;
                }
            }
            deathSpritePos = nextPos;
            shape.setPosition(deathSpritePos);
        }
        setAnimation("Death");
        updateAnimation(dt);
        const float sprScale = 0.9f;
        sprite.setOrigin({ 44.5f, 96.f });
        sprite.setScale(facingRight ? sf::Vector2f{ sprScale, sprScale } : sf::Vector2f{ -sprScale, sprScale });
        sf::Vector2f dp = deathSpritePos;
        dp.x += shape.getSize().x * 0.5f;
        dp.y += shape.getSize().y - 3.f;
        sprite.setPosition(dp);
        return;
    }

    if (dashCooldownTimer > 0.f) dashCooldownTimer -= dt;
    if (attackTimer > 0.f) attackTimer -= dt;
    if (hitFreezeTimer > 0.f) hitFreezeTimer -= dt;
    if (damageFlashTimer > 0.f) damageFlashTimer -= dt;
    if (spikeInvulTimer > 0.f) spikeInvulTimer -= dt;
    if (wallJumpLockTimer > 0.f) wallJumpLockTimer -= dt;

    questPollTimer += dt;
    if (questPollTimer >= QUEST_POLL_INTERVAL) {
        questPollTimer = 0.f;
        auto& api = ApiClient::instance();
        if (api.player.loggedIn) {
            api.getQuestsAsync([](std::vector<ApiQuest>) {});
            std::thread([&api]() { api.refreshPlayerAsync(); }).detach();
        }
    }

    bool jumpKeyDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool jumpJustPressed = jumpKeyDown && !jumpHeld;

    if (jumpJustPressed)            jumpBufferTimer = JUMP_BUFFER_TIME;
    else if (jumpBufferTimer > 0.f) jumpBufferTimer -= dt;
    if (onGround)                   coyoteTimer = COYOTE_TIME;
    else if (coyoteTimer > 0.f)     coyoteTimer -= dt;

    if (isDashing) {
        dashTimer -= dt;
        if (dashTimer <= 0.f) { isDashing = false; velocity = {}; }
        else velocity = dashDirection * dashSpeed;
    }
    else {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) &&
            dashCooldownTimer <= 0.f && stamina >= 50.f && (!limitedDashMode || dashAvailable))
        {
            sf::Vector2f dDir{};
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dDir.x = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dDir.x = 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    dDir.y = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  dDir.y = 1.f;
            float len = std::sqrt(dDir.x * dDir.x + dDir.y * dDir.y);
            if (len > 0.f) {
                dashDirection = dDir / len;
                isDashing = true; dashTimer = dashDuration;
                dashCooldownTimer = dashCooldown; stamina -= 50.f;
                if (limitedDashMode) dashAvailable = false;
            }
        }

        bool touchingWall = checkWallContactGMap(gmap);
        if (touchingWall && !onGround && velocity.y > 0.f && wallJumpLockTimer <= 0.f) {
            bool holdingTowardWall = (wallDirection == -1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) ||
                (wallDirection == -1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) ||
                (wallDirection == 1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) ||
                (wallDirection == 1 && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right));
            if (holdingTowardWall) {
                isSliding = true;
                velocity.y = 35.f;
            }
            else {
                isSliding = false;
                if (velocity.y < 120.f) velocity.y = 120.f;
            }
            facingRight = (wallDirection == -1);
            bool wantWallJump = jumpJustPressed || jumpBufferTimer > 0.f;
            if (wantWallJump) {
                if (wallBothSides) { velocity.y = -JUMP_VELOCITY; velocity.x = 0.f; }
                else {
                    velocity.x = static_cast<float>(-wallDirection) * speed * 2.0f;
                    velocity.y = -JUMP_VELOCITY * 0.75f;
                    facingRight = (-wallDirection > 0);
                }
                isSliding = false; wallJumpLockTimer = WALL_JUMP_LOCK_TIME;
                coyoteTimer = 0.f; jumpBufferTimer = 0.f;
                jumpAnimDone = false; setAnimation("Jump");
            }
        }
        else if (!touchingWall || onGround || velocity.y <= 0.f) {
            isSliding = false;
            if (onGround && stamina < maxStamina) {
                stamina += staminaRegenRate * dt;
                if (stamina > maxStamina) stamina = maxStamina;
            }
        }

        if (!isSliding && !isDashing) {
            bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
            bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

            if (hitFreezeTimer > 0.f) { velocity.x = 0.f; }
            else if (wallJumpLockTimer > 0.f) {
                const float accel = speed * 0.5f * dt * 10.f;
                if (left && !right)       velocity.x = std::max(velocity.x - accel, -speed);
                else if (right && !left)  velocity.x = std::min(velocity.x + accel, speed);
            }
            else {
                const float airAccel = 1400.f;
                if (left && !right) {
                    if (onGround) velocity.x = -speed; else velocity.x = std::max(velocity.x - airAccel * dt, -speed);
                    facingRight = false;
                }
                else if (right && !left) {
                    if (onGround) velocity.x = speed; else velocity.x = std::min(velocity.x + airAccel * dt, speed);
                    facingRight = true;
                }
                else {
                    if (onGround) velocity.x = 0.f;
                    else {
                        const float f = 800.f;
                        if (velocity.x > 0.f) velocity.x = std::max(0.f, velocity.x - f * dt);
                        else                  velocity.x = std::min(0.f, velocity.x + f * dt);
                    }
                }
            }

            if (!onGround) {
                if (left && !right) facingRight = false;
                else if (right && !left) facingRight = true;
            }

            bool shouldJump = (coyoteTimer > 0.f) && (jumpJustPressed || jumpBufferTimer > 0.f);
            if (shouldJump) {
                const float DIAG_H_MULT = 1.1f;
                float jumpVx = 0.f;
                if (wallJumpLockTimer <= 0.f) {
                    if (left && !right)      jumpVx = -speed * DIAG_H_MULT;
                    else if (right && !left) jumpVx = speed * DIAG_H_MULT;
                }
                velocity.y = -JUMP_VELOCITY;
                if (!isDashing) velocity.x = jumpVx;
                onGround = false; coyoteTimer = 0.f; jumpBufferTimer = 0.f;
                if (jumpVx < 0.f) facingRight = false; else if (jumpVx > 0.f) facingRight = true;
            }
            if (!jumpKeyDown && velocity.y < -JUMP_CUT_VELOCITY && !isDashing)
                velocity.y = -JUMP_CUT_VELOCITY;
        }

        if (!isDashing) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
        }
    }

    if (sf::Keyboard::isKeyPressed(attackKey) && attackTimer <= 0.f && currentAnimation != "Attack") {
        setAnimation("Attack"); animationFrame = 0; animationTimer = 0.f;
        attackHitDealt = false; attackTimer = attackCooldown;
    }
    if (currentAnimation == "Attack" && !attackHitDealt) {
        bool atkUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
        bool atkDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);
        if (atkUp && !atkDown) attackDir = { 0.f, -1.f };
        else if (atkDown && !atkUp)   attackDir = { 0.f,  1.f };
        else if (facingRight)         attackDir = { 1.f,  0.f };
        else                          attackDir = { -1.f,  0.f };
    }
    if (currentAnimation == "Attack" && animationFrame == 3 && !attackHitDealt) {
        attackHitDealt = true; hitFreezeTimer = HIT_FREEZE_DURATION;
        sf::FloatRect pb = shape.getGlobalBounds();
        const float sR = 75.f, vR = 60.f;
        float cx = pb.position.x + pb.size.x * 0.5f, cy = pb.position.y + pb.size.y * 0.5f;
        float hw = pb.size.x * 0.5f, hh = pb.size.y * 0.5f;
        bool ar = attackDir.x > 0.f, al = attackDir.x < 0.f;
        bool au = attackDir.y < 0.f, ad = attackDir.y > 0.f;
        sf::FloatRect atk;
        if (!ar && !al && au && !ad) atk = { { cx - hw,      cy - hh - vR }, { pb.size.x, vR        } };
        else if (!ar && !al && !au && ad) atk = { { cx - hw,      cy + hh      }, { pb.size.x, vR        } };
        else if (ar && !al && au && !ad) atk = { { cx,           cy - hh - vR }, { sR,        vR + hh   } };
        else if (!ar && al && au && !ad) atk = { { cx - sR,      cy - hh - vR }, { sR,        vR + hh   } };
        else if (ar && !al && !au && ad) atk = { { cx,           cy           }, { sR,        hh + vR   } };
        else if (!ar && al && !au && ad) atk = { { cx - sR,      cy           }, { sR,        hh + vR   } };
        else if (ar)                       atk = { { cx,           cy - hh      }, { sR,        pb.size.y } };
        else                               atk = { { cx - sR,      cy - hh      }, { sR,        pb.size.y } };
        for (auto& e : enemies) {
            if (e.isAlive() && rectsIntersect(atk, e.getBounds())) {
                bool wasAlive = e.isAlive();
                e.takeDamage(attackDamage);
                if (wasAlive && !e.isAlive()) {
                    int idx = (int)(&e - &enemies[0]);
                    MultiplayerClient::instance().sendEnemyKill(idx, 0);
                }
            }
        }
    }

    float pw = shape.getSize().x, ph = shape.getSize().y;
    onGround = false;

    sf::Vector2f posX = { shape.getPosition().x + velocity.x * dt, shape.getPosition().y };
    for (const auto& c : gmap.colliders) {
        float ox = std::min(posX.x + pw, c.x + c.width) - std::max(posX.x, c.x);
        float oy = std::min(posX.y + ph, c.y + c.height) - std::max(posX.y, c.y);
        if (ox <= 0.f || oy <= 0.f) continue;
        if (posX.x + pw * 0.5f < c.x + c.width * 0.5f) posX.x -= ox;
        else                                              posX.x += ox;
        if (!isDashing) velocity.x = 0.f;
    }

    sf::Vector2f posXY = { posX.x, shape.getPosition().y + velocity.y * dt };
    for (const auto& c : gmap.colliders) {
        float ox = std::min(posXY.x + pw, c.x + c.width) - std::max(posXY.x, c.x);
        float oy = std::min(posXY.y + ph, c.y + c.height) - std::max(posXY.y, c.y);
        if (ox <= 0.f || oy <= 0.f) continue;
        float dist = (posXY.y + ph) - c.y;
        if (posXY.y + ph * 0.5f < c.y + c.height * 0.5f) {
            if (dist > 0.f && dist <= LEDGE_FORGIVENESS && velocity.y >= 0.f) {
                posXY.y -= dist; velocity.y = 0.f; onGround = true;
            }
            else if (dist > LEDGE_FORGIVENESS) {
                posXY.y -= oy; velocity.y = 0.f; onGround = true;
            }
        }
        else {
            posXY.y += oy; velocity.y = 0.f;
        }
    }

    shape.setPosition(posXY);

    // Урон от ENT_TRAP / ENT_BACKTRAP (шипы, пилы) из мап-криейтора
    {
        sf::FloatRect ib = getInnerBounds();
        int trapDmg = TrapSystem::checkDamage(ib, gmap, dt);
        if (trapDmg > 0) applyDamage(trapDmg);
    }

    if (limitedDashMode && onGround) dashAvailable = true;
    jumpHeld = jumpKeyDown;

    if (currentAnimation == "Attack") {
        int total = animations.count("Attack") ? (int)animations["Attack"].frames.size() : 1;
        if (animationFrame >= total - 1 && attackTimer <= 0.f) {
            if (!onGround) { fallTimer = 0.f; jumpAnimDone = false; setAnimation("Jump"); }
            else if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
            else setAnimation("idle");
        }
    }
    else if (onGround) {
        fallTimer = 0.f; jumpAnimDone = false;
        if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
        else                    setAnimation("idle");
    }
    else {
        fallTimer += dt;
        if (velocity.y < 0.f && !jumpAnimDone) {
            if (currentAnimation != "Jump") { jumpAnimDone = false; setAnimation("Jump"); }
        }
        else setAnimation("Fall");
    }
    updateAnimation(dt);

    {
        float mapWorldH = static_cast<float>(GMAP_H) * tileSize + 200.f;
        if (shape.getPosition().y > mapWorldH) {
            hp = 0; velocity = {}; isDashing = false; dashCooldownTimer = 0.f;
            return;
        }
    }

    {
        sf::FloatRect innerBounds = getInnerBounds();
        for (auto& e : enemies) {
            if (!e.isAlive()) continue;
            int dmg = e.checkAndGetContactDamage(innerBounds, dt);
            if (dmg > 0) applyDamage(dmg);
        }
    }

    const float sprScale = 0.9f;
    sprite.setOrigin({ 44.5f, 96.f });
    sprite.setScale(facingRight ? sf::Vector2f{ sprScale, sprScale } : sf::Vector2f{ -sprScale, sprScale });
    sf::Vector2f sp = shape.getPosition();
    sp.x += shape.getSize().x * 0.5f;
    sp.y += shape.getSize().y - 3.f;
    sprite.setPosition(sp);
}