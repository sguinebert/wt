#pragma once

#include "basic_connection.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <Wt/Dbo/SqlConnectionBase.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboPostgresDllDefs.h>
#include <Wt/cuehttp/detail/engines.hpp>

namespace pg = postgrespp;
//class postgresStatement;

namespace postgrespp {

//using connection = basic_connection;


class connection : public basic_connection, public Wt::Dbo::SqlConnectionBase
{
    friend class SqlConnection;
public:
    template <class ExecutorT>
    connection(ExecutorT& exc, const char* pgconninfo) : basic_connection(exc, pgconninfo), connInfo_(pgconninfo) {}

//    ~connection()
//    {
//        assert(statementCache_.empty());
//    }

    WTDBOPOSTGRES_API std::unique_ptr<Wt::Dbo::SqlStatement> prepareStatement(const std::string& sql);

    void clearStatementCache()
    {
        statementCache_.clear();
    }

    awaitable<void> executeSql(const std::string& sql)
    {
        std::unique_ptr<Wt::Dbo::SqlStatement> s = prepareStatement(sql);
        co_await s->execute();
        co_return;
    }

    awaitable<void> executeSqlStateful(const std::string& sql)
    {
        statefulSql_.push_back(sql);
        co_await executeSql(sql);
    }

    Wt::Dbo::SqlStatement *getStatement(const std::string& id)
    {
        StatementMap::const_iterator start;
        StatementMap::const_iterator end;
        std::tie(start, end) = statementCache_.equal_range(id);
        Wt::Dbo::SqlStatement *result = nullptr;
        for (auto i = start; i != end; ++i) {
            result = i->second.get();
            if (result->use())
                return result;
        }
        if (result) {
            auto count = statementCache_.count(id);
            if (count >= WARN_NUM_STATEMENTS_THRESHOLD) {
                //LOG_WARN("Warning: number of instances ({}) of prepared statement '{}' for this connection exceeds threshold ({}). This could indicate a programming error.", (count + 1), id, WARN_NUM_STATEMENTS_THRESHOLD);
                //fmtlog::poll();
            }
            auto stmt = prepareStatement(result->sql());
            result = stmt.get();
            saveStatement(id, std::move(stmt));
        }
        return nullptr;
    }


    /*
     * margin: a grace period beyond the lifetime
     */
    void checkConnection(std::chrono::seconds margin)
    {
        if (maximumLifetime_ > std::chrono::seconds{0} && connectTime_ != std::chrono::steady_clock::time_point{})
        {
            auto t = std::chrono::steady_clock::now();
            if (t - connectTime_ > maximumLifetime_ + margin)
            {
                //LOG_INFO("maximum connection lifetime passed, trying to reconnect...");
                if (!reconnect())
                {
                    throw std::runtime_error("Could not reconnect to server...");
                }
            }
        }
    }

    void disconnect()
    {
        auto conn = underlying_handle();
        if (conn)
            PQfinish(conn);

        conn = 0;

//        std::vector<SqlStatement *> statements = getStatements();

//        /* Evict also the statements -- the statements themselves can stay,
//          only running statements behavior is affected (but we are dealing with
//          that while calling disconnect) */
//        for (std::size_t i = 0; i < statements.size(); ++i)
//        {
//            SqlStatement *s = statements[i];
//            PostgresStatement *ps = dynamic_cast<PostgresStatement *>(s);
//            ps->rebuild();
//        }
    }

    bool reconnect()
    {
        ////LOG_INFO("{} reconnecting...", this);

//        if (conn_)
//        {
//            if (PQstatus(conn_) == CONNECTION_OK)
//            {
//                PQfinish(conn_);
//            }

//            conn_ = 0;
//        }

//        clearStatementCache();

//        if (!connInfo_.empty())
//        {
//            bool result = connect(connInfo_);

//            if (result)
//            {
//                const std::vector<std::string> &statefulSql = getStatefulSql();
//                for (unsigned i = 0; i < statefulSql.size(); ++i)
//                    executeSql(statefulSql[i]);
//            }

//            return result;
//        }
//        else
            return false;
    }
    /*! \brief Sets a timeout.
   *
   * Sets a timeout for queries. When the query exceeds this timeout, the connection
   * is closed using disconnect() and an exception is thrown.
   *
   * In practice, as a result, the connection (and statements) can still be used again
   * when a successful reconnect() is performed.
   *
   * A value of 0 disables the timeout handling, allowing operations
   * to take as much time is they require.
   *
   * The default value is 0.
   */
    void setTimeout(std::chrono::microseconds timeout);

    /*! \brief Returns the timeout.
   *
   * \sa setTimeout()
   */
    std::chrono::microseconds timeout() const { return timeout_; }

//    void saveStatement(const std::string& id,
//                       std::unique_ptr<Wt::Dbo::SqlStatement> statement)
//    {
//        statementCache_.emplace(id, std::move(statement));
//    }

//    std::string property(const std::string& name) const
//    {
//        std::map<std::string, std::string>::const_iterator i = properties_.find(name);

//        if (i != properties_.end())
//            return i->second;
//        else
//            return std::string();
//    }

//    void setProperty(const std::string& name,
//                                    const std::string& value)
//    {
//        properties_[name] = value;
//    }

    bool usesRowsFromTo() const
    {
        return false;
    }

    Wt::Dbo::LimitQuery limitQueryMethod() const
    {
        return Wt::Dbo::LimitQuery::Limit;
    }

    bool supportAlterTable() const
    {
        return false;
    }

    bool supportDeferrableFKConstraint() const
    {
        return false;
    }

    const char *alterTableConstraintString() const
    {
        return "constraint";
    }

    bool showQueries() const
    {
        return property("show-queries") == "true";
    }

    std::string textType(int size) const
    {
        if (size == -1)
            return "text";
        else{
            return "varchar(" + std::to_string(size) + ")";
        }
    }

    std::string longLongType() const
    {
        return "bigint";
    }

    const char *booleanType() const
    {
        return "boolean";
    }

    bool supportUpdateCascade() const
    {
        return true;
    }

    bool requireSubqueryAlias() const
    {
        return true;
    }

    std::string autoincrementInsertInfix(const std::string &) const
    {
        return "";
    }

    void prepareForDropTables()
    { }

//    std::vector<Wt::Dbo::SqlStatement *> getStatements() const
//    {
//        std::vector<Wt::Dbo::SqlStatement *> result;

//        for (StatementMap::const_iterator i = statementCache_.begin();
//             i != statementCache_.end(); ++i)
//            result.push_back(i->second.get());

//        return result;
//    }
    //strict cloning with the same executor
    std::unique_ptr<connection> clone()  {
        //auto& engine = cue::http::detail::engines::default_engines();
        return std::make_unique<connection>(this->socket().get_executor(), connInfo_.data());
    }
    connection clone(Wt::http::detail::engines& engine)  {
        return connection(engine.get(), connInfo_.data());
    }
    using txn_t = basic_transaction<void, void>;
    awaitable<void> startTransaction() {
        co_await async_transaction(use_awaitable);
        co_return;
    }
    awaitable<void> commitTransaction() {
        //exec("commit transaction", false);
        //co_await tx_.commit(use_awaitable);
        co_return;
    }

    awaitable<void> rollbackTransaction() {
        //exec("rollback transaction", false);
        //co_await tx_.rollback(use_awaitable);
        co_return;
    }

    /** @name Methods that return dialect information
   */
    //!@{
    std::string autoincrementSql() const {
        return std::string();
    }
    std::vector<std::string>
    autoincrementCreateSequenceSql(const std::string &table,
                                   const std::string &id) const {
        return std::vector<std::string>();
    }
    std::vector<std::string>
    autoincrementDropSequenceSql(const std::string &table,
                                 const std::string &id) const {
        return std::vector<std::string>();
    }
    std::string autoincrementType() const {
        return "bigserial";
    }
    std::string autoincrementInsertSuffix(const std::string& id) const {
        return " returning \"" + id + "\"";
    }
    const char *dateTimeType(Wt::Dbo::SqlDateTimeType type) const {
        switch (type)
        {
        case Wt::Dbo::SqlDateTimeType::Date:
            return "date";
        case Wt::Dbo::SqlDateTimeType::DateTime:
            return "timestamp";
        case Wt::Dbo::SqlDateTimeType::Time:
            return "interval";
        }

        std::stringstream ss;
        ss << __FILE__ << ":" << __LINE__ << ": implementation error";
        //throw PostgresException(ss.str());
    }
    const char *blobType() const {
        return "bytea not null";
    }

    void setCancelSignal(asio::cancellation_signal* cancel) { cancel_wait_ = cancel;  }

//protected:
//    const std::vector<std::string>& getStatefulSql() const { return statefulSql_; }

private:
//    typedef std::multimap<std::string, std::unique_ptr<Wt::Dbo::SqlStatement>> StatementMap;

    std::string connInfo_;
//    StatementMap statementCache_;
//    std::map<std::string, std::string> properties_;
//    std::vector<std::string> statefulSql_;

    std::chrono::microseconds timeout_;
    std::chrono::seconds maximumLifetime_;
    std::chrono::steady_clock::time_point connectTime_;

    asio::cancellation_signal* cancel_wait_;
};


}

