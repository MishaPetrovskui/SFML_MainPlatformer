#include "Player.h"
#include "Enemy.h"
#include "ApiClient.h"
#include <algorithm>

// using namespace в .cpp — нормально, конфликт byte был только из-за заголовка
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
    const std::string& fallPath)
{
    // --- walkToRight / walkToLeft ---
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
    // walkToLeft — та же текстура, те же фреймы (флип через setScale)
    animations["walkToLeft"].texture.loadFromFile(walkPath);
    animations["walkToLeft"].frames = animations["walkToRight"].frames;

    // --- Attack ---
    animations["Attack"].texture.loadFromFile(attackPath);
    animations["Attack"].frames = {
        IntRect({0,   0}, {128, 128}), IntRect({128, 0}, {128, 128}),
        IntRect({256, 0}, {128, 128}), IntRect({384, 0}, {128, 128}),
        IntRect({512, 0}, {128, 128}), IntRect({640, 0}, {128, 128}),
        IntRect({768, 0}, {128, 128}), IntRect({896, 0}, {128, 128}),
        IntRect({1024,0}, {128, 128}),
    };

    // --- Idle ---
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

    // --- Jump / Death — переиспользуем idle ---
    animations["Jump"].texture.loadFromFile(idlePath);
    animations["Jump"].frames = animations["idle"].frames;

    // --- Fall ---
    // Если есть fall.png — грузим его (128×128 фреймы в ряд, любое кол-во).
    // Иначе fallback: первые 4 фрейма idle (нейтральная поза в воздухе).
    // Имя файла: "fall.png" — тот же формат что walk/idle/attack.
    if (!fallPath.empty()) {
        animations["Fall"].texture.loadFromFile(fallPath);
        // Читаем столько фреймов сколько есть в файле — по ширине текстуры
        auto& fallTex = animations["Fall"].texture;
        int frameCount = (int)(fallTex.getSize().x / 128);
        if (frameCount < 1) frameCount = 1;
        for (int i = 0; i < frameCount; ++i)
            animations["Fall"].frames.push_back(IntRect({ i * 128, 0 }, { 128, 128 }));
    }
    else {
        // Fallback: берём первые 4 фрейма idle — персонаж висит в нейтральной позе
        animations["Fall"].texture.loadFromFile(idlePath);
        animations["Fall"].frames = {
            IntRect({0,   0}, {128, 128}),
            IntRect({128, 0}, {128, 128}),
            IntRect({256, 0}, {128, 128}),
            IntRect({384, 0}, {128, 128}),
        };
    }

    animations["Death"].texture.loadFromFile(walkPath);
    animations["Death"].frames = { IntRect({0, 0}, {128, 128}) };
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

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            int tile = map[y][x];
            if (tile == -1) continue;
            float tx = x * tileSize;
            float ty = y * tileSize;
            float tw = tileSize;
            float th = tileSize;
            if (py + ph > ty && py < ty + th) {
                if (std::abs(px - (tx + tw)) < 5.f) { wallDirection = -1; return true; }
                if (std::abs((px + pw) - tx) < 5.f) { wallDirection = 1; return true; }
            }
        }
    }
    return false;
}

void Player::updateAnimation(float dt) {
    if (animations.find(currentAnimation) == animations.end()) return;
    Animation& anim = animations[currentAnimation];
    if (anim.frames.empty()) return;

    float spd = (currentAnimation == "idle") ? 0.12f
        : (currentAnimation == "Fall") ? 0.07f   // быстрее — вступление проходит за ~0.5с
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
        else if (currentAnimation == "Fall") {
            int total = (int)anim.frames.size();
            if (animationFrame >= total) {
                // Вступление закончено — зацикливаем последние 4 фрейма
                // Если фреймов меньше 4 — зацикливаем последний один
                int loopStart = std::max(0, total - 4);
                // Вычисляем позицию внутри петли чтобы не прыгать на начало
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
        setAnimation("Death");
        updateAnimation(dt);
        sprite.setPosition(shape.getPosition());
        return;
    }

    if (dashCooldownTimer > 0.f) dashCooldownTimer -= dt;
    if (attackTimer > 0.f) attackTimer -= dt;
    if (hitFreezeTimer > 0.f) hitFreezeTimer -= dt;
    if (damageFlashTimer > 0.f) damageFlashTimer -= dt;
    if (spikeInvulTimer > 0.f) spikeInvulTimer -= dt;
    if (wallJumpLockTimer > 0.f) wallJumpLockTimer -= dt;

    // Quest polling: каждые QUEST_POLL_INTERVAL секунд запрашиваем квесты с сервера.
    // Это означает что если игрок заклеймил награду на сайте — игра подхватит
    // обновлённое состояние без перелогина. Реализовано через ApiClient.
    questPollTimer += dt;
    if (questPollTimer >= QUEST_POLL_INTERVAL) {
        questPollTimer = 0.f;
        // Вызываем refresh через ApiClient — он обновит coins и quests асинхронно
        auto& api = ApiClient::instance();
        if (api.player.loggedIn) {
            // Перезапрашиваем данные игрока (монеты могут измениться после клейма на сайте)
            api.getQuestsAsync([](std::vector<ApiQuest>) {});  // просто обновляем кэш
            // Синхронизируем монеты через /api/player/me
            std::thread([&api]() {
                // Используем внутренний getAuth — публичный только через friend или добавить метод
                // Смотри ниже: нужно добавить refreshPlayerAsync в ApiClient
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
            bool pressingIntoWall =
                (wallDirection == -1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) ||
                (wallDirection == 1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)));

            if (pressingIntoWall && stamina > 0.f) {
                isSliding = true;
                velocity.y = 50.f;
                stamina -= staminaConsumption * dt;
                if (stamina < 0.f) stamina = 0.f;

                facingRight = (wallDirection == -1);
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
            bool wantWallJump = jumpJustPressed || jumpBufferTimer > 0.f;

            if (wantWallJump) {
                velocity.y = -JUMP_VELOCITY * 0.88f;
                velocity.x = static_cast<float>(-wallDirection) * speed * 2.f;
                isSliding = false;
                wallJumpLockTimer = WALL_JUMP_LOCK_TIME;
                coyoteTimer = 0.f;
                jumpBufferTimer = 0.f;
                facingRight = (-wallDirection > 0);
                setAnimation("Jump");
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                velocity.y = 200.f;
                velocity.x = static_cast<float>(-wallDirection) * speed * 1.2f;
                isSliding = false;
            }
        }

        if (!isSliding && !isDashing) {

            if (wallJumpLockTimer <= 0.f) {
                bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
                bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

                if (hitFreezeTimer > 0.f) {
                    velocity.x = 0.f;  // заморозка при ударе
                }
                else if (left && !right) {
                    velocity.x = -speed;
                    facingRight = false;
                }
                else if (right && !left) {
                    velocity.x = speed;
                    facingRight = true;
                }
                else {
                    velocity.x = 0.f;
                }
            }
            bool canJump = (coyoteTimer > 0.f);
            bool shouldJump = canJump && (jumpJustPressed || jumpBufferTimer > 0.f);

            if (shouldJump) {
                const float DIAG_H_MULT = 1.1f;
                bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
                bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

                float jumpVx = 0.f;
                if (wallJumpLockTimer <= 0.f) {
                    if (left && !right) jumpVx = -speed * DIAG_H_MULT;
                    else if (right && !left)  jumpVx = speed * DIAG_H_MULT;
                }

                velocity.y = -JUMP_VELOCITY;
                if (!isDashing) velocity.x = jumpVx;
                onGround = false;
                coyoteTimer = 0.f;
                jumpBufferTimer = 0.f;

                if (jumpVx < 0.f) facingRight = false;
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

    // До удара (frame < 4) — направление атаки следует за игроком каждый кадр
    // На frame 4 — фиксируем направление и наносим урон
    if (currentAnimation == "Attack" && !attackHitDealt) {
        bool atkUp = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
        bool atkDown = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

        if (atkUp && !atkDown)          attackDir = { 0.f, -1.f };
        else if (atkDown && !atkUp)          attackDir = { 0.f,  1.f };
        else if (facingRight)                attackDir = { 1.f,  0.f };
        else                                 attackDir = { -1.f, 0.f };
    }

    // Deal damage at hit frame (frame 4 = peak of sword swing)
    if (currentAnimation == "Attack" && animationFrame == 4 && !attackHitDealt) {
        attackHitDealt = true;
        hitFreezeTimer = HIT_FREEZE_DURATION;  // заморозить игрока при ударе
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
            if (rectsIntersect(attackRect, e.getBounds()))
                e.takeDamage(attackDamage);
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

    //  if (currentAnimation == "Attack") {
    //      if (animationFrame >= (int)animation["Attack"].size() - 1 && attackTimer <= 0.f) {
    //          if (!onGround)          setAnimation("Jump");
    //          else if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
    //          else                    setAnimation("idle");
    //      }
    //  }

    //  if (!onGround) {
    //      setAnimation("Jump");
    //  }
    //  else if (velocity.x != 0.f) {
    //      setAnimation(facingRight ? "walkToRight" : "walkToLeft");
    //  }
    //  else {
    //      //if (attackTimer > 0.f || (sf::Keyboard::isKeyPressed(attackKey) && attackTimer <= 0.f)) {
    //      //    if (currentAnimation != "Attack") setAnimation("Attack");
    //      //}
    //      //else {
    //      //    setAnimation("idle");
    //      //}
          //setAnimation("idle");
    //  }
    if (currentAnimation == "Attack") {
        int attackFrameCount = animations.count("Attack") ? (int)animations["Attack"].frames.size() : 1;
        if (animationFrame >= attackFrameCount - 1 && attackTimer <= 0.f) {
            if (!onGround && velocity.y > 0.f)
                setAnimation("Fall");
            else if (!onGround)
                setAnimation("Jump");
            else if (velocity.x != 0.f) setAnimation(facingRight ? "walkToRight" : "walkToLeft");
            else setAnimation("idle");
        }
    }
    else if (!onGround) {
        if (velocity.y <= 0.f) {
            // Летим вверх — всегда Jump, сбрасываем таймер падения
            fallTimer = 0.f;
            setAnimation("Jump");
        }
        else {
            // Летим вниз — накапливаем таймер
            fallTimer += dt;
            if (fallTimer >= FALL_DELAY) {
                // Достаточно долго падаем — включаем Fall
                if (currentAnimation != "Fall")
                    setAnimation("Fall");
                // Fall уже играет — не трогаем (не сбрасываем фрейм)
            }
            // иначе ещё держим Jump (короткий прыжок — Fall не мелькнёт)
        }
    }
    else if (velocity.x != 0.f) {
        setAnimation(facingRight ? "walkToRight" : "walkToLeft");
        fallTimer = 0.f;
    }
    else {
        setAnimation("idle");
        fallTimer = 0.f;
    }

    updateAnimation(dt);

    // Спрайт: origin на центре тела (x=44.5) и ногах (y=96), флип через setScale
    const float sprScale = 0.9f;
    const float FEET_ROW = 96.f;
    const float BODY_CX = 44.5f;

    sprite.setOrigin({ BODY_CX, FEET_ROW });
    sprite.setScale(facingRight ? sf::Vector2f{ sprScale, sprScale }
    : sf::Vector2f{ -sprScale, sprScale });

    sf::Vector2f spritePos = shape.getPosition();
    spritePos.x += shape.getSize().x * 0.5f;  // центр хитбокса по X
    spritePos.y += shape.getSize().y;          // низ хитбокса по Y
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

    if (damageFlashTimer > 0.f) {
        float alpha = damageFlashTimer / DAMAGE_FLASH_DURATION;
        sprite.setColor(sf::Color(255, (uint8_t)(255 * (1.f - alpha)), (uint8_t)(255 * (1.f - alpha)), 255));
    }
    else {
        sprite.setColor(sf::Color::White);
    }

    window.draw(sprite);

    if (debugMode) {
        // Hitbox (cyan = outer shape, yellow = inner bounds) — always in debugMode
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

        // Stats text — only in pause mode
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
}

void Player::setSpawnPoint(float x, float y) {
    spawnPoint = { x, y };
}