#pragma once

#define FL_INTERNAL_ASSERT_IMPL(log, task, check, msg, ...) { if (!(check)) { ##log(msg, __VA_ARGS__); ##task; } }
#define FL_INTERNAL_ASSERT_MSG(log, task, check, ...) FL_INTERNAL_ASSERT_IMPL(log, task, check, "Assertion failed: %s", __VA_ARGS__)
#define FL_INTERNAL_ASSERT_MSGARGS(log, task, check, msg, ...) FL_INTERNAL_ASSERT_IMPL(log, task, check, "Assertion failed: " ##msg, __VA_ARGS__)
#define FL_INTERNAL_ASSERT_NOMSG(log, task, check) FL_INTERNAL_ASSERT_IMPL(log, task, check, "Assertion '%s' failed at %s:%d", #check, std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define FL_INTERNAL_ASSERT_FIND(_1, _2, _3, macro, ...) macro
#define FL_INTERNAL_ASSERT_PICK(...) FL_EXPAND_ARGS( FL_INTERNAL_ASSERT_FIND(__VA_ARGS__, FL_INTERNAL_ASSERT_MSGARGS, FL_INTERNAL_ASSERT_MSG, FL_INTERNAL_ASSERT_NOMSG) )

#ifdef FL_ENABLE_ASSERTS
    #define FL_ASSERT(...) FL_EXPAND_ARGS( FL_INTERNAL_ASSERT_PICK(__VA_ARGS__)(FL_LOG_ERROR, FL_DEBUGBREAK(), __VA_ARGS__) )
#else
    #define FL_ASSERT(...)
#endif

#define FL_CRASH_ASSERT(...) FL_EXPAND_ARGS( FL_INTERNAL_ASSERT_PICK(__VA_ARGS__)(FL_LOG_CRITICAL, *(int *)0 = 0, __VA_ARGS__) )