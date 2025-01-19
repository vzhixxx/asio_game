#include "request_handler.h"
#include <iostream>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <boost/beast.hpp>


using namespace std::string_view_literals;
using namespace std::literals;

namespace fs = std::filesystem;


namespace http_handler
{

// Возвращает true, если каталог p содержится внутри base_path.
static bool IsSubPath(fs::path & path, const fs::path & base)
{
    auto rel = fs::relative(path, base);
    return !rel.empty() && rel.native()[0] != '.';
}

StringResponse MakeStringResponse(http::status     status,
                                  std::string_view body,
                                  unsigned         http_version,
                                  bool             keep_alive,
                                  std::string_view content_type)
{
    StringResponse response(status, http_version);

    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);

    return response;
}

FileResponse MakeFileResponse(http::status                status,
                              http::file_body::value_type body,
                              unsigned                    http_version,
                              bool                        keep_alive,
                              std::string_view            content_type)
{
    FileResponse response(status, http_version);

    response.set(http::field::content_type, content_type);
    response.body() = std::move(body);

    // Метод prepare_payload заполняет заголовки Content-Length и Transfer-Encoding
    // в зависимости от свойств тела сообщения
    response.prepare_payload();
    
    return response;
}

RequestHandler::RequestHandler(Strand api_strand,
                               std::filesystem::path path_static,
                               model::Game &game,
                               db::ConnectionPool * connection_pool)
              : api_strand_(std::move(api_strand))
              , path_static_(std::move(path_static))
{
    api_handler_ptr_ = std::make_unique<api_handler::ApiHandler>(game, connection_pool);
}

void RequestHandler::ReportError(beast::error_code ec, std::string_view what)
{
    std::cerr << what << ": "sv << ec.message() << std::endl;
}



StringResponse RequestHandler::ReportServerError(std::string_view code, std::string_view error, unsigned version, bool keep_alive) const
{
    boost::json::object jsonErr;
    jsonErr["code"]    = code;
    jsonErr["message"] = error;

    return MakeStringResponse(http::status::internal_server_error,
                              boost::json::serialize(jsonErr),
                              version, keep_alive, ContentType::APP_JSON);
}

static std::string_view mime_type(const std::filesystem::path & path)
{
    using beast::iequals;

    std::string     sExt = path.extension().string();
    std::string_view ext = sExt;

    if (iequals(ext, ".htm"))  return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php"))  return "text/html";
    if (iequals(ext, ".css"))  return "text/css";
    if (iequals(ext, ".txt"))  return "text/plain";
    if (iequals(ext, ".js"))   return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml"))  return "application/xml";
    if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))  return "video/x-flv";
    if (iequals(ext, ".png"))  return "image/png";
    if (iequals(ext, ".jpe"))  return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg"))  return "image/jpeg";
    if (iequals(ext, ".gif"))  return "image/gif";
    if (iequals(ext, ".bmp"))  return "image/bmp";
    if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif"))  return "image/tiff";
    if (iequals(ext, ".svg"))  return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    if (iequals(ext, ".mp3"))  return "audio/mpeg";

    return "application/octet-stream";
}

RequestHandler::RestApiRes RequestHandler::OnFileFetch(const std::filesystem::path & path) const
{
    auto pFileBody = std::make_unique<http::file_body::value_type>();
    if (sys::error_code ec; pFileBody->open(path.c_str(), beast::file_mode::read, ec), ec)
    {
        std::stringstream ss;
        ss << "Failed to open file `"sv << path << '`';
        return
        {
            http::status::not_found,
            ss.str(),
            ContentType::TEXT_PLAIN,
            nullptr
        };
    }

    return RestApiRes
    {
        http::status::ok,
        ""sv,
        mime_type(path),
        std::move(pFileBody)
    };
}

RequestHandler::RestApiRes RequestHandler::HandleStaticRequest(std::string_view uriFilename) const
{
    RestApiRes res;

    if (uriFilename == "/"sv)
        uriFilename = "/index.html"sv;

    if (auto url_view = boost::urls::parse_origin_form(uriFilename); url_view)
    {
        fs::path uriPath  = path_static_;
        uriPath          += url_view.value().path();
        fs::path absPath  = fs::weakly_canonical(uriPath);

        if (IsSubPath(absPath, path_static_))
        {
            if (fs::exists(absPath))
                res = OnFileFetch(absPath);
            else
                ThrowInvalidFilePath(uriFilename);
        }
        else
            ThrowNotAllowedPath(uriFilename);
    }
    else if (url_view .has_error())
    {
        auto sErr = "Failed parse URI with error: "s + url_view.error().message();
        res = OnBadRequest("badRequest"sv, sErr);
    }
    else
        res = OnBadRequest();

    return res;
}

RequestHandler::RestApiRes RequestHandler::OnBadRequest(std::string_view code, std::string_view message) const
{
    boost::json::object jsonErr;
    jsonErr["code"]    = code;
    jsonErr["message"] = message;

    return
    {
        http::status::bad_request,
        boost::json::serialize(jsonErr),
        ContentType::APP_JSON,
        nullptr
    };
}

void RequestHandler::ThrowInvalidFilePath(std::string_view uriFilename)
{
    RestApiRes err = 
    {
        http::status::not_found,
        "Invalid file path: "s + uriFilename.data(),
        ContentType::TEXT_PLAIN,
        nullptr
    };

    throw err;
}

void RequestHandler::ThrowNotAllowedPath(std::string_view uriFilename)
{
    RestApiRes err = 
    {
        http::status::bad_request,
        "Not allowed path: "s + uriFilename.data(),
        ContentType::TEXT_PLAIN,
        nullptr
    };

    throw err;
}

}
