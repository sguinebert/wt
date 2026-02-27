/*
 * Copyright (C) 2017 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include "Wt/Dbo/backend/MSSQLServer.h"

#include "Wt/Dbo/core/Exception.h"
#include "Wt/Dbo/Logger.h"
#include "Wt/Dbo/backend/MSSql/MSSQLStatement.h"

#include "Wt/Date/date.h"

//#include "Wt/Dbo/backend/MSSql/nanodbc/nanodbc.h"

#ifdef WT_WIN32
#define NOMINMAX
#include <windows.h>
#endif // WT_WIN32
#include <sql.h>
#include <sqlext.h>
#ifndef WT_WIN32
#include <sqlucode.h>

#include <codecvt>
#include <string>
#endif // WT_WIN32

#include <cstring>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

// define from sqlncli.h
// See: https://docs.microsoft.com/en-us/sql/relational-databases/native-client/features/using-multiple-active-result-sets-mars
#ifndef SQL_COPT_SS_MARS_ENABLED
#define SQL_COPT_SS_MARS_ENABLED 1224
#define SQL_MARS_ENABLED_YES 1L
#endif

namespace Wt {
  namespace Dbo {

LOGGER("Dbo.backend.MSSQLServer");

    namespace backend {

namespace {

static bool handleErr(SQLSMALLINT handleType,
                      SQLHANDLE handle,
                      SQLRETURN rc,
                      dbo_error* out = nullptr,
                      std::string_view context = {})
{
  if (SQL_SUCCEEDED(rc))
    return true;

  SQLCHAR sqlState[6];
  std::memset(sqlState, 0, sizeof(sqlState));
  SQLINTEGER nativeErr = 0;
  SQLSMALLINT msgLength = 0;
  SQLRETURN diagRc = SQL_ERROR;
  SQLCHAR buf[SQL_MAX_MESSAGE_LENGTH];
  std::memset(buf, 0, sizeof(buf));
  diagRc = SQLGetDiagRec(
    handleType,
    handle,
    1,
    sqlState,
    &nativeErr,
    buf,
    sizeof(buf),
    &msgLength
  );
  const std::string message =
    SQL_SUCCEEDED(diagRc)
      ? std::string(reinterpret_cast<const char*>(buf), msgLength)
      : std::string("ODBC operation failed");
  const std::string state =
    SQL_SUCCEEDED(diagRc)
      ? std::string(reinterpret_cast<const char*>(sqlState), 5)
      : std::string();

  dbo_error err{
    DboErrc::Sql,
    message,
    "mssql",
    state,
    static_cast<int>(nativeErr),
    std::string(context)
  };

  LOG_ERROR("MSSQL error: {}", message);
  fmtlog::poll();

  if (out)
    *out = std::move(err);

  return false;
}

#ifndef WT_WIN32
std::u16string toUTF16(const std::string &str)
{
  return
    std::wstring_convert<
      std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(str.data());
}
#endif // WT_WIN32

#ifdef WT_WIN32
typedef std::basic_string<SQLWCHAR> ConnectionStringType;
#else // WT_WIN32
typedef std::u16string ConnectionStringType;
#endif // WT_WIN32

}

struct MSSQLServer::Impl {
  Impl(const ConnectionStringType &str)
    : env(NULL),
      dbc(NULL),
      stmt(NULL),
      connectionString(str)
  {
    resultBuffer.buf = (char*)malloc(256);
    resultBuffer.size = 256;
  }

  Impl(const Impl &other)
    : env(NULL),
      dbc(NULL),
      stmt(NULL),
      connectionString(other.connectionString)
  {
    resultBuffer.buf = (char*)malloc(256);
    resultBuffer.size = 256;
  }

  ~Impl()
  {
    free(resultBuffer.buf);
    if (stmt)
      SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    if (dbc) {
      SQLDisconnect(dbc);
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (env)
      SQLFreeHandle(SQL_HANDLE_ENV, env);
  }

  bool connect()
  {
    lastError.reset();

    // Create SQL env handle
    SQLRETURN res = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    if (res == SQL_ERROR) {
      lastError = dbo_error{
        DboErrc::Connection,
        "Failed to allocate ODBC environment handle",
        "mssql",
        {},
        0,
        "MSSQLServer::Impl::connect"
      };
      return false;
    }
    // Set ODBC version to 3
    res = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    {
      dbo_error err;
      if (!handleErr(SQL_HANDLE_ENV, env, res, &err, "SQLSetEnvAttr")) {
        lastError = std::move(err);
        return false;
      }
    }
    // Create SQL connection handle
    res = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    {
      dbo_error err;
      if (!handleErr(SQL_HANDLE_ENV, env, res, &err, "SQLAllocHandle(DBC)")) {
        lastError = std::move(err);
        return false;
      }
    }
    // Turn off autocommit
    res = SQLSetConnectAttrW(dbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
    {
      dbo_error err;
      if (!handleErr(SQL_HANDLE_DBC, dbc, res, &err, "SQLSetConnectAttrW(AUTOCOMMIT)")) {
        lastError = std::move(err);
        return false;
      }
    }
    // Turn on MARS (Multiple Active Result Sets)
    res = SQLSetConnectAttrW(dbc, SQL_COPT_SS_MARS_ENABLED, (SQLPOINTER)SQL_MARS_ENABLED_YES, SQL_IS_UINTEGER);
    {
      dbo_error err;
      if (!handleErr(SQL_HANDLE_DBC, dbc, res, &err, "SQLSetConnectAttrW(MARS)")) {
        lastError = std::move(err);
        return false;
      }
    }
    // Connect
    res = SQLDriverConnectW(
      dbc,
      NULL,
      (SQLWCHAR*)&(connectionString)[0],
      connectionString.size(),
      NULL,
      0,
      NULL,
      SQL_DRIVER_NOPROMPT);
    {
      dbo_error err;
      if (!handleErr(SQL_HANDLE_DBC, dbc, res, &err, "SQLDriverConnectW")) {
        lastError = std::move(err);
        return false;
      }
    }

    return true;
  }

  SQLHENV env; // Environment handle
  SQLHDBC dbc; // Connection handle
  SQLHSTMT stmt; // Statement handle for executeSql
  ConnectionStringType connectionString;
  struct ResultBuffer {
    char *buf;
    std::size_t size;
  } resultBuffer;
  std::optional<dbo_error> lastError;
};

class MSSQLServerStatement final : public SqlStatement {
public:
  MSSQLServerStatement(MSSQLServer &conn, const std::string &sql)
    : paramValues_(NULL),
      parameterCount_(0),
      resultColCount_(0),
      stmt_(NULL),
      conn_(conn),
      sql_(sql),
      affectedRows_(0),
      lastId_(-1)
  {
    if (!conn.impl_ || !conn.impl_->dbc) {
      pendingError_ = dbo_error{
        DboErrc::Connection,
        "MSSQL statement created without active connection",
        "mssql",
        {},
        0,
        "MSSQLServerStatement::ctor"
      };
      return;
    }

    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, conn.impl_->dbc, &stmt_);
    if (!setError(SQL_HANDLE_DBC, conn.impl_->dbc, rc, "SQLAllocHandle(STMT)"))
      return;
#ifdef WT_WIN32
    if (sql.empty()) {
      // Empty query, should be an error, but we'll leave the reporting
      // of that error to ODBC
      SQLWCHAR wstr[] = L"";
      rc = SQLPrepareW(stmt_, wstr, 0);
    } else {
      int wstrlen = MultiByteToWideChar(CP_UTF8, 0, &sql[0], sql.size(), NULL, 0);
      assert(wstrlen != 0);
      SQLWCHAR *wstr = new SQLWCHAR[wstrlen + 1];
      wstrlen = MultiByteToWideChar(CP_UTF8, 0, &sql[0], sql.size(), wstr, wstrlen);
      assert(wstrlen != 0);
      wstr[wstrlen] = 0;
      rc = SQLPrepareW(stmt_, wstr, wstrlen);
      delete[] wstr;
    }
#else // WT_WIN32
    std::u16string wstr = toUTF16(sql);
    rc = SQLPrepareW(stmt_, (SQLWCHAR*)&wstr[0], wstr.size());
#endif // WT_WIN32
    if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLPrepareW"))
      return;
    rc = SQLNumParams(stmt_, &parameterCount_);
    if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLNumParams"))
      return;
    SQLSMALLINT numCols = 0;
    rc = SQLNumResultCols(stmt_, &numCols);
    if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLNumResultCols"))
      return;
    resultColCount_ = numCols;
    if (parameterCount_ > 0) {
      paramValues_ = new Value[parameterCount_];
      std::memset(paramValues_, 0, parameterCount_ * sizeof(Value));
    }
  }

  virtual ~MSSQLServerStatement()
  {
    if (stmt_) {
      SQLFreeStmt(stmt_, SQL_CLOSE);
      SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
    }
    for (SQLSMALLINT i = 0; i < parameterCount_; ++i)
      paramValues_[i].clear();
    delete[] paramValues_;
  }


  void reset() override
  {
    pendingError_.reset();
    SQLFreeStmt(stmt_, SQL_CLOSE);
  }

  void bind(int column, const std::string &value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    bool newPtr = true;
    if (value.empty()) {
      newPtr = createOrResizeBuffer(v, 0);
      v.lengthOrInd = 0;
    } else {
#ifdef WT_WIN32
      // Convert value from UTF-8 to WCHAR (UTF-16)
      // Measure length required
      int bufsize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), value.size(), NULL, 0);
      assert(bufsize != 0);
      newPtr = createOrResizeBuffer(v, (bufsize + 1) * sizeof(WCHAR));
      bufsize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), value.size(), (WCHAR*)v.v.buf.p, bufsize);
      assert(bufsize != 0);
      ((WCHAR*)v.v.buf.p)[bufsize] = 0;
      v.lengthOrInd = bufsize * sizeof(WCHAR);
#else // WT_WIN32
      newPtr = createOrResizeBuffer(v, value.size() + 1);
      memcpy(v.v.buf.p, value.c_str(), value.size() + 1);
      v.lengthOrInd = value.size();
#endif // WT_WIN32
    }
    if (newPtr || v.type != SQL_C_WCHAR) {
      v.type = SQL_C_WCHAR;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
#ifdef WT_WIN32
        /*ValueType: */SQL_C_WCHAR,
#else // WT_WIN32
        /*ValueType: */SQL_C_CHAR, // UTF-8 to UTF-16 done by SQL Server driver
#endif // WT_WIN32
        /*ParameterType: */SQL_WVARCHAR,
        /*ColumnSize: */0, // Ignored
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */v.v.buf.p,
        /*BufferLength: */v.v.buf.size,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(int column, short value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type == SQL_C_SSHORT) {
      v.v.s = value;
      v.lengthOrInd = 0;
    } else {
      v.clear();
      v.v.s = value;
      v.type = SQL_C_SSHORT;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_SSHORT,
        /*ParameterType: */SQL_INTEGER,
        /*ColumnSize: */0, // ignored
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */&v.v.s,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(int column, int value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type == SQL_C_SLONG) {
      v.v.i = value;
      v.lengthOrInd = 0;
    } else {
      v.clear();
      v.v.i = value;
      v.type = SQL_C_SLONG;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_SLONG,
        /*ParameterType: */SQL_INTEGER,
        /*Columnsize: */0, // seems ignored?
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */&v.v.i,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(int column, long long value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type == SQL_C_SBIGINT) {
      v.v.ll = value;
      v.lengthOrInd = 0;
    } else {
      v.clear();
      v.v.ll = value;
      v.type = SQL_C_SBIGINT;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_SBIGINT,
        /*ParameterType: */SQL_BIGINT,
        /*Columnsize: */0, // ignored
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */&v.v.ll,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(int column, float value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type == SQL_C_FLOAT) {
      v.v.f = value;
      v.lengthOrInd = 0;
    } else {
      v.clear();
      v.v.f = value;
      v.type = SQL_C_FLOAT;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_FLOAT,
        /*ParameterType: */SQL_REAL,
        /*Columnsize: */0, // seems ignored?
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */&v.v.f,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(int column, double value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type == SQL_C_DOUBLE) {
      v.v.d = value;
      v.lengthOrInd = 0;
    } else {
      v.clear();
      v.v.d = value;
      v.type = SQL_C_DOUBLE;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_DOUBLE,
        /*ParameterType: */SQL_DOUBLE,
        /*Columnsize: */0, // seems ignored?
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */&v.v.d,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bind(
    int column,
    const std::chrono::system_clock::time_point& value,
    SqlDateTimeType type) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (type == SqlDateTimeType::Date) {
      if (v.type != SQL_C_TYPE_DATE)
        v.clear();
      v.lengthOrInd = 0;
      SQL_DATE_STRUCT &date = paramValues_[column].v.date;
      auto daypoint = date::floor<date::days>(value);
      auto ymd = date::year_month_day(daypoint);
      date.year = (int)ymd.year();
      date.month = (unsigned)ymd.month();
      date.day = (unsigned)ymd.day();
      if (v.type != SQL_C_TYPE_DATE) {
        v.type = SQL_C_TYPE_DATE;
        SQLRETURN rc = SQLBindParameter(
          /*StatementHandle: */stmt_,
          /*ParameterNumber: */column + 1,
          /*InputOutputType: */SQL_PARAM_INPUT,
          /*ValueType: */SQL_C_TYPE_DATE,
          /*ParameterType: */SQL_TYPE_DATE,
          /*ColumnSize: */0,
          /*DecimalDigits: */0,
          /*ParameterValuePtr: */&date,
          /*BufferLength: */0,
          /*StrLen_or_IndPtr: */&v.lengthOrInd
        );
        if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
      }
    } else {
      if (v.type != SQL_C_TYPE_TIMESTAMP)
        v.clear();
      v.lengthOrInd = 0;
      SQL_TIMESTAMP_STRUCT &ts = paramValues_[column].v.timestamp;
      auto daypoint = date::floor<date::days>(value);
      auto ymd = date::year_month_day(daypoint);
      auto tod = date::make_time(value - daypoint);
      ts.year = (int)ymd.year();
      ts.month = (unsigned)ymd.month();
      ts.day = (unsigned)ymd.day();
      ts.hour = tod.hours().count();
      ts.minute = tod.minutes().count();
      ts.second = tod.seconds().count();
      ts.fraction = std::chrono::nanoseconds(std::chrono::duration_cast<
        std::chrono::duration<long long, std::ratio_multiply<std::ratio<100>, std::nano>>>(tod.subseconds())).count();
      if (v.type != SQL_C_TYPE_TIMESTAMP) {
        v.type = SQL_C_TYPE_TIMESTAMP;
        SQLRETURN rc = SQLBindParameter(
          /*StatementHandle: */stmt_,
          /*ParameterNumber: */column + 1,
          /*InputOutputType: */SQL_PARAM_INPUT,
          /*ValueType: */SQL_C_TYPE_TIMESTAMP,
          /*ParameterType: */SQL_TYPE_TIMESTAMP,
          /*ColumnSize: */0,
          /*DecimalDigits: */7, // SQL Server limit: max 7 decimal digits
          /*ParameterValuePtr: */&ts,
          /*BufferLength: */0,
          /*StrLen_or_IndPtr: */&v.lengthOrInd
        );
        if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
      }
    }
  }

  void bind(
    int column,
    const std::chrono::duration<int, std::milli>& value) override
  {
    long long msec = value.count();
    bind(column, msec);
  }

  void bind(
    int column,
    const std::vector<unsigned char>& value) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    bool newPtr = createOrResizeBuffer(v, value.size());
    v.lengthOrInd = value.size();
    memcpy(v.v.buf.p, value.data(), value.size());
    if (newPtr || v.type != SQL_C_BINARY) {
      v.type = SQL_C_BINARY;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_BINARY,
        /*ParameterType: */SQL_VARBINARY,
        /*ColumnSize: */0, // ignored
        /*DecimalDigits: */0, // ignored
        /*ParameterValuePtr: */v.v.buf.p,
        /*BufferLength: */v.v.buf.size,
        /*StrLen_or_IndPtr: */&v.lengthOrInd
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  void bindNull(int column) override
  {
    if (!checkColumnIndex(column)) return;
    Value &v = paramValues_[column];
    if (v.type != 0)
      v.lengthOrInd = SQL_NULL_DATA;
    else {
      v.clear();
      v.type = SQL_C_CHAR;
      v.v.buf.p = 0;
      v.v.buf.size = SQL_NULL_DATA;
      SQLRETURN rc = SQLBindParameter(
        /*StatementHandle: */stmt_,
        /*ParameterNumber: */column + 1,
        /*InputOutputType: */SQL_PARAM_INPUT,
        /*ValueType: */SQL_C_CHAR,
        /*ParameterType: */SQL_VARCHAR,
        /*Columnsize: */0,
        /*DecimalDigits: */0,
        /*ParameterValuePtr: */NULL,
        /*BufferLength: */0,
        /*StrLen_or_IndPtr: */&v.v.buf.size
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLBindParameter")) return;
    }
  }

  awaitable<dbo_result<void>> execute() override
  {
    if (pendingError_) {
      co_return std::unexpected(*pendingError_);
    }
    if (!stmt_) {
      co_return std::unexpected(dbo_error{
        DboErrc::Sql,
        "MSSQL statement is not initialized",
        "mssql",
        {},
        0,
        "MSSQLServerStatement::execute"
      });
    }

    if (conn_.showQueries()) {
      LOG_INFO("{}", sql_);
      fmtlog::poll();
    }

    SQLRETURN rc = SQLExecute(stmt_);
    if (rc != SQL_NO_DATA && !setError(SQL_HANDLE_STMT, stmt_, rc, "SQLExecute")) {
      co_return std::unexpected(*pendingError_);
    }

    rc = SQLRowCount(stmt_, &affectedRows_);
    if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLRowCount")) {
      co_return std::unexpected(*pendingError_);
    }

    const std::string_view returning = " OUTPUT Inserted.";
    if (sql_.find(returning) != std::string::npos) {
      bool nr = nextRow();
      if (nr) {
        getResult(0, &lastId_);
        affectedRows_ = 1;
      }
    }

    co_return dbo_result<void>{};
  }

  long long insertedId() override
  {
    return lastId_;
  }

  int affectedRowCount() override
  {
    return static_cast<int>(affectedRows_);
  }

  bool nextRow() override
  {
    if (pendingError_)
      return false;

    SQLRETURN rc = SQLFetch(stmt_);
    if (rc == SQL_NO_DATA)
      return false;

    return setError(SQL_HANDLE_STMT, stmt_, rc, "SQLFetch");
  }

  int columnCount() const override
  {
    return resultColCount_;
  }

  bool getResult(int column, std::string *value, int /*size*/) override
  {
    MSSQLServer::Impl::ResultBuffer &resultBuffer = conn_.impl_->resultBuffer;
    std::size_t resultBufferPos = 0;
    SQLLEN strLen_or_ind = SQL_NO_TOTAL;
    while (strLen_or_ind == SQL_NO_TOTAL) {
      SQLRETURN rc = SQLGetData(
        /*StatementHandle: */stmt_,
        /*ColumnNumber: */column + 1,
#ifdef WT_WIN32
        /*TargetType: */SQL_C_WCHAR,
#else // WT_WIN32
        /*TargetType: */SQL_C_CHAR, // conversion from UTF-16 to UTF-8 done by driver
#endif // WT_WIN32
        /*TargetValue: */&resultBuffer.buf[resultBufferPos],
        /*BufferLength: */resultBuffer.size - resultBufferPos,
        /*StrLen_or_IndPtr: */&strLen_or_ind
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLGetData(string)"))
        return false;
#ifdef WT_WIN32
      const int charSize = 2;
#else // WT_WIN32
      const int charSize = 1;
#endif // WT_WIN32
      if (strLen_or_ind == SQL_NULL_DATA) {
        return false; // NULL
      } else if (strLen_or_ind == SQL_NO_TOTAL || strLen_or_ind + charSize > resultBuffer.size - resultBufferPos) {
        std::size_t pos = resultBuffer.size - charSize;
        resultBuffer.size *= 2;
        while (strLen_or_ind != SQL_NO_TOTAL &&
               strLen_or_ind + charSize > resultBuffer.size - resultBufferPos)
          resultBuffer.size *= 2;
        resultBufferPos = pos;
        resultBuffer.buf = (char*)realloc(resultBuffer.buf, resultBuffer.size);
        strLen_or_ind = SQL_NO_TOTAL;
      }
    }
    if (resultBufferPos == 0 && strLen_or_ind == 0) {
      value->clear();
      return true; // empty string
    }
    std::size_t totalDataSize = resultBufferPos + strLen_or_ind;
#ifdef WT_WIN32
    int strlen = WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)resultBuffer.buf, totalDataSize / sizeof(WCHAR), NULL, 0, NULL, NULL);
    assert(strlen != 0);
    value->clear();
    value->resize(strlen);
    strlen = WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)resultBuffer.buf, totalDataSize / sizeof(WCHAR), &(*value)[0], strlen, NULL, NULL);
    assert(strlen != 0);
#else // WT_WIN32
    *value = std::string(resultBuffer.buf, totalDataSize);
#endif // WT_WIN32
    return true;
  }

  bool getResult(int column, short * value) override
  {
    return getRes<SQL_C_SSHORT>(column, value);
  }

  bool getResult(int column, int * value) override
  {
    return getRes<SQL_C_SLONG>(column, value);
  }

  bool getResult(int column, long long * value) override
  {
    return getRes<SQL_C_SBIGINT>(column, value);
  }

  bool getResult(int column, float * value) override
  {
    return getRes<SQL_C_FLOAT>(column, value);
  }

  bool getResult(int column, double * value) override
  {
    return getRes<SQL_C_DOUBLE>(column, value);
  }

  bool getResult(
    int column,
    std::chrono::system_clock::time_point *value,
    SqlDateTimeType type) override
  {
    if (type == SqlDateTimeType::Date) {
      SQL_DATE_STRUCT date;
      bool result = getRes<SQL_C_TYPE_DATE>(column, &date);
      if (!result)
        return false; // NULL
      *value = date::sys_days{ date::year{ date.year } / date.month / date.day };
      return true;
    } else {
      SQL_TIMESTAMP_STRUCT ts;
      bool result = getRes<SQL_C_TYPE_TIMESTAMP>(column, &ts);
      if (!result)
        return false; // NULL
      *value = 
        date::sys_days{ date::year{ ts.year } / ts.month / ts.day } +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::hours{ ts.hour } +
          std::chrono::minutes{ ts.minute } +
          std::chrono::seconds{ ts.second } +
          std::chrono::nanoseconds{ ts.fraction }
        );
      return true;
    }
    assert(false);
    return false;
  }

  bool getResult(
    int column,
    std::chrono::duration<int, std::milli> *value) override
  {
    long long msec;
    bool res = getResult(column, &msec);
    if (!res)
      return res;

    *value = std::chrono::duration<int, std::milli>(msec);
    return true;
  }

  bool getResult(
    int column,
    std::vector<unsigned char> *value,
    int size) override
  {
    MSSQLServer::Impl::ResultBuffer &resultBuffer = conn_.impl_->resultBuffer;
    std::size_t resultBufferPos = 0;
    SQLLEN strLen_or_ind = SQL_NO_TOTAL;
    while (strLen_or_ind == SQL_NO_TOTAL) {
      SQLRETURN rc = SQLGetData(
        /*StatementHandle: */stmt_,
        /*ColumnNumber: */column + 1,
        /*TargetType: */SQL_C_BINARY,
        /*TargetValue: */&resultBuffer.buf[resultBufferPos],
        /*BufferLength: */resultBuffer.size - resultBufferPos,
        /*StrLen_or_IndPtr: */&strLen_or_ind
      );
      if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLGetData(binary)"))
        return false;
      if (strLen_or_ind == SQL_NULL_DATA) {
        return false; // NULL
      } else if (strLen_or_ind == SQL_NO_TOTAL || strLen_or_ind > resultBuffer.size - resultBufferPos) {
        std::size_t pos = resultBuffer.size;
        resultBuffer.size *= 2;
        while (strLen_or_ind != SQL_NO_TOTAL &&
               strLen_or_ind > resultBuffer.size - resultBufferPos)
          resultBuffer.size *= 2;
        resultBufferPos = pos;
        resultBuffer.buf = (char*)realloc(resultBuffer.buf, resultBuffer.size);
        strLen_or_ind = SQL_NO_TOTAL;
      }
    }
    std::size_t totalDataSize = resultBufferPos + strLen_or_ind;
    *value = std::vector<unsigned char>(resultBuffer.buf, resultBuffer.buf + totalDataSize);
    return true;
  }

  std::string sql() const override
  {
    return sql_;
  }

private:
  struct Value {
    union {
      struct {
        void *p; // buffer pointer 
        SQLLEN size; // buffer size
      } buf;
      short s;
      int i;
      long long ll;
      float f;
      double d;
      SQL_DATE_STRUCT date;
      SQL_TIME_STRUCT time;
      SQL_TIMESTAMP_STRUCT timestamp;
    } v;
    SQLLEN lengthOrInd; // length for binary data or string or NULL indicator
    SQLSMALLINT type; // SQL_C... type

    Value()
    {
      std::memset(&v, 0, sizeof(v));
      lengthOrInd = 0;
      type = 0;
    }

    ~Value()
    {
      if (type == SQL_C_BINARY ||
          type == SQL_C_WCHAR)
        free(v.buf.p);
    }

#ifdef WT_CXX11
    Value(const Value &other) = delete;
    Value &operator=(const Value &other) = delete;
    Value(Value &&other) = delete;
    Value &operator=(Value &&other) = delete;
#endif // WT_CXX11

    void clear()
    {
      if (type == SQL_C_BINARY ||
          type == SQL_C_WCHAR)
        free(v.buf.p);

      std::memset(&v, 0, sizeof(v));
      lengthOrInd = 0;
      type = 0;
    }

  private:
#ifndef WT_CXX11
    Value(const Value &other);
    Value& operator=(const Value &other);
#endif // WT_CXX11
  };
  Value *paramValues_;
  SQLSMALLINT parameterCount_;
  SQLSMALLINT resultColCount_;

  SQLHSTMT stmt_;
  MSSQLServer &conn_;
  std::string sql_;
  SQLLEN affectedRows_;
  long long lastId_;
  std::optional<dbo_error> pendingError_;

  bool checkColumnIndex(int column)
  {
    if (column >= parameterCount_) {
      pendingError_ = dbo_error{
        DboErrc::Sql,
        std::string("Trying to bind too many parameters (parameter count = ")
          + std::to_string(parameterCount_)
          + ", column = "
          + std::to_string(column)
          + ")",
        "mssql",
        {},
        0,
        "MSSQLServerStatement::checkColumnIndex"
      };
      return false;
    }
    return true;
  }

  // bool returns whether buffer changed
  bool createOrResizeBuffer(Value &v, SQLLEN size)
  {
    if (v.type == SQL_C_WCHAR || v.type == SQL_C_BINARY) {
      // We already have a buffer
      if (v.v.buf.size >= size)
        return false;
      v.v.buf.size = size;
      v.v.buf.p = realloc(v.v.buf.p, v.v.buf.size);
      return true;
    }
    else {
      // New buffer
      v.clear();
      v.v.buf.size = size == 0 ? 1 : size;
      v.v.buf.p = malloc(v.v.buf.size);
      return true;
    }
  }

  template<SQLSMALLINT TargetType, typename ReturnType>
  bool getRes(int column, ReturnType * value)
  {
    SQLLEN strLen_or_ind = 0;
    SQLRETURN rc = SQLGetData(
      /*StatementHandle: */stmt_,
      /*ColumnNumber: */column + 1,
      /*TargetType: */TargetType,
      /*TargetValue: */value,
      /*BufferLength: */0,
      /*StrLen_or_IndPtr*/&strLen_or_ind
    );
    if (!setError(SQL_HANDLE_STMT, stmt_, rc, "SQLGetData(scalar)"))
      return false;
    return strLen_or_ind != SQL_NULL_DATA;
  }

  bool setError(SQLSMALLINT handleType,
                SQLHANDLE handle,
                SQLRETURN rc,
                std::string_view context)
  {
    dbo_error err;
    if (handleErr(handleType, handle, rc, &err, context))
      return true;
    pendingError_ = std::move(err);
    return false;
  }
};

//MSSQLServer::MSSQLServer()
//  : impl_(0)
//{ }

MSSQLServer::MSSQLServer(asio::io_context& ctx, const std::string &connectionString)
  : impl_(0)
{
  if (!connect(connectionString)) {
    LOG_ERROR("MSSQLServer: constructor connect failed");
    fmtlog::poll();
  }
}

MSSQLServer::MSSQLServer(const MSSQLServer &other)
  : SqlConnectionBase(other),
    impl_(other.impl_ ? new Impl(*other.impl_) : 0)
{
  //connection_ = new nanodbc::connection(NANODBC_TEXT("other.connectionString_"));

  if (impl_ && !impl_->connect()) {
    if (impl_->lastError) {
      LOG_ERROR("MSSQLServer copy connect failed: {}", impl_->lastError->message);
      fmtlog::poll();
    }
    delete impl_;
    impl_ = nullptr;
  }
}

MSSQLServer &MSSQLServer::operator=(const MSSQLServer &other)
{
    //connection_ = other.connection_;
    impl_ = other.impl_;
    properties_ = std::move(other.properties_);
    //statementCache_ = std::move(other.statementCache_);
    statefulSql_ = std::move(other.statefulSql_);
    return *this;
}

MSSQLServer::~MSSQLServer()
{
  clearStatementCache();
  delete impl_;
}

std::unique_ptr<MSSQLServer> MSSQLServer::clone() const
{
  return std::unique_ptr<MSSQLServer>(new MSSQLServer(*this));
}

MSSQLServer MSSQLServer::clone(asio::io_context &ctx) const
{
  return MSSQLServer(*this);
}

bool MSSQLServer::connect(const std::string &connectionString)
{
  if (impl_) {
    LOG_ERROR("MSSQLServer::connect called while already connected");
    fmtlog::poll();
    return false;
  }

  //connection_ = new nanodbc::connection(NANODBC_TEXT(connectionString));

#ifdef WT_WIN32
  ConnectionStringType connStr;
  if (!connectionString.empty()) {
    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, &connectionString[0], connectionString.size(), 0, 0);
    assert(wstrlen != 0);
    connStr.resize(wstrlen);
    wstrlen = MultiByteToWideChar(CP_UTF8, 0, &connectionString[0], connectionString.size(), &connStr[0], connStr.size());
    assert(wstrlen != 0);
  }
#else // WT_WIN32
  ConnectionStringType connStr = toUTF16(connectionString);
#endif // WT_WIN32

  Impl *impl = new Impl(connStr);
  if (!impl->connect()) {
    if (impl->lastError) {
      LOG_ERROR("MSSQLServer::connect failed: {}", impl->lastError->message);
      fmtlog::poll();
    }
    delete impl;
    return false;
  }

  impl_ = impl;
  return true;
}

// This is an asynchronous operation that wraps the C-based API.



awaitable<dbo_result<void>> MSSQLServer::executeSql(const std::string &sql)
{
  co_await async_mutex_.async_scoped_lock(use_nothrow_awaitable);

  if (!impl_) {
    co_return std::unexpected(dbo_error{
      DboErrc::Connection,
      "MSSQLServer is not connected",
      "mssql",
      {},
      0,
      "MSSQLServer::executeSql"});
  }

  if (showQueries()) {
    LOG_INFO("{}", sql);
    fmtlog::poll();
  }

  SQLRETURN rc = SQL_SUCCESS;
  if (!impl_->stmt) {
    rc = SQLAllocHandle(SQL_HANDLE_STMT, impl_->dbc, &impl_->stmt);
    dbo_error err;
    if (!handleErr(SQL_HANDLE_DBC, impl_->dbc, rc, &err, "SQLAllocHandle(STMT)")) {
      co_return std::unexpected(std::move(err));
    }
  }
#ifdef WT_WIN32
  if (sql.empty()) {
    SQLWCHAR wstr[] = L"";
    rc = SQLExecDirectW(impl_->stmt, wstr, 0);
  } else {
    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, &sql[0], sql.size(), 0, 0);
    assert(wstrlen != 0);
    SQLWCHAR *wstr = new SQLWCHAR[wstrlen + 1];
    wstrlen = MultiByteToWideChar(CP_UTF8, 0, &sql[0], sql.size(), wstr, wstrlen);
    assert(wstrlen != 0);
    wstr[wstrlen] = 0;
    rc = SQLExecDirectW(impl_->stmt, wstr, wstrlen);
    delete[] wstr;
  }
#else // WT_WIN32
  std::u16string wstr = toUTF16(sql);
  rc = SQLExecDirectW(impl_->stmt, (SQLWCHAR*)&wstr[0], wstr.size());
#endif // WT_WIN32
  {
    dbo_error err;
    if (!handleErr(SQL_HANDLE_STMT, impl_->stmt, rc, &err, "SQLExecDirectW")) {
      SQLFreeStmt(impl_->stmt, SQL_CLOSE);
      co_return std::unexpected(std::move(err));
    }
  }
  SQLFreeStmt(impl_->stmt, SQL_CLOSE);
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> MSSQLServer::startTransaction()
{
  if (showQueries()) {
    LOG_INFO("begin transaction -- implicit");
    fmtlog::poll();
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> MSSQLServer::commitTransaction()
{
  if (!impl_) {
    co_return std::unexpected(dbo_error{
      DboErrc::Connection,
      "MSSQLServer is not connected",
      "mssql",
      {},
      0,
      "MSSQLServer::commitTransaction"});
  }

  if (showQueries()) {
    LOG_INFO("commit transaction -- using SQLEndTran");
    fmtlog::poll();
  }

  SQLRETURN rc = SQLEndTran(SQL_HANDLE_DBC, impl_->dbc, SQL_COMMIT);
  dbo_error err;
  if (!handleErr(SQL_HANDLE_DBC, impl_->dbc, rc, &err, "SQLEndTran(COMMIT)")) {
    err.code = DboErrc::Transaction;
    err.context = "MSSQLServer::commitTransaction";
    co_return std::unexpected(std::move(err));
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> MSSQLServer::rollbackTransaction()
{
  if (!impl_) {
    co_return std::unexpected(dbo_error{
      DboErrc::Connection,
      "MSSQLServer is not connected",
      "mssql",
      {},
      0,
      "MSSQLServer::rollbackTransaction"});
  }

  if (showQueries()) {
    LOG_INFO("rollback transaction -- using SQLEndTran");
    fmtlog::poll();
  }

  SQLRETURN rc = SQLEndTran(SQL_HANDLE_DBC, impl_->dbc, SQL_ROLLBACK);
  dbo_error err;
  if (!handleErr(SQL_HANDLE_DBC, impl_->dbc, rc, &err, "SQLEndTran(ROLLBACK)")) {
    err.code = DboErrc::Transaction;
    err.context = "MSSQLServer::rollbackTransaction";
    co_return std::unexpected(std::move(err));
  }
  co_return dbo_result<void>{};
}

std::unique_ptr<SqlStatement> MSSQLServer::prepareStatement(const std::string &sql)
{
  if (!impl_)
    return nullptr;
  return std::unique_ptr<SqlStatement>(new MSSQLServerStatement(*this, sql));
}

SqlStatement *MSSQLServer::getStatement(const std::string &id)
{
  StatementMap::const_iterator start;
  StatementMap::const_iterator end;
  std::tie(start, end) = statementCache_.equal_range(id);
  SqlStatement *result = nullptr;
  for (auto i = start; i != end; ++i) {
    result = i->second.get();
    if (result->use())
      return result;
  }
  if (result) {
    auto count = statementCache_.count(id);
    if (count >= WARN_NUM_STATEMENTS_THRESHOLD) {
      LOG_WARN("Warning: number of instances ({}) of prepared statement '{}' for this connection exceeds threshold ({}). This could indicate a programming error.", (count + 1), id, WARN_NUM_STATEMENTS_THRESHOLD);
      fmtlog::poll();
    }
    auto stmt = prepareStatement(result->sql());
    result = stmt.get();
    saveStatement(id, std::move(stmt));
  }
  return nullptr;
}

}
}
}
