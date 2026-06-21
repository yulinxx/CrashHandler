#ifndef CRASHHANDLER_API_H
#define CRASHHANDLER_API_H

#if defined(_WIN32) || defined(_WIN64)
#ifdef CRASHHANDLER_EXPORTS
#define CRASHHANDLER_API __declspec(dllexport)
#else
#define CRASHHANDLER_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifdef CRASHHANDLER_EXPORTS
#define CRASHHANDLER_API __attribute__((visibility("default")))
#else
#define CRASHHANDLER_API
#endif
#elif defined(__APPLE__)
#ifdef CRASHHANDLER_EXPORTS
#define CRASHHANDLER_API __attribute__((visibility("default")))
#else
#define CRASHHANDLER_API
#endif
#else
#define CRASHHANDLER_API
#endif

#endif
