#pragma once

#include <minwindef.h>

namespace Hooking
{
	void* GetModuleSection(HMODULE module, const char* sectionName);
}