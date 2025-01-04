#include "connection_pool.h"

using namespace db;


ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection()
{
    std::unique_lock lock(mutex_);
    // Блокируем текущий поток и ждём, пока cond_var_ не получит уведомление и не освободится
    // хотя бы одно соединение
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });
    // После выхода из цикла ожидания мьютекс остаётся захваченным

    return {std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr &&conn)
{
    // Возвращаем соединение обратно в пул
    {
        std::lock_guard lock(mutex_);
        assert(used_connections_ != 0);
        pool_[--used_connections_] = std::move(conn);
    }
    // Уведомляем один из ожидающих потоков об изменении состояния пула
    cond_var_.notify_one();
}

void ConnectionPool::PrepareQuery(pqxx::zview tag, pqxx::zview query)
{
    if (!tag.empty() && !query.empty()) {
        std::lock_guard locker(mutex_);

        for (const auto & p : pool_) {
            p->prepare(tag, query);
        }
    }
}
