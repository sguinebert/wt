/*
 * Copyright (C) 2023 Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_SQL_CONNECTION_H_
#define WT_DBO_SQL_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>
#include <concepts>
#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/Dbo/core/Error.h>
#include <Wt/Dbo/reflect/Dialect.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <Wt/Dbo/sql/Statement.h>

namespace Wt {
namespace Dbo {

enum class DialectKind;

template<typename T>
concept SqlBackend = requires(T t, T const ct,
                              asio::io_context& ctx,
                              const std::string& str,
                              int i,
                              SqlDateTimeType dtType)
{
    // clone (Non-const)
    { t.clone(ctx) } -> std::convertible_to<T>; //
    
    // SQL exec (Async)
    { t.executeSql(str) } -> std::same_as<awaitable<dbo_result<void>>>;
    { t.executeSqlStateful(str) } -> std::same_as<awaitable<dbo_result<void>>>;
    
    // Transactions (Async)
    { t.startTransaction() } -> std::same_as<awaitable<dbo_result<void>>>;
    { t.commitTransaction() } -> std::same_as<awaitable<dbo_result<void>>>;
    { t.rollbackTransaction() } -> std::same_as<awaitable<dbo_result<void>>>;
    
    // Statements
    { t.prepareStatement(str) } -> std::convertible_to<std::unique_ptr<SqlStatement>>;
    { t.getStatement(str) } -> std::convertible_to<SqlStatement*>;
    { t.saveStatement(str, std::unique_ptr<SqlStatement>()) };
    { t.clearStatementCache() };
    { t.getStatements() } -> std::convertible_to<std::vector<SqlStatement*>>;
    
    // Properties
    { t.setProperty(str, str) };
    { ct.property(str) } -> std::convertible_to<std::string>;
    { ct.showQueries() } -> std::convertible_to<bool>;
    
    // SQL dialect (Const)
    { ct.autoincrementSql() } -> std::convertible_to<std::string>;
    { ct.autoincrementCreateSequenceSql(str, str) } -> std::convertible_to<std::vector<std::string>>;
    { ct.autoincrementDropSequenceSql(str, str) } -> std::convertible_to<std::vector<std::string>>;
    { ct.autoincrementType() } -> std::convertible_to<std::string>;
    { ct.autoincrementInsertInfix(str) } -> std::convertible_to<std::string>;
    { ct.autoincrementInsertSuffix(str) } -> std::convertible_to<std::string>;
    { ct.textType(i) } -> std::convertible_to<std::string>;
    { ct.longLongType() } -> std::convertible_to<std::string>;
    { ct.booleanType() } -> std::convertible_to<const char*>;
    { ct.dateTimeType(dtType) } -> std::convertible_to<const char*>;
    { ct.blobType() } -> std::convertible_to<const char*>;
    
    // Capacités
    { ct.supportUpdateCascade() } -> std::convertible_to<bool>;
    { ct.requireSubqueryAlias() } -> std::convertible_to<bool>;
    { ct.limitQueryMethod() } -> std::convertible_to<LimitQuery>;
    { ct.usesRowsFromTo() } -> std::convertible_to<bool>;
    { ct.supportAlterTable() } -> std::convertible_to<bool>;
    { ct.supportDeferrableFKConstraint() } -> std::convertible_to<bool>;
    { ct.alterTableConstraintString() } -> std::convertible_to<const char*>;
    { ct.prepareForDropTables() };
    { ct.getStatefulSql() } -> std::convertible_to<const std::vector<std::string>&>;

    // Compile-time dialect dispatch
    { ct.dialectKind() } -> std::convertible_to<DialectKind>;
};

class WTDBO_API SqlConnection
{
public:
    // --- Constructeurs ---
    SqlConnection() = default;

    // Accept concept SqlBackend.
    template<SqlBackend BackendT>
    explicit SqlConnection(BackendT backend)
        : pimpl_(std::make_unique<Model<BackendT>>(std::move(backend)))
        // Note: On ne peut pas récupérer l'executor du backend de manière générique 
        // sans l'ajouter au concept. On initialise avec un executor par défaut ou null.
    {
    }

    template<SqlBackend BackendT>
    explicit SqlConnection(asio::io_context& context, BackendT backend)
        : pimpl_(std::make_unique<Model<BackendT>>(std::move(backend))),
          executor_(context.get_executor())
    {
    }

    // Move semantics (unique_ptr is move-only)
    SqlConnection(SqlConnection&&) noexcept = default;
    SqlConnection& operator=(SqlConnection&&) noexcept = default;

    // No copy (unique_ptr)
    SqlConnection(const SqlConnection&) = delete;
    SqlConnection& operator=(const SqlConnection&) = delete;

    ~SqlConnection() = default;

    // --- Gestion ---
    asio::any_io_executor get_executor() const { return executor_; }

    void setCancelSignal(asio::cancellation_signal *cancel) {
        // experimental
    }

    dbo_result<std::unique_ptr<SqlConnection>> clone(asio::io_context& ctx) {
        if (!pimpl_) return std::unique_ptr<SqlConnection>{};
        try {
            auto newConn = pimpl_->clone(ctx);
            newConn->executor_ = ctx.get_executor();
            return newConn;
        } catch (const std::exception& e) {
            return std::unexpected(dbo_error{
                DboErrc::Connection,
                e.what(),
                {},
                {},
                0,
                "SqlConnection::clone(asio::io_context&)"});
        }
    }

    std::unique_ptr<SqlConnection> clone() const {
        return {}; 
    }

    bool inTransaction(bool set) {
        if (set)
            inTransaction_ = true;
        return inTransaction_;
    }

    static constexpr auto no_backend_error() {
        return std::unexpected(dbo_error{DboErrc::Connection, "no backend"});
    }

    awaitable<dbo_result<void>> executeSql(const std::string& sql) {
        if (!pimpl_) co_return no_backend_error();
        co_return co_await pimpl_->executeSql(sql);
    }

    awaitable<dbo_result<void>> executeSqlStateful(const std::string& sql) {
        if (!pimpl_) co_return no_backend_error();
        co_return co_await pimpl_->executeSqlStateful(sql);
    }

    awaitable<dbo_result<void>> startTransaction() {
        if (!pimpl_) co_return no_backend_error();
        auto r = co_await pimpl_->startTransaction();
        if (r) inTransaction_ = true;
        co_return r;
    }

    awaitable<dbo_result<void>> commitTransaction() {
        if (!pimpl_) co_return no_backend_error();
        auto r = co_await pimpl_->commitTransaction();
        if (r) inTransaction_ = false;
        co_return r;
    }

    awaitable<dbo_result<void>> rollbackTransaction() {
        if (!pimpl_) co_return no_backend_error();
        auto r = co_await pimpl_->rollbackTransaction();
        if (r) inTransaction_ = false;
        co_return r;
    }

    // --- Statements ---

    SqlStatement* getStatement(const std::string& id) {
        return pimpl_ ? pimpl_->getStatement(id) : nullptr;
    }

    void saveStatement(const std::string& id, std::unique_ptr<SqlStatement> statement) {
        if (pimpl_) pimpl_->saveStatement(id, std::move(statement));
    }

    dbo_result<std::unique_ptr<SqlStatement>> prepareStatement(const std::string& sql) {
        if (!pimpl_) return std::unique_ptr<SqlStatement>{};
        try {
            auto stmt = pimpl_->prepareStatement(sql);
            if (!stmt) {
                return std::unexpected(dbo_error{
                    DboErrc::Sql,
                    "prepareStatement returned null",
                    {},
                    {},
                    0,
                    "SqlConnection::prepareStatement"});
            }
            return stmt;
        } catch (const std::exception& e) {
            return std::unexpected(dbo_error{
                DboErrc::Sql, e.what(), {}, {}, 0, "SqlConnection::prepareStatement"});
        }
    }

    // --- Properties ---

    void setProperty(const std::string& name, const std::string& value) {
        if (pimpl_) pimpl_->setProperty(name, value);
    }

    std::string property(const std::string& name) const {
        return pimpl_ ? pimpl_->property(name) : std::string();
    }

    bool showQueries() const {
        return pimpl_ ? pimpl_->showQueries() : false;
    }

    // --- SQL Dialect ---

    std::string autoincrementSql() const {
        return pimpl_ ? pimpl_->autoincrementSql() : "";
    }

    std::vector<std::string> autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const {
        return pimpl_ ? pimpl_->autoincrementCreateSequenceSql(table, id) : std::vector<std::string>();
    }

    std::vector<std::string> autoincrementDropSequenceSql(const std::string &table, const std::string &id) const {
        return pimpl_ ? pimpl_->autoincrementDropSequenceSql(table, id) : std::vector<std::string>();
    }

    std::string autoincrementType() const {
        return pimpl_ ? pimpl_->autoincrementType() : "";
    }

    std::string autoincrementInsertInfix(const std::string& id) const {
        return pimpl_ ? pimpl_->autoincrementInsertInfix(id) : "";
    }

    std::string autoincrementInsertSuffix(const std::string& id) {
        return pimpl_ ? pimpl_->autoincrementInsertSuffix(id) : "";
    }

    void prepareForDropTables() {
        if (pimpl_) pimpl_->prepareForDropTables();
    }

    const char *dateTimeType(SqlDateTimeType type) const {
        return pimpl_ ? pimpl_->dateTimeType(type) : "";
    }

    const char *blobType() const {
        return pimpl_ ? pimpl_->blobType() : "";
    }

    std::string textType(int size) const {
        return pimpl_ ? pimpl_->textType(size) : "";
    }

    std::string longLongType() const {
        return pimpl_ ? pimpl_->longLongType() : "bigint";
    }

    const char *booleanType() const {
        return pimpl_ ? pimpl_->booleanType() : "boolean";
    }

    bool supportUpdateCascade() const {
        return pimpl_ ? pimpl_->supportUpdateCascade() : true;
    }

    bool requireSubqueryAlias() const {
        return pimpl_ ? pimpl_->requireSubqueryAlias() : false;
    }

    LimitQuery limitQueryMethod() const {
        return pimpl_ ? pimpl_->limitQueryMethod() : LimitQuery::Limit;
    }

    bool usesRowsFromTo() const {
        return pimpl_ ? pimpl_->usesRowsFromTo() : false;
    }

    bool supportAlterTable() const {
        return pimpl_ ? pimpl_->supportAlterTable() : false;
    }

    bool supportDeferrableFKConstraint() const {
        return pimpl_ ? pimpl_->supportDeferrableFKConstraint() : false;
    }

    const char *alterTableConstraintString() const {
        return pimpl_ ? pimpl_->alterTableConstraintString() : "constraint";
    }

    DialectKind dialectKind() const {
        return pimpl_ ? pimpl_->dialectKind() : DialectKind::Postgres;
    }

protected:
    void clearStatementCache() {
        if (pimpl_) pimpl_->clearStatementCache();
    }

    std::vector<SqlStatement *> getStatements() const {
        return pimpl_ ? pimpl_->getStatements() : std::vector<SqlStatement *>();
    }

    const std::vector<std::string>& getStatefulSql() const {
        static const std::vector<std::string> empty;
        return pimpl_ ? pimpl_->getStatefulSql() : empty;
    }

private:

    struct Concept {
        virtual ~Concept() = default;
        
        // Lifecycle
        virtual std::unique_ptr<SqlConnection> clone(asio::io_context& ctx) = 0;
        
        // Async Ops
        virtual awaitable<dbo_result<void>> executeSql(const std::string& sql) = 0;
        virtual awaitable<dbo_result<void>> executeSqlStateful(const std::string& sql) = 0;
        virtual awaitable<dbo_result<void>> startTransaction() = 0;
        virtual awaitable<dbo_result<void>> commitTransaction() = 0;
        virtual awaitable<dbo_result<void>> rollbackTransaction() = 0;
        
        // Statements
        virtual SqlStatement* getStatement(const std::string& id) = 0;
        virtual void saveStatement(const std::string& id, std::unique_ptr<SqlStatement> statement) = 0;
        virtual std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql) = 0;
        virtual void clearStatementCache() = 0;
        virtual std::vector<SqlStatement *> getStatements() const = 0;
        
        // Properties
        virtual void setProperty(const std::string& name, const std::string& value) = 0;
        virtual std::string property(const std::string& name) const = 0;
        virtual bool showQueries() const = 0;
        
        // Dialect
        virtual std::string autoincrementSql() const = 0;
        virtual std::vector<std::string> autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const = 0;
        virtual std::vector<std::string> autoincrementDropSequenceSql(const std::string &table, const std::string &id) const = 0;
        virtual std::string autoincrementType() const = 0;
        virtual std::string autoincrementInsertInfix(const std::string& id) const = 0;
        virtual std::string autoincrementInsertSuffix(const std::string& id) const = 0;
        virtual std::string textType(int size) const = 0;
        virtual std::string longLongType() const = 0;
        virtual const char *booleanType() const = 0;
        virtual const char *dateTimeType(SqlDateTimeType type) const = 0;
        virtual const char *blobType() const = 0;
        
        virtual bool supportUpdateCascade() const = 0;
        virtual bool requireSubqueryAlias() const = 0;
        virtual LimitQuery limitQueryMethod() const = 0;
        virtual bool usesRowsFromTo() const = 0;
        virtual bool supportAlterTable() const = 0;
        virtual bool supportDeferrableFKConstraint() const = 0;
        virtual const char *alterTableConstraintString() const = 0;
        virtual void prepareForDropTables() = 0;
        virtual const std::vector<std::string>& getStatefulSql() const = 0;
        virtual DialectKind dialectKind() const = 0;
    };

    template<typename BackendT>
    struct Model final : Concept {
        BackendT backend_;

        explicit Model(BackendT backend) : backend_(std::move(backend)) {}

        std::unique_ptr<SqlConnection> clone(asio::io_context& ctx) override {
            return std::make_unique<SqlConnection>(backend_.clone(ctx));
        }

        awaitable<dbo_result<void>> executeSql(const std::string& sql) override { co_return co_await backend_.executeSql(sql); }
        awaitable<dbo_result<void>> executeSqlStateful(const std::string& sql) override { co_return co_await backend_.executeSqlStateful(sql); }
        awaitable<dbo_result<void>> startTransaction() override { co_return co_await backend_.startTransaction(); }
        awaitable<dbo_result<void>> commitTransaction() override { co_return co_await backend_.commitTransaction(); }
        awaitable<dbo_result<void>> rollbackTransaction() override { co_return co_await backend_.rollbackTransaction(); }

        SqlStatement* getStatement(const std::string& id) override { return backend_.getStatement(id); }
        void saveStatement(const std::string& id, std::unique_ptr<SqlStatement> statement) override { backend_.saveStatement(id, std::move(statement)); }
        std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql) override { return backend_.prepareStatement(sql); }
        void clearStatementCache() override { backend_.clearStatementCache(); }
        std::vector<SqlStatement *> getStatements() const override { return backend_.getStatements(); }

        void setProperty(const std::string& name, const std::string& value) override { backend_.setProperty(name, value); }
        std::string property(const std::string& name) const override { return backend_.property(name); }
        bool showQueries() const override { return backend_.showQueries(); }

        std::string autoincrementSql() const override { return backend_.autoincrementSql(); }
        std::vector<std::string> autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const override { return backend_.autoincrementCreateSequenceSql(table, id); }
        std::vector<std::string> autoincrementDropSequenceSql(const std::string &table, const std::string &id) const override { return backend_.autoincrementDropSequenceSql(table, id); }
        std::string autoincrementType() const override { return backend_.autoincrementType(); }
        std::string autoincrementInsertInfix(const std::string& id) const override { return backend_.autoincrementInsertInfix(id); }
        std::string autoincrementInsertSuffix(const std::string& id) const override { return backend_.autoincrementInsertSuffix(id); }
        
        std::string textType(int size) const override { return backend_.textType(size); }
        std::string longLongType() const override { return backend_.longLongType(); }
        const char *booleanType() const override { return backend_.booleanType(); }
        const char *dateTimeType(SqlDateTimeType type) const override { return backend_.dateTimeType(type); }
        const char *blobType() const override { return backend_.blobType(); }

        bool supportUpdateCascade() const override { return backend_.supportUpdateCascade(); }
        bool requireSubqueryAlias() const override { return backend_.requireSubqueryAlias(); }
        LimitQuery limitQueryMethod() const override { return backend_.limitQueryMethod(); }
        bool usesRowsFromTo() const override { return backend_.usesRowsFromTo(); }
        bool supportAlterTable() const override { return backend_.supportAlterTable(); }
        bool supportDeferrableFKConstraint() const override { return backend_.supportDeferrableFKConstraint(); }
        const char *alterTableConstraintString() const override { return backend_.alterTableConstraintString(); }
        void prepareForDropTables() override { backend_.prepareForDropTables(); }
        const std::vector<std::string>& getStatefulSql() const override { return backend_.getStatefulSql(); }
        DialectKind dialectKind() const override { return backend_.dialectKind(); }
    };

    std::unique_ptr<Concept> pimpl_;
    asio::any_io_executor executor_;
    bool inTransaction_ = false;
};

} // namespace Dbo
} // namespace Wt

#endif // WT_DBO_SQL_CONNECTION_H_
