#include "ModuleSegments.h"

#include <Windows.h>
#include <DbgHelp.h>

void* Hooking::GetModuleSection(HMODULE module, const char* sectionName)
{
	IMAGE_NT_HEADERS* ntHeader = ImageNtHeader(module);

	IMAGE_SECTION_HEADER* sectionHeaderBase = (IMAGE_SECTION_HEADER*)(ntHeader + 1);

	for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER& sectionHeader = sectionHeaderBase[i];

		if (strncmp((const char*)sectionHeader.Name, sectionName, std::size(sectionHeader.Name)))
			continue;

		return (void*)((std::byte*)module + sectionHeader.VirtualAddress);
	}

	return nullptr;
}
