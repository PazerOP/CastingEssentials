#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

#include <forward_list>
#include <list>
#include <map>
#include <vector>

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
	ConCommand ce_itemschema_print;
	ConCommand ce_itemschema_reload;

	void PrintAliases();

	void LevelInit() override;
	void LoadItemSchema();

	static void GetFirstPrefabType(const char* prefabsGroup, char* prefabOut, size_t maxOutSize);
	int GetFirstItemUsingPrefab(KeyValues* items, const char* prefabName);

	enum class ItemEquipSlot
	{
		Primary,
		Secondary,
		Melee,
		PDA,
		PDA2,
		Building,

		Action,
		Utility,
		Taunt,

		Head,
		Misc,

		Quest,

		Unspecified,
	};

	struct ItemProperties
	{
		std::string m_Name;
		std::vector<ItemProperties*> m_Prefabs;
		std::vector<std::string> m_PrefabNames;
		std::string m_XifierClassRemap;
		std::string m_CraftClass;
		std::string m_PlayerModel;
		std::string m_IconName;
		ItemEquipSlot m_EquipSlot;

		const std::string& GetXifierClassRemap() const;
		const std::string& GetCraftClass() const;
		const std::string& GetPlayerModel() const;
		const std::string& GetIconName() const;
		bool HasPrefab(const char* prefabName) const;
		ItemEquipSlot GetEquipSlot() const;
	};

	void LoadItemProperties(KeyValues* propsIn, ItemProperties& propsOut);

	struct ItemPrefab : public ItemProperties
	{
	};

	struct ItemEntry : public ItemProperties
	{
		unsigned int m_ID;
	};

	struct ItemRemap
	{
		std::string m_FromModel;
		unsigned int m_FromID;
		bool FromMatch(const ItemEntry& entry) const;

		unsigned int m_ToID;

		bool IsUnmap() const { return m_FromModel.empty() && m_FromID == m_ToID; }
	};

	std::vector<ItemRemap> m_Remaps;
	void LoadOverrides(const std::forward_list<ItemEntry>& items);
	bool HasOverride() const;

	ItemPrefab* FindPrefab(const char* prefabName);
	void LoadPrefabs(KeyValues* prefabs);
	void LinkPrefabs();

	bool m_PrefabsLinked = false;
	std::forward_list<ItemPrefab> m_Prefabs;

	void LoadItems(KeyValues* inItems, std::forward_list<ItemEntry>& outItems);
	void LoadUniqueItems(std::forward_list<ItemEntry>& items);

	struct UniqueItemEntry
	{
		// If true, this weapon is not accurately represented by its default
		// ingame model. Don't automatically map children to this
		// UniqueItemEntry just because they match the model name.
		bool m_IsForceUnmapped = false;
		ItemEntry* m_MasterEntry = nullptr;
		std::vector<ItemRemap*> m_Remaps;
		std::forward_list<ItemEntry> m_Entries;

		unsigned int GetLowestID() const;
	};

	UniqueItemEntry* FindItemInUniqueItems(const std::string& partModelName);

	void ApplyUniqueItemOverrides();
	std::forward_list<UniqueItemEntry> m_UniqueItems;

	std::map<int, int> m_Mappings;
};