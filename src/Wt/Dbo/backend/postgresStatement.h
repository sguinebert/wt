#ifndef POSTGRESSTATEMENT_H
#define POSTGRESSTATEMENT_H


#include "connection.hpp"
#include "libpq-fe.h"
#include "Wt/Dbo/SqlStatement.h"

#include "Wt/Date/date.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/karma.hpp>


#include <cerrno>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cstring>
#include <ctime>

#define PG_SCOPE_BEGIN namespace Wt { namespace Dbo { namespace Backend {
#define PG_SCOPE_END }}}

//PostgresException(const std::string &msg, const std::string &code)
//    : Exception(msg, code)
//{
//}
//};
using namespace Wt::Dbo;

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
}

PG_SCOPE_BEGIN

class postgresStatement final : public SqlStatement
{
public:
    postgresStatement(postgrespp::connection &conn, const std::string &sql)
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

    virtual ~postgresStatement()
    {
//        if (result_)
//            PQclear(result_);
        delete[] paramValues_;
        delete[] paramTypes_;
    }

    virtual void reset() override
    {
        params_.clear();

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

        std::stringstream ss;
        ss.imbue(std::locale::classic());
        if (absValue != value)
            ss << '-';
        ss << std::setfill('0')
           << std::setw(2) << hours.count() << ':'
           << std::setw(2) << minutes.count() << ':'
           << std::setw(2) << seconds.count() << '.'
           << std::setw(3) << milliseconds.count();

        //LOG_DEBUG("{} bind {} {}", this, column, ss.str());

        setValue(column, ss.str());
    }

    virtual void bind(int column, const std::chrono::system_clock::time_point &value,
                      SqlDateTimeType type) override
    {
        std::stringstream ss;
        ss.imbue(std::locale::classic());
        if (type == SqlDateTimeType::Date)
        {
            auto daypoint = date::floor<date::days>(value);
            auto ymd = date::year_month_day(daypoint);
            ss << (int)ymd.year() << '-' << (unsigned)ymd.month() << '-' << (unsigned)ymd.day();
        }
        else
        {
            auto daypoint = date::floor<date::days>(value);
            auto ymd = date::year_month_day(daypoint);
            auto tod = date::make_time(value - daypoint);
            ss << (int)ymd.year() << '-' << (unsigned)ymd.month() << '-' << (unsigned)ymd.day() << ' ';
            ss << std::setfill('0')
               << std::setw(2) << tod.hours().count() << ':'
               << std::setw(2) << tod.minutes().count() << ':'
               << std::setw(2) << tod.seconds().count() << '.'
               << std::setw(3) << date::floor<std::chrono::milliseconds>(tod.subseconds()).count();
            /*
            * Add explicit timezone offset. Postgres will ignore this for a TIMESTAMP
            * column, but will treat the timestamp as UTC in a TIMESTAMP WITH TIME
            * ZONE column -- possibly in a legacy table.
            */
            ss << "+00";
        }
        //LOG_DEBUG("{} bind {} {}", this, column, ss.str());

        setValue(column, ss.str());
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

    virtual awaitable<result_base> execute() override
    {
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
        handleErr(PQresultStatus(result_), result_);

        co_return res_;
    }

    virtual result_base sync_execute() override
    {
        return result_base();
    }

    virtual void async_execute(std::function<void(result_base)> cb) override
    {

    }

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
            throw std::runtime_error("Postgres: nextRow(): statement already finished");
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

        //const char *v = PQgetvalue(result_, row_, column);

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
        {
            std::istringstream in(v);
            in.imbue(std::locale::classic());
            in >> date::parse("%F", *value);
        }
        else
        {
            /*
            * Handle timezone offset. Postgres will append a timezone offset [+-]dd
            * if a column is defined as TIMESTAMP WITH TIME ZONE -- possibly
            * in a legacy table. If offset is present, subtract it for UTC output.
            */
            int offsetHour = 0;
            if (v.size() >= 3 && std::strchr("+-", v[v.size() - 3]))
            {
                offsetHour = std::stoi(v.substr(v.size() - 3));
                v = v.substr(0, v.size() - 3);
            }
            std::istringstream in(v);
            in.imbue(std::locale::classic());
            in >> date::parse("%F %T", *value);
            *value -= std::chrono::hours{offsetHour};
        }

        return true;
    }

    virtual bool getResult(int column, std::chrono::duration<int, std::milli> *value) override
    {
        if (PQgetisnull(result_, row_, column))
            return false;

        //std::string v = PQgetvalue(result_, row_, column);
        std::string v = res_.at(row_).at(column).as<std::string>();
        bool neg = false;
        if (!v.empty() && v[0] == '-')
        {
            neg = true;
            v = v.substr(1);
        }

        std::istringstream in(v);
        in.imbue(std::locale::classic());
        in >> date::parse("%T", *value);
        if (neg)
            *value = -(*value);

        return true;
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

    void handleErr(int err, PGresult *result)
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

            const auto pgerr = PQerrorMessage(conn_.underlying_handle());
            std::cerr << "error : " << res_.error_message() << std::endl;

            throw std::runtime_error(code);
        }
    }

    void setValue(int column, const std::string &value)
    {
        if (column >= paramCount_)
            throw std::runtime_error("Binding too many parameters");

        for (int i = (int)params_.size(); i <= column; ++i)
            params_.push_back(Param());

        params_[column].value = value;
        params_[column].isnull = false;
    }

    void convertToNumberedPlaceholders()
    {
        std::stringstream result;

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
                        result << '?';
                        ++i;
                    }
                    else
                    {
                        result << '$' << placeholder++;
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
                        result << sql_[i];
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
            result << sql_[i];
        }

        paramCount_ = placeholder - 1;
        sql_ = result.str();
    }
};

PG_SCOPE_END
#endif // POSTGRESSTATEMENT_H
