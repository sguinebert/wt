#include "connection.hpp"

#include "Wt/Dbo/backend/Postgres/PostgresStatement.h"


std::unique_ptr<SqlStatement> postgrespp::connection::prepareStatement(const std::string &sql) {
    if (!underlying_handle() || PQstatus(underlying_handle()) != CONNECTION_OK)
    {
        //LOG_WARN("connection lost to server, trying to reconnect...");
        //fmtlog::poll();
        if (!reconnect())
        {
            return nullptr;
        }
    }
    
    return std::unique_ptr<Wt::Dbo::SqlStatement>(new Wt::Dbo::backend::PostgresStatement(*this, sql));
}
