#include "auth/jwt.hpp"
#include "auth/user_store.hpp"
#include "auth/auth_service.hpp"

#include <iostream>
#include <cassert>
#include <cstdio>

static void remove_db(const std::string& path) {
    std::remove(path.c_str());
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

    std::cout << "\nAll backend/auth tests passed\n";
    return 0;
}