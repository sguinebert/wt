#ifndef POSTGRESSTATEMENT_H
#define POSTGRESSTATEMENT_H


#include "connection.hpp"
#include "libpq-fe.h"
#include "Wt/Dbo/sql/Statement.h"
#include "Wt/Dbo/core/Exception.h"
#include "Wt/fmt/format.h"

#include "Wt/Date/date.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/karma.hpp>


#include <cerrno>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <vector>
#include <cstring>
#include <ctime>
#include <optional>

#define PG_SCOPE_BEGIN namespace Wt { namespace Dbo { namespace backend {
#define PG_SCOPE_END }}}

//PostgresException(const std::string &msg, const std::string &code)
//    : Exception(msg, code)
//{
//}
//};
namespace karma = boost::spirit::karma;

namespace
{

inline struct timeval toTimeval(std::chrono::microseconds ms)
{
    std::chrono::seconds s = date::floor<std::chrono::seconds>(ms);
    struct timeval result;
    result.tv_sec = s.count();
    result.tv_usec = (ms - s).count();
    return result;
}

// adjust rendering for JS flaots
template <typename T, int Precision>
struct PostgresPolicy : karma::real_policies<T>
{
    // not 'nan', but 'NaN'
    template <typename CharEncoding, typename Tag, typename OutputIterator>
    static bool nan(OutputIterator &sink, T n, bool force_sign)
    {
        return karma::string_inserter<CharEncoding, Tag>::call(sink, "NaN");
    }

    // not 'inf', but 'Infinity'
    template <typename CharEncoding, typename Tag, typename OutputIterator>
    static bool inf(OutputIterator &sink, T n, bool force_sign)
    {
        return karma::sign_inserter::call(sink, false, (n < 0), force_sign) &&
               karma::string_inserter<CharEncoding, Tag>::call(sink, "Infinity");
    }

    static int floatfield(T t)
    {
        return (t != 0.0) && ((t < 0.001) || (t > 1E8)) ? karma::real_policies<T>::fmtflags::scientific : karma::real_policies<T>::fmtflags::fixed;
    }

    // 7 significant numbers; about float precision
    static unsigned precision(T) { return Precision; }
};

using PostgresReal = karma::real_generator<float, PostgresPolicy<float, 7>>;
using PostgresDouble = karma::real_generator<double, PostgresPolicy<double, 15>>;

static inline std::string double_to_s(const double d)
{
    char buf[30];
    char *p = buf;
    if (d != 0)
    {
        karma::generate(p, PostgresDouble(), d);
    }
    else
    {
        *p++ = '0';
    }
    *p = '\0';
    return std::string(buf, p);
}

static inline std::string float_to_s(const float f)
{
    char buf[30];
    char *p = buf;
    if (f != 0)
    {
        karma::generate(p, PostgresReal(), f);
    }
    else
    {
        *p++ = '0';
    }
    *p = '\0';
    return std::string(buf, p);
}

static inline bool parse_unsigned(std::string_view sv, int& out)
{
    if (sv.empty())
        return false;

    int parsed = 0;
    const char* first = sv.data();
    const char* last = first + sv.size();
    auto [ptr, ec] = std::from_chars(first, last, parsed);
    if (ec != std::errc{} || ptr != last)
        return false;

    out = parsed;
    return true;
}

static inline bool parse_iso_date(std::string_view sv,
                                  std::chrono::system_clock::time_point& out)
{
    if (sv.size() != 10 || sv[4] != '-' || sv[7] != '-')
        return false;

    int y = 0;
    int m = 0;
    int d = 0;
    if (!parse_unsigned(sv.substr(0, 4), y)
        || !parse_unsigned(sv.substr(5, 2), m)
        || !parse_unsigned(sv.substr(8, 2), d))
        return false;

    const date::year yy{y};
    const date::month mm{static_cast<unsigned>(m)};
    const date::day dd{static_cast<unsigned>(d)};
    if (!(yy.ok() && mm.ok() && dd.ok()))
        return false;

    out = date::sys_days{yy / mm / dd};
    return true;
}

static inline bool parse_hms_duration(std::string_view sv,
                                      std::chrono::system_clock::duration& out)
{
    const auto firstColon = sv.find(':');
    if (firstColon == std::string_view::npos)
        return false;

    const auto secondColon = sv.find(':', firstColon + 1);
    if (secondColon == std::string_view::npos)
        return false;

    int hh = 0;
    int mm = 0;
    int ss = 0;
    if (!parse_unsigned(sv.substr(0, firstColon), hh)
        || !parse_unsigned(sv.substr(firstColon + 1, secondColon - firstColon - 1), mm))
        return false;
    if (mm < 0 || mm > 59)
        return false;

    const std::string_view secondsPart = sv.substr(secondColon + 1);
    if (secondsPart.empty())
        return false;

    const auto dot = secondsPart.find('.');
    const std::string_view secToken = (dot == std::string_view::npos)
      ? secondsPart
      : secondsPart.substr(0, dot);
    if (!parse_unsigned(secToken, ss) || ss < 0 || ss > 59)
        return false;

    out = std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::hours{hh} + std::chrono::minutes{mm} + std::chrono::seconds{ss});

    if (dot != std::string_view::npos) {
        const std::string_view frac = secondsPart.substr(dot + 1);
        if (frac.empty())
            return false;

        long long nanos = 0;
        std::size_t consumed = 0;
        for (; consumed < frac.size() && consumed < 9; ++consumed) {
            const char c = frac[consumed];
            if (c < '0' || c > '9')
                return false;
            nanos = nanos * 10 + (c - '0');
        }
        for (; consumed < 9; ++consumed)
            nanos *= 10;
        for (; consumed < frac.size(); ++consumed) {
            const char c = frac[consumed];
            if (c < '0' || c > '9')
                return false;
        }

        out += std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::nanoseconds{nanos});
    }

    return true;
}

static inline bool split_timezone_suffix(std::string_view sv,
                                         std::string_view& tsNoTz,
                                         std::chrono::system_clock::duration& tzOffset)
{
    tsNoTz = sv;
    tzOffset = std::chrono::system_clock::duration::zero();

    auto setOffset = [&](char sign, int hh, int mm) {
        auto offset = std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::hours{hh} + std::chrono::minutes{mm});
        tzOffset = (sign == '-') ? -offset : offset;
    };

    // +HH:MM / -HH:MM
    if (sv.size() >= 6) {
        const std::size_t pos = sv.size() - 6;
        const char sign = sv[pos];
        if ((sign == '+' || sign == '-') && sv[pos + 3] == ':') {
            int hh = 0;
            int mm = 0;
            if (!parse_unsigned(sv.substr(pos + 1, 2), hh)
                || !parse_unsigned(sv.substr(pos + 4, 2), mm)
                || mm > 59)
                return false;
            setOffset(sign, hh, mm);
            tsNoTz = sv.substr(0, pos);
            return true;
        }
    }

    // +HH / -HH
    if (sv.size() >= 3) {
        const std::size_t pos = sv.size() - 3;
        const char sign = sv[pos];
        if (sign == '+' || sign == '-') {
            int hh = 0;
            if (!parse_unsigned(sv.substr(pos + 1, 2), hh))
                return false;
            setOffset(sign, hh, 0);
            tsNoTz = sv.substr(0, pos);
        }
    }

    return true;
}

static inline bool parse_iso_timestamp(std::string_view sv,
                                       std::chrono::system_clock::time_point& out)
{
    if (sv.size() < 19 || (sv[10] != ' ' && sv[10] != 'T'))
        return false;

    std::chrono::system_clock::time_point day{};
    std::chrono::system_clock::duration time{};
    if (!parse_iso_date(sv.substr(0, 10), day)
        || !parse_hms_duration(sv.substr(11), time))
        return false;

    out = day + time;
    return true;
}

static inline bool parse_interval_millis(std::string_view sv,
                                         std::chrono::duration<int, std::milli>& out)
{
    bool neg = false;
    if (!sv.empty() && sv.front() == '-') {
        neg = true;
        sv.remove_prefix(1);
    }

    std::chrono::system_clock::duration dur{};
    if (!parse_hms_duration(sv, dur))
        return false;

    auto ms = std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(dur);
    out = neg ? -ms : ms;
    return true;
}
}

PG_SCOPE_BEGIN

class PostgresException : public Exception
{
public:
PostgresException(const std::string &msg)
    : Exception(msg)
{
}

PostgresException(const std::string &msg, const std::string &code)
    : Exception(msg, code)
{
}
};

class PostgresStatement final : public SqlStatement
{
public:
    PostgresStatement(postgrespp::connection &conn, const std::string &sql)
        : conn_(conn),
        sql_(sql), res_{nullptr}
    {
        convertToNumberedPlaceholders();

        lastId_ = -1;
        row_ = affectedRows_ = 0;
        result_ = nullptr;

        paramValues_ = nullptr;
        paramTypes_ = paramLengths_ = paramFormats_ = nullptr;
        columnCount_ = 0;

        snprintf(name_, 64, "SQL%p%08X", (void *)this, rand());

        //LOG_DEBUG("{} for: {}", this, sql_);

        state_ = Done;
    }

    virtual ~PostgresStatement()
    {
//        if (result_)
//            PQclear(result_);
        delete[] paramValues_;
        delete[] paramTypes_;
    }

    virtual void reset() override
    {
        params_.clear();
        pendingError_.reset();

        state_ = Done;
    }

    void rebuild()
    {
        if (result_)
        {
            PQclear(result_);
            result_ = 0;
            delete[] paramValues_;
            paramValues_ = 0;
            delete[] paramTypes_;
            paramTypes_ = paramLengths_ = paramFormats_ = 0;
        }
    }

    virtual void bind(int column, const std::string &value) override
    {
        //LOG_DEBUG("{} bind {} {}", this, column, value);

        setValue(column, value);
    }

    virtual void bind(int column, short value) override
    {
        bind(column, static_cast<int>(value));
    }

    virtual void bind(int column, int value) override
    {
        //LOG_DEBUG("{} bind {} {}", this, column, value);

        setValue(column, std::to_string(value));
    }

    virtual void bind(int column, long long value) override
    {
        //LOG_DEBUG("{} bind {} {}", this, column, value);

        setValue(column, std::to_string(value));
    }

    virtual void bind(int column, float value) override
    {
        //LOG_DEBUG("{} bind {} {}", this, column, value);

        setValue(column, float_to_s(value));
    }

    virtual void bind(int column, double value) override
    {
        //LOG_DEBUG("{} bind {} {}", this, column, value);

        setValue(column, double_to_s(value));
    }

    virtual void bind(int column, const std::chrono::duration<int, std::milli> &value) override
    {
        auto absValue = value < std::chrono::milliseconds::zero() ? -value : value;
        auto hours = date::floor<std::chrono::hours>(absValue);
        auto minutes = date::floor<std::chrono::minutes>(absValue) - hours;
        auto seconds = date::floor<std::chrono::seconds>(absValue) - hours - minutes;
        auto milliseconds = date::floor<std::chrono::milliseconds>(absValue) - hours - minutes - seconds;

        std::string text = fmt::format("{:02}:{:02}:{:02}.{:03}",
                                       hours.count(),
                                       minutes.count(),
                                       seconds.count(),
                                       milliseconds.count());
        if (absValue != value)
            text.insert(text.begin(), '-');

        setValue(column, text);
    }

    virtual void bind(int column, const std::chrono::system_clock::time_point &value,
                      SqlDateTimeType type) override
    {
        if (type == SqlDateTimeType::Date)
        {
            auto daypoint = date::floor<date::days>(value);
            auto ymd = date::year_month_day(daypoint);
            setValue(column, fmt::format("{:04}-{:02}-{:02}",
                                         static_cast<int>(ymd.year()),
                                         static_cast<unsigned>(ymd.month()),
                                         static_cast<unsigned>(ymd.day())));
        }
        else
        {
            auto daypoint = date::floor<date::days>(value);
            auto ymd = date::year_month_day(daypoint);
            auto tod = date::make_time(value - daypoint);
            const auto subms = date::floor<std::chrono::milliseconds>(tod.subseconds()).count();
            setValue(column, fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}+00",
                                         static_cast<int>(ymd.year()),
                                         static_cast<unsigned>(ymd.month()),
                                         static_cast<unsigned>(ymd.day()),
                                         tod.hours().count(),
                                         tod.minutes().count(),
                                         tod.seconds().count(),
                                         subms));
        }
    }

    virtual void bind(int column, const std::vector<unsigned char> &value) override
    {
        //LOG_DEBUG("{} bind {} (blob, size={})", this, column, value.size());

        for (int i = (int)params_.size(); i <= column; ++i)
            params_.push_back(Param());

        Param &p = params_[column];
        p.value.resize(value.size());
        if (value.size() > 0)
            std::memcpy(const_cast<char *>(p.value.data()), &(*value.begin()),
                        value.size());
        p.isbinary = true;
        p.isnull = false;

        // FIXME if first null was bound, check here and invalidate the prepared
        // statement if necessary because the type changes
    }

    virtual void bindNull(int column) override
    {
        //LOG_DEBUG("{} bind {} null", this, column);

        for (int i = (int)params_.size(); i <= column; ++i)
            params_.push_back(Param());

        params_[column].isnull = true;
    }

    #define BYTEAOID 17
    const std::chrono::seconds TRANSACTION_LIFETIME_MARGIN = std::chrono::seconds(120);

    virtual awaitable<dbo_result<void>> execute() override
    {
      try {
        if (pendingError_) {
            co_return std::unexpected(*pendingError_);
        }

        conn_.checkConnection(TRANSACTION_LIFETIME_MARGIN);

        if (conn_.showQueries()){
            //LOG_INFO(fmt::runtime(sql_));
            //fmtlog::poll();
        }

        if (!result_)
        {
            paramValues_ = new char *[params_.size()];

            for (unsigned i = 0; i < params_.size(); ++i)
            {
                if (params_[i].isbinary)
                {
                    paramTypes_ = new int[params_.size() * 3];
                    paramLengths_ = paramTypes_ + params_.size();
                    paramFormats_ = paramLengths_ + params_.size();
                    for (unsigned j = 0; j < params_.size(); ++j)
                    {
                        paramTypes_[j] = params_[j].isbinary ? BYTEAOID : 0;
                        paramFormats_[j] = params_[j].isbinary ? 1 : 0;
                        paramLengths_[j] = 0;
                    }

                    break;
                }
            }

            co_await conn_.async_prepare(name_, sql_, paramTypes_ ? params_.size() : 0, (Oid *)paramTypes_, use_awaitable);

        }

        for (unsigned i = 0; i < params_.size(); ++i)
        {
            if (params_[i].isnull)
                paramValues_[i] = nullptr;
            else if (params_[i].isbinary)
            {
                paramValues_[i] = const_cast<char *>(params_[i].value.data());
                paramLengths_[i] = params_[i].value.length();
            }
            else
                paramValues_[i] = const_cast<char *>(params_[i].value.c_str());
        }

        res_ = co_await conn_.async_exec_prepared(name_, use_awaitable,
                                                  paramValues_, paramLengths_, paramFormats_, params_.size());

        //res_ = std::make_unique<postgrespp::result>(std::move(res));


//        std::cout << "res count : " << res_.size() << " - " << (int)res_.status() << std::endl;
//        try {
//            for(auto& r: res_) {
//                for(int i(0); i < res_.columnCount(); i++)
//                    std::cout << r.at(i).as<int>() << " - ";
//                std::cout << std::endl;
//        }
//        }catch (...) {
//            std::cerr << "conversion fail" << std::endl;
//        }


        result_ = res_.get();

//        auto ffff = res_.at(0).at(0).as<double>();

//        auto cccc = PQgetlength(result_, 0, 0);
//        std::string_view v (PQgetvalue(result_, 0, 0), cccc );

//        std::cout << "res brute : " << cccc <<  " " << v << std::endl;

        row_ = 0;
        if (res_.status() == postgrespp::result::status_t::COMMAND_OK)
        {
            affectedRows_ = res_.affected_rows();
        }
        else if (PQresultStatus(result_) == PGRES_TUPLES_OK)
            affectedRows_ = res_.size();//affectedRows_ = PQntuples(result_);

        columnCount_ = res_.columnCount();

        std::string error;

        bool isInsertReturningId = false;
        if (affectedRows_ == 1)
        {
            const std::string returning = " returning ";
            std::size_t j = sql_.rfind(returning);
            if (j != std::string::npos) //&& sql_.find(' ', j + returning.length()) == std::string::npos
                isInsertReturningId = true;
        }

        if (isInsertReturningId)
        {
            state_ = NoFirstRow;
            if (PQntuples(result_) == 1 && PQnfields(result_) > 0)
            {
                //lastId_ = std::stoll(PQgetvalue(result_, 0, 0));
                lastids_.clear();
                for (auto i = 0; i < PQnfields(result_); i++)
                {
                    lastids_.push_back(PQgetvalue(result_, 0, i));
                }
            }
        }
        else
        {
            if (PQntuples(result_) == 0)
            {
                state_ = NoFirstRow;
            }
            else
            {
                state_ = FirstRow;
            }
        }

//        PGresult *nullResult = PQgetResult(conn_.underlying_handle());
//        if (nullResult != 0)
//        {
//            throw std::runtime_error("PQgetResult() returned more results");
//        }
        auto err = handleErr(PQresultStatus(result_), result_);
        if (!err) {
            co_return std::unexpected(err.error());
        }

        co_return dbo_result<void>{};
      } catch (const std::exception& e) {
        co_return std::unexpected(dbo_error{DboErrc::Sql, e.what(), "postgres"});
      }
    }

//    virtual result_base sync_execute() override
//    {
//        return result_base();
//    }

//    virtual void async_execute(std::function<void(result_base)> cb) override
//    {

//    }

    virtual long long insertedId() override
    {
        if (!lastids_.empty())
            return std::stoll(Wt::cpp17::any_cast<char *>(lastids_[0]));
        return -1;
    }

    virtual std::vector<Wt::cpp17::any> insertedNaturalIds() override
    {
        return lastids_;
    }

    virtual int affectedRowCount() override
    {
        return affectedRows_;
    }

    virtual bool nextRow() override
    {
        switch (state_)
        {
        case NoFirstRow:
            state_ = Done;
            return false;
        case FirstRow:
            state_ = NextRow;
            return true;
        case NextRow:
            if (row_ + 1 < PQntuples(result_))
            {
                row_++;
                return true;
            }
            else
            {
                state_ = Done;
                return false;
            }
            break;
        case Done:
            return false;
        }

        return false;
    }

    virtual int columnCount() const override
    {
        return columnCount_;
    }

    virtual bool getResult(int column, std::string *value, int size) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //*value = PQgetvalue(result_, row_, column);
        *value = res_.at(row_).at(column).as<std::string>();

        //LOG_DEBUG("{} result string {} {}", this, column, value);

        return true;
    }

    virtual bool getResult(int column, short *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        *value = res_.at(row_).at(column).as<short>();
//        int intValue;
//        if (getResult(column, &intValue))
//        {
//            *value = intValue;
//            return true;
//        }
//        else
//            return false;
        return true;
    }

    virtual bool getResult(int column, int *value) override
    {
        //std::cout << "result " << res_.at(row_).at(column).as<int>() << std::endl;

        if (PQgetisnull(result_, row_, column))
            return false;

        *value = res_.at(row_).at(column).as<int>();

        /*
          * booleans are mapped to int values
          */
//        if (*v == 'f')
//            *value = 0;
//        else if (*v == 't')
//            *value = 1;
//        else
//            *value = std::stoi(v);

        //LOG_DEBUG("{} result int {} {}", this, column, value);

        return true;
    }

    virtual bool getResult(int column, long long *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //*value = std::stoll(PQgetvalue(result_, row_, column));
        //*value = res_.at(row_).at(column).as<int>();

        *value = res_.at(row_).at(column).as<long long>();

        //LOG_DEBUG("{} result long long {} {}", this, column, value);

        return true;
    }

    virtual bool getResult(int column, float *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //*value = std::stof(PQgetvalue(result_, row_, column));
        *value = res_.at(row_).at(column).as<double>();

        //LOG_DEBUG("{} result float {} {}", this, column, value);

        return true;
    }

    virtual bool getResult(int column, double *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //*value = std::stod(PQgetvalue(result_, row_, column));
        *value = res_.at(row_).at(column).as<double>();

        //LOG_DEBUG("{} result double {} {}", this, column, value);

        return true;
    }

    virtual bool getResult(int column,
                           std::chrono::system_clock::time_point *value,
                           SqlDateTimeType type) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //std::string v = PQgetvalue(result_, row_, column);
        std::string v = res_.at(row_).at(column).as<std::string>();

        if (type == SqlDateTimeType::Date)
            return parse_iso_date(v, *value);

        /*
         * Handle timezone offset. Postgres may append a timezone offset if
         * a column is TIMESTAMP WITH TIME ZONE. If offset is present,
         * subtract it for UTC output.
         */
        std::string_view tsNoTz{};
        std::chrono::system_clock::duration tzOffset{};
        if (!split_timezone_suffix(v, tsNoTz, tzOffset))
            return false;
        if (!parse_iso_timestamp(tsNoTz, *value))
            return false;
        *value -= tzOffset;

        return true;
    }

    virtual bool getResult(int column, std::chrono::duration<int, std::milli> *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //std::string v = PQgetvalue(result_, row_, column);
        std::string v = res_.at(row_).at(column).as<std::string>();
        return parse_interval_millis(v, *value);
    }

    virtual bool getResult(int column, std::vector<unsigned char> *value,
                           int size) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        const char *escaped = PQgetvalue(result_, row_, column);

        std::size_t vlength;
        unsigned char *v = PQunescapeBytea((unsigned char *)escaped, &vlength);

        value->resize(vlength);
        std::copy(v, v + vlength, value->begin());
        PQfreemem(v);

        //LOG_DEBUG("{} result blob {} (blob, size ={})", this, column, vlength);

        return true;
    }

    virtual std::string sql() const override
    {
        return sql_;
    }

private:
    struct Param
    {
        std::string value;
        bool isnull, isbinary;

        Param() : isnull(true), isbinary(false) {}
    };

    postgrespp::connection &conn_;
    std::string sql_;
    char name_[64];
    PGresult *result_;
    //std::unique_ptr<postgrespp::result> res_;
    postgrespp::result res_;
    enum
    {
        NoFirstRow,
        FirstRow,
        NextRow,
        Done
    } state_;
    std::vector<Param> params_;

    int paramCount_;
    char **paramValues_;
    int *paramTypes_, *paramLengths_, *paramFormats_;

    long long lastId_;
    std::vector<Wt::cpp17::any> lastids_;
    int row_, affectedRows_, columnCount_;
    std::optional<dbo_error> pendingError_;

    dbo_result<void> handleErr(int err, PGresult *result)
    {
        if (err != PGRES_COMMAND_OK && err != PGRES_TUPLES_OK)
        {
            std::string code;

            if (result)
            {
                char *v = PQresultErrorField(result, PG_DIAG_SQLSTATE);
                if (v)
                    code = v;
            }

            std::string message;
            if (result) {
              message = std::string(PQresultErrorMessage(result));
            }
            if (message.empty()) {
              message = std::string(PQerrorMessage(conn_.underlying_handle()));
            }
            if (message.empty()) {
              message = res_.error_message();
            }
            if (message.empty()) {
              message = "postgres statement error";
            }

            return std::unexpected(dbo_error{DboErrc::Sql, message, code});
        }

        return dbo_result<void>{};
    }

    void setValue(int column, const std::string &value)
    {
        if (column >= paramCount_) {
            pendingError_ = dbo_error{
                DboErrc::Sql,
                "postgres bind: too many parameters",
                "postgres"};
            return;
        }

        for (int i = (int)params_.size(); i <= column; ++i)
            params_.push_back(Param());

        params_[column].value = value;
        params_[column].isnull = false;
    }

    void convertToNumberedPlaceholders()
    {
        std::string result;
        result.reserve(sql_.size() + 16);

        enum
        {
            Statement,
            SQuote,
            DQuote
        } state = Statement;
        int placeholder = 1;

        for (unsigned i = 0; i < sql_.length(); ++i)
        {
            switch (state)
            {
            case Statement:
                if (sql_[i] == '\'')
                    state = SQuote;
                else if (sql_[i] == '"')
                    state = DQuote;
                else if (sql_[i] == '?')
                {
                    if (i + 1 != sql_.length() &&
                        sql_[i + 1] == '?')
                    {
                        // escape question mark with double question mark
                        result.push_back('?');
                        ++i;
                    }
                    else
                    {
                        result.push_back('$');
                        result.append(std::to_string(placeholder++));
                    }
                    continue;
                }
                else if (sql_[i] == '$') //FIX ME
                    placeholder++;
                break;
            case SQuote:
                if (sql_[i] == '\'')
                {
                    if (i + 1 == sql_.length())
                        state = Statement;
                    else if (sql_[i + 1] == '\'')
                    {
                        result.push_back(sql_[i]);
                        ++i; // skip to next
                    }
                    else
                        state = Statement;
                }
                break;
            case DQuote:
                if (sql_[i] == '"')
                    state = Statement;
                break;
            }
            result.push_back(sql_[i]);
        }

        paramCount_ = placeholder - 1;
        sql_ = std::move(result);
    }
};

PG_SCOPE_END
#endif // POSTGRESSTATEMENT_H
