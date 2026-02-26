# Modern WtFindBoost.cmake
# - Expose (compat): BOOST_INCLUDE_DIRS, BOOST_LIB_DIRS,
#                    BOOST_WT_FOUND, BOOST_WT_MT_FOUND,
#                    BOOST_WTHTTP_FOUND, BOOST_WTHTTP_MT_FOUND,
#                    BOOST_SUPPORT_LIBRARIES, BOOST_WT_LIBRARIES
#                    BOOST_FS_LIB, BOOST_PO_LIB, BOOST_SYSTEM_LIB, BOOST_THREAD_LIB
# - Prefer imported targets (Boost::<comp>)

# --- inputs / knobs ---------------------------------------------------------
# BOOST_PREFIX       : like before; if set and Boost_ROOT not set, we honor it
# BOOST_DYNAMIC(BOOL): ON -> dynamic, OFF -> static (Windows)
# MULTI_THREADED     : ON/OFF (as in your tree)
# BOOST_MIN_VERSION  : minimum acceptable Boost (default 1.70)

# ---------------------------------------------------------------------------
set(BOOST_WT_FOUND        FALSE)
set(BOOST_WT_MT_FOUND     FALSE)
set(BOOST_WTHTTP_FOUND    FALSE)
set(BOOST_WTHTTP_MT_FOUND FALSE)
set(BOOST_SUPPORT_LIBRARIES "")

# Map legacy BOOST_PREFIX to Boost_ROOT if needed
if(DEFINED BOOST_PREFIX AND NOT DEFINED Boost_ROOT)
  set(Boost_ROOT "${BOOST_PREFIX}")
endif()

# Choose static vs dynamic (mostly relevant on Windows)
if(WIN32)
  if(DEFINED BOOST_DYNAMIC)
    if(BOOST_DYNAMIC)
      set(Boost_USE_STATIC_LIBS OFF)
    else()
      set(Boost_USE_STATIC_LIBS ON)
    endif()
  endif()
endif()

# Always prefer multithreaded libs when the legacy flag is ON
if(DEFINED MULTI_THREADED)
  if(MULTI_THREADED)
    set(Boost_USE_MULTITHREADED ON)
  else()
    set(Boost_USE_MULTITHREADED OFF)
  endif()
else()
  set(Boost_USE_MULTITHREADED ON)
  set(MULTI_THREADED ON)
endif()

set(Boost_USE_STATIC_RUNTIME OFF)

# Base components
set(_WT_REQ_COMPONENTS program_options filesystem system)
if(MULTI_THREADED)
  list(APPEND _WT_REQ_COMPONENTS thread)
endif()

# Optional component (available since Boost 1.81)
set(_WT_OPT_COMPONENT url)

# We’ll try config mode first (Boost ≥ 1.70 provides BoostConfig.cmake if built with b2 cmake)
set(_WT_MIN_VER "${BOOST_MIN_VERSION}")
if(NOT _WT_MIN_VER)
  set(_WT_MIN_VER "1.70")
endif()

# Try CONFIG with a version **range** to favor the newest found
# Requires CMake ≥ 3.19. If your CMake is older, replace the following call by:
#   find_package(Boost ${_WT_MIN_VER} CONFIG QUIET COMPONENTS ${_WT_REQ_COMPONENTS})
find_package(Boost ${_WT_MIN_VER}...<2.0 CONFIG QUIET COMPONENTS ${_WT_REQ_COMPONENTS})

# Fallback to module mode (FindBoost) if config mode not available
if(NOT Boost_FOUND)
  set(Boost_NO_BOOST_CMAKE ON)
  find_package(Boost ${_WT_MIN_VER} QUIET COMPONENTS ${_WT_REQ_COMPONENTS})
endif()

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost (>=${_WT_MIN_VER}) not found. "
                      "Hint: set Boost_ROOT or BOOST_PREFIX to your install.")
endif()

# Try optional Boost.URL if version is recent
set(_WT_HAVE_URL FALSE)
if(DEFINED Boost_VERSION AND Boost_VERSION VERSION_GREATER_EQUAL 1.81.0)
  # Try to load URL target quietly; ignore if missing
  find_package(Boost QUIET CONFIG COMPONENTS ${_WT_OPT_COMPONENT})
  if(TARGET Boost::${_WT_OPT_COMPONENT})
    set(_WT_HAVE_URL TRUE)
  endif()
endif()

# --- Map modern results to your legacy variables ---------------------------
# include dirs
set(BOOST_INCLUDE_DIRS "${Boost_INCLUDE_DIRS}")

# library dirs (may be empty in CONFIG mode; imported targets don’t need it)
if(DEFINED Boost_LIBRARY_DIRS)
  set(BOOST_LIB_DIRS "${Boost_LIBRARY_DIRS}")
else()
  set(BOOST_LIB_DIRS "")
endif()

# legacy single-library variables as imported targets
set(BOOST_FS_LIB      Boost::filesystem)
set(BOOST_PO_LIB      Boost::program_options)
set(BOOST_SYSTEM_LIB  Boost::system)
if(MULTI_THREADED)
  set(BOOST_THREAD_LIB Boost::thread)
else()
  set(BOOST_THREAD_LIB "")
endif()

# libraries to link with Wt core / wthttp (keep original intent)
set(BOOST_WT_LIBRARIES
  Boost::thread
  Boost::system
  Boost::filesystem
)
set(BOOST_WTHTTP_LIBRARIES
  Boost::thread
  Boost::program_options
  Boost::system
  Boost::filesystem
)

# headers target if present
if(TARGET Boost::headers)
  list(APPEND BOOST_WT_LIBRARIES Boost::headers)
  list(APPEND BOOST_WTHTTP_LIBRARIES Boost::headers)
endif()

# optional URL
if(_WT_HAVE_URL)
  list(APPEND BOOST_WT_LIBRARIES    Boost::url)
  list(APPEND BOOST_WTHTTP_LIBRARIES Boost::url)
endif()

# flags
set(BOOST_WT_FOUND        TRUE)
set(BOOST_WTHTTP_FOUND    TRUE)
set(BOOST_WT_MT_FOUND     ${MULTI_THREADED})
set(BOOST_WTHTTP_MT_FOUND ${MULTI_THREADED})

# Cosmetic: show the version we actually picked
if(DEFINED Boost_VERSION_STRING)
  message(STATUS "Boost found (CONFIG): ${Boost_VERSION_STRING}")
elseif(DEFINED Boost_VERSION)
  message(STATUS "Boost found (MODULE): ${Boost_VERSION}")
endif()
