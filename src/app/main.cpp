#include <iostream>
#include <thread>
#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

//
#include "../lib/json_loader.h"
#include "request_handler.h"
#include "logging.h"
#include "ticker.h"
#include "connection_pool.h"


using namespace std::literals;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace fs       = std::filesystem;
namespace net      = boost::asio;
namespace sys      = boost::system;
namespace logging  = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks    = boost::log::sinks;
namespace json     = boost::json;

namespace
{

// Запускает функцию fn на numWorkers потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned numWorkers, const Fn& fn)
{
    numWorkers = std::max(1u, numWorkers);
    std::vector<std::jthread> workers;
    workers.reserve(numWorkers - 1);
    // Запускаем numWorkers-1 рабочих потоков, выполняющих функцию fn
    while (--numWorkers) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace


struct Args
{
    int tick_period = 0;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};


[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[])
{
    namespace po = boost::program_options;

    Args args;
    po::options_description desc("All options"s);
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t",          po::value(&args.tick_period)->value_name("milliseconds"), "set tick period")
        ("config-file,c",          po::value(&args.config_file)->value_name("file"),         "set config file path")
        ("www-root,w",             po::value(&args.www_root)->value_name("dir"),             "set static files root")
        ("randomize-spawn-points", po::value(&args.randomize_spawn_points)->value_name(" "), "spawn dogs at random positions")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s))
    {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Server config file missed"s);
    }

    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("www-root directory with static files not present"s);
    }

    return args;
}


int main(int argc, const char* argv[])
{
    logging::add_common_attributes();

    server_logging::AddConsoleLog();
    server_logging::InitBoostLogFilter();

    const char* db_url = std::getenv("DB_URL");
    if (!db_url) {
        return EXIT_FAILURE;
    }

    try 
    {   
        if (auto args = ParseCommandLine(argc, argv); args)
        {
            db::ConnectionPool conn_pool{2, [db_url] {
                return std::make_shared<pqxx::connection>(db_url);
            }};

            std::filesystem::path serverBin  = argv[0];
            std::filesystem::path configPath = fs::weakly_canonical(serverBin.parent_path() / args->config_file);
            std::filesystem::path staticPath = fs::weakly_canonical(serverBin.parent_path() / args->www_root);

            // 1. Загружаем карту из файла и модель игры
            auto pGame = json_loader::LoadGame(configPath);
            pGame->SetRandomizeSpawnPoints(args->randomize_spawn_points);
            pGame->SetTickPeriod(args->tick_period);

            // 2. Инициализируем io_context
            unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(static_cast<int>(num_threads));

            // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number)
            {
                if (!ec)
                    ioc.stop();
            });

            // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
            // strand для выполнения запросов к API
            auto api_strand = net::make_strand(ioc);

            // Создаём обработчик запросов в куче, управляемый shared_ptr
            auto handler = std::make_shared<http_handler::RequestHandler>(api_strand,
                                                                                       staticPath,
                                                                                       *pGame,
                                                                                       conn_pool);

            server_logging::LoggingRequestHandler<http_handler::RequestHandler> logging_handler(std::move(handler));

            std::shared_ptr<model::Ticker> pTicker;
            if (int period = pGame->GetTickPeriod(); period > 0) {
                pTicker = std::make_shared<model::Ticker>(api_strand,
                                                          milliseconds(period),
                                                          [game = pGame.get()](auto && elapsed_ms) {
                    game->Think(elapsed_ms.count());
                });

                pTicker->Start();
            }

            // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
            constexpr std::string_view svAddress = "0.0.0.0"sv;
            constexpr net::ip::port_type port    = 8080;
            const auto address                   = net::ip::make_address(svAddress);

            http_server::ServeHttp(ioc, {address, port}, logging_handler);

            // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
            //std::cout << "Server has started..."sv << std::endl;
            {
                json::object msg;
                msg["port"]    = port;
                msg["address"] = svAddress;

                BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, msg)
                                        << "server started"sv;
            }

            // 6. Запускаем обработку асинхронных операций
            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });
        }
    }   
    catch (const std::exception& ex)
    {   
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }   

    {   
        json::value custom_data{{"code"s, 0}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "server exited"sv;        
    }

    return EXIT_SUCCESS;
}
