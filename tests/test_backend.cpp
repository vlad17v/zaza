#include "auth/jwt.hpp"
#include "auth/user_store.hpp"
#include "auth/auth_service.hpp"

#include <iostream>
#include <cassert>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <functional>

static void remove_db(const std::string& path) {
    std::remove(path.c_str());
}

static std::string extractQueryParam(const std::string& target,
                                     const std::string& key) {
    auto search = key + "=";
    auto pos    = target.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    auto end = target.find('&', pos);
    if (end == std::string::npos)
        return target.substr(pos);
    return target.substr(pos, end - pos);
}

static std::string extractPath(const std::string& target) {
    auto pos = target.find('?');
    if (pos == std::string::npos) return target;
    return target.substr(0, pos);
}

int main() {
    std::cout << "=== JWT ===\n";

    {
        auth::Jwt jwt("secret");
        auto token = jwt.sign("user1", 3600);
        assert(!token.empty());
        auto payload = jwt.verify(token);
        assert(payload.userId == "user1");
        assert(payload.exp > 0);
        std::cout << "[jwt] sign/verify OK\n";
    }

    {
        auth::Jwt jwt("secret");
        auto token = jwt.sign("user1", -1);
        try {
            jwt.verify(token);
            assert(false);
        } catch (const auth::JwtExpired&) {}
        std::cout << "[jwt] expired OK\n";
    }

    {
        auth::Jwt jwt_sign("secret_a");
        auth::Jwt jwt_verify("secret_b");
        auto token = jwt_sign.sign("user1", 3600);
        try {
            jwt_verify.verify(token);
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[jwt] wrong secret OK\n";
    }

    {
        auth::Jwt jwt("secret");
        try {
            jwt.verify("not.a.token");
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        try {
            jwt.verify("");
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        auto token = jwt.sign("user1", 3600);
        token.back() = (token.back() == 'a') ? 'b' : 'a';
        try {
            jwt.verify(token);
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[jwt] invalid token OK\n";
    }

    {
        auth::Jwt jwt("secret");
        auto token = jwt.sign("", 3600);
        auto payload = jwt.verify(token);
        assert(payload.userId == "");
        std::cout << "[jwt] empty userId OK\n";
    }

    {
        auth::Jwt jwt("");
        auto token = jwt.sign("user1", 3600);
        auto payload = jwt.verify(token);
        assert(payload.userId == "user1");
        auth::Jwt jwt2("");
        auto payload2 = jwt2.verify(token);
        assert(payload2.userId == "user1");
        auth::Jwt jwt3("nonempty");
        try {
            jwt3.verify(token);
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[jwt] empty secret OK\n";
    }

    std::cout << "\n=== UserStore ===\n";

    {
        const std::string db = "/tmp/test_us1.db";
        remove_db(db);
        auth::UserStore store(db);
        store.addUser("alice", "password123");
        auto user = store.findUser("alice");
        assert(user.has_value());
        assert(user->userId == "alice");
        assert(!user->passwordHash.empty());
        std::cout << "[userstore] add/find OK\n";
    }

    {
        const std::string db = "/tmp/test_us2.db";
        remove_db(db);
        auth::UserStore store(db);
        store.addUser("bob", "qwerty");
        assert( store.checkPassword("bob", "qwerty"));
        assert(!store.checkPassword("bob", "wrong"));
        assert(!store.checkPassword("bob", ""));
        assert(!store.checkPassword("bob", "QWERTY"));
        std::cout << "[userstore] checkPassword OK\n";
    }

    {
        const std::string db = "/tmp/test_us3.db";
        remove_db(db);
        auth::UserStore store(db);
        assert(!store.findUser("nobody").has_value());
        assert(!store.checkPassword("nobody", "pass"));
        std::cout << "[userstore] not found OK\n";
    }

    {
        const std::string db = "/tmp/test_us4.db";
        remove_db(db);
        auth::UserStore store(db);
        store.addUser("carol", "pass1");
        try {
            store.addUser("carol", "pass2");
            assert(false);
        } catch (const auth::UserStoreError&) {}
        assert( store.checkPassword("carol", "pass1"));
        assert(!store.checkPassword("carol", "pass2"));
        std::cout << "[userstore] duplicate OK\n";
    }

    {
        const std::string db = "/tmp/test_us5.db";
        remove_db(db);
        auth::UserStore store(db);
        store.addUser("", "pass");
        assert(store.findUser("").has_value());
        assert( store.checkPassword("", "pass"));
        store.addUser("user_nopass", "");
        assert( store.checkPassword("user_nopass", ""));
        assert(!store.checkPassword("user_nopass", "anything"));
        std::cout << "[userstore] empty fields OK\n";
    }

    {
        const std::string db = "/tmp/test_us6.db";
        remove_db(db);
        auth::UserStore store(db);
        store.addUser("u1", "p1");
        store.addUser("u2", "p2");
        store.addUser("u3", "p3");
        assert( store.checkPassword("u1", "p1"));
        assert( store.checkPassword("u2", "p2"));
        assert( store.checkPassword("u3", "p3"));
        assert(!store.checkPassword("u1", "p2"));
        assert(!store.checkPassword("u2", "p3"));
        assert(!store.checkPassword("u3", "p1"));
        std::cout << "[userstore] multiple users OK\n";
    }

    {
        const std::string db = "/tmp/test_us7.db";
        remove_db(db);
        {
            auth::UserStore store(db);
            store.addUser("persistent_user", "mypass");
        }
        {
            auth::UserStore store(db);
            assert(store.findUser("persistent_user").has_value());
            assert(store.checkPassword("persistent_user", "mypass"));
        }
        std::cout << "[userstore] persistence OK\n";
    }

    {
        try {
            auth::UserStore store("/nonexistent_dir/test.db");
            assert(false);
        } catch (const auth::UserStoreError&) {}
        std::cout << "[userstore] invalid path OK\n";
    }

    std::cout << "\n=== AuthService ===\n";

    {
        const std::string db = "/tmp/test_as1.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        svc.registerUser("dave", "securepass");
        auto result = svc.login("dave", "securepass");
        assert(!result.token.empty());
        assert(result.userId == "dave");
        std::cout << "[authservice] login success OK\n";
    }

    {
        const std::string db = "/tmp/test_as2.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        svc.registerUser("eve", "correctpass");
        try {
            svc.login("eve", "wrongpass");
            assert(false);
        } catch (const auth::LoginError&) {}
        std::cout << "[authservice] login wrong password OK\n";
    }

    {
        const std::string db = "/tmp/test_as3.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        try {
            svc.login("nobody", "pass");
            assert(false);
        } catch (const auth::LoginError&) {}
        std::cout << "[authservice] login nonexistent OK\n";
    }

    {
        const std::string db = "/tmp/test_as4.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        svc.registerUser("frank", "pass");
        auto result = svc.login("frank", "pass");
        auto payload = svc.verifyToken(result.token);
        assert(payload.userId == "frank");
        std::cout << "[authservice] verifyToken OK\n";
    }

    {
        const std::string db = "/tmp/test_as5.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        try {
            svc.verifyToken("invalid.token.here");
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[authservice] verifyToken invalid OK\n";
    }

    {
        const std::string db = "/tmp/test_as6.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db, -1);
        svc.registerUser("grace", "pass");
        auto result = svc.login("grace", "pass");
        try {
            svc.verifyToken(result.token);
            assert(false);
        } catch (const auth::JwtExpired&) {}
        std::cout << "[authservice] verifyToken expired OK\n";
    }

    {
        const std::string db = "/tmp/test_as7.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        svc.registerUser("henry", "pass1");
        try {
            svc.registerUser("henry", "pass2");
            assert(false);
        } catch (const auth::UserStoreError&) {}
        auto result = svc.login("henry", "pass1");
        assert(!result.token.empty());
        std::cout << "[authservice] register duplicate OK\n";
    }

    {
        const std::string db = "/tmp/test_as8.db";
        remove_db(db);
        auth::AuthService svc("jwt_secret", db);
        svc.registerUser("ivan", "pass");
        auto result = svc.login("ivan", "pass");
        auto payload = svc.verifyToken(result.token);
        assert(payload.userId == result.userId);
        assert(payload.userId == "ivan");
        std::cout << "[authservice] token userId correct OK\n";
    }

    std::cout << "\n=== extractQueryParam ===\n";

    {
        assert(extractQueryParam("/ws?token=abc123", "token") == "abc123");
        std::cout << "[ws_utils] simple token OK\n";
    }

    {
        assert(extractQueryParam("/ws?token=abc&foo=bar", "token") == "abc");
        std::cout << "[ws_utils] token with other params OK\n";
    }

    {
        assert(extractQueryParam("/ws?foo=bar&token=xyz", "token") == "xyz");
        std::cout << "[ws_utils] token not first OK\n";
    }

    {
        assert(extractQueryParam("/ws?foo=bar", "token") == "");
        std::cout << "[ws_utils] missing token OK\n";
    }

    {
        assert(extractQueryParam("/ws", "token") == "");
        std::cout << "[ws_utils] no query string OK\n";
    }

    {
        assert(extractQueryParam("/ws?token=", "token") == "");
        std::cout << "[ws_utils] empty token value OK\n";
    }

    {
        assert(extractQueryParam("", "token") == "");
        std::cout << "[ws_utils] empty target OK\n";
    }

    {
        assert(extractQueryParam("/ws?token=a=b", "token") == "a=b");
        std::cout << "[ws_utils] token with equals in value OK\n";
    }

    std::cout << "\n=== extractPath ===\n";

    {
        assert(extractPath("/api/auth/login") == "/api/auth/login");
        std::cout << "[ws_utils] path no query OK\n";
    }

    {
        assert(extractPath("/api/auth/login?foo=bar") == "/api/auth/login");
        std::cout << "[ws_utils] path with query OK\n";
    }

    {
        assert(extractPath("/ws?token=abc&x=y") == "/ws");
        std::cout << "[ws_utils] path with multiple params OK\n";
    }

    {
        assert(extractPath("") == "");
        std::cout << "[ws_utils] empty path OK\n";
    }

    {
        assert(extractPath("?token=abc") == "");
        std::cout << "[ws_utils] only query OK\n";
    }

    std::cout << "\n=== WsServer JWT validation (no network) ===\n";

    {
        const std::string db = "/tmp/test_ws1.db";
        remove_db(db);
        auth::AuthService svc("ws_secret", db);
        svc.registerUser("alice", "pass");
        auto result  = svc.login("alice", "pass");
        auto payload = svc.verifyToken(result.token);
        assert(payload.userId == "alice");
        std::cout << "[ws jwt] valid token verifies OK\n";
    }

    {
        const std::string db = "/tmp/test_ws2.db";
        remove_db(db);
        auth::AuthService svc("ws_secret", db);
        try {
            svc.verifyToken("bad.token.value");
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[ws jwt] invalid token rejected OK\n";
    }

    {
        const std::string db = "/tmp/test_ws3.db";
        remove_db(db);
        auth::AuthService svc("ws_secret", db, -1);
        svc.registerUser("bob", "pass");
        auto result = svc.login("bob", "pass");
        try {
            svc.verifyToken(result.token);
            assert(false);
        } catch (const auth::JwtExpired&) {}
        std::cout << "[ws jwt] expired token rejected OK\n";
    }

    {
        const std::string db = "/tmp/test_ws4.db";
        remove_db(db);
        auth::AuthService svc("ws_secret", db);
        try {
            svc.verifyToken("");
            assert(false);
        } catch (const auth::JwtInvalid&) {}
        std::cout << "[ws jwt] empty token rejected OK\n";
    }

    {
        const std::string db = "/tmp/test_ws5.db";
        remove_db(db);
        auth::AuthService svc("ws_secret", db);
        svc.registerUser("carol", "pass");
        auto result = svc.login("carol", "pass");
        auto token  = extractQueryParam("/ws?token=" + result.token, "token");
        assert(token == result.token);
        auto payload = svc.verifyToken(token);
        assert(payload.userId == "carol");
        std::cout << "[ws jwt] token extracted from URL and verified OK\n";
    }

    {
        auto token = extractQueryParam("/ws?foo=bar&token=", "token");
        assert(token.empty());
        std::cout << "[ws jwt] empty token from URL rejected by verifyToken\n";
    }

    std::cout << "\n=== WsServer send / removeSession (mock) ===\n";

    {
        std::unordered_map<std::string, std::string> sent_messages;
        std::unordered_map<std::string, bool> sessions;

        auto mock_send = [&](const std::string& userId,
                            const std::string& message) -> bool {
            auto it = sessions.find(userId);
            if (it == sessions.end() || !it->second) return false;
            sent_messages[userId] = message;
            return true;
        };

        auto mock_remove = [&](const std::string& userId) {
            sessions.erase(userId);
        };

        sessions["alice"] = true;
        sessions["bob"]   = true;

        assert( mock_send("alice", "hello"));
        assert( mock_send("bob",   "world"));
        assert(!mock_send("carol", "nobody"));
        assert(sent_messages["alice"] == "hello");
        assert(sent_messages["bob"]   == "world");
        assert(sent_messages.find("carol") == sent_messages.end());
        std::cout << "[ws send] send to connected OK\n";
        std::cout << "[ws send] send to disconnected OK\n";

        mock_remove("alice");
        assert(!mock_send("alice", "after disconnect"));
        assert( mock_send("bob",   "still connected"));
        std::cout << "[ws removeSession] removed session unreachable OK\n";

        mock_remove("bob");
        assert(!mock_send("bob", "after disconnect"));
        assert(sessions.empty());
        std::cout << "[ws removeSession] all sessions removed OK\n";

        mock_remove("nobody");
        std::cout << "[ws removeSession] remove nonexistent OK\n";
    }

    std::cout << "\n=== HttpServer routing (mock) ===\n";

    {
        using Handler = std::function<std::pair<unsigned,std::string>(
                            const std::string& method,
                            const std::string& path,
                            const std::string& body)>;

        std::unordered_map<std::string, Handler> routes;

        auto add_route = [&](const std::string& method,
                            const std::string& path,
                            Handler            handler) {
            routes[method + " " + path] = std::move(handler);
        };

        auto dispatch = [&](const std::string& method,
                            const std::string& target,
                            const std::string& body)
            -> std::pair<unsigned, std::string> {
            auto path = extractPath(target);
            auto key  = method + " " + path;
            auto it   = routes.find(key);
            if (it == routes.end())
                return {404, R"({"error":"not found"})"};
            try {
                return it->second(method, path, body);
            } catch (const std::exception& e) {
                return {500, std::string(R"({"error":")") + e.what() + "\"}"};
            }
        };

        add_route("POST", "/api/auth/login", [](auto, auto, auto body) {
            return std::make_pair(200u, R"({"token":"jwt123"})");
        });

        add_route("GET", "/api/turn/credentials", [](auto, auto, auto) {
            return std::make_pair(200u, R"({"urls":[]})");
        });

        add_route("GET", "/api/throws", [](auto, auto, auto) -> std::pair<unsigned,std::string> {
            throw std::runtime_error("something went wrong");
        });

        auto [s1, b1] = dispatch("POST", "/api/auth/login", R"({"userId":"u","password":"p"})");
        assert(s1 == 200);
        assert(b1 == R"({"token":"jwt123"})");
        std::cout << "[http routing] found route POST OK\n";

        auto [s2, b2] = dispatch("GET", "/api/turn/credentials", "");
        assert(s2 == 200);
        assert(b2 == R"({"urls":[]})");
        std::cout << "[http routing] found route GET OK\n";

        auto [s3, b3] = dispatch("GET", "/api/unknown", "");
        assert(s3 == 404);
        std::cout << "[http routing] not found 404 OK\n";

        auto [s4, b4] = dispatch("DELETE", "/api/auth/login", "");
        assert(s4 == 404);
        std::cout << "[http routing] wrong method 404 OK\n";

        auto [s5, b5] = dispatch("GET", "/api/turn/credentials?token=abc", "");
        assert(s5 == 200);
        std::cout << "[http routing] query string stripped OK\n";

        auto [s6, b6] = dispatch("GET", "/api/throws", "");
        assert(s6 == 500);
        assert(b6.find("something went wrong") != std::string::npos);
        std::cout << "[http routing] handler exception 500 OK\n";

        auto [s7, b7] = dispatch("POST", "/api/auth/login?foo=bar", "");
        assert(s7 == 200);
        std::cout << "[http routing] POST with query string stripped OK\n";
    }

    std::cout << "\nAll backend tests passed\n";
    return 0;
}