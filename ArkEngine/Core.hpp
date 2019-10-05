#pragma once

#include <deque>
#include <iostream>
#include <vector>
#include "Util.hpp"

//#define VENGINE_BUILD_DLL 0
//
//#if VENGINE_BUILD_DLL
//	#define ARK_ENGINE_API __declspec(dllexport)
//#else
//	#define ARK_ENGINE_API __declspec(dllimport)
//#endif

#define ARK_ENGINE_API
#define ARK_API

// used internally
static inline constexpr int ArkInvalidIndex = -1;
static inline constexpr int ArkInvalidID = -1;


//#define ARK_ASSERT(...) std::assert

