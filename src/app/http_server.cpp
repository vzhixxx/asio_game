#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

using namespace std::literals;
using namespace std::string_view_literals;

namespace http_server
{

SessionBase::SessionBase(tcp::socket&& socket)
           : stream_(std::move(socket))
{

}

void SessionBase::Run()
{
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read()
{
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    stream_.expires_after(30s);
    // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
    // По окончании операции будет вызван метод OnRead
    http::async_read(stream_,
                     buffer_,
                     request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read)
{
    if (ec == http::error::end_of_stream)
        // Нормальная ситуация - клиент закрыл соединение
        Close();
    else if (ec)
        ReportError(ec, "read"sv);
    else
    {
        HandleRequest(stream_.socket().remote_endpoint(), std::move(request_));
    }
}

void SessionBase::OnWrite(bool close,
                          beast::error_code ec,
                          [[maybe_unused]] size_t bytes_written)
{
    if (ec)
        ReportError(ec, "write"sv);
    else if (close)
        Close();                            // Семантика ответа требует закрыть соединение
    else
        Read();                             // Считываем следующий запрос
}

void SessionBase::Close()
{
    stream_.socket().shutdown(tcp::socket::shutdown_send);
}

}  // namespace http_server