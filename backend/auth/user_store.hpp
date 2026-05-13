#pragma once

#include <string>
#include <optional>
#include <stdexcept>

struct sqlite3;

namespace auth {

struct User {
    std::string userId;
    std::string passwordHash;
};

struct UserStoreError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class UserStore {
public:
    explicit UserStore(const std::string& db_path);
    ~UserStore();

    UserStore(const UserStore&)            = delete;
    UserStore& operator=(const UserStore&) = delete;

    void                addUser(const std::string& userId,
                                const std::string& password);

    std::optional<User> findUser(const std::string& userId) const;

    bool                checkPassword(const std::string& userId,
                                      const std::string& password) const;

private:
    sqlite3*    db_ = nullptr;

    void        initSchema();
    std::string hashPassword(const std::string& password) const;
};

}