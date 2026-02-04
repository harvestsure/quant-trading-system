#pragma once

#ifdef _MSC_VER
    #define QTS_DECL_EXPORT __declspec(dllexport)
    #define QTS_DECL_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    #define QTS_DECL_EXPORT __attribute__((visibility("default")))
    #define QTS_DECL_IMPORT __attribute__((visibility("default")))
#else
    #define QTS_DECL_EXPORT
    #define QTS_DECL_IMPORT
#endif