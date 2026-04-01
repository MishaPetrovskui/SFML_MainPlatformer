#pragma once
// GameScene.h — игровая сцена
// Рендер ловушек (шипы/кристаллы/пила) 1-в-1 с мап-криейтором:
//   gmap.updateTraps(dt)        — анимация пилы
//   gmap.drawEntities(...)      — отрисовка (ENT_BACKTRAP и ENT_TRAP)
// TrapSystem используется ТОЛЬКО для расчёта урона (checkDamage).

#include "Gamemap.h"
#include "TrapSystem.h"
#include "Player.h"
#include "Enemy.h"
#include "MultiplayerClient.h"
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

class GameScene {
public:
    GameMap   gmap;
    Player* player = nullptr;
    std::vector<Enemy> enemies;

    // spriteSheet нужен только для GhostRenderer / UI-иконок,
    // ловушки теперь рисуются через gmap.sheet (как в мап-криейторе).
    std::map<int, sf::Sprite> spriteSheet;

    bool useFog = true;
    bool debugMode = false;
    float tileSize = 32.f;

    // Загрузить карту + текстуры. tilesConfig = путь к tiles.cfg (id path\n...)
    bool init(sf::RenderWindow& window, const char* mapPath,
        const std::string& tilesConfig = "")
    {
        if (!tilesConfig.empty())
            gmap.loadTextures(tilesConfig);
        else
            _loadDefaultTextures();

        if (!gmap.load(mapPath)) return false;
        gmap.calculateStaticLight(tileSize);
        if (useFog) gmap.initFog(window.getSize().x, window.getSize().y);

        // Сбрасываем счётчик неуязвимости урона от ловушек
        TrapSystem::reset();

        // Спавним врагов из entities
        enemies.clear();
        static sf::Texture _fallback;
        if (_fallback.getSize().x == 0)
            _fallback.resize({ 32, 32 });

        for (auto& e : gmap.entities) {
            if (e.type != ENT_ENEMY) continue;
            Enemy en(e.position.x, e.position.y, _fallback, tileSize);
            en.initFromMapEntity(e.health, e.damage, e.speed, e.light);
            en.loadAnimations("Sprites/slime_walk.png", "Sprites/slime_attack.png");
            enemies.push_back(std::move(en));
        }

        return true;
    }

    // Вызывать каждый кадр
    void update(float dt, sf::View& gameView, sf::RenderWindow& window) {
        if (!player) return;

        // ── Урон от ловушек (только счётчик invul) ────────────────────────────
        TrapSystem::update(dt, gmap);

        player->update(dt, gmap, tileSize, gameView, window, enemies);

        // Строим tile-map для Enemy::update
        static int  _enemyMap[500][501] = {};
        static bool _mapBuilt = false;
        if (!_mapBuilt) {
            for (int y = 0; y < 500; ++y)
                for (int x = 0; x < 500; ++x)
                    _enemyMap[y][x] = gmap.tiles[y * GMAP_W + x];
            _mapBuilt = true;
        }

        sf::Vector2f playerPos = player->getPosition();
        playerPos.x += 15.f; playerPos.y += 20.f;
        for (auto& e : enemies)
            e.update(dt, playerPos, _enemyMap, 500, 500, tileSize);

        // Мультиплеер: синхронизация убийств врагов
        auto kills = MultiplayerClient::instance().getAndClearEnemyKills();
        for (int idx : kills)
            if (idx >= 0 && idx < (int)enemies.size())
                enemies[idx].takeDamage(9999);
    }

    // Рендер — ТОЧНО как в мап-криейторе
    void draw(sf::RenderWindow& window, sf::View& gameView, sf::Font& font) {
        window.setView(gameView);

        // 1. Фон
        gmap.drawBackground(window, tileSize, gameView, useFog);

        // 2. Фоновые ловушки (ENT_BACKTRAP) — за тайлами
        //    gmap.drawEntities использует gmap.sheet + gmap._trapAnimFrame,
        //    то есть в точности то же, что мап-криейтор.
        gmap.drawEntities(window, gameView, /*backOnly=*/true, useFog, tileSize);

        // 3. Тайлы переднего плана
        gmap.drawTiles(window, tileSize, gameView, useFog);

        // 4. Передние ловушки (ENT_TRAP) — между тайлами и персонажами
        gmap.drawEntities(window, gameView, /*backOnly=*/false, useFog, tileSize);

        // 5. Враги
        for (auto& e : enemies) {
            if (!e.isAlive()) continue;
            sf::Vector2f ec = e.getCenter();
            int etx = std::clamp((int)(ec.x / tileSize), 0, GMAP_W - 1);
            int ety = std::clamp((int)(ec.y / tileSize), 0, GMAP_H - 1);
            float lv = useFog
                ? std::clamp(LightConst::TILES_MIN + gmap.getSmoothLight(etx, ety) * 0.9f, 0.f, 1.f)
                : 1.f;
            e.draw(window, debugMode, lv);
        }

        // 6. Игрок
        if (player) player->draw(window, gameView, font, debugMode);

        // 7. Туман (BlendMultiply поверх всего)
        if (useFog) {
            std::vector<LightSource> dynLights;
            if (player) {
                sf::Vector2f pp = player->getPosition();
                dynLights.push_back({ { pp.x + 15.f, pp.y + 20.f },
                                      LightConst::PLAYER_R, LightConst::PLAYER_COL });
            }
            for (auto& e : enemies) {
                if (e.isAlive() && e.getLightEmit() > 0.f)
                    dynLights.push_back({ e.getCenter(), e.getLightRadius(), e.getLightColor() });
            }
            gmap.buildFog(gameView, dynLights);
            gmap.applyFog(window);
        }

        if (debugMode) gmap.drawColliders(window);
    }

private:
    // Загрузка дефолтных текстур — точно как в мап-криейторе
    void _loadDefaultTextures() {
        // Обычные текстуры — грузим целиком
        struct TexDef { int id; const char* path; };
        static const TexDef defs[] = {
            { 0,  "Sprites/Undefined.png"     },
            { 1,  "Sprites/rock_6.png"        },
            { 2,  "Sprites/slime 2.png"       },
            { 3,  "Sprites/background.png"    },
            { 4,  "Sprites/StoneBrick.png"    },
            { 5,  "Sprites/StoneBrickBack.png"},
            { 8,  "Sprites/SmallSpike.png"    },
            { 9,  "Sprites/spikes.png"        },
            { 10, "Sprites/Spikes_3.png"      },
            { 11, "Sprites/Spikes_4.png"      },
        };
        for (auto& d : defs) {
            gmap.textures[d.id] = sf::Texture();
            if (gmap.textures[d.id].loadFromFile(d.path)) {
                gmap.sheet.insert_or_assign(d.id, sf::Sprite(gmap.textures[d.id]));
                spriteSheet.insert_or_assign(d.id, sf::Sprite(gmap.textures[d.id]));
            }
        }

        // Факел — обрезаем до 32x32 (как в мап-криейторе)
        gmap.textures[6] = sf::Texture();
        if (gmap.textures[6].loadFromFile("Sprites/Torch.png", true, sf::IntRect({ 0, 0 }, { 32, 32 }))) {
            gmap.sheet.insert_or_assign(6, sf::Sprite(gmap.textures[6]));
            spriteSheet.insert_or_assign(6, sf::Sprite(gmap.textures[6]));
        }

        // Пила — обрезаем до 32x32 (левая половина), точно как в мап-криейторе:
        // tx_Saw.loadFromFile("Sprites/Saw.png", true, IntRect({ 0,0 }, { 32, 32 }));
        gmap.textures[7] = sf::Texture();
        if (gmap.textures[7].loadFromFile("Sprites/Saw.png", true, sf::IntRect({ 0, 0 }, { 32, 32 }))) {
            gmap.sheet.insert_or_assign(7, sf::Sprite(gmap.textures[7]));
            spriteSheet.insert_or_assign(7, sf::Sprite(gmap.textures[7]));
        }
    }
};