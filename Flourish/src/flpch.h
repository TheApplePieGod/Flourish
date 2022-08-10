#pragma once

#include "Flourish/Core/PlatformDetection.h"

#ifdef FL_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
#endif

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <thread>
#include <functional>
#include <filesystem>
#include <optional>

#include "Flourish/Core/Base.h"
#include "Flourish/Core/Log.h"
#include "Flourish/Core/Assert.h"

#ifdef FL_PLATFORM_WINDOWS
	#include <Windows.h>
#endif
