#pragma once

#include "PluginBase/Modules.h"

#include <vector>

class ConCommand;
class ConVar;
class IConVar;
class KeyValues;

class ItemSchema : public Module
{
public:
	ItemSchema();

	static ItemSchema* GetModule() { return Modules().GetModule<ItemSchema>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<ItemSchema>().c_str(); }

	static bool CheckDependencies();

	// Converts a "specialized" version of an item (a botkiller medigun, for example) into its
	// "base" item (a stock medigun)
	int GetBaseItemID(int specializedID) const;

private:
	void LoadItemSchema();

	static void GetFirstPrefabType(const char* prefabsGroup, char* prefabOut, size_t maxOutSize);
	int GetFirstItemUsingPrefab(KeyValues* items, const char* prefabName);

	// O(1) lookup
	std::unique_ptr<int> m_Mappings;
	size_t m_MappingsSize;
};