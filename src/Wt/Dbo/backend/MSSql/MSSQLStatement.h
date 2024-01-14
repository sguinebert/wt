/*
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Contributed by: Paul Harrison
 */
#pragma once
#include <Wt/Dbo/backend/MSSQLServer.h>
#include <Wt/Dbo/SqlStatement.h>
//#include <Wt/Dbo/backend/WDboMSSQLDllDefs.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <Wt/Dbo/Exception.h>
#include <Wt/WLogger.h>



namespace Wt {
namespace Dbo {
namespace backend {
class MSSQLServerException : public Exception
{
public:
    MSSQLServerException(const std::string& msg,
                         const std::string &sqlState = std::string())
        : Exception(msg, sqlState)
    { }
};


}
}
}

