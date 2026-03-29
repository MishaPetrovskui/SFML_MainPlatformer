#pragma once
// MultiplayerClient.h  — WebSocket-based multiplayer for PixelRun
// Uses WinHTTP WebSocket API (already linked via winhttp.lib in ApiClient.h)
//
// Usage in main.cpp:
//   #include "MultiplayerClient.h"
//   // After login:
//   MultiplayerClient::instance().connect("localhost", 5001, token, username);
//   // In game loop (PLAYING state):
//   MultiplayerClient::instance().sendPosition(x, y, level, facingRight, animName);
//   auto others = MultiplayerClient::instance().getOtherPlayers();
//   // Draw others as ghost sprites
//   // On exit / back to menu:
//   MultiplayerClient::instance().disconnect();

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#ifdef byte
#undef byte
#endif

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <SFML/Graphics.hpp>

// ── Other player info received from server ───────────────────────────────────
struct MpOtherPlayer {
    int         id = 0;
    std::string username;
    float       x = 0.f;
    float       y = 0.f;
    bool        facingRight = true;
    std::string anim = "idle";
};

// ── Minimal JSON helpers (no external lib) ───────────────────────────────────
namespace MpJson {
    // Extract string value for key, e.g. "username":"Bob" -> "Bob"
    static std::string str(const std::string& json, const std::string& key) {
        auto search = "\"" + key + "\":\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = json.find('"', pos);
        return end == std::string::npos ? "" : json.substr(pos, end - pos);
    }
    // Extract numeric value (int or float stored as string)
    static float num(const std::string& json, const std::string& key) {
        auto search = "\"" + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string::npos) return 0.f;
        pos += search.size();
        while (pos < json.size() && (json[pos] == ' ')) pos++;
        try { return std::stof(json.substr(pos)); }
        catch (...) { return 0.f; }
    }
    static bool boolean(const std::string& json, const std::string& key) {
        auto search = "\"" + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string::npos) return false;
        pos += search.size();
        return json.find("true", pos) == pos;
    }
    // Parse array of player objects from {"players":[{...},{...}]}
    static std::vector<MpOtherPlayer> parsePlayers(const std::string& json) {
        std::vector<MpOtherPlayer> result;
        auto arrStart = json.find("[");
        if (arrStart == std::string::npos) return result;
        size_t pos = arrStart;
        while (true) {
            auto objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;
            // Find matching closing brace
            int depth = 0;
            size_t objEnd = objStart;
            for (; objEnd < json.size(); ++objEnd) {
                if (json[objEnd] == '{') depth++;
                else if (json[objEnd] == '}') { depth--; if (depth == 0) break; }
            }
            if (objEnd >= json.size()) break;
            auto obj = json.substr(objStart, objEnd - objStart + 1);
            MpOtherPlayer p;
            p.id = static_cast<int>(num(obj, "id"));
            p.username = str(obj, "username");
            p.x = num(obj, "x");
            p.y = num(obj, "y");
            p.facingRight = boolean(obj, "facingRight");
            p.anim = str(obj, "anim");
            if (p.id > 0) result.push_back(p);
            pos = objEnd + 1;
        }
        return result;
    }
}

// ── MultiplayerClient (singleton) ────────────────────────────────────────────
class MultiplayerClient {
public:
    static MultiplayerClient& instance() {
        static MultiplayerClient inst;
        return inst;
    }

    // Call after successful login
    void connect(const std::string& host, int port,
        const std::string& token, const std::string& username)
    {
        disconnect(); // close any existing connection
        _username = username;
        _running = true;
        _thread = std::thread([this, host, port, token]() {
            runLoop(host, port, token);
            });
    }

    void disconnect() {
        _running = false;
        // Signal the WS to close so the receive loop unblocks
        if (_wsHandle) {
            WinHttpWebSocketClose(_wsHandle, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                nullptr, 0);
            WinHttpCloseHandle(_wsHandle);
            _wsHandle = nullptr;
        }
        if (_thread.joinable()) _thread.join();
        std::lock_guard<std::mutex> lk(_mtx);
        _others.clear();
    }

    // Send our position to server (~10 times per second is fine)
    void sendPosition(float x, float y, int level, bool facingRight, const std::string& anim) {
        if (!_wsHandle || !_running) return;
        // Build minimal JSON
        std::ostringstream ss;
        ss << "{\"x\":" << x
            << ",\"y\":" << y
            << ",\"lv\":" << level
            << ",\"fr\":" << (facingRight ? "true" : "false")
            << ",\"anim\":\"" << anim << "\""
            << ",\"name\":\"" << _username << "\"}";
        auto msg = ss.str();
        // Non-blocking send (fire and forget; errors handled in recv loop)
        std::lock_guard<std::mutex> lk(_sendMtx);
        WinHttpWebSocketSend(_wsHandle, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            (PVOID)msg.c_str(), (DWORD)msg.size());
    }

    // Get snapshot of other players (thread-safe)
    std::vector<MpOtherPlayer> getOtherPlayers() {
        std::lock_guard<std::mutex> lk(_mtx);
        return _others;
    }

    bool isConnected() const { return _running && _wsHandle != nullptr; }

private:
    HINTERNET        _wsHandle = nullptr;
    std::atomic_bool _running{ false };
    std::thread      _thread;
    std::mutex       _mtx;      // protects _others
    std::mutex       _sendMtx;  // protects send calls
    std::string      _username;
    std::vector<MpOtherPlayer> _others;

    void runLoop(const std::string& host, int port, const std::string& token) {
        // Build upgrade request
        HINTERNET hSession = WinHttpOpen(L"PixelRun/1.0 WS",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) { _running = false; return; }

        std::wstring wHost(host.begin(), host.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); _running = false; return; }

        // Path with token query param
        std::string path = "/ws/game?token=" + token;
        std::wstring wPath(path.begin(), path.end());

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_ESCAPE_DISABLE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }

        // Mark as WebSocket upgrade
        BOOL bSet = WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
        if (!bSet) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); _running = false; return; }

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }

        HINTERNET hWs = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
        WinHttpCloseHandle(hRequest); // no longer needed after upgrade
        if (!hWs) {
            WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }
        _wsHandle = hWs;

        // Receive loop
        std::string accumulator;
        accumulator.reserve(4096);
        std::vector<BYTE> buf(4096);

        while (_running) {
            DWORD bytesRead = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType;
            DWORD err = WinHttpWebSocketReceive(hWs, buf.data(), (DWORD)buf.size(),
                &bytesRead, &bufType);
            if (err != ERROR_SUCCESS) break;

            accumulator.append(reinterpret_cast<char*>(buf.data()), bytesRead);

            bool isFinal = (bufType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE);
            if (!isFinal) continue;

            if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;

            // Parse and update other players
            auto players = MpJson::parsePlayers(accumulator);
            {
                std::lock_guard<std::mutex> lk(_mtx);
                _others = std::move(players);
            }
            accumulator.clear();
        }

        _wsHandle = nullptr;
        WinHttpWebSocketClose(hWs, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(hWs);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        _running = false;
    }
};

// ── GhostRenderer: draws other players using the real player sprite ──────────
class GhostRenderer {
public:
    // walkTexPath = "Sprites/player_walk.png" (same sheet as local player)
    void init(sf::Font& font, const std::string& walkTexPath = "") {
        _font = &font;
        if (!walkTexPath.empty()) {
            _hasSprite = _walkTex.loadFromFile(walkTexPath);
            if (_hasSprite)
                _frameCount = std::max(1, (int)(_walkTex.getSize().x / FRAME_W));
        }
        _body.setSize({ 30.f, 40.f });
        _body.setFillColor(sf::Color(80, 160, 255, 100));
        _body.setOutlineColor(sf::Color(120, 200, 255, 180));
        _body.setOutlineThickness(1.5f);
        _badge.setFillColor(sf::Color(0, 0, 0, 160));
        _badge.setOutlineColor(sf::Color(120, 200, 255, 140));
        _badge.setOutlineThickness(1.f);
    }

    void update(float dt) {
        _animTimer += dt;
        if (_animTimer >= ANIM_SPD) {
            _animTimer = 0.f;
            _animFrame = (_animFrame + 1) % std::max(1, _frameCount);
        }
    }

    void draw(sf::RenderWindow& window,
        const std::vector<MpOtherPlayer>& players,
        const sf::View& gameView)
    {
        window.setView(gameView);
        for (const auto& p : players) {
            // ── Sprite — exactly mirrors Player::draw() math ─────────────────
            if (_hasSprite) {
                sf::Sprite spr(_walkTex);
                spr.setTextureRect(sf::IntRect({ _animFrame * FRAME_W, 0 }, { FRAME_W, FRAME_H }));
                // Player::draw uses origin (44.5, 96) — feet-left of sprite
                spr.setOrigin({ 44.5f, 96.f });
                // Player::draw scale = 0.9, flip on facing
                spr.setScale(p.facingRight
                    ? sf::Vector2f{ SSCALE, SSCALE }
                : sf::Vector2f{ -SSCALE, SSCALE });
                // Player::draw anchor: center-x of hitbox, bottom-3 of hitbox
                //   hitbox is 30×40 → center x = p.x+15, bottom-3 = p.y+37
                spr.setPosition({ p.x + 15.f, p.y + 37.f });
                spr.setColor(sf::Color(170, 215, 255, 210)); // subtle blue tint
                window.draw(spr);
            }
            else {
                _body.setPosition({ p.x, p.y });
                window.draw(_body);
            }

            // ── Username badge ───────────────────────────────────────────────
            if (_font && !p.username.empty()) {
                sf::Text label(*_font, p.username, 11);
                label.setFillColor(sf::Color(215, 238, 255, 235));
                label.setOutlineColor(sf::Color(0, 0, 0, 210));
                label.setOutlineThickness(1.5f);
                sf::FloatRect lb = label.getLocalBounds();
                float lx = p.x + 15.f - lb.size.x * 0.5f;
                float ly = p.y - 22.f;
                const float pad = 4.f;
                _badge.setSize({ lb.size.x + pad * 2.f, lb.size.y + pad + 2.f });
                _badge.setPosition({ lx - pad, ly - 2.f });
                window.draw(_badge);
                label.setPosition({ lx, ly });
                window.draw(label);
            }
        }
    }

private:
    static constexpr int   FRAME_W = 128;
    static constexpr int   FRAME_H = 128;
    static constexpr float SSCALE = 0.9f;
    static constexpr float ANIM_SPD = 0.10f;

    sf::RectangleShape _body;
    sf::RectangleShape _badge;
    sf::Font* _font = nullptr;
    sf::Texture        _walkTex;
    bool               _hasSprite = false;
    int                _frameCount = 15;
    int                _animFrame = 0;
    float              _animTimer = 0.f;
};