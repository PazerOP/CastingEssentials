#pragma once

#include "PluginBase/Modules.h"

#include <map>

class ConCommand;
class ConVar;
class IConVar;
class KeyValues;

class ItemSchema : public Module<ItemSchema>
{
public:
	ItemSchema();

	static bool CheckDependencies();

	// Converts a "specialized" version of an item (a botkiller medigun, for example) into its
	// "base" item (a stock medigun)
	int GetBaseItemID(int specializedID) const;

private:
	void LoadItemSchema();

	static void GetFirstPrefabType(const char* prefabsGroup, char* prefabOut, size_t maxOutSize);
	int GetFirstItemUsingPrefab(KeyValues* items, const char* prefabName);

	std::map<int, int> m_Mappings;
};