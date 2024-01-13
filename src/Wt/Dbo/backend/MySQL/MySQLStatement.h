/*
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Contributed by: Paul Harrison
 */
#pragma once
#include <Wt/Dbo/backend/MySQL.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboMySQLDllDefs.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <Wt/Dbo/Exception.h>
#include <Wt/WLogger.h>

#include <Wt/cpp20/date.hpp>
#include <Wt/cpp20/async_mutex.h>

#include <boost/mysql.hpp>

using namespace boost::mysql;

namespace Wt {
namespace Dbo {
namespace backend {

class MySQLException : public Exception
{
  public:
  MySQLException(const std::string& msg)
  : Exception(msg)
  { }
};

/* \brief MySQL prepared statement.
* @todo should the getResult requests all be type checked...
*/
class MySQLStatement final : public SqlStatement
{
public:
    MySQLStatement(MySQL& conn, const std::string& sql)
        : conn_(conn),
        sql_(sql)
    {
        lastId_ = -1;
        irow_ = affectedRows_ = 0;
        columnCount_ = 0;
//        result_ = nullptr;
//        out_pars_ = nullptr;
//        errors_ = nullptr;
//        is_nulls_ = nullptr;
        lastOutCount_ = 0;


        //paramCount_ =

//        conn_.checkConnection();
//        stmt_ =  mysql_stmt_init(conn_.connection()->mysql);
//        mysql_stmt_attr_set(stmt_, STMT_ATTR_UPDATE_MAX_LENGTH, &mysqltrue_);
//        if(mysql_stmt_prepare(stmt_, sql_.c_str(), sql_.length()) != 0) {
//            throw MySQLException("error creating prepared statement: '"
//                                 + sql + "': " + mysql_stmt_error(stmt_));
//        }

//        columnCount_ = static_cast<int>(mysql_stmt_field_count(stmt_));

//        paramCount_ = mysql_stmt_param_count(stmt_);

//        if (paramCount_ > 0) {
//            in_pars_ =
//                (MYSQL_BIND *)malloc(sizeof(MYSQL_BIND) * paramCount_);
//            std::memset(in_pars_, 0, sizeof(MYSQL_BIND) * paramCount_);
//        } else {
//            in_pars_ = nullptr;
//        }

//        LOG_DEBUG("new SQLStatement for: {}", sql_);

        state_ = Done;
    }

    virtual ~MySQLStatement()
    {
//        LOG_DEBUG("closing prepared stmt {}", sql_);
//        for(unsigned int i = 0;   i < mysql_stmt_param_count(stmt_) ; ++i)
//            freeColumn(i);
//        if (in_pars_) free(in_pars_);

//        if(out_pars_) free_outpars();

//        if (errors_) delete[] errors_;

//        if (is_nulls_) delete[] is_nulls_;

//        if(result_) {
//            mysql_free_result(result_);
//        }

//        mysql_stmt_close(stmt_);
//        stmt_ = nullptr;
    }

    virtual void reset() override
    {
        state_ = Done;
        has_truncation_ = false;
    }

    virtual void bind(int column, const std::string& value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;

//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);

//        unsigned long * len = (unsigned long *)malloc(sizeof(unsigned long));

//        char * data;
//        //memset(&in_pars_[column], 0, sizeofin_pars_[column]);// Check
//        in_pars_[column].buffer_type = MYSQL_TYPE_STRING;

//        unsigned long bufLen = value.length() + 1;
//        *len = value.length();
//        data = (char *)malloc(bufLen);
//        std::memcpy(data, value.c_str(), value.length());
//        freeColumn(column);
//        in_pars_[column].buffer = data;
//        in_pars_[column].buffer_length = bufLen;
//        in_pars_[column].length = len;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, short value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;

//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);
//        short * data = (short *)malloc(sizeof(short));
//        *data = value;
//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_SHORT;
//        in_pars_[column].buffer = data;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;

    }

    virtual void bind(int column, int value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;

//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);
//        int * data = (int *)malloc(sizeof(int));
//        *data = value;
//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_LONG;
//        in_pars_[column].buffer = data;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, long long value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;

//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);
//        long long * data = (long long *)malloc(sizeof(long long));
//        *data = value;
//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_LONGLONG;
//        in_pars_[column].buffer = data;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, float value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;
//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);
//        float * data = (float *) malloc(sizeof(float));
//        *data = value;
//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_FLOAT;
//        in_pars_[column].buffer = data;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, double value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;


//        LOG_DEBUG("{} bind {} {}", (long long)this, column, value);
//        double * data = (double *)malloc(sizeof(double));
//        *data = value;
//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_DOUBLE;
//        in_pars_[column].buffer = data;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, const std::chrono::system_clock::time_point& value,
                      SqlDateTimeType type) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

#ifdef WT_DEBUG_ENABLED
        if (WT_LOGGING("debug", WT_LOGGER)) {
            using namespace cpp20::date;
            std::ostringstream ss;
            ss.imbue(std::locale::classic());
            ss << value;
            WT_LOG("debug") << WT_LOGGER << ": " << this << " bind " << column << " " << ss.str();
        }
#endif

        params_[column] = datetime();

//        MYSQL_TIME*  ts = (MYSQL_TIME*)malloc(sizeof(MYSQL_TIME));

//        auto day_tp = cpp20::date::floor<cpp20::date::days>(value);
//        cpp20::date::year_month_day date(day_tp);
//        ts->year = static_cast<int>(date.year());
//        ts->month = static_cast<unsigned>(date.month());
//        ts->day = static_cast<unsigned>(date.day());
//        ts->neg = 0;

//        if (type == SqlDateTimeType::Date){
//            //in_pars_[column].buffer_type = MYSQL_TYPE_DATE;
//            ts->hour = 0;
//            ts->minute = 0;
//            ts->second = 0;
//            ts->second_part = 0;

//        } else{
//            //in_pars_[column].buffer_type = MYSQL_TYPE_DATETIME;

//            auto time = cpp20::date::make_time(value - day_tp);
//            ts->hour = time.hours().count();
//            ts->minute = time.minutes().count();
//            ts->second = time.seconds().count();
//            if(conn_.getFractionalSecondsPart() > 0){
//                ts->second_part = std::chrono::duration_cast<std::chrono::microseconds>(time.subseconds()).count();
//            } else
//                ts->second_part = 0;
//        }
//        freeColumn(column);
//        in_pars_[column].buffer = ts;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, const std::chrono::duration<int, std::milli>& value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        params_[column] = value;

//        auto absValue = value < std::chrono::duration<int, std::milli>::zero() ? -value : value;
//        auto hours = cpp20::date::floor<std::chrono::hours>(absValue);
//        auto minutes = cpp20::date::floor<std::chrono::minutes>(absValue) - hours;
//        auto seconds = cpp20::date::floor<std::chrono::seconds>(absValue) - hours - minutes;
//        auto msecs = cpp20::date::floor<std::chrono::milliseconds>(absValue) - hours - minutes - seconds;

//        LOG_DEBUG("{} bind {} {}ms", (long long)this, column, value.count());

//        MYSQL_TIME* ts  = (MYSQL_TIME *)malloc(sizeof(MYSQL_TIME));

//        //IMPL note that there is not really a "duration" type in mysql...
//        //mapping to a datetime
//        in_pars_[column].buffer_type = MYSQL_TYPE_TIME;//MYSQL_TYPE_DATETIME;

//        ts->year = 0;
//        ts->month = 0;
//        ts->day = 0;
//        ts->neg = absValue != value;

//        ts->hour = hours.count();
//        ts->minute = minutes.count();
//        ts->second = seconds.count();

//        if (conn_.getFractionalSecondsPart() > 0)
//            ts->second_part = std::chrono::microseconds(msecs).count();
//        else
//            ts->second_part = 0;

//        freeColumn(column);
//        in_pars_[column].buffer = ts;
//        in_pars_[column].length = nullptr;
//        in_pars_[column].is_null = nullptr;
    }

    virtual void bind(int column, const std::vector<unsigned char>& value) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        LOG_DEBUG("{} bind {}  (blob, size={})", (long long)this, column, value.size());

        params_[column] = value;


//        unsigned long * len = (unsigned long *)malloc(sizeof(unsigned long));

//        char * data;
//        in_pars_[column].buffer_type = MYSQL_TYPE_BLOB;

//        *len = value.size();
//        data = (char *)malloc(*len);
//        if (value.size() > 0) // must not dereference begin() for empty vectors
//            std::memcpy(data, &(*value.begin()), *len);

//        freeColumn(column);
//        in_pars_[column].buffer = data;
//        in_pars_[column].buffer_length = *len;
//        in_pars_[column].length = len;
//        in_pars_[column].is_null = nullptr;

        // FIXME if first null was bound, check here and invalidate the prepared
        // statement if necessary because the type changes
    }

    void bindNull(int column) override
    {
        if (column >= paramCount_)
            throw MySQLException(std::string("Try to bind too much?"));

        LOG_DEBUG("{} bind {} null", (long long)this, column);

        params_[column] = nullptr;

//        freeColumn(column);
//        in_pars_[column].buffer_type = MYSQL_TYPE_NULL;
//        in_pars_[column].is_null = const_cast<WT_MY_BOOL*>(&mysqltrue_);
//        unsigned long * len = (unsigned long *)malloc(sizeof(unsigned long));
//        in_pars_[column].buffer = nullptr;
//        in_pars_[column].buffer_length = 0;
//        in_pars_[column].length = len;
    }

    awaitable<void> prepare()
    {
        error_code ec;
        std::tie(ec, stmt_) = co_await conn_.connection()->async_prepare_statement(sql_, use_nothrow_awaitable);

        if(!ec) {
            paramCount_ = stmt_.num_params();
            params_.resize(paramCount_);
        }
    }

    awaitable<void> execute() override
    {
        if (conn_.showQueries())
            LOG_INFO(fmt::runtime(sql_));

        conn_.checkConnection();

        error_code ec;
        std::tie(ec, stmt_) = co_await conn_.connection()->async_prepare_statement(sql_, use_nothrow_awaitable);


        if(!ec) {
            paramCount_ = stmt_.num_params();
            auto bound = stmt_.bind(params_.begin(), params_.end());

            diagnostics diag;
            auto [ec] = co_await conn_.connection()->async_execute(bound, result_, diag, use_nothrow_awaitable);
            if(!ec){
                if(columnCount_ == 0) { // assume not select
                    affectedRows_ = result_.affected_rows();
                    state_ = NoFirstRow;
                    if (affectedRows_ == 1 ) {
                        lastId_ = result_.last_insert_id();
                    }
                }
                else {
                    irow_ = 0;

                    //but suffer from "commands out of sync" errors with the usage
                    //patterns that Wt::Dbo uses if not called.
                    if( result_.has_value() ) {
                        if(result_.size() > 0){
                            state_ = NextRow;
                        }
                        else {
                            state_ = NoFirstRow; // not sure how/if this can happen
                        }
                    }
                    else {
                        throw MySQLException(std::string("error getting result metadata ")
                                             + std::string(result_.info()));
                    }
                }
            }
            else {
                throw MySQLException(
                    std::string("error executing prepared statement ")+
                    ec.to_string());
            }
        }
        else {
            throw MySQLException(
                std::string("error executing prepared statement ")+
                ec.to_string());
        }

//        if(mysql_stmt_bind_param(stmt_, &in_pars_[0]) == 0){
//            if (mysql_stmt_execute(stmt_) == 0) {
//                if(columnCount_ == 0) { // assume not select
//                    affectedRows_ = mysql_stmt_affected_rows(stmt_);
//                    state_ = NoFirstRow;
//                    if (affectedRows_ == 1 ) {
//                        lastId_ = mysql_stmt_insert_id(stmt_);
//                    }
//                }
//                else {
//                    row_ = 0;

//                    if(result_){
//                        mysql_free_result(result_);
//                    }

//                    result_ = mysql_stmt_result_metadata(stmt_);
//                    mysql_stmt_store_result(stmt_); //possibly not efficient,
//                    //but suffer from "commands out of sync" errors with the usage
//                    //patterns that Wt::Dbo uses if not called.
//                    if( result_ ) {
//                        if(mysql_num_fields(result_) > 0){
//                            state_ = NextRow;
//                        }
//                        else {
//                            state_ = NoFirstRow; // not sure how/if this can happen
//                        }
//                    }
//                    else {
//                        throw MySQLException(std::string("error getting result metadata ")
//                                             + mysql_stmt_error(stmt_));
//                    }
//                }
//            }
//            else {
//                throw MySQLException(
//                    std::string("error executing prepared statement ")+
//                    mysql_stmt_error(stmt_));
//            }
//        }
//        else {
//            throw MySQLException(std::string("error binding parameters")+
//                                 mysql_stmt_error(stmt_));
//        }

        co_return;
    }

    virtual long long insertedId() override
    {
        return lastId_;
    }

    virtual int affectedRowCount() override
    {
        return (int)affectedRows_;
    }

    virtual bool nextRow() override
    {
        //int status = 0;
        switch (state_) {
        case NoFirstRow:
            state_ = Done;
            return false;
        case NextRow:
            //bind the output..
            //bind_output();

            if(irow_ < result_.affected_rows()) {
                row_ = result_.rows().at(irow_);
                irow_++;
                return true;
            }
            else
            {
                state_ = Done;
                return false;
            }

//            if ((status = mysql_stmt_fetch(stmt_)) == 0 ||
//                status == MYSQL_DATA_TRUNCATED) {
//                if (status == MYSQL_DATA_TRUNCATED)
//                    has_truncation_ = true;
//                else
//                    has_truncation_ = false;
//                irow_++;
//                return true;
//            } else {
//                if(status == MYSQL_NO_DATA ) {
//                    lastOutCount_ = mysql_num_fields(result_);
//                    mysql_free_result(result_);
//                    mysql_stmt_free_result(stmt_);
//                    result_ = nullptr;
//                    state_ = Done;
//                    return false;
//                } else {
//                    throw MySQLException(std::string("MySQL: row fetch failure: ") +
//                                         mysql_stmt_error(stmt_));
//                }
//            }
            break;
        case Done:
            throw MySQLException("MySQL: nextRow(): statement already finished");
        }

        return false;
    }

    virtual int columnCount() const override {
        return columnCount_;
    }

    virtual bool getResult(int column, std::string *value, int size) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_string();
        return true;

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        if(*(out_pars_[column].length) > 0){
//            char * str;
//            if (*(out_pars_[column].length) + 1 > out_pars_[column].buffer_length) {
//                free(out_pars_[column].buffer);
//                out_pars_[column].buffer = malloc(*(out_pars_[column].length)+1);
//                out_pars_[column].buffer_length = *(out_pars_[column].length)+1;
//            }
//            mysql_stmt_fetch_column(stmt_,  &out_pars_[column], column, 0);

//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));


//            str = static_cast<char*>( out_pars_[column].buffer);
//            *value = std::string(str, *out_pars_[column].length);

//            LOG_DEBUG("{} result string ", (long long)this, column , value);

//            return true;
//        }
//        else
//            return false;
    }

    virtual bool getResult(int column, short *value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_uint64();
        return true;

//        if (has_truncation_ && *out_pars_[column].error)
//            throw MySQLException("MySQL: getResult(): truncated result for "
//                                 "column " + std::to_string(column));

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        *value = *static_cast<short*>(out_pars_[column].buffer);

//        return true;
    }

    virtual bool getResult(int column, int *value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_int64();
        return true;


//        if (*(out_pars_[column].is_null) == 1)
//            return false;
//        switch (out_pars_[column].buffer_type ){
//        case MYSQL_TYPE_TINY:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = *static_cast<char*>(out_pars_[column].buffer);
//            break;

//        case MYSQL_TYPE_SHORT:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = *static_cast<short*>(out_pars_[column].buffer);
//            break;

//        case MYSQL_TYPE_INT24:
//        case MYSQL_TYPE_LONG:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = *static_cast<int*>(out_pars_[column].buffer);
//            break;

//        case MYSQL_TYPE_LONGLONG:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = (int)*static_cast<long long*>(out_pars_[column].buffer);
//            break;

//        case MYSQL_TYPE_NEWDECIMAL:
//        {
//            std::string strValue;
//            if (!getResult(column, &strValue, 0))
//                return false;

//            try {
//                *value = std::stoi(strValue);
//            } catch (std::exception&) {
//                try {
//                    *value = (int)std::stod(strValue);
//                } catch (std::exception&) {
//                    std::cout << "Error: MYSQL_TYPE_NEWDECIMAL " << strValue
//                              << "could not be casted to int" << std::endl;
//                    return false;
//                }
//            }
//        }
//        break;
//        default:
//            return false;
//        }

//        LOG_DEBUG("{} result int {} {}", (long long)this, column, *value);

//        return true;
    }

    virtual bool getResult(int column, long long *value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_uint64();
        return true;

//        if (has_truncation_ && *out_pars_[column].error)
//            throw MySQLException("MySQL: getResult(): truncated result for column "
//                                 + std::to_string(column));

//        if (*(out_pars_[column].is_null) == 1)
//            return false;
//        switch (out_pars_[column].buffer_type ){
//        case MYSQL_TYPE_LONG:

//            *value = *static_cast<int*>(out_pars_[column].buffer);
//            break;

//        case MYSQL_TYPE_LONGLONG:

//            *value = *static_cast<long long*>(out_pars_[column].buffer);
//            break;

//        default:

//            throw MySQLException("MySQL: getResult(long long): unknown type: "
//                                 + std::to_string(out_pars_[column].buffer_type));
//            break;
//        }

//        LOG_DEBUG("{} result long long {} {}", (long long)this, column, *value);

//        return true;
    }

    virtual bool getResult(int column, float *value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_float();
        return true;

//        if (has_truncation_ && *out_pars_[column].error)
//            throw MySQLException("MySQL: getResult(): truncated result for column "
//                                 + std::to_string(column));

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        *value = *static_cast<float*>(out_pars_[column].buffer);

//        LOG_DEBUG("{} result float {} {}", (long long)this, column, *value);

//        return true;
    }

    virtual bool getResult(int column, double *value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = c.as_double();
        return true;

//        if (*(out_pars_[column].is_null) == 1)
//            return false;
//        switch (out_pars_[column].buffer_type ){
//        case MYSQL_TYPE_DOUBLE:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = *static_cast<double*>(out_pars_[column].buffer);
//            break;
//        case MYSQL_TYPE_FLOAT:
//            if (has_truncation_ && *out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for "
//                                     "column " + std::to_string(column));
//            *value = *static_cast<float*>(out_pars_[column].buffer);
//            break;
//        case MYSQL_TYPE_NEWDECIMAL:
//        {
//            std::string strValue;
//            if (!getResult(column, &strValue, 0))
//                return false;

//            try {
//                *value = std::stod(strValue);
//            } catch(std::exception& e) {
//                std::cout << "Error: MYSQL_TYPE_NEWDECIMAL " << strValue
//                          << "could not be casted to double" << std::endl;
//                return false;
//            }
//        }
//        break;
//        default:
//            return false;
//        }

//        LOG_DEBUG("{} result double {} {}", (long long)this, column, *value);

//        return true;
    }

    virtual bool getResult(int column, std::chrono::system_clock::time_point *value,
                           SqlDateTimeType type) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        auto date = c.as_datetime();
        *value = date.as_time_point();
        return true;


//        if (has_truncation_ && *out_pars_[column].error)
//            throw MySQLException("MySQL: getResult(): truncated result for column "
//                                 + std::to_string(column));

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        MYSQL_TIME* ts = static_cast<MYSQL_TIME*>(out_pars_[column].buffer);

//        auto day_tp = cpp20::date::sys_days(cpp20::date::year(ts->year) / ts->month / ts->day);
//        if (type == SqlDateTimeType::Date){
//            *value = day_tp;
//        } else{
//            auto time = std::chrono::hours(ts->hour) +
//                        std::chrono::minutes(ts->minute) +
//                        std::chrono::seconds(ts->second) +
//                        std::chrono::microseconds(ts->second_part);
//            *value = day_tp + std::chrono::duration_cast<std::chrono::system_clock::time_point::duration>(time);
//        }


//#ifdef WT_DEBUG_ENABLED
//        if (WT_LOGGING("debug", WT_LOGGER)) {
//            using namespace cpp20::date;
//            std::ostringstream ss;
//            ss.imbue(std::locale::classic());
//            ss << *value;
//            WT_LOG("debug") << WT_LOGGER << ": " << this << " result time " << column << " " << ss.str();
//        }
//#endif

//        return true;
    }

    virtual bool getResult(int column, std::chrono::duration<int, std::milli>* value) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        *value = std::chrono::duration_cast<std::chrono::milliseconds>(c.as_time());
        return true;


//        if (has_truncation_ && *out_pars_[column].error)
//            throw MySQLException("MySQL: getResult(): truncated result for column "
//                                 + std::to_string(column));

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        MYSQL_TIME* ts = static_cast<MYSQL_TIME*>(out_pars_[column].buffer);
//        auto msecs = cpp20::date::floor<std::chrono::milliseconds>(
//            std::chrono::microseconds(ts->second_part));
//        auto absValue = std::chrono::hours(ts->hour) + std::chrono::minutes(ts->minute)
//                        + std::chrono::seconds(ts->second) + msecs;
//        *value = ts->neg ? -absValue : absValue;

//        LOG_DEBUG("{} result time {} {}ms", (long long)this, column, value->count());

//        return true;
    }

    virtual bool getResult(int column, std::vector<unsigned char> *value,
                           int size) override
    {
        if(row_.empty() || row_.at(column).is_null())
            return false;

        auto c = row_.at(column);
        auto blob = c.as_blob();
        (*value).assign(blob.begin(), blob.end());
        return true;

//        if (*(out_pars_[column].is_null) == 1)
//            return false;

//        if(*(out_pars_[column].length) > 0){
//            if (*(out_pars_[column].length) > out_pars_[column].buffer_length) {
//                free(out_pars_[column].buffer);
//                out_pars_[column].buffer = malloc(*(out_pars_[column].length));
//                out_pars_[column].buffer_length = *(out_pars_[column].length);
//            }
//            mysql_stmt_fetch_column(stmt_,  &out_pars_[column], column, 0);

//            if (*out_pars_[column].error)
//                throw MySQLException("MySQL: getResult(): truncated result for column "
//                                     + std::to_string(column));


//            std::size_t vlength = *(out_pars_[column].length);
//            unsigned char *v =
//                static_cast<unsigned char*>(out_pars_[column].buffer);

//            value->resize(vlength);
//            std::copy(v, v + vlength, value->begin());

//            LOG_DEBUG("{} result blob {} (blob, size = {})", (long long)this, column, vlength);

//            return true;
//        }
//        else
//            return false;
    }

    virtual std::string sql() const override {
        return sql_;
    }

private:
    MySQL& conn_;
    //tcp_ssl_connection* sqlconn_;
    std::string sql_;
    char name_[64];
    bool has_truncation_;
    results result_;
    row_view row_;
    statement stmt_;
    std::vector<field> params_;
    //MYSQL_RES *result_;
    //MYSQL_STMT* stmt_;
    //MYSQL_BIND* in_pars_;
    //MYSQL_BIND* out_pars_;
    int paramCount_;
//    WT_MY_BOOL* errors_;
//    WT_MY_BOOL* is_nulls_;
    unsigned int lastOutCount_;
    // true value to use because mysql specifies that pointer to the boolean
    // is passed in many cases....
//    static const WT_MY_BOOL mysqltrue_;
    enum { NoFirstRow, NextRow, Done } state_;
    long long lastId_, irow_, affectedRows_;
    int columnCount_;

//    void bind_output() {
//        if (!out_pars_) {
//            out_pars_ =(MYSQL_BIND *)malloc(
//                mysql_num_fields(result_) * sizeof(MYSQL_BIND));
//            std::memset(out_pars_, 0,
//                        mysql_num_fields(result_) * sizeof(MYSQL_BIND));
//            errors_ = new WT_MY_BOOL[mysql_num_fields(result_)];
//            is_nulls_ = new WT_MY_BOOL[mysql_num_fields(result_)];
//            for(unsigned int i = 0; i < mysql_num_fields(result_); ++i){
//                MYSQL_FIELD* field = mysql_fetch_field_direct(result_, i);
//                out_pars_[i].buffer_type = field->type;
//                out_pars_[i].error = &errors_[i];
//                out_pars_[i].is_null = &is_nulls_[i];
//                switch(field->type){
//                case MYSQL_TYPE_TINY:
//                    out_pars_[i].buffer = malloc(1);
//                    out_pars_[i].buffer_length = 1;
//                    break;

//                case MYSQL_TYPE_SHORT:
//                    out_pars_[i].buffer = malloc(sizeof(short));
//                    out_pars_[i].buffer_length = sizeof(short);
//                    break;

//                case MYSQL_TYPE_LONG:
//                    out_pars_[i].buffer = malloc(sizeof(int));
//                    out_pars_[i].buffer_length = sizeof(int);
//                    break;
//                case MYSQL_TYPE_FLOAT:
//                    out_pars_[i].buffer = malloc(sizeof(float));
//                    out_pars_[i].buffer_length = sizeof(float);
//                    break;

//                case MYSQL_TYPE_LONGLONG:
//                    out_pars_[i].buffer = malloc(sizeof(long long));
//                    out_pars_[i].buffer_length = sizeof(long long);
//                    break;
//                case MYSQL_TYPE_DOUBLE:
//                    out_pars_[i].buffer = malloc(sizeof(double));
//                    out_pars_[i].buffer_length = sizeof(double);
//                    break;

//                case MYSQL_TYPE_TIME:
//                case MYSQL_TYPE_DATE:
//                case MYSQL_TYPE_DATETIME:
//                case MYSQL_TYPE_TIMESTAMP:
//                    out_pars_[i].buffer = malloc(sizeof(MYSQL_TIME));
//                    out_pars_[i].buffer_length = sizeof(MYSQL_TIME);
//                    break;

//                case MYSQL_TYPE_NEWDECIMAL: // newdecimal is stored as string.
//                case MYSQL_TYPE_STRING:
//                case MYSQL_TYPE_VAR_STRING:
//                case MYSQL_TYPE_BLOB:
//                    out_pars_[i].buffer = malloc(256);
//                    out_pars_[i].buffer_length = 256; // Reserve 256 bytes, if the content is longer, it will be reallocated later
//                    //http://dev.mysql.com/doc/refman/5.0/en/mysql-stmt-fetch.html
//                    break;
//                default:
//                    LOG_ERROR("MySQL Backend Programming Error: unknown type {}", (int)field->type);
//                }
//                out_pars_[i].buffer_type = field->type;
//                out_pars_[i].length = (unsigned long *) malloc(sizeof(unsigned long));
//            }
//        }
//        for (unsigned int i = 0; i < mysql_num_fields(result_); ++i) {
//            // Clear error for MariaDB Connector/C (see issue #6407)
//            *out_pars_[i].error = 0;
//        }
//        mysql_stmt_bind_result(stmt_, out_pars_);
//    }

//    void freeColumn(int column)
//    {
//        if(in_pars_[column].length != nullptr ) {
//            free(in_pars_[column].length);
//            in_pars_[column].length = nullptr;
//        }

//        if(in_pars_[column].buffer != nullptr ) {
//            free(in_pars_[column].buffer);
//            in_pars_[column].buffer = nullptr;
//        }
//    }

//    void free_outpars(){

//        unsigned int count;
//        if(!result_){
//            count = lastOutCount_;
//        }else
//            count = mysql_num_fields(result_);

//        for (unsigned int i = 0; i < count; ++i){
//            if(out_pars_[i].buffer != nullptr)free(out_pars_[i].buffer);
//            if(out_pars_[i].length != nullptr)free(out_pars_[i].length);

//        }
//        free(out_pars_);
//        out_pars_ = nullptr;
//    }

};

}
}
}

