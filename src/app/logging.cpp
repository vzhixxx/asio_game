#include "logging.h"
#include <iostream>
#include <boost/date_time.hpp>


using namespace std::literals;

namespace server_logging
{

void Formatter(const logging::record_view & rec, logging::formatting_ostream & strm)
{
    strm << "{\"timestamp\":\""sv;

    // Момент времени приходится вручную конвертировать в строку.
    // Для получения истинного значения атрибута нужно добавить разыменование. 
    auto ts = *rec[timestamp];
    strm << to_iso_extended_string(ts) << "\",\"data\":"sv;

    // JSON custom log data
    auto json_data = *rec[additional_data];
    strm << json::serialize(json_data);

    // выводим само сообщение
    strm << ",\"message\":\""sv << rec[expr::smessage] << "\"}";
}

void AddConsoleLog()
{
    auto pLogSinkConsole = logging::add_console_log( 
        std::clog,
        keywords::format     = &server_logging::Formatter,
        keywords::auto_flush = true);
}

void AddFileLog()
{
    auto pLogSinkFile = logging::add_file_log(
        keywords::auto_flush    = true,
        keywords::file_name     = "sample_%N.log",
        //keywords::open_mode     = std::ios_base::app | std::ios_base::out,
        keywords::format        = &server_logging::Formatter,
        // ротируем по достижению размера 10 мегабайт
        keywords::rotation_size = 10 * 1024 * 1024,
        // ротируем ежедневно в полдень
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(12, 0, 0));
}   

void InitBoostLogFilter()
{
#if 1
    logging::core::get()->set_filter(
        logging::trivial::severity >= logging::trivial::info
    );
#endif
}

}
