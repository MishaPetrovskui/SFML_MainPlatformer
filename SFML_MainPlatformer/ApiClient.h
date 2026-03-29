#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#ifdef byte
#undef byte
#endif

struct ApiPlayerInfo {
    int id = 0;
    std::string username;
    std::string token;
    int coins = 0;
    std::string equippedPlayerSkin = "default";
    std::string equippedBarSkin = "default";
    std::string equippedSlashSkin = "default";
    bool loggedIn = false;
};

struct ApiQuest {
    std::string questId;
    std::string title;
    std::string description;
    std::string type;
    int targetValue = 0;
    int reward = 0;
    int currentValue = 0;
    bool completed = false;
    bool claimed = false;
};

struct ApiSkin {
    std::string id;
    std::string name;
    std::string type;
    int price = 0;
    bool owned = false;
    bool equipped = false;
};

struct ApiLeaderboardEntry {
    int rank = 0;
    std::string username;
    float time = 0.f;
    int kills = 0;
    int coins = 0;
};

struct ApiLobbyRoom {
    int level = 0;
    std::string name;
    int online = 0;
    std::vector<std::string> playerNames; // up to ~6
};

enum class ApiStatus { Idle, Loading, Success, Error };

class ApiClient {
public:
    static ApiClient& instance() { static ApiClient inst; return inst; }

    const std::string BASE_URL = "localhost";
    const int PORT = 5001;

    ApiPlayerInfo player;
    std::atomic<ApiStatus> status{ ApiStatus::Idle };
    std::mutex  statusMutex;
    std::string statusMessage;

    void loginAsync(const std::string& email, const std::string& password) {
        if (status == ApiStatus::Loading) return;
        status = ApiStatus::Loading;
        { std::lock_guard<std::mutex> l(statusMutex); statusMessage = "Connecting..."; }
        std::string e = email, p = password;
        std::thread([this, e, p]() {
            std::string body = "{\"email\":\"" + e + "\",\"password\":\"" + p + "\"}";
            std::string resp = post("/api/auth/login", body);
            std::lock_guard<std::mutex> lock(statusMutex);
            if (!resp.empty() && resp.find("token") != std::string::npos) {
                player.loggedIn = true;
                player.token = extractStr(resp, "token");
                player.id = extractInt(resp, "id");
                player.username = extractStr(resp, "username");
                player.coins = extractInt(resp, "coins");
                player.equippedPlayerSkin = extractStr(resp, "equippedPlayerSkin");
                player.equippedBarSkin = extractStr(resp, "equippedBarSkin");
                player.equippedSlashSkin = extractStr(resp, "equippedSlashSkin");
                statusMessage = "Welcome, " + player.username + "!";
                status = ApiStatus::Success;
            }
            else {
                statusMessage = resp.empty() ? "Server unavailable." : "Invalid email or password.";
                status = ApiStatus::Error;
            }
            }).detach();
    }

    void registerAsync(const std::string& username, const std::string& email, const std::string& password) {
        if (status == ApiStatus::Loading) return;
        status = ApiStatus::Loading;
        { std::lock_guard<std::mutex> l(statusMutex); statusMessage = "Registering..."; }
        std::string u = username, e = email, p = password;
        std::thread([this, u, e, p]() {
            std::string body = "{\"username\":\"" + u + "\",\"email\":\"" + e + "\",\"password\":\"" + p + "\"}";
            std::string resp = post("/api/auth/register", body);
            std::lock_guard<std::mutex> lock(statusMutex);
            if (!resp.empty() && resp.find("token") != std::string::npos) {
                player.loggedIn = true;
                player.token = extractStr(resp, "token");
                player.id = extractInt(resp, "id");
                player.username = extractStr(resp, "username");
                statusMessage = "Registered as " + player.username + "!";
                status = ApiStatus::Success;
            }
            else {
                statusMessage = resp.empty() ? "Server unavailable." : "Email already exists.";
                status = ApiStatus::Error;
            }
            }).detach();
    }

    void submitRecordAsync(int level, float time, int coins, int kills) {
        if (!player.loggedIn) return;
        std::string body =
            "{\"level\":" + std::to_string(level) +
            ",\"time\":" + std::to_string(time) +
            ",\"coins\":" + std::to_string(coins) +
            ",\"kills\":" + std::to_string(kills) + "}";
        std::thread([this, body]() { postAuth("/api/records/submit", body); }).detach();
    }

    void getQuestsAsync(std::function<void(std::vector<ApiQuest>)> callback) {
        if (!player.loggedIn) { callback({}); return; }
        std::thread([this, callback]() {
            callback(parseQuests(getAuth("/api/quests")));
            }).detach();
    }

    void claimQuestAsync(const std::string& questId, std::function<void(bool, int)> callback) {
        if (!player.loggedIn) { callback(false, 0); return; }
        std::string qid = questId;
        std::thread([this, qid, callback]() {
            std::string resp = postAuth("/api/quests/claim/" + qid, "{}");
            int reward = extractInt(resp, "reward");
            callback(!resp.empty(), reward);
            }).detach();
    }

    std::vector<ApiLeaderboardEntry> getLeaderboard(int level) {
        return parseLeaderboard(get("/api/records/leaderboard/" + std::to_string(level) + "?top=10"));
    }
    void getShopAsync(std::function<void(std::vector<ApiSkin>)> callback) {
        std::thread([this, callback]() {
            std::vector<ApiSkin> all = parseSkins(get("/api/shop/skins"));
            if (player.loggedIn) {
                std::vector<ApiSkin> owned = parseSkins(getAuth("/api/shop/owned"));
                for (auto& s : all) {
                    for (auto& o : owned) {
                        if (o.id == s.id) { s.owned = true; break; }
                    }
                    if (s.id == player.equippedPlayerSkin ||
                        s.id == player.equippedBarSkin ||
                        s.id == player.equippedSlashSkin) s.equipped = true;
                    if (s.price == 0) s.owned = true;
                }
            }
            callback(all);
            }).detach();
    }

    void buySkinAsync(const std::string& skinId, std::function<void(bool, std::string)> callback) {
        if (!player.loggedIn) { callback(false, "Not logged in"); return; }
        std::string sid = skinId;
        std::thread([this, sid, callback]() {
            std::string resp = postAuth("/api/shop/buy/" + sid, "{}");
            callback(!resp.empty() && resp.find("error") == std::string::npos, resp);
            }).detach();
    }

    void refreshPlayerAsync() {
        if (!player.loggedIn) return;
        std::thread([this]() {
            std::string resp = getAuth("/api/player/me");
            if (resp.empty()) return;
            std::lock_guard<std::mutex> lock(statusMutex);
            int newCoins = extractInt(resp, "coins");
            if (newCoins != player.coins) {
                player.coins = newCoins;
            }
            std::string newName = extractStr(resp, "username");
            if (!newName.empty()) player.username = newName;
            }).detach();
    }

    void equipSkinAsync(const std::string& skinId, std::function<void(bool)> callback) {
        if (!player.loggedIn) { callback(false); return; }
        std::string sid = skinId;
        std::thread([this, sid, callback]() {
            std::string body = "{\"skinId\":\"" + sid + "\"}";
            std::string resp = postAuth("/api/shop/equip", body);
            bool ok = !resp.empty() && resp.find("error") == std::string::npos;
            if (ok) {
                std::lock_guard<std::mutex> lock(statusMutex);
                auto skin = std::find_if(cachedSkins.begin(), cachedSkins.end(),
                    [&sid](const ApiSkin& s) { return s.id == sid; });
                if (skin != cachedSkins.end()) {
                    if (skin->type == "player") player.equippedPlayerSkin = sid;
                    else if (skin->type == "bar") player.equippedBarSkin = sid;
                    else if (skin->type == "slash") player.equippedSlashSkin = sid;
                }
            }
            callback(ok);
            }).detach();
    }

    std::vector<ApiSkin> cachedSkins;
    std::vector<ApiLobbyRoom> cachedRooms; // updated by getLobbyRoomsAsync

    void getLobbyRoomsAsync() {
        std::thread([this]() {
            std::string resp = get("/api/lobby/rooms");
            if (resp.empty()) return;
            std::vector<ApiLobbyRoom> rooms;
            size_t pos = 0;
            while ((pos = resp.find('{', pos)) != std::string::npos) {
                size_t end = resp.find('}', pos);
                if (end == std::string::npos) break;
                std::string item = resp.substr(pos, end - pos + 1);
                // skip nested objects (players array)
                if (item.find("\"level\":") != std::string::npos &&
                    item.find("\"online\":") != std::string::npos) {
                    ApiLobbyRoom r;
                    r.level = extractInt(item, "level");
                    r.online = extractInt(item, "online");
                    r.name = extractStr(item, "name");
                    if (r.level > 0) rooms.push_back(r);
                }
                pos = end + 1;
            }
            // parse player names from "players":[{"username":"..."},...]
            for (auto& r : rooms) {
                std::string search = "\"players\":[";
                size_t arr = resp.find(search);
                // find the right occurrence (match level context)
                size_t searchFrom = 0;
                while (arr != std::string::npos) {
                    size_t arrEnd = resp.find(']', arr);
                    if (arrEnd == std::string::npos) break;
                    std::string arrStr = resp.substr(arr, arrEnd - arr + 1);
                    // find usernames in this array
                    r.playerNames.clear();
                    size_t p2 = 0;
                    while (true) {
                        auto u = arrStr.find("\"username\":\"", p2);
                        if (u == std::string::npos) break;
                        u += 12;
                        auto ue = arrStr.find('"', u);
                        if (ue == std::string::npos) break;
                        r.playerNames.push_back(arrStr.substr(u, ue - u));
                        p2 = ue + 1;
                    }
                    break; // simplified: just grab first players array per room
                }
            }
            std::lock_guard<std::mutex> lk(statusMutex);
            cachedRooms = rooms;
            }).detach();
    }

    std::string getStatusMessage() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return statusMessage;
    }

private:
    std::string request(const std::string& method, const std::string& path,
        const std::string& body, const std::string& token = "")
    {
        HINTERNET hSession = WinHttpOpen(L"PixelRun/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";
        std::wstring wHost(BASE_URL.begin(), BASE_URL.end());
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
        std::wstring wPath(path.begin(), path.end());
        std::wstring wMethod(method.begin(), method.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
        std::wstring headers = L"Content-Type: application/json\r\n";
        if (!token.empty()) {
            std::wstring wToken(token.begin(), token.end());
            headers += L"Authorization: Bearer " + wToken + L"\r\n";
        }
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.size(), WINHTTP_ADDREQ_FLAG_ADD);
        BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            body.empty() ? nullptr : (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
        std::string result;
        if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD size = 0;
            do {
                DWORD downloaded = 0;
                WinHttpQueryDataAvailable(hRequest, &size);
                if (size == 0) break;
                std::vector<char> buf(size + 1, 0);
                WinHttpReadData(hRequest, buf.data(), size, &downloaded);
                result += buf.data();
            } while (size > 0);
        }
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return result;
    }

    std::string get(const std::string& path) { return request("GET", path, ""); }
    std::string getAuth(const std::string& path) { return request("GET", path, "", player.token); }
    std::string post(const std::string& path, const std::string& body) { return request("POST", path, body); }
    std::string postAuth(const std::string& path, const std::string& body) { return request("POST", path, body, player.token); }

    std::vector<ApiSkin> parseSkins(const std::string& json) {
        std::vector<ApiSkin> result;
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t end = json.find("}", pos);
            if (end == std::string::npos) break;
            std::string item = json.substr(pos, end - pos + 1);
            ApiSkin s;
            s.id = extractStr(item, "id");
            s.name = extractStr(item, "name");
            s.type = extractStr(item, "type");
            s.price = extractInt(item, "price");
            if (!s.id.empty()) result.push_back(s);
            pos = end + 1;
        }
        return result;
    }

    std::string extractStr(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        return end == std::string::npos ? "" : json.substr(pos, end - pos);
    }
    int extractInt(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        try { return std::stoi(json.substr(pos)); }
        catch (...) { return 0; }
    }
    std::vector<ApiLeaderboardEntry> parseLeaderboard(const std::string& json) {
        std::vector<ApiLeaderboardEntry> result;
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t end = json.find("}", pos);
            if (end == std::string::npos) break;
            std::string item = json.substr(pos, end - pos + 1);
            ApiLeaderboardEntry e;
            e.rank = extractInt(item, "rank"); e.username = extractStr(item, "username");
            e.kills = extractInt(item, "kills"); e.coins = extractInt(item, "coins");
            try { e.time = std::stof(extractStr(item, "time")); }
            catch (...) {}
            if (e.rank > 0) result.push_back(e);
            pos = end + 1;
        }
        return result;
    }
    std::vector<ApiQuest> parseQuests(const std::string& json) {
        std::vector<ApiQuest> result;
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t end = json.find("}", pos);
            if (end == std::string::npos) break;
            std::string item = json.substr(pos, end - pos + 1);
            ApiQuest q;
            q.questId = extractStr(item, "questId"); q.title = extractStr(item, "title");
            q.description = extractStr(item, "description"); q.type = extractStr(item, "type");
            q.targetValue = extractInt(item, "targetValue"); q.reward = extractInt(item, "reward");
            q.currentValue = extractInt(item, "currentValue");
            q.completed = (item.find("\"completed\":true") != std::string::npos);
            q.claimed = (item.find("\"claimed\":true") != std::string::npos);
            if (!q.questId.empty()) result.push_back(q);
            pos = end + 1;
        }
        return result;
    }
};