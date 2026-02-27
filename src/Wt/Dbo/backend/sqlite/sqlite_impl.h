#ifndef SQLITER_BASIC_SQLITE_IMPL_H
#define SQLITER_BASIC_SQLITE_IMPL_H

#include <functional>
#include <sqlite3.h>
#include <string>

namespace sqliter {
namespace asio {

static int query_callback(void *h, int argc, char **argv, char **azColName);

class sqlite_impl
{
private:
  using query_cb_type = std::function<void(int, char **, char **)>;

public:
  struct query_handler
  {
    query_cb_type callback;
  };

  void open(const std::string &db_name)
  {
    last_error_.clear();
    if (sqlite3_open(db_name.c_str(), &_db) != SQLITE_OK)
    {
      last_error_ = sqlite3_errmsg(_db);
      sqlite3_close(_db);
      _db = nullptr;
    }
  }
  void destroy()
  {
    sqlite3_close(_db);
    _db = nullptr;
  }

  void query(const std::string &sql, const query_handler handler)
  {
    last_error_.clear();
    int rc = sqlite3_exec(_db, sql.c_str(), query_callback, (void *)&handler, NULL);
    if (rc != SQLITE_OK)
    {
      last_error_ = sqlite3_errmsg(_db);
    }
  }

  void query(const std::string &sql)
  {
    last_error_.clear();
    int rc = sqlite3_exec(_db, sql.c_str(), query_callback, NULL, NULL);
    if (rc != SQLITE_OK)
    {
      last_error_ = sqlite3_errmsg(_db);
    }
  }

  const std::string& last_error() const
  {
    return last_error_;
  }

private:
  sqlite3 *_db;
  std::string last_error_;
};

static int query_callback(void *h, int argc, char **argv, char **azColName)
{
  sqlite_impl::query_handler *handler = (sqlite_impl::query_handler *)h;
  if (handler && handler->callback != nullptr)
  {
    handler->callback(argc, argv, azColName);
  }
  return 0;
}

}  // namespace asio
}  // namespace sqliter

#endif
