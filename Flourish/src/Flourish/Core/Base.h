#pragma once

#ifdef FL_DEBUG
	#if defined(_MSC_VER)
		#define FL_DEBUGBREAK() __debugbreak()
	#elif defined(__clang__)
		#if __has_builtin(__builtin_debugtrap)
			#define FL_DEBUGBREAK() __builtin_debugtrap()
		#else
			#define FL_DEBUGBREAK() __builtin_trap()
		#endif
	#elif defined(__GNUC__)
		#define FL_DEBUGBREAK() __builtin_trap()
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define FL_ENABLE_ASSERTS
#else
	#define FL_DEBUGBREAK()
#endif

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using b32 = int32_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float_t;
using d64 = double_t;
using uptr = intptr_t;
using wchar = wchar_t;
using char8 = char;
using char16 = char16_t;

#define FL_EXPAND_ARGS(args) args
#define FL_BIT(x) (1 << x)
#define FL_PLACEMENT_NEW(ptr, type, ...) new (ptr) type(__VA_ARGS__)
#define FL_ALIGN_UP(val, align) ((val + (align - 1)) & ~(align - 1))
