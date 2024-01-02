/*
 * Copyright (C) 2023
 *
 * LICENSE MIT.
 *
 * Contributed by: Guinebert Sylvain
 */
#pragma once
//#include <Wt/Dbo/SqlConnection.h>
//#include <Wt/Dbo/SqlStatement.h>
#include <Wt/WIOService.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
//#include <Wt/WLogger.h>

//#include <chrono>
using namespace boost;

namespace Wt {
  namespace Dbo {

class Redis
{
public:
    Redis(Wt::WIOService& ioservice, std::string_view ip, std::string_view port) : ioservice_(ioservice), address_(ip), port_(port)
    {
        conf_ = {
                 .use_ssl = true,
                 .addr = {address_, port_},
                 };
    }

//    awaitable<void> setFixedPoolConnection(int num)
//    {
//        for(auto i = 0; i < num; i++) {
//            pool_.emplace_back(ioservice_.get());
//            co_await pool_.back().async_run(conf_, {}, use_awaitable);
//        }
//        co_return;
//    }

//    redis::connection& getConnection() {
//        return pool_.back();
//    }
    template<class Token>
    auto getConnection(Token&& handler)
    {
        auto initiator = [this] (auto&& handler)
        {
            redis::connection conn(ioservice_.get());
            conn.async_run(conf_, {}, [this, conn = std::move(conn), handler = std::move(handler)]{
                handler(conn);
            });

        };
        return asio::async_initiate<Token, void(redis::connection)>(initiator, handler);
    }

private:
    Wt::WIOService& ioservice_;
    std::string address_, port_;
    std::vector<redis::connection> pool_;
    redis::config conf_;
};

  }//Dbo
}//Wt
