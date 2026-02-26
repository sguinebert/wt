#ifndef WHTTPDLLDEFS_H_
#define WHTTPDLLDEFS_H_

// Export macro for libwhttp standalone library

#if defined(WIN32) || defined(_WIN32)
  #ifdef whttp_EXPORTS
    #define WHTTP_API __declspec(dllexport)
  #else
    #ifdef WHTTP_STATIC
      #define WHTTP_API
    #else
      #define WHTTP_API __declspec(dllimport)
    #endif
  #endif
#else
  #ifdef whttp_EXPORTS
    #define WHTTP_API __attribute__((visibility("default")))
  #else
    #define WHTTP_API
  #endif
#endif

#endif // WHTTPDLLDEFS_H_
