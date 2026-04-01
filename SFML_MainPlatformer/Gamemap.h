#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <memory>
#include <algorithm>
#include <queue>

static constexpr int GMAP_W = 500;
static constexpr int GMAP_H = 500;

struct MapHeader { int magic, version; };

struct Collider {
    float x, y, width, height;
    Collider() : x(-1), y(-1), width(0), height(0) {}
    Collider(float X, float Y, float W, float H) : x(X), y(Y), width(W), height(H) {}
};

enum EntityType {
    ENT_NONE = 0,
    ENT_ENEMY = 1,
    ENT_NPC = 2,
    ENT_STARTPOINT = 3,
    ENT_SPAWNPOINT = 4,
    ENT_FINISH = 5,
    ENT_COIN = 6,
    ENT_TRAP = 7,
    ENT_LIGHTELEMENT = 8,
    ENT_DECORATION = 9,
    ENT_BACKTRAP = 10
};

struct MapEntity {
    sf::Vector2f position;
    int textureId;
    EntityType type;
    float speed;
    int health, defense, damage;
    float light = 0.f;
    MapEntity() :position(-1, -1), textureId(-1), type(ENT_NONE),
        speed(0), health(0), defense(0), damage(0), light(0) {
    }
};

// Описывает динамический источник света (игрок, враги, частицы)
struct LightSource {
    sf::Vector2f worldPos;
    float        radius;      // радиус в пикселях мира
    sf::Color    color;       // цвет в центре; alpha = интенсивность
};

// Константы освещения — меняй здесь чтобы настроить "темноту" игры
namespace LightConst {
    // BFS-затухание для статической карты
    static constexpr float AIR_DECAY = 0.045f;
    static constexpr float BLOCK_DECAY = 0.22f;

    // Цвет "пустоты" (без источников) — очень тёмно-синий
    static constexpr uint8_t FOG_R = 8;
    static constexpr uint8_t FOG_G = 10;
    static constexpr uint8_t FOG_B = 20;

    // Минимальная яркость тайла (0..1) — чтобы карта не была 100% чёрной
    static constexpr float TILE_MIN = 0.04f;  // фон
    static constexpr float TILES_MIN = 0.07f;  // передний план

    // Масштаб light-значения из мап-криейтора → радиус в пикселях мира
    static constexpr float LIGHT_PX = 350.f;

    // Радиус света игрока (пиксели мира)
    static constexpr float PLAYER_R = 170.f;
    static constexpr sf::Color PLAYER_COL = sf::Color(195, 185, 155, 255);
}

class GameMap {
public:
    std::unique_ptr<int[]> background;
    std::unique_ptr<int[]> tiles;
    std::vector<MapEntity> entities;
    std::vector<Collider>  colliders;
    std::map<int, sf::Texture> textures;
    std::map<int, sf::Sprite>  sheet;

    std::vector<float> lightMap;

    GameMap() {
        background = std::make_unique<int[]>(GMAP_W * GMAP_H);
        tiles = std::make_unique<int[]>(GMAP_W * GMAP_H);
        std::fill(background.get(), background.get() + GMAP_W * GMAP_H, -1);
        std::fill(tiles.get(), tiles.get() + GMAP_W * GMAP_H, -1);
        lightMap.assign(GMAP_W * GMAP_H, 0.f);
    }

    // ── Загрузка текстур ──────────────────────────────────────────────────────
    int loadTextures(const std::string& configPath) {
        textures.clear(); sheet.clear();
        std::ifstream f(configPath);
        if (!f.is_open()) {
            std::cout << "[GameMap] texture config not found: " << configPath << "\n";
            return 0;
        }
        int loaded = 0;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            int id; std::string path;
            if (!(ss >> id >> path)) continue;

            // Fallback path
            auto tryLoad = [&](const std::string& p) -> bool {
                return textures[id].loadFromFile(p);
                };
            textures[id] = sf::Texture();
            bool ok = tryLoad(path);
            if (!ok) {
                std::string alt = "Sprites/" + path.substr(path.find_last_of("/\\") + 1);
                ok = tryLoad(alt);
                if (ok) path = alt;
            }

            if (ok) {
                // id=7 (Saw): грузим ПОЛНЫЙ стрип (64x32) для анимации.
                // id=6 (Torch): только первый кадр 32x32 — как в мап-криейторе.
                if (id == 6) {
                    textures[id] = sf::Texture();
                    textures[id].loadFromFile(path, true, sf::IntRect({ 0, 0 }, { 32, 32 }));
                }
                else if (id == 7) {
                    // Saw: полный стрип → анимируем через _trapAnimFrame
                    gmap_sawTex = sf::Texture();
                    gmap_sawTex.loadFromFile(path);
                    textures[id] = gmap_sawTex;
                }
                sheet.insert_or_assign(id, sf::Sprite(textures[id]));
                ++loaded;
                std::cout << "[GameMap] tile " << id << " -> " << path << "\n";
            }
            else {
                std::cout << "[GameMap] WARN: cannot load " << path << " (tile " << id << ")\n";
                textures.erase(id);
            }
        }
        return loaded;
    }

    // ── Загрузка карты ────────────────────────────────────────────────────────
    bool load(const char* path) {
        FILE* f;
        if (fopen_s(&f, path, "rb")) {
            std::cout << "[GameMap] file not found: " << path << "\n";
            return false;
        }
        MapHeader hdr;
        if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != 12345 || hdr.version != 1) {
            std::cout << "[GameMap] invalid header\n"; fclose(f); return false;
        }
        if (fread(background.get(), sizeof(int), GMAP_W * GMAP_H, f) != (size_t)(GMAP_W * GMAP_H)) {
            std::cout << "[GameMap] bg read error\n"; fclose(f); return false;
        }
        if (fread(tiles.get(), sizeof(int), GMAP_W * GMAP_H, f) != (size_t)(GMAP_W * GMAP_H)) {
            std::cout << "[GameMap] tiles read error\n"; fclose(f); return false;
        }
        size_t ec = 0; fread(&ec, sizeof(size_t), 1, f);
        if (ec > 10000) { fclose(f); return false; }
        entities.resize(ec);
        if (ec) fread(entities.data(), sizeof(MapEntity), ec, f);
        size_t cc = 0; fread(&cc, sizeof(size_t), 1, f);
        if (cc > 10000) { fclose(f); return false; }
        colliders.resize(cc);
        if (cc) fread(colliders.data(), sizeof(Collider), cc, f);
        fclose(f);
        std::cout << "[GameMap] loaded: " << ec << " entities, " << cc << " colliders\n";
        return true;
    }

    // ── BFS-освещение (статика: факелы, LightElem) ────────────────────────────
    // Вызвать ОДИН РАЗ после load(). Враги (ENT_ENEMY) исключены — они динамика.
    void calculateStaticLight(float tileSize) {
        std::fill(lightMap.begin(), lightMap.end(), 0.f);
        std::queue<std::pair<int, int>> q;

        for (auto& e : entities) {
            if (e.light <= 0.f || e.type == ENT_ENEMY) continue;
            int tx = (int)(e.position.x / tileSize);
            int ty = (int)(e.position.y / tileSize);
            if (tx < 0 || ty < 0 || tx >= GMAP_W || ty >= GMAP_H) continue;
            if (e.light > lightMap[ty * GMAP_W + tx]) {
                lightMap[ty * GMAP_W + tx] = e.light;
                q.push({ tx, ty });
            }
        }

        const std::pair<int, int> dirs[] = { {1,0},{-1,0},{0,1},{0,-1} };
        while (!q.empty()) {
            auto [x, y] = q.front(); q.pop();
            float cur = lightMap[y * GMAP_W + x];
            for (auto [dx, dy] : dirs) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= GMAP_W || ny >= GMAP_H) continue;
                float decay = (tiles[ny * GMAP_W + nx] == -1)
                    ? LightConst::AIR_DECAY : LightConst::BLOCK_DECAY;
                float nv = cur - decay;
                if (nv > 0.f && nv > lightMap[ny * GMAP_W + nx]) {
                    lightMap[ny * GMAP_W + nx] = nv;
                    q.push({ nx, ny });
                }
            }
        }
    }

    // 3x3 сглаженная выборка из lightMap
    float getSmoothLight(int x, int y) const {
        float sum = 0.f; int cnt = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= GMAP_W || ny >= GMAP_H) continue;
                sum += lightMap[ny * GMAP_W + nx]; ++cnt;
            }
        return cnt > 0 ? sum / cnt : 0.f;
    }

    // ── Fog-of-war RenderTexture ──────────────────────────────────────────────
    // Вызвать один раз при старте, передав размер окна
    bool initFog(unsigned ww, unsigned wh) {
        _fogW = ww; _fogH = wh;
        _fogReady = _fogRT.resize({ ww, wh });
        if (_fogReady) _fogRT.setSmooth(true);
        return _fogReady;
    }

    // Строит fog-текстуру каждый кадр.
    // dynLights = игрок + живые враги с lightEmit > 0 + любые другие.
    void buildFog(const sf::View& cam, const std::vector<LightSource>& dynLights = {}) {
        if (!_fogReady) return;

        _fogRT.clear(sf::Color(LightConst::FOG_R, LightConst::FOG_G, LightConst::FOG_B, 255));

        // Работаем в экранных координатах
        sf::View sv({ (float)_fogW * 0.5f, (float)_fogH * 0.5f },
            { (float)_fogW,        (float)_fogH });
        _fogRT.setView(sv);

        float sx = (float)_fogW / cam.getSize().x;
        float sy = (float)_fogH / cam.getSize().y;

        auto toScreen = [&](sf::Vector2f wp) -> sf::Vector2f {
            sf::Vector2f tl = cam.getCenter() - cam.getSize() * 0.5f;
            return { (wp.x - tl.x) * sx, (wp.y - tl.y) * sy };
            };

        // Статические источники (факелы, LightElem, Decoration с light > 0)
        for (auto& e : entities) {
            if (e.light <= 0.f || e.type == ENT_ENEMY) continue;
            float r = e.light * LightConst::LIGHT_PX * std::min(sx, sy);
            _circle(toScreen(e.position), r, sf::Color(255, 200, 100, 255));
        }

        // Динамические (игрок, враги-glow, и т.д.)
        for (auto& ls : dynLights) {
            float r = ls.radius * std::min(sx, sy);
            _circle(toScreen(ls.worldPos), r, ls.color);
        }

        _fogRT.display();
    }

    // Накладывает fog поверх текущего кадра (вызывать после всех draw, до UI)
    void applyFog(sf::RenderWindow& window) {
        if (!_fogReady) return;
        sf::View saved = window.getView();
        window.setView(window.getDefaultView());
        sf::Sprite s(_fogRT.getTexture());
        window.draw(s, sf::RenderStates(sf::BlendMultiply));
        window.setView(saved);
    }

    // ── Отрисовка фона ────────────────────────────────────────────────────────
    void drawBackground(sf::RenderWindow& window, float tileSize, const sf::View& cam,
        bool useLightTint = true) {
        auto [x0, y0, x1, y1] = _vt(tileSize, cam);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                int id = background[y * GMAP_W + x];
                if (id < 0) continue;
                auto it = sheet.find(id);
                if (it == sheet.end()) continue;
                if (useLightTint) {
                    float lv = std::min(LightConst::TILES_MIN + getSmoothLight(x, y) * 0.65f, 1.f);
                    uint8_t b = (uint8_t)(lv * 255.f);
                    it->second.setColor({ b, b, b, 255 });
                }
                else {
                    it->second.setColor(sf::Color::White);
                }
                it->second.setPosition({ x * tileSize, y * tileSize });
                window.draw(it->second);
            }
    }

    // ── Отрисовка тайлов ──────────────────────────────────────────────────────
    void drawTiles(sf::RenderWindow& window, float tileSize, const sf::View& cam,
        bool useLightTint = true) {
        auto [x0, y0, x1, y1] = _vt(tileSize, cam);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                int id = tiles[y * GMAP_W + x];
                if (id < 0) continue;
                auto it = sheet.find(id);
                if (it == sheet.end()) continue;
                if (useLightTint) {
                    float lv = std::min(LightConst::TILES_MIN + getSmoothLight(x, y) * 0.65f, 1.f);
                    uint8_t b = (uint8_t)(lv * 255.f);
                    it->second.setColor({ b, b, b, 255 });
                }
                else {
                    it->second.setColor(sf::Color::White);
                }
                it->second.setPosition({ x * tileSize, y * tileSize });
                window.draw(it->second);
            }
    }

    // ── Отрисовка энтити (не-врагов) — точно как в мап-криейторе ────────────
    // backOnly=true  → ENT_BACKTRAP (за тайлами)
    // backOnly=false → всё остальное кроме ENT_ENEMY и ENT_BACKTRAP (перед тайлами)
    void drawEntities(sf::RenderWindow& window, const sf::View& cam, bool backOnly,
        bool useLightTint = true, float tileSize = 32.f) {
        sf::Vector2f tl = cam.getCenter() - cam.getSize() / 2.f;
        sf::Vector2f br = cam.getCenter() + cam.getSize() / 2.f;

        for (int i = 0; i < (int)entities.size(); ++i) {
            const MapEntity& e = entities[i];
            if (e.textureId < 0)     continue;
            if (e.type == ENT_ENEMY) continue;

            bool isBack = (e.type == ENT_BACKTRAP);
            if (backOnly != isBack) continue;

            // Frustum culling
            if (e.position.x + tileSize < tl.x || e.position.x > br.x) continue;
            if (e.position.y + tileSize < tl.y || e.position.y > br.y) continue;

            bool isSaw = (e.textureId == 7);

            // ── Пила: берём полный stрип из gmap_sawTex, анимируем кадром ───
            if (isSaw) {
                if (gmap_sawTex.getSize().x == 0) continue; // текстура не загружена
                sf::Sprite spr(gmap_sawTex);
                int frameX = _trapAnimFrame * 32;
                spr.setTextureRect(sf::IntRect({ frameX, 0 }, { 32, 32 }));
                spr.setScale({ 1.f, 1.f });
                spr.setPosition(e.position);
                _applyLight(spr, e, tileSize, useLightTint);
                window.draw(spr);
                continue;
            }

            // ── Все остальные (шипы, кристаллы, факелы, декорации) ──────────
            auto it = sheet.find(e.textureId);
            if (it == sheet.end()) continue;

            // Копия спрайта — не трогаем оригинал в sheet
            sf::Sprite spr = it->second;
            sf::Vector2u texSize = spr.getTexture().getSize();
            if (texSize.x == 0 || texSize.y == 0) continue;

            spr.setTextureRect(sf::IntRect({ 0, 0 },
                { (int)texSize.x, (int)texSize.y }));

            // Масштаб: вписываем в tileSize×tileSize
            spr.setScale({ tileSize / (float)texSize.x,
                           tileSize / (float)texSize.y });

            // Шипы/кристаллы (id 8-11): выравниваем по нижнему краю тайла.
            // Текстура шипов уже, чем tileSize → после scale она занимает
            // ровно tileSize по высоте, поэтому смещение не нужно.
            // Позиция — верхний левый угол тайла (как в мап-криейторе).
            spr.setPosition(e.position);

            _applyLight(spr, e, tileSize, useLightTint);
            window.draw(spr);
        }
    }

    // ── Отрисовка коллайдеров (debug) ────────────────────────────────────────
    void drawColliders(sf::RenderWindow& window) const {
        for (auto& c : colliders) {
            sf::RectangleShape r({ c.width, c.height });
            r.setPosition({ c.x, c.y });
            r.setFillColor(sf::Color::Transparent);
            r.setOutlineColor(sf::Color(0, 255, 0, 180));
            r.setOutlineThickness(2.f);
            window.draw(r);
        }
    }

    // ── Полный рендер карты одним вызовом ─────────────────────────────────────
    // dynLights  — источники движущегося света (игрок обязателен, враги опционально)
    // useFog     — включить fog overlay
    void drawAll(sf::RenderWindow& window, float tileSize, sf::View& cam,
        const std::vector<LightSource>& dynLights = {},
        bool useFog = true, bool debugColliders = false)
    {
        drawBackground(window, tileSize, cam, useFog);
        //drawEntities(window, cam, true, useFog, tileSize);
        drawTiles(window, tileSize, cam, useFog);
        //drawEntities(window, cam, false, useFog, tileSize);
        if (debugColliders) drawColliders(window);
        if (useFog) { buildFog(cam, dynLights); applyFog(window); }
    }

    // ── Прочие утилиты ────────────────────────────────────────────────────────
    sf::Vector2f getSpawnPoint() const {
        for (auto& e : entities)
            if (e.type == ENT_SPAWNPOINT || e.type == ENT_STARTPOINT)
                return e.position;
        return { 100.f, 300.f };
    }

    sf::Vector2f getSafeSpawn(float pw, float ph) const {
        sf::Vector2f pos = getSpawnPoint();
        bool l = false;
        for (int i = 0; i < 10; ++i) {
            sf::Vector2f f = resolveColliders(pos, pw, ph, l);
            if (f == pos) break; pos = f;
        }
        for (int i = 0; i < 50; ++i) {
            sf::Vector2f d = pos; d.y += 2.f;
            bool ll = false;
            sf::Vector2f f = resolveColliders(d, pw, ph, ll);
            if (ll) { pos = f; break; }
            pos = d;
        }
        return pos;
    }

    sf::Vector2f getFinishPoint() const {
        for (auto& e : entities)
            if (e.type == ENT_FINISH) return e.position;
        return { -1.f, -1.f };
    }

    sf::Vector2f resolveColliders(sf::Vector2f pos, float pw, float ph, bool& landed) const {
        landed = false;
        for (int iter = 0; iter < 3; ++iter) {
            for (auto& c : colliders) {
                float ox = std::min(pos.x + pw, c.x + c.width) - std::max(pos.x, c.x);
                float oy = std::min(pos.y + ph, c.y + c.height) - std::max(pos.y, c.y);
                if (ox <= 0.f || oy <= 0.f) continue;
                if (oy <= ox) {
                    if (pos.y + ph * 0.5f < c.y + c.height * 0.5f) { pos.y -= oy; landed = true; }
                    else pos.y += oy;
                }
                else {
                    if (pos.x + pw * 0.5f < c.x + c.width * 0.5f) pos.x -= ox;
                    else                                         pos.x += ox;
                }
            }
        }
        return pos;
    }

    // ── Анимация пилы (id=7) ──────────────────────────────────────────────────
    // Пила в Saw.png — горизонтальный стрип (каждый кадр 32x32).
    // Вызывать updateTraps(dt) каждый кадр из GameScene::update().
    int   _trapAnimFrame = 0;
    float _trapAnimTimer = 0.f;
    static constexpr float SAW_FRAME_SPD = 0.06f; // секунд на кадр
    static constexpr int   SAW_FRAME_CNT = 2;      // в Saw.png 2 кадра (левая/правая 32px)

    // Загружает пилу как стрип (все кадры) — вызывается из _loadDefaultTextures / loadTextures.
    // При необходимости можно вызвать вручную.
    void _reloadSawTexture(const std::string& path = "Sprites/Saw.png") {
        gmap_sawTex.loadFromFile(path);
        // sprite для sheet создаётся в updateTraps при первом кадре
        if (gmap_sawTex.getSize().x > 0) {
            sheet.insert_or_assign(7, sf::Sprite(gmap_sawTex));
        }
    }
    sf::Texture gmap_sawTex; // полная текстура пилы (не обрезанная)

public:
    // Вызывать каждый кадр — обновляет анимацию пилы
    void updateTraps(float dt) {
        _trapAnimTimer += dt;
        if (_trapAnimTimer >= SAW_FRAME_SPD) {
            _trapAnimTimer -= SAW_FRAME_SPD;
            _trapAnimFrame = (_trapAnimFrame + 1) % SAW_FRAME_CNT;
        }
    }

private:
    sf::RenderTexture _fogRT;
    unsigned _fogW = 0, _fogH = 0;
    bool     _fogReady = false;

    // Вспомогательный метод: применяет световой тинт к спрайту на основе lightMap
    void _applyLight(sf::Sprite& spr, const MapEntity& e, float tileSize, bool useLightTint) {
        if (useLightTint) {
            int tx = std::clamp((int)((e.position.x + tileSize * 0.5f) / tileSize), 0, GMAP_W - 1);
            int ty = std::clamp((int)((e.position.y + tileSize * 0.5f) / tileSize), 0, GMAP_H - 1);
            float lv = std::max(getSmoothLight(tx, ty), e.light);
            lv = std::clamp(LightConst::TILES_MIN + lv * 0.9f, 0.f, 1.f);
            uint8_t b = (uint8_t)(lv * 255.f);
            spr.setColor({ b, b, b, 255 });
        }
        else {
            spr.setColor(sf::Color::White);
        }
    }

    // Рисует конус света (radial gradient) на _fogRT через BlendAdd
    void _circle(sf::Vector2f sc, float r, sf::Color col) {
        if (r <= 0.f) return;
        const int N = 40;
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, N + 2);
        fan[0].position = sc;
        fan[0].color = col;
        for (int i = 0; i <= N; ++i) {
            float a = 2.f * 3.14159265f * i / N;
            fan[i + 1].position = { sc.x + std::cos(a) * r, sc.y + std::sin(a) * r };
            fan[i + 1].color = sf::Color(col.r, col.g, col.b, 0);
        }
        _fogRT.draw(fan, sf::RenderStates(sf::BlendAdd));
    }

    struct TileRange { int x0, y0, x1, y1; };
    TileRange _vt(float ts, const sf::View& cam) const {
        sf::Vector2f tl = cam.getCenter() - cam.getSize() / 2.f;
        sf::Vector2f br = cam.getCenter() + cam.getSize() / 2.f;
        return {
            std::max(0,      (int)(tl.x / ts)),
            std::max(0,      (int)(tl.y / ts)),
            std::min(GMAP_W, (int)(br.x / ts) + 2),
            std::min(GMAP_H, (int)(br.y / ts) + 2)
        };
    }
};