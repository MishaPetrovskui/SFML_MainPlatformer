#include "Player.h"
#include <algorithm>

Player::Player(float startX, float startY) {
    shape.setSize({ 30.f, 40.f });
    shape.setFillColor(sf::Color::Red);
    shape.setPosition({ startX, startY });
    spawnPoint = { startX, startY };

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
}

bool Player::checkWallContact(char map[][501], int mapWidth, int mapHeight, float tileSize) {
    sf::FloatRect playerBounds = shape.getGlobalBounds();
    float px = playerBounds.position.x;
    float py = playerBounds.position.y;
    float pw = playerBounds.size.x;
    float ph = playerBounds.size.y;

    wallDirection = 0;

    // Проверка стен слева и справа
    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            char tile = map[y][x];
            if (tile == ' ' || tile == '\0') continue;

            float tx = x * tileSize;
            float ty = y * tileSize;
            float tw = tileSize;
            float th = tileSize;

            // Проверка пересечения по вертикали
            if (py + ph > ty && py < ty + th) {
                // Проверка касания левой стороны игрока с правой стороной блока
                if (std::abs(px - (tx + tw)) < 5.f) {
                    wallDirection = -1; // стена слева
                    return true;
                }
                // Проверка касания правой стороны игрока с левой стороной блока
                if (std::abs((px + pw) - tx) < 5.f) {
                    wallDirection = 1; // стена справа
                    return true;
                }
            }
        }
    }

    return false;
}

void Player::update(float dt, char map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::RenderWindow& window) {

    // Обновление кулдауна деша
    if (dashCooldownTimer > 0.f) {
        dashCooldownTimer -= dt;
    }

    // Логика деша
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
        // Активация деша (Shift)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) && dashCooldownTimer <= 0.f && stamina >= 50.f) {
            sf::Vector2f dashDir = { 0.f, 0.f };

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
                dashDir.x = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
                dashDir.x = 1.f;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
                dashDir.y = -1.f;
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
                dashDir.y = 1.f;

            // Нормализация направления
            float length = std::sqrt(dashDir.x * dashDir.x + dashDir.y * dashDir.y);
            if (length > 0.f) {
                dashDirection = dashDir / length;
                isDashing = true;
                dashTimer = dashDuration;
                dashCooldownTimer = dashCooldown;
                stamina -= 50.f; // расход стамины на деш
            }
        }

        // Проверка контакта со стеной
        bool touchingWall = checkWallContact(map, mapWidth, mapHeight, tileSize);

        // Скольжение по стене
        if (touchingWall && !onGround && velocity.y > 0.f) {
            // Проверка удержания клавиши в сторону стены
            bool pressingIntoWall = false;
            if (wallDirection == -1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))) {
                pressingIntoWall = true;
            }
            else if (wallDirection == 1 && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))) {
                pressingIntoWall = true;
            }

            if (pressingIntoWall && stamina > 0.f) {
                isSliding = true;
                velocity.y = 50.f; // медленное скольжение вниз
                stamina -= staminaConsumption * dt;
                if (stamina < 0.f) stamina = 0.f;

                shape.setFillColor(sf::Color::Yellow); // визуальный индикатор скольжения
            }
            else {
                isSliding = false;
                shape.setFillColor(sf::Color::Red);
            }
        }
        else {
            isSliding = false;
            if (!isDashing) {
                shape.setFillColor(sf::Color::Red);
            }

            // Восстановление стамины на земле
            if (onGround && stamina < maxStamina) {
                stamina += staminaRegenRate * dt;
                if (stamina > maxStamina) stamina = maxStamina;
            }
        }

        // Отталкивание от стены при скольжении
        if (isSliding) {
            // Прыжок вверх от стены (Space/W)
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) {

                if (stamina >= 20.f) { // минимальная стамина для прыжка
                    velocity.y = -420.f;
                    velocity.x = -wallDirection * speed * 1.5f; // отталкивание в противоположную сторону
                    stamina -= 20.f;
                    isSliding = false;
                    shape.setFillColor(sf::Color::Red);
                }
            }
            // Отскок вниз от стены (S)
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                velocity.y = 200.f;
                velocity.x = -wallDirection * speed * 1.2f;
                isSliding = false;
                shape.setFillColor(sf::Color::Red);
            }
        }

        // Обычное управление (если не скользим и не дэшим)
        if (!isSliding && !isDashing) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
                velocity.x = -speed;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
                velocity.x = speed;
            }
            else {
                velocity.x = 0.f;
            }

            // Обычный прыжок с земли
            if ((/*sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||*/
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) && onGround) {
                velocity.y = -420.f;
                onGround = false;
            }
        }

        // Гравитация
        if (!isDashing) {
            velocity.y += gravity * dt;
            if (velocity.y > 1000.f) velocity.y = 1000.f;
        }
    }

    // Визуальный индикатор деша
    if (isDashing) {
        shape.setFillColor(sf::Color::Cyan);
    }

    // Движение и коллизии
    sf::Vector2f nextPos = shape.getPosition() + velocity * dt;
    float px = nextPos.x;
    float py = nextPos.y;
    float pw = shape.getSize().x;
    float ph = shape.getSize().y;

    onGround = false;

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            char tile = map[y][x];
            if (tile == ' ' || tile == '\0') continue;

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

    // Проверка падения (возврат на спавн)
    float mapHeightPx = mapHeight * tileSize;
    if (shape.getPosition().y > mapHeightPx + 100.f) {
        shape.setPosition(spawnPoint);
        lives--;
        velocity = { 0.f, 0.f };
        stamina = maxStamina;
        isDashing = false;
        dashCooldownTimer = 0.f;
        shape.setFillColor(sf::Color::Red);
    }
}

void Player::draw(sf::RenderWindow& window, sf::View& view1) {
    window.draw(shape);

    // Отрисовка полоски стамины
    sf::RectangleShape staminaBar;
    staminaBar.setSize({ stamina, 5.f });
    staminaBar.setFillColor(sf::Color::Green);
    staminaBar.setPosition({ shape.getPosition().x, shape.getPosition().y - 10.f });
    window.draw(staminaBar);

    // Фон полоски стамины
    sf::RectangleShape staminaBg;
    staminaBg.setSize({ maxStamina, 5.f });
    staminaBg.setFillColor(sf::Color(50, 50, 50));
    staminaBg.setPosition({ shape.getPosition().x, shape.getPosition().y - 10.f });
    window.draw(staminaBg);
    window.draw(staminaBar);

    window.setView(view1);
}

void Player::reset() {
    shape.setPosition(spawnPoint);
    velocity = { 0.f, 0.f };
    stamina = maxStamina;
    lives = 1;
    isDashing = false;
    dashCooldownTimer = 0.f;
    shape.setFillColor(sf::Color::Red);
}
