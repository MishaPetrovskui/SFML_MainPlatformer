#pragma once
// TrapSystem.h — только урон от ENT_TRAP / ENT_BACKTRAP
// Рендер делает Gamemap::drawEntities (как в мап-криейторе).
// Пила (SAW_TEX_ID=7): drawEntities обрезает левую половину текстуры.

#include "Gamemap.h"
#include <SFML/Graphics.hpp>

namespace TrapSystem {

    static constexpr int   SAW_TEX_ID = 7;
    static constexpr float SPIKE_DPS = 40.f;
    static constexpr float SAW_DPS = 80.f;
    static constexpr float INVUL_DUR = 0.40f;

    static float _invulTimer = 0.f;
    static float _accumDmg = 0.f;

    inline void reset() { _invulTimer = 0.f; _accumDmg = 0.f; }
    inline void update(float dt, const GameMap&) { if (_invulTimer > 0.f) _invulTimer -= dt; }

    inline int checkDamage(const sf::FloatRect& pb,
        const GameMap& gmap,
        float dt,
        float tileSize = 32.f)
    {
        if (_invulTimer > 0.f) return 0;

        auto ov = [](const sf::FloatRect& a, const sf::FloatRect& b) {
            return a.position.x < b.position.x + b.size.x &&
                a.position.x + a.size.x > b.position.x &&
                a.position.y < b.position.y + b.size.y &&
                a.position.y + a.size.y > b.position.y;
            };

        float dps = 0.f;
        for (auto& e : gmap.entities) {
            if (e.type != ENT_TRAP && e.type != ENT_BACKTRAP) continue;
            if (e.textureId < 0) continue;
            if (!ov(pb, sf::FloatRect(e.position, { tileSize, tileSize }))) continue;
            dps += (e.textureId == SAW_TEX_ID) ? SAW_DPS : SPIKE_DPS;
        }

        if (dps <= 0.f) { _accumDmg = 0.f; return 0; }
        _accumDmg += dps * dt;
        int dmg = (int)_accumDmg;
        if (dmg > 0) { _accumDmg -= dmg; _invulTimer = INVUL_DUR; }
        return dmg;
    }

} // namespace TrapSystem