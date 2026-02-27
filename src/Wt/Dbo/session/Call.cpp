#include <Wt/Dbo/session/Call.h>
#include <Wt/Dbo/session/Session.h>
#include <Wt/Dbo/sql/Statement.h>
#include <Wt/WLogger.h>

namespace Wt {
  namespace Dbo {

Call::~Call() noexcept
{
}

Call::Call(const Call& other)
  : copied_(false),
    run_(false),
    statement_(other.statement_),
    column_(other.column_), session_(other.session_)//, sql_(std::move(other.sql_))
{
  const_cast<Call&>(other).copied_ = true;
}

awaitable<dbo_result<void>> Call::run()
{
  struct DoneFinalizer {
    SqlStatement* statement;
    ~DoneFinalizer() { statement->done(); }
  };

  run_ = true;

  if (!statement_)
    co_return std::unexpected(dbo_error{
      DboErrc::Sql,
      "Call::run(): statement is null (prepare failed)",
      {}, {}, 0,
      "Call::run"});

  DoneFinalizer done{statement_};
  co_return co_await statement_->execute();
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
