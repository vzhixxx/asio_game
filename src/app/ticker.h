#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>


namespace model
{
namespace asio = boost::asio;
namespace sys  = boost::system;

class Ticker : public std::enable_shared_from_this<Ticker>
{
public:
    using Strand  = asio::strand<asio::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
            : strand_(std::move(strand))
            , handler_(std::move(handler)) {
        period_ = period;
    }

    void Start() {
        last_tick_ = std::chrono::steady_clock::now();
        timer_.expires_after(period_);

        ScheduleTick();
    }

private:

    void ScheduleTick() {

        timer_.async_wait([this](const auto & ec) {
            OnTick(ec);
        });
    }

    void OnTick(const sys::error_code & ec) {

        auto current_tick = std::chrono::steady_clock::now();
        auto elapsed_ms = duration_cast<std::chrono::milliseconds>(current_tick - last_tick_);

        handler_(elapsed_ms);

        last_tick_ = current_tick;

        ScheduleTick();
    }

    Strand strand_;
    asio::steady_timer timer_{strand_};
    std::chrono::milliseconds period_{};
    Handler handler_;

    std::chrono::steady_clock::time_point last_tick_;
};

}
