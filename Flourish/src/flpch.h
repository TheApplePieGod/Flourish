#pragma once

#include "Flourish/Core/PlatformDetection.h"

#ifdef FL_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
#endif

#ifdef FL_USE_TRACY
    #define TRACY_ENABLE
    #include "tracy/Tracy.hpp"
    #define FL_PROFILE_FUNCTION() ZoneScoped
#else
    #define FL_PROFILE_FUNCTION()
#endif

#if defined(FL_HAS_AFTERMATH) && defined(FL_DEBUG)
    #define FL_USE_AFTERMATH
#endif

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <functional>
#include <filesystem>
#include <optional>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <filesystem>
#include <fstream>

#include "Flourish/Core/Base.h"
#include "Flourish/Core/Log.h"
#include "Flourish/Core/Assert.h"

#ifdef FL_PLATFORM_WINDOWS
	#include <Windows.h>
#elif defined(FL_PLATFORM_MACOS)
    #include <dlfcn.h>
#elif defined(FL_PLATFORM_ANDROID)
    #include <android/native_activity.h>
#endif
