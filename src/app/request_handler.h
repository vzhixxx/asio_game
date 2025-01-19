#pragma once

#include "api_handler.h"

namespace http_handler
{
namespace sys   = boost::system;    
namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;


using StringResponse = api_handler::StringResponse;
using FileResponse   = http::response<http::file_body>;


struct ContentType
{
    constexpr static std::string_view TEXT_PLAIN = "text/plain";
    constexpr static std::string_view TEXT_HTML  = "text/html";
    constexpr static std::string_view TEXT_CSS   = "text/plain";

    constexpr static std::string_view APP_JS     = "application/javascript";
    constexpr static std::string_view APP_JSON   = "application/json";
    constexpr static std::string_view APP_XML    = "application/xml";

    constexpr static std::string_view IMAGE_PNG  = "image/png";
    constexpr static std::string_view IMAGE_JPEG = "image/jpeg";
    constexpr static std::string_view IMAGE_GIF  = "image/gif";
    constexpr static std::string_view IMAGE_BMP  = "image/bmp";
    constexpr static std::string_view IMAGE_ICO  = "image/vnd.microsoft.icon";
    constexpr static std::string_view IMAGE_TIFF = "image/tiff";
    constexpr static std::string_view IMAGE_SVG  = "image/svg+xml";

    constexpr static std::string_view AUDIO_MPEG = "audio/mpeg";
};

StringResponse  MakeStringResponse(http::status                status,
                                   std::string_view            body,
                                   unsigned                    http_version,
                                   bool                        keep_alive,
                                   std::string_view            content_type = ContentType::TEXT_HTML);
FileResponse    MakeFileResponse  (http::status                status,
                                   http::file_body::value_type body,
                                   unsigned                    http_version,
                                   bool                        keep_alive,
                                   std::string_view            content_type = ContentType::TEXT_PLAIN);


class RequestHandler : public std::enable_shared_from_this<RequestHandler>
{
    using FileBodyPtr   = std::unique_ptr<http::file_body::value_type>;
    using RestApiRes    = std::tuple<http::status, std::string, std::string_view, FileBodyPtr>;
    using ApiHandlerPtr = std::unique_ptr<api_handler::ApiHandler>;

public:

    using Strand = asio::strand<asio::io_context::executor_type>;

    RequestHandler(Strand api_strand,
                   std::filesystem::path path_static,
                   model::Game& game,
                   db::ConnectionPool * connection_pool);

                    RequestHandler  (const RequestHandler&) = delete;
    RequestHandler& operator=       (const RequestHandler&) = delete;


    template <typename Body, typename Allocator, typename Send>
    void operator()(const asio::ip::tcp::endpoint & endpoint,
                    http::request<Body, http::basic_fields<Allocator>> && req,
                    Send&& send)
    {
        // Обработать запрос request и отправить ответ, используя send

        bool isBadRequest = false;

        if (auto target = req.target(); target.starts_with("/api/"))
        {
            auto handle = [self = shared_from_this(), send, target,
                           req = std::forward<decltype(req)>(req)] {
                
                try
                {
                    // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                    assert(self->api_strand_.running_in_this_thread());

                    const std::string_view body          = req.body();
                    const std::string_view content_type  = req[http::field::content_type];
                    const std::string_view authorization = req[http::field::authorization];

                    return send(self->api_handler_ptr_->HandleApiRequest(req.method(),
                                                                         target,
                                                                         content_type,
                                                                         body,
                                                                         authorization,
                                                                         req.version(),
                                                                         req.keep_alive()));
                }
                catch (...)
                {
                    send(self->ReportServerError("", "exception gained", req.version(), req.keep_alive()));
                }
            };
            
            return asio::dispatch(api_strand_, handle);
        }
        else if (http::verb::get == req.method() || http::verb::head == req.method())
        {
            auto fnSendErr = [version = req.version(), keep_alive = req.keep_alive()](auto && res, auto && send)
            {
                auto rsp = MakeStringResponse(std::get<0>(res),
                                              std::get<1>(res),
                                              version,
                                              keep_alive,
                                              std::get<2>(res));
                send(rsp);
            };

            try
            {
                if (auto res = HandleStaticRequest(target); std::get<3>(res) != nullptr)
                {
                    auto rsp = MakeFileResponse(std::get<0>(res),
                                                std::move(*std::get<3>(res)),
                                                req.version(),
                                                req.keep_alive(),
                                                std::get<2>(res));
                    send(rsp);
                }
                else
                    fnSendErr(std::move(res), std::move(send));
            }
            catch (const RestApiRes & err)
            {
                fnSendErr(std::move(err), std::move(send));
            }
        }
        else
            isBadRequest = true;

        if (isBadRequest)
        {
            auto rsp = MakeStringResponse(http::status::method_not_allowed,
                                          "Invalid HTTP method",
                                          req.version(),
                                          req.keep_alive(),
                                          ContentType::TEXT_HTML);
            send(rsp);
        }
    }

    void ReportError (beast::error_code ec, std::string_view what);

private:

    StringResponse ReportServerError(std::string_view code, std::string_view error, unsigned version, bool keep_alive) const;


    RestApiRes HandleStaticRequest(std::string_view filename) const;
    RestApiRes OnFileFetch(const std::filesystem::path & path) const;
    RestApiRes OnBadRequest(std::string_view code = "badRequest", std::string_view message = "Bad request") const;

    static void ThrowInvalidFilePath(std::string_view uriFilename);
    static void ThrowNotAllowedPath(std::string_view uriFilename);


    Strand api_strand_;
    std::filesystem::path path_static_;
    ApiHandlerPtr api_handler_ptr_;
    
};

}  // namespace http_handler
