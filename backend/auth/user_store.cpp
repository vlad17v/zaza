#include "user_store.hpp"

#include <sqlite3.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace auth {

std::string UserStore::hashPassword(const std::string& password) const {
    uint8_t hash[SHA256_DIGEST_LENGTH];

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, password.data(), password.size());
    EVP_DigestFinal_ex(ctx, hash, nullptr);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (auto b : hash)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

UserStore::UserStore(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw UserStoreError("cannot open database: " + err);
    }
    initSchema();
}

UserStore::~UserStore() {
    if (db_) sqlite3_close(db_);
}

void UserStore::initSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "  userId       TEXT PRIMARY KEY,"
        "  passwordHash TEXT NOT NULL"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg;
        sqlite3_free(errMsg);
        throw UserStoreError("initSchema failed: " + err);
    }
}

void UserStore::addUser(const std::string& userId,
                        const std::string& password) {
    if (findUser(userId).has_value())
        throw UserStoreError("user already exists: " + userId);

    const char* sql =
        "INSERT INTO users (userId, passwordHash) VALUES (?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw UserStoreError("addUser prepare failed");

    sqlite3_bind_text(stmt, 1, userId.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashPassword(password).c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        throw UserStoreError("addUser failed: " + std::string(sqlite3_errmsg(db_)));
}

std::optional<User> UserStore::findUser(const std::string& userId) const {
    const char* sql =
        "SELECT userId, passwordHash FROM users WHERE userId = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw UserStoreError("findUser prepare failed");

    sqlite3_bind_text(stmt, 1, userId.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<User> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User u;
        u.userId       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        u.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        result = std::move(u);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool UserStore::checkPassword(const std::string& userId,
                               const std::string& password) const {
    auto user = findUser(userId);
    if (!user.has_value()) return false;

    return user->passwordHash == hashPassword(password);
}

}