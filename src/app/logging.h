#pragma once

#include <chrono>

#include "http_server.h"

#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <boost/json.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>


BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

BOOST_LOG_ATTRIBUTE_KEYWORD(file, "File", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line, "Line", int)

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)


namespace server_logging
{
    namespace fs       = std::filesystem;
    namespace sys      = boost::system;

    namespace logging  = boost::log;
    namespace keywords = boost::log::keywords;
    namespace sinks    = boost::log::sinks;
    namespace expr     = boost::log::expressions;
    namespace json     = boost::json;
    namespace beast    = boost::beast;
    namespace http     = beast::http;
    namespace asio     = boost::asio;
    

    void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm);

    void AddConsoleLog(void);

    void AddFileLog(void);

    void InitBoostLogFilter(void);


    template<class BaseRequestHandler>
    class LoggingRequestHandler
    {
        using ClockT        = std::chrono::system_clock; 
        using TimePoint     = ClockT::time_point;
        using ReqHandlerPtr = std::shared_ptr<BaseRequestHandler>;

    public:

        LoggingRequestHandler(ReqHandlerPtr handler) : decorated_(std::move(handler)) {
        }

        template <typename Body, typename Allocator, typename Send>
        void operator()(const asio::ip::tcp::endpoint & endpoint,
                        http::request<Body, http::basic_fields<Allocator>>&& req,
                        Send&& send)
        {
            LogRequest(endpoint, req);

            auto fnOnResponse = [this, &endpoint, tpBegin = ClockT::now(), snd = std::move(send)](auto&& response) {
                LogResponse(endpoint, tpBegin, response);

                snd(std::move(response));
            };

            (*decorated_)(endpoint, std::move(req), std::move(fnOnResponse));
        }

        void ReportError (beast::error_code ec, std::string_view where)
        {
            using namespace std::literals;

            json::object msg;
            msg["code"]  = 1;
            msg["text"]  = ec.message();
            msg["where"] = where;

            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, msg)
                                    << "error"sv;
        }

    private:

        template <typename Body, typename Allocator>
        void LogRequest(const asio::ip::tcp::endpoint & endpoint, const http::request<Body, http::basic_fields<Allocator>> & req)
        {
            using namespace std::literals;

            json::object msg;
            msg["ip"]     = endpoint.address().to_string();
            msg["URI"]    = req.target();
            msg["method"] = req.method_string();

            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, msg)
                                    << "request received"sv;
        }

        template <typename Body, typename Fields>
        void LogResponse(const asio::ip::tcp::endpoint & endpoint, const TimePoint & tpBegin, const http::response<Body, Fields> & response)
        {
            using namespace std::literals;
            using namespace std::chrono;

            auto elapsedMs = duration_cast<milliseconds>(system_clock::now() - tpBegin);

            json::object msg;
            msg["ip"]            = endpoint.address().to_string();
            msg["response_time"] = elapsedMs.count();
            msg["code"]          = response.result_int();
            msg["content_type"]  = response[http::field::content_type];

            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, msg)
                                    << "response sent"sv;
        }


        ReqHandlerPtr decorated_;
};

}
