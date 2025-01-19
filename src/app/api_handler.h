#pragma once

#include "http_server.h"
#include "app.h"
#include "connection_pool.h"

#include <boost/json.hpp>

namespace api_handler
{
namespace sys   = boost::system;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
namespace json  = boost::json;

using StringResponse = http::response<http::string_body>;


class ApiHandlerException : public std::exception
{
public:
    
    explicit ApiHandlerException(StringResponse && rsp) : response_(std::move(rsp)) {
    }

    [[nodiscard]] const StringResponse & GetStringResponse() const noexcept {
        return response_;
    }

private:
    
    StringResponse response_;
};


class ApiHandler
{
    using ApplicationPtr = std::unique_ptr<app::Application>;

public:

    ApiHandler(model::Game &game, db::ConnectionPool * connection_pool);
    
    StringResponse HandleApiRequest(http::verb method,
                                    std::string_view target,
                                    std::string_view content_type,
                                    std::string_view body,
                                    std::string_view authorization,
                                    unsigned version,
                                    bool keep_alive);
    StringResponse HandleApiRequestV1(http::verb method,
                                      std::string_view target,
                                      std::string_view content_type,
                                      std::string_view body,
                                      std::string_view authorization,
                                      unsigned version,
                                      bool keep_alive);
    [[nodiscard]] StringResponse HandleApiMapsRequest(std::string_view target,
                                                      unsigned version,
                                                      bool keep_alive) const;
    StringResponse HandleApiGameRequest(http::verb method,
                                        std::string_view target,
                                        std::string_view content_type,
                                        std::string_view body,
                                        std::string_view authorization,
                                        unsigned version,
                                        bool keep_alive);
    StringResponse OnGameJoin(std::string_view content_type,
                              std::string_view body,
                              unsigned version,
                              bool keep_alive);
    [[nodiscard]] StringResponse OnGamePlayers([[maybe_unused]] std::string_view body,
                                               std::string_view authorization,
                                               unsigned version,
                                               bool keep_alive) const;
    [[nodiscard]] StringResponse OnGamePlayer(std::string_view target,
                                              std::string_view content_type,
                                              std::string_view body,
                                              std::string_view authorization,
                                              unsigned version,
                                              bool keep_alive) const;
    [[nodiscard]] StringResponse OnGameTick(std::string_view content_type,
                                            std::string_view body,
                                            unsigned version,
                                            bool keep_alive) const;
    [[nodiscard]] StringResponse OnGameState(std::string_view authorization,
                                             unsigned version,
                                             bool keep_alive) const;
    [[nodiscard]] StringResponse OnGameRecords(std::string_view authorization,
                                               std::string_view body,
                                               unsigned version,
                                               bool keep_alive) const;
    [[nodiscard]] StringResponse OnCmdMaps(unsigned version,
                                           bool keep_alive) const;
    [[nodiscard]] StringResponse OnCmdFetchMap(std::string_view mapName,
                                               unsigned version,
                                               bool keep_alive) const;
private:
    [[nodiscard]] model::Player &FindPlayerByToken(std::string_view authorization,
                                                   unsigned version,
                                                   bool keep_alive) const;

    static json::value ParseJson(std::string_view content_type,
                                 std::string_view body,
                                 unsigned version,
                                 bool keep_alive);
    static void ThrowMethodNotAllowed(unsigned version,
                                      bool keep_alive,
                                      std::string_view message,
                                      std::string_view allow,
                                      std::string_view code="invalidMethod");
    static void ThrowPostOnlyExpected(unsigned version,
                                      bool keep_alive);
    static void ThrowGetHeadOnlyExpected(unsigned version,
                                         bool keep_alive);
    static void ThrowInvalidArgument(unsigned version,
                                     bool keep_alive,
                                     std::string_view message);
    static void ThrowBadRequest(unsigned version,
                                bool keep_alive,
                                std::string_view message,
                                std::string_view code="badRequest");
    static void ThrowNotFound(unsigned version,
                              bool keep_alive,
                              std::string_view message,
                              std::string_view code="notFound");

private:

    ApplicationPtr app_;
    db::ConnectionPool * connection_pool_ = nullptr;
};

}