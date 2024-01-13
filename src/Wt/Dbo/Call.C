#include <Wt/Dbo/Call.h>
#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/WLogger.h>

namespace Wt {
  namespace Dbo {

Call::~Call() noexcept(false)
{
//  if (!copied_ && !run_)
//        loge("Call not run or copied");//run();
}

Call::Call(const Call& other)
  : copied_(false),
    run_(false),
    statement_(other.statement_),
    column_(other.column_), session_(other.session_)//, sql_(std::move(other.sql_))
{
  const_cast<Call&>(other).copied_ = true;
}

awaitable<void> Call::run()
{
  try {
    run_ = true;
    /*auto result =*/ co_await statement_->execute();
    statement_->done();
    co_return;
  } catch (...) {
    statement_->done();
    throw;
  }
  co_return; //std::variant<result_pp, result_mysql, ...>
}

Call::Call(Session& session, const std::string& sql)
  : copied_(false),
    run_(false),
    session_(session)
{
  statement_ = session.getOrPrepareStatement(sql);
  column_ = 0;
}



}
}
