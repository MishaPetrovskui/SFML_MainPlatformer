#include "Player.h"
#include <algorithm>

Player::Player(float startX, float startY) {
    shape.setSize({ 30.f, 40.f });
    shape.setFillColor(sf::Color::Cyan);
    shape.setPosition({ startX, startY });

    velocity = { 0.f, 0.f };
    speed = 200.f;
    gravity = 900.f;
    onGround = false;
}

void Player::update(float dt, char map[][501], int mapWidth, int mapHeight, float tileSize, sf::View& view1, sf::Vector2f& view1POS, sf::RenderWindow& window) {
    // ”правл≥нн€
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
    {
        velocity.x = -speed;
        view1.move({ -speed * dt, 0.f });
    }
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
    {
        velocity.x = speed;
		view1.move({ speed * dt, 0.f });

    }
    else
        velocity.x = 0.f;

    if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) && onGround) {
        velocity.y = -420.f;
        onGround = false;
    }

    velocity.y += gravity * dt;
    if (velocity.y > 1000.f) velocity.y = 1000.f;

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
                    velocity.x = 0.f;
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
}

void Player::draw(sf::RenderWindow& window, sf::View& view1) {
    window.draw(shape);
    window.setView(view1);
}