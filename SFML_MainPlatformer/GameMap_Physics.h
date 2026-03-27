#pragma once
#include "Gamemap.h"
#include "Player.h"
#include "Enemy.h"
#include <vector>
#include <cmath>

namespace GMP {

    static constexpr float PLAYER_W = 30.f;
    static constexpr float PLAYER_H = 40.f;
    static constexpr float TILE_SIZE = 32.f;
    inline bool isOnGround(sf::Vector2f pos, float pw, float ph,
        const GameMap& gmap, float probeDepth = 2.f)
    {
        float footY = pos.y + ph;
        for (const auto& c : gmap.colliders) {
            if (pos.x + pw <= c.x || pos.x >= c.x + c.width) continue;
            if (footY >= c.y - probeDepth && footY <= c.y + probeDepth)
                return true;
        }
        return false;
    }
    inline int checkWall(sf::Vector2f pos, float pw, float ph,
        const GameMap& gmap, bool& wallBothSides,
        float probe = 5.f)
    {
        bool left = false, right = false;
        for (const auto& c : gmap.colliders) {
            float oy = std::min(pos.y + ph, c.y + c.height) - std::max(pos.y, c.y);
            if (oy <= 0.f) continue;
            if (std::abs(pos.x - (c.x + c.width)) < probe) left = true;
            if (std::abs((pos.x + pw) - c.x) < probe) right = true;
        }
        wallBothSides = left && right;
        if (left)  return -1;
        if (right) return  1;
        return 0;
    }


    struct ResolveResult {
        sf::Vector2f pos;
        sf::Vector2f velocity;
        bool onGround = false;
        bool hitCeiling = false;
        bool hitWallLeft = false;
        bool hitWallRight = false;
    };

    inline ResolveResult resolveMove(sf::Vector2f pos, float pw, float ph,
        sf::Vector2f velocity, float dt,
        const GameMap& gmap,
        float ledgeForgiveness = 5.f)
    {
        ResolveResult res;
        res.velocity = velocity;

        sf::Vector2f posX = pos;
        posX.x += velocity.x * dt;

        for (const auto& c : gmap.colliders) {
            float ox = std::min(posX.x + pw, c.x + c.width) - std::max(posX.x, c.x);
            float oy = std::min(posX.y + ph, c.y + c.height) - std::max(posX.y, c.y);
            if (ox <= 0.f || oy <= 0.f) continue;

            if (posX.x + pw * 0.5f < c.x + c.width * 0.5f) {
                posX.x -= ox;
                res.hitWallRight = true;
            }
            else {
                posX.x += ox;
                res.hitWallLeft = true;
            }
            res.velocity.x = 0.f;
        }

        sf::Vector2f posXY = posX;
        posXY.y += velocity.y * dt;

        for (const auto& c : gmap.colliders) {
            float ox = std::min(posXY.x + pw, c.x + c.width) - std::max(posXY.x, c.x);
            float oy = std::min(posXY.y + ph, c.y + c.height) - std::max(posXY.y, c.y);
            if (ox <= 0.f || oy <= 0.f) continue;

            if (posXY.y + ph * 0.5f < c.y + c.height * 0.5f) {
                float dist = (posXY.y + ph) - c.y;
                if (dist > 0.f && dist <= ledgeForgiveness && velocity.y >= 0.f) {
                    posXY.y -= dist;
                    res.velocity.y = 0.f;
                    res.onGround = true;
                }
                else if (dist > ledgeForgiveness) {
                    posXY.y -= oy;
                    res.velocity.y = 0.f;
                    res.onGround = true;
                }
            }
            else {
                posXY.y += oy;
                res.velocity.y = 0.f;
                res.hitCeiling = true;
            }
        }

        res.pos = posXY;
        return res;
    }

    template<int H, int W>
    inline void buildMapFromColliders(int(&outMap)[H][W + 1],
        const GameMap& gmap,
        float tileSize,
        int solidTileId = 1)
    {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x <= W; ++x)
                outMap[y][x] = -1;

        for (const auto& c : gmap.colliders) {
            int tx0 = std::max(0, (int)std::floor(c.x / tileSize));
            int ty0 = std::max(0, (int)std::floor(c.y / tileSize));
            int tx1 = std::min(W - 1, (int)std::ceil((c.x + c.width) / tileSize) - 1);
            int ty1 = std::min(H - 1, (int)std::ceil((c.y + c.height) / tileSize) - 1);
            for (int ty = ty0; ty <= ty1; ++ty)
                for (int tx = tx0; tx <= tx1; ++tx)
                    outMap[ty][tx] = solidTileId;
        }
    }

}
