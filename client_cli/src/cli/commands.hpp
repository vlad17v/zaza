#pragma once

#include "session/session.hpp"

#include <string>
#include <vector>
#include <functional>

namespace cli {

using WsSendCallback   = std::function<void(const std::string& json)>;
using CaptureCallback  = std::function<void(bool start,
                                             const std::string& filename)>;
using SendfileCallback = std::function<void(const std::string& filename)>;

struct CommandResult {
    bool        ok      = true;
    std::string message;
};

class Commands {
public:
    explicit Commands(session::Session& session);

    CommandResult login   (const std::vector<std::string>& args);
    CommandResult call    (const std::vector<std::string>& args);
    CommandResult accept  (const std::vector<std::string>& args);
    CommandResult reject  (const std::vector<std::string>& args);
    CommandResult hangup  (const std::vector<std::string>& args);
    CommandResult mute    (const std::vector<std::string>& args);
    CommandResult unmute  (const std::vector<std::string>& args);
    CommandResult status  (const std::vector<std::string>& args);
    CommandResult record  (const std::vector<std::string>& args);
    CommandResult stop    (const std::vector<std::string>& args);
    CommandResult sendfile(const std::vector<std::string>& args);
    CommandResult quit    (const std::vector<std::string>& args);

    bool shouldQuit() const { return quit_; }

    void setWsSend        (WsSendCallback   cb);
    void setCaptureCallback (CaptureCallback  cb);
    void setSendfileCallback(SendfileCallback cb);

private:
    session::Session& session_;
    bool              quit_        = false;
    WsSendCallback    ws_send_;
    CaptureCallback   capture_cb_;
    SendfileCallback  sendfile_cb_;
};

}