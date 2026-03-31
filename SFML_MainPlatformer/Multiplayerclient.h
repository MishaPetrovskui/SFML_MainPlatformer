#pragma once
// MultiplayerClient.h  —  WebSocket multiplayer for PixelRun
// Key additions vs original:
//   • sendPosition() gains a "phase" param ("playing"|"menu"|"paused")
//   • consumeLogout() — returns true once when server sent {"type":"logout"}
//   • GhostRenderer respects gamePhase (fades paused players)

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

// ── Data received from server ─────────────────────────────────────────────────
struct MpOtherPlayer {
    int         id = 0;
    std::string username;
    float       x = 0.f;
    float       y = 0.f;
    bool        facingRight = true;
    std::string anim = "idle";
    std::string gamePhase = "playing"; // "playing"|"paused"
};

// ── Minimal JSON helpers ──────────────────────────────────────────────────────
namespace MpJson {
    static std::string str(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find('"', pos);
        return end == std::string::npos ? "" : json.substr(pos, end - pos);
    }
    static float num(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0.f;
        pos += search.size();
        while (pos < json.size() && json[pos] == ' ') ++pos;
        try { return std::stof(json.substr(pos)); }
        catch (...) { return 0.f; }
    }
    static bool boolean(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return false;
        pos += search.size();
        return json.find("true", pos) == pos;
    }
    static std::vector<MpOtherPlayer> parsePlayers(const std::string& json) {
        std::vector<MpOtherPlayer> result;
        size_t arrStart = json.find('[');
        if (arrStart == std::string::npos) return result;
        size_t pos = arrStart;
        while (true) {
            size_t objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;
            int depth = 0; size_t objEnd = objStart;
            for (; objEnd < json.size(); ++objEnd) {
                if (json[objEnd] == '{') ++depth;
                else if (json[objEnd] == '}') { --depth; if (!depth) break; }
            }
            if (objEnd >= json.size()) break;
            std::string obj = json.substr(objStart, objEnd - objStart + 1);
            MpOtherPlayer p;
            p.id = static_cast<int>(num(obj, "id"));
            p.username = str(obj, "username");
            p.x = num(obj, "x");
            p.y = num(obj, "y");
            p.facingRight = boolean(obj, "facingRight");
            p.anim = str(obj, "anim");
            p.gamePhase = str(obj, "gamePhase");
            if (p.gamePhase.empty()) p.gamePhase = "playing";
            if (p.id > 0) result.push_back(p);
            pos = objEnd + 1;
        }
        return result;
    }
}

// ── MultiplayerClient singleton ───────────────────────────────────────────────
class MultiplayerClient {
public:
    static MultiplayerClient& instance() {
        static MultiplayerClient inst;
        return inst;
    }

    // ── Connection ────────────────────────────────────────────────────────────

    void connect(const std::string& host, int port,
        const std::string& token, const std::string& username)
    {
        disconnect();
        _username = username;
        _logoutReceived = false;
        _running = true;
        _thread = std::thread([this, host, port, token]() {
            runLoop(host, port, token);
            });
    }

    void disconnect() {
        _running = false;
        if (_wsHandle) {
            WinHttpWebSocketClose(_wsHandle, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
            WinHttpCloseHandle(_wsHandle);
            _wsHandle = nullptr;
        }
        if (_thread.joinable()) _thread.join();
        std::lock_guard<std::mutex> lk(_mtx);
        _others.clear();
        _pendingKills.clear();
    }

    // ── Send helpers ──────────────────────────────────────────────────────────

    /// phase: "playing" | "menu" | "paused"
    void sendPosition(float x, float y, int level, bool facingRight,
        const std::string& anim,
        const std::string& phase = "playing")
    {
        if (!_wsHandle || !_running) return;
        std::ostringstream ss;
        ss << "{\"x\":" << x
            << ",\"y\":" << y
            << ",\"lv\":" << level
            << ",\"fr\":" << (facingRight ? "true" : "false")
            << ",\"anim\":\"" << anim << "\""
            << ",\"phase\":\"" << phase << "\""
            << ",\"name\":\"" << _username << "\""
            << ",\"src\":\"game\"}";
        _sendRaw(ss.str());
    }

    void sendEnemyKill(int enemyIndex, int level) {
        if (!_wsHandle || !_running) return;
        std::ostringstream ss;
        ss << "{\"type\":\"enemy_kill\",\"enemyIndex\":" << enemyIndex
            << ",\"lv\":" << level
            << ",\"src\":\"game\"}";
        _sendRaw(ss.str());
    }

    // ── Query ─────────────────────────────────────────────────────────────────

    std::vector<int> getAndClearEnemyKills() {
        std::lock_guard<std::mutex> lk(_mtx);
        return std::move(_pendingKills);
    }

    std::vector<MpOtherPlayer> getOtherPlayers() {
        std::lock_guard<std::mutex> lk(_mtx);
        return _others;
    }

    bool isConnected() const { return _running && _wsHandle != nullptr; }

    /// Returns true exactly once when the server forwarded a logout event.
    /// Call this every frame; when it returns true, clear the local session.
    bool consumeLogout() {
        if (!_logoutReceived.load()) return false;
        _logoutReceived = false;
        return true;
    }

private:
    HINTERNET         _wsHandle = nullptr;
    std::atomic_bool  _running{ false };
    std::atomic_bool  _logoutReceived{ false };
    std::thread       _thread;
    std::mutex        _mtx;
    std::mutex        _sendMtx;
    std::string       _username;
    std::vector<MpOtherPlayer> _others;
    std::vector<int>           _pendingKills;

    void _sendRaw(const std::string& msg) {
        std::lock_guard<std::mutex> lk(_sendMtx);
        WinHttpWebSocketSend(_wsHandle, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            (PVOID)msg.c_str(), (DWORD)msg.size());
    }

    void runLoop(const std::string& host, int port, const std::string& token) {
        HINTERNET hSession = WinHttpOpen(L"PixelRun/1.0 WS",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) { _running = false; return; }

        std::wstring wHost(host.begin(), host.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); _running = false; return; }

        std::string  path = "/ws/game?token=" + token;
        std::wstring wPath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_ESCAPE_DISABLE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }

        WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0)
            || !WinHttpReceiveResponse(hRequest, nullptr))
        {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }

        HINTERNET hWs = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
        WinHttpCloseHandle(hRequest);
        if (!hWs) {
            WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            _running = false; return;
        }
        _wsHandle = hWs;

        std::string      accumulator;
        std::vector<BYTE> buf(4096);
        accumulator.reserve(4096);

        while (_running) {
            DWORD bytesRead = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType;
            DWORD err = WinHttpWebSocketReceive(hWs, buf.data(), (DWORD)buf.size(),
                &bytesRead, &bufType);
            if (err != ERROR_SUCCESS) break;

            accumulator.append(reinterpret_cast<char*>(buf.data()), bytesRead);

            bool isFinal = (bufType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE
                || bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE);
            if (!isFinal) continue;
            if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;

            std::string msgType = MpJson::str(accumulator, "type");

            if (msgType == "logout") {
                // Server forwarded a logout from the web client
                _logoutReceived = true;
            }
            else if (msgType == "enemy_kill") {
                int idx = static_cast<int>(MpJson::num(accumulator, "enemyIndex"));
                std::lock_guard<std::mutex> lk(_mtx);
                _pendingKills.push_back(idx);
            }
            else {
                // "players" packet — position update
                auto players = MpJson::parsePlayers(accumulator);
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

// ── GhostRenderer ─────────────────────────────────────────────────────────────
class GhostRenderer {
public:
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
            bool isPaused = (p.gamePhase == "paused");
            uint8_t alpha = isPaused ? 130 : 210;

            if (_hasSprite) {
                sf::Sprite spr(_walkTex);
                spr.setTextureRect(sf::IntRect({ _animFrame * FRAME_W, 0 }, { FRAME_W, FRAME_H }));
                spr.setOrigin({ 44.5f, 96.f });
                spr.setScale(p.facingRight
                    ? sf::Vector2f{ SSCALE, SSCALE }
                : sf::Vector2f{ -SSCALE, SSCALE });
                spr.setPosition({ p.x + 15.f, p.y + 37.f });
                spr.setColor(sf::Color(170, 215, 255, alpha));
                window.draw(spr);
            }
            else {
                _body.setFillColor(sf::Color(80, 160, 255, (uint8_t)(alpha / 2)));
                _body.setPosition({ p.x, p.y });
                window.draw(_body);
            }

            if (_font && !p.username.empty()) {
                sf::Text label(*_font, p.username, 11);
                label.setFillColor(sf::Color(215, 238, 255, alpha));
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

                // Small "||" pause indicator above the badge
                if (isPaused) {
                    sf::Text pauseMark(*_font, "||", 9);
                    pauseMark.setFillColor(sf::Color(255, 220, 100, 200));
                    pauseMark.setPosition({ lx + lb.size.x * 0.5f - 4.f, ly - 14.f });
                    window.draw(pauseMark);
                }
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