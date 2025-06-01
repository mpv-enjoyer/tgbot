#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <fstream>
#include <optional>
#include <iostream>
#include <sstream>
#include <mutex>
#include <memory>

#include <tgbot/tgbot.h>

enum ErrorCodes
{
    OK = 0,
    NO_SECRET = 1,
    REQUEST_REBOOT = 2
};

namespace StringTools
{
    template<typename T>
    std::string toString(const T& value) { return std::to_string(value); }
    template<>
    std::string toString<>(const std::string& value) { return value; }
    std::string toString(const char* value) { return std::string(value); }

    inline std::string concat() { return std::string(); }
    template <typename First, typename... T>
    inline std::string concat(const First& first, T... remaining)
    {
        return toString(first) + concat(remaining...);
    }
};

class Secrets
{
public:
    bool isInit() const { return m_init; }
    std::string getToken() const { return m_token; }
    std::string getOwnerID() const { return m_owner_id; }
    int64_t getOwnerIDNum() const { return m_owner_id_num; }
    static Secrets create()
    {
        return Secrets();
    };
private:
    bool m_init = false;
    std::string m_token;
    std::string m_owner_id;
    int64_t m_owner_id_num;
    std::optional<std::string> getEnvFromFile(const char* env) {
        std::ifstream in(std::string("secrets/")+std::string(env));
        if (in.fail()) return { };
        std::ostringstream sstr;
        sstr << in.rdbuf();
        return sstr.str();
    }
    Secrets()
    {
        auto env = getEnvFromFile("API_TOKEN");
        if (!env) return;
        m_token = *env;

        env = getEnvFromFile("OWNER_ID");
        if (!env) return;
        m_owner_id = *env;
        std::stringstream s(m_owner_id);
        s >> m_owner_id_num;

        m_init = true;
    }
};

using namespace TgBot;

template <typename... T>
void logToOwner(Bot& bot, const Secrets& secrets, T... message)
{
    auto output = StringTools::concat("LOG: ", message...);
    bot.getApi().sendMessage(secrets.getOwnerID(), output);
};

int main() {
    const Secrets secrets = Secrets::create();
    if (!secrets.isInit()) return ErrorCodes::NO_SECRET;

    bool want_restart = false;

    Bot bot(secrets.getToken());
    bot.getEvents().onCommand("start", [&bot, &secrets](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Hi!");
        auto str = StringTools::concat("hello to chatID ", message->chat->id);
        if (message->senderChat != nullptr) str = StringTools::concat(str, " senderChatID ", message->senderChat->id);
        bot.getApi().sendMessage(secrets.getOwnerID(), str);
    });
    bot.getEvents().onCommand("id", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, StringTools::concat("Your ID is ", message->chat->id));
    });
    bot.getEvents().onCommand("reboot", [&bot, &want_restart, &secrets](Message::Ptr message) {
        if (message->from && secrets.getOwnerIDNum() == message->from->id)
        {
            bot.getApi().sendMessage(message->chat->id, StringTools::concat("OK. ", message->chat->id, "<->", message->from->id));
            want_restart = true;
        }
        else if (message->chat->id == secrets.getOwnerIDNum())
        {
            bot.getApi().sendMessage(message->chat->id, StringTools::concat("OK. ", message->chat->id));
            want_restart = true;
        }
    });
    bot.getEvents().onCommand("скажи_привет", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "привет)");
    });
    bot.getEvents().onCommand("скажи ", [&bot](Message::Ptr message) {
        const static std::string prefix = "/скажи ";
        auto str = message->text.substr(prefix.size());
        bot.getApi().sendMessage(message->chat->id, str);
    });

    signal(SIGINT, [](int s) {
        printf("got SIGINT\n");
        exit(ErrorCodes::OK);
    });

    try {
        logToOwner(bot, secrets, "Bot username: ", bot.getApi().getMe()->username);
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
            if (want_restart) return ErrorCodes::REQUEST_REBOOT;
        }
    } catch (std::exception& e) {
        logToOwner(bot, secrets, "ERROR: ", e.what());
    }

    return 0;
}

