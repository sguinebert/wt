#include "connection.hpp"

#include "Wt/Dbo/backend/Postgres/postgresStatement.h"


std::unique_ptr<SqlStatement> postgrespp::connection::prepareStatement(const std::string &sql) {
    if (PQstatus(underlying_handle()) != CONNECTION_OK)
    {
        //LOG_WARN("connection lost to server, trying to reconnect...");
        //fmtlog::poll();
        if (!reconnect())
        {
            throw std::runtime_error("Could not reconnect to server...");
        }
    }

    return std::unique_ptr<Wt::Dbo::SqlStatement>(new Wt::Dbo::Backend::postgresStatement(*this, sql));
}
