#include "MSSQLStatement.h"

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
