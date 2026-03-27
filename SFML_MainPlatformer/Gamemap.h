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

static constexpr int GMAP_W = 500;
static constexpr int GMAP_H = 500;

struct MapHeader { int magic, version; };

struct Collider {
    float x, y, width, height;
    Collider() : x(-1), y(-1), width(0), height(0) {}
    Collider(float X, float Y, float W, float H) :x(X), y(Y), width(W), height(H) {}
};

enum EntityType { ENT_NONE = 0, ENT_ENEMY, ENT_NPC, ENT_STARTPOINT, ENT_SPAWNPOINT, ENT_FINISH, ENT_COIN, ENT_TRAP };

struct MapEntity {
    sf::Vector2f position;
    int textureId;
    EntityType type;
    float speed;
    int health, defense, damage;
    float light = 0.f;
    MapEntity() :position(-1, -1), textureId(-1), type(ENT_NONE), speed(0), health(0), defense(0), damage(0), light(0) {}
};

class GameMap {
public:
    std::unique_ptr<int[]> background;
    std::unique_ptr<int[]> tiles;
    std::vector<MapEntity> entities;
    std::vector<Collider>  colliders;
    std::map<int, sf::Texture> textures;
    std::map<int, sf::Sprite>  sheet;

    GameMap() {
        background = std::make_unique<int[]>(GMAP_W * GMAP_H);
        tiles = std::make_unique<int[]>(GMAP_W * GMAP_H);
        std::fill(background.get(), background.get() + GMAP_W * GMAP_H, -1);
        std::fill(tiles.get(), tiles.get() + GMAP_W * GMAP_H, -1);
    }

    int loadTextures(const std::string& configPath) {
        textures.clear();
        sheet.clear();
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
            textures[id] = sf::Texture();
            bool ok = textures[id].loadFromFile(path);
            if (!ok) {
                std::string alt = "Sprites/" + path.substr(path.find_last_of("/\\") + 1);
                ok = textures[id].loadFromFile(alt);
                if (ok) path = alt;
            }
            if (ok) {
                sheet.emplace(id, sf::Sprite(textures[id]));
                ++loaded;
                std::cout << "[GameMap] tile " << id << " -> " << path << "\n";
            }
            else {
                std::cout << "[GameMap] WARN: failed to load " << path << " for tile " << id << "\n";
                textures.erase(id);
            }
        }
        return loaded;
    }

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
        size_t ec = 0;
        fread(&ec, sizeof(size_t), 1, f);
        if (ec > 10000) { fclose(f);return false; }
        entities.resize(ec);
        if (ec) fread(entities.data(), sizeof(MapEntity), ec, f);
        size_t cc = 0;
        fread(&cc, sizeof(size_t), 1, f);
        if (cc > 10000) { fclose(f);return false; }
        colliders.resize(cc);
        if (cc) fread(colliders.data(), sizeof(Collider), cc, f);
        fclose(f);
        std::cout << "[GameMap] loaded: " << ec << " entities, " << cc << " colliders\n";
        return true;
    }

    void drawBackground(sf::RenderWindow& window, float tileSize, sf::View& cam) const {
        sf::Vector2f tl = cam.getCenter() - cam.getSize() / 2.f;
        sf::Vector2f br = cam.getCenter() + cam.getSize() / 2.f;
        int x0 = std::max(0, (int)(tl.x / tileSize));
        int y0 = std::max(0, (int)(tl.y / tileSize));
        int x1 = std::min(GMAP_W, (int)(br.x / tileSize) + 2);
        int y1 = std::min(GMAP_H, (int)(br.y / tileSize) + 2);
        for (int y = y0;y < y1;++y)
            for (int x = x0;x < x1;++x) {
                int id = background[y * GMAP_W + x];
                if (id < 0) continue;
                auto it = sheet.find(id);
                if (it == sheet.end()) continue;
                const_cast<sf::Sprite&>(it->second).setPosition({ x * tileSize,y * tileSize });
                window.draw(it->second);
            }
    }

    void drawTiles(sf::RenderWindow& window, float tileSize, sf::View& cam) const {
        sf::Vector2f tl = cam.getCenter() - cam.getSize() / 2.f;
        sf::Vector2f br = cam.getCenter() + cam.getSize() / 2.f;
        int x0 = std::max(0, (int)(tl.x / tileSize));
        int y0 = std::max(0, (int)(tl.y / tileSize));
        int x1 = std::min(GMAP_W, (int)(br.x / tileSize) + 2);
        int y1 = std::min(GMAP_H, (int)(br.y / tileSize) + 2);
        for (int y = y0;y < y1;++y)
            for (int x = x0;x < x1;++x) {
                int id = tiles[y * GMAP_W + x];
                if (id < 0) continue;
                auto it = sheet.find(id);
                if (it == sheet.end()) continue;
                const_cast<sf::Sprite&>(it->second).setPosition({ x * tileSize,y * tileSize });
                window.draw(it->second);
            }
    }

    void drawColliders(sf::RenderWindow& window) const {
        for (auto& c : colliders) {
            sf::RectangleShape r({ c.width,c.height });
            r.setPosition({ c.x,c.y });
            r.setFillColor(sf::Color::Transparent);
            r.setOutlineColor(sf::Color(0, 255, 0, 180));
            r.setOutlineThickness(2.f);
            window.draw(r);
        }
    }

    sf::Vector2f getSpawnPoint() const {
        for (auto& e : entities)
            if (e.type == ENT_SPAWNPOINT || e.type == ENT_STARTPOINT)
                return e.position;
        return { 100.f,300.f };
    }

    sf::Vector2f getSafeSpawn(float pw, float ph) const {
        sf::Vector2f pos = getSpawnPoint();

        bool landed = false;

        for (int i = 0; i < 10; ++i) {
            sf::Vector2f fixed = resolveColliders(pos, pw, ph, landed);
            if (fixed == pos) break;
            pos = fixed;
        }

        for (int i = 0; i < 50; ++i) {
            sf::Vector2f down = pos;
            down.y += 2.f;

            bool l = false;
            sf::Vector2f fixed = resolveColliders(down, pw, ph, l);

            if (l) {
                pos = fixed;
                break;
            }
            pos = down;
        }
        return pos;
    }

    sf::Vector2f getFinishPoint() const {
        for (auto& e : entities)
            if (e.type == ENT_FINISH) return e.position;
        return { -1.f,-1.f };
    }

    sf::Vector2f resolveColliders(sf::Vector2f pos, float pw, float ph, bool& landed) const {
        landed = false;
        for (int iter = 0;iter < 3;++iter) {
            for (auto& col : colliders) {
                float ox = std::min(pos.x + pw, col.x + col.width) - std::max(pos.x, col.x);
                float oy = std::min(pos.y + ph, col.y + col.height) - std::max(pos.y, col.y);
                if (ox <= 0.f || oy <= 0.f) continue;
                if (oy <= ox) {
                    if (pos.y + ph * 0.5f < col.y + col.height * 0.5f) { pos.y -= oy; landed = true; }
                    else { pos.y += oy; }
                }
                else {
                    if (pos.x + pw * 0.5f < col.x + col.width * 0.5f) pos.x -= ox;
                    else pos.x += ox;
                }
            }
        }
        return pos;
    }
};