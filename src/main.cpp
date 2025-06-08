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

#include <sqlite3.h>
#include <memory>
#include <filesystem>

enum ErrorCodes
{
    OK = 0,
    NO_SECRET = 1,
    REQUEST_REBOOT = 2,
    NO_DB = 3
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
        std::string str = sstr.str();
        while (str.find('\n') != std::string::npos)
        {
            str.erase(str.begin() + str.find('\n'));
        }
        return str;
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

struct SQLite
{
    static int callback(void *sqlite_notype, int argc, char **argv, char **azColName) {
        auto sqlite = reinterpret_cast<SQLite*>(sqlite_notype);
        for(int i=0; i < argc; i++) {
            sqlite->response_buffer.push_back({argv[i], azColName[i]});
            printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
        printf("\n");
        return 0;
    }

    int updateNumber() {
        auto sqlite = this;
        auto response = sqlite->statement("SELECT * FROM number");
        int number_updated = std::atoi(response[0].first.c_str()) + 1;
        auto number_updated_str = std::to_string(number_updated);
        auto statement = std::string("UPDATE number SET num = ") + number_updated_str;
        sqlite->statement(statement.c_str());
        return number_updated;
    }

    std::vector<std::pair<std::string, std::string>> response_buffer;
    sqlite3 *db = nullptr;
    int error = 0;
    char *zErrMsg = 0;
    bool existed = false;

    SQLite(const char* filename) {
        existed = std::filesystem::exists(filename);
        error = sqlite3_open(filename, &db);
        if (error) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        }
        if (!existed)
        {
            statement("CREATE TABLE number (num INTEGER)");
            statement("INSERT INTO number VALUES (0)");
        }
    }
    std::vector<std::pair<std::string, std::string>> statement(const char* statement_, int (*callback_)(void *sqlite_notype, int argc, char **argv, char **azColName) = nullptr)
    {
        response_buffer.clear();
        if (!db) fprintf(stderr, "no DB statement\n");
        if (!callback_) callback_ = callback;
        error = sqlite3_exec(db, statement_, callback_, this, &zErrMsg);
        if( error != SQLITE_OK ){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        return response_buffer;
    }
    ~SQLite() { sqlite3_close(db); }
};


int main(int argc, char** argv) {
    if (argc != 2) return ErrorCodes::NO_DB;

    const Secrets secrets = Secrets::create();
    if (!secrets.isInit()) return ErrorCodes::NO_SECRET;
    SQLite sqlite(argv[1]);

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
    bot.getEvents().onCommand("скажи", [&bot](Message::Ptr message) {
        const static std::string prefix = "/скажи ";
        auto str = message->text.substr(prefix.size());
        bot.getApi().sendMessage(message->chat->id, str);
    });
    bot.getEvents().onCommand("what", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "ничо");
    });
    bot.getEvents().onCommand("добавить", [&bot, &sqlite](Message::Ptr message) {
        int number_updated = sqlite.updateNumber();
        bot.getApi().sendMessage(message->chat->id, std::to_string(number_updated));
    });

    signal(SIGINT, [](int s) {
        printf("got SIGINT\n");
        exit(ErrorCodes::OK);
    });

    try {
        std::cout << "API token is " << bot.getToken() << std::endl;
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

