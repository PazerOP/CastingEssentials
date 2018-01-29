#include "ItemSchema.h"
#include "PluginBase/Interfaces.h"

#include <convar.h>
#include <filesystem.h>
#include <KeyValues.h>
#include <utlbuffer.h>

#include <algorithm>
#include <vector>

// dumb macro names
#undef min
#undef max

ItemSchema::ItemSchema()
{
	ce_itemschema_print.reset(new ConCommand("ce_itemschema_print", [](const auto&) { GetModule()->PrintAliases(); }, "Prints the current state of the item schema module."));
	ce_itemschema_reload.reset(new ConCommand("ce_itemschema_reload", [](const auto&) { GetModule()->LoadItemSchema(); }, "Reloads the item schema module."));
}

void ItemSchema::PrintAliases()
{
	bool anyPrinted = false;
	for (const auto& unique : m_UniqueItems)
	{
		if (std::distance(unique.m_Entries.begin(), unique.m_Entries.end()) < 2)
			continue;

		Color randColor(RandomInt(128, 255), RandomInt(128, 255), RandomInt(128, 255), 255);

		ConColorMsg(randColor, "%u: Master Item: %s (id %u) %s\n", unique.GetLowestID(), unique.m_MasterEntry->m_Name.c_str(), unique.m_MasterEntry->m_ID,
			unique.m_MasterEntry->GetPlayerModel().c_str());

		for (const auto& mapped : unique.m_Entries)
		{
			if (&mapped == unique.m_MasterEntry)
				continue;

			ConColorMsg(randColor, "\tMapped item: %s (id %u) %s\n", mapped.m_Name.c_str(), mapped.m_ID, mapped.GetPlayerModel().c_str());
		}

		anyPrinted = true;
	}

	if (!anyPrinted)
		Msg("No items currently loaded by the item schema module.\n");

	Msg("Unaliased weapons:\n");
	for (const auto& unique : m_UniqueItems)
	{
		if (std::distance(unique.m_Entries.begin(), unique.m_Entries.end()) > 1)
			continue;

		Msg("\tUnique Item: %s (id %u) %s\n", unique.m_MasterEntry->m_Name.c_str(), unique.m_MasterEntry->m_ID,
			unique.m_MasterEntry->GetPlayerModel().c_str());
	}
}

void ItemSchema::LevelInitPreEntity()
{
	LoadItemSchema();
}

bool ItemSchema::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetFileSystem())
	{
		PluginWarning("Required interface IFileSystem for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

int ItemSchema::GetBaseItemID(int specializedID) const
{
	if (specializedID < 0)
		return specializedID;

	auto found = m_Mappings.find(specializedID);

	return found == m_Mappings.end() ? specializedID : found->second;
}

#pragma warning(push)
#pragma warning(disable : 4706)	// Assignment within conditional expression
void ItemSchema::LoadItemSchema()
{
	m_PrefabsLinked = false;

	KeyValues::AutoDelete basekv(new KeyValues("none"));

	constexpr const char* FILENAME = "scripts/items/items_game.txt";
	if (!basekv->LoadFromFile(Interfaces::GetFileSystem(), FILENAME))
	{
		PluginWarning("Failed to load %s for module %s!\n", FILENAME, GetModuleName());
		return;
	}

	LoadPrefabs(basekv->FindKey("prefabs"));
	LinkPrefabs();

	KeyValues* inItems = basekv->FindKey("items");
	std::forward_list<ItemEntry> outItems;
	LoadItems(inItems, outItems);

	const size_t itemsCount = std::distance(outItems.begin(), outItems.end());

	LoadOverrides(outItems);
	LoadUniqueItems(outItems);

	PluginMsg("Generated item schema mappings: %zu items down to %zu.\n",
		itemsCount, std::distance(m_UniqueItems.begin(), m_UniqueItems.end()));

	// Build fast lookup map with just the IDs
	m_Mappings.clear();
	for (const auto& uniqueItem : m_UniqueItems)
	{
		const auto lowest = uniqueItem.GetLowestID();
		for (const auto& mapping : uniqueItem.m_Entries)
		{
			if (mapping.m_ID == lowest)
				continue;	// Don't add a pointless alias to ourselves

			m_Mappings[mapping.m_ID] = lowest;
		}
	}

	return;
}
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4706)	// Assignment within conditional expression
void ItemSchema::LoadPrefabs(KeyValues* prefabs)
{
	m_Prefabs.clear();

	KeyValues* prefab = prefabs->GetFirstTrueSubKey();

	do
	{
		ItemPrefab newPrefab;
		LoadItemProperties(prefab, newPrefab);

		newPrefab.m_Name = prefab->GetName();

		Assert(!newPrefab.m_Name.empty());

		m_Prefabs.emplace_front(std::move(newPrefab));

	} while (prefab = prefab->GetNextTrueSubKey());
}
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4706)	// Assignment within conditional expression
void ItemSchema::LoadItems(KeyValues* inItems, std::forward_list<ItemEntry>& outItems)
{
	KeyValues* item = inItems->GetFirstTrueSubKey();

	do
	{
		const char* name = item->GetName();
		if (!isdigit(name[0]))
			continue;

		ItemEntry newItem;
		LoadItemProperties(item, newItem);

		const auto& craftClass = newItem.GetCraftClass();

		if (newItem.GetEquipSlot() != ItemEquipSlot::Primary &&
			newItem.GetEquipSlot() != ItemEquipSlot::Secondary &&
			newItem.GetEquipSlot() != ItemEquipSlot::Melee &&
			newItem.GetEquipSlot() != ItemEquipSlot::PDA &&
			newItem.GetEquipSlot() != ItemEquipSlot::PDA2 &&
			newItem.GetEquipSlot() != ItemEquipSlot::Building ||
			!stricmp(craftClass.c_str(), "craft_token"))
		{
			continue;	// We only care about weapons for now
		}

		newItem.m_ID = atoi(name);
		newItem.m_Name = item->GetString("name");

		outItems.emplace_front(std::move(newItem));

	} while (item = item->GetNextTrueSubKey());
}
#pragma warning(pop)

void ItemSchema::LinkPrefabs()
{
	for (auto& prefab : m_Prefabs)
	{
		for (auto& prefabName : prefab.m_PrefabNames)
			prefab.m_Prefabs.push_back(FindPrefab(prefabName.c_str()));
	}

	m_PrefabsLinked = true;
}

void ItemSchema::GetFirstPrefabType(const char* prefabsGroup, char* prefabOut, size_t maxOutSize)
{
	while (maxOutSize-- && *prefabsGroup && !isspace(*prefabsGroup))
		*prefabOut++ = *prefabsGroup++;

	Assert(maxOutSize != (size_t)-1 && maxOutSize > 0);

	*prefabOut = '\0';
}

ItemSchema::ItemPrefab* ItemSchema::FindPrefab(const char* prefabName)
{
	size_t i = 0;
	std::string* wtf[512];

	for (auto& prefab : m_Prefabs)
	{
		wtf[i++] = &prefab.m_Name;

		if (!stricmp(prefab.m_Name.c_str(), prefabName))
			return &prefab;
	}

	assert(false);
	PluginWarning("Unable to find prefab \"%s\"!", prefabName);
	return nullptr;
}

#pragma warning(push)
#pragma warning(disable : 4706)	// Assignment within conditional expression
int ItemSchema::GetFirstItemUsingPrefab(KeyValues* items, const char* prefabName)
{
	Assert(items);
	Assert(prefabName && prefabName[0]);

	const size_t prefabNameLength = strlen(prefabName);

	KeyValues* kv = items->GetFirstTrueSubKey();
	if (!kv)
	{
		Assert(false);
		return -1;
	}

	do
	{
		const char* prefabTypes = kv->GetString("prefab");
		bool match = false;
		while (*prefabTypes)
		{
			const size_t wordLength = strcspn(prefabTypes, WHITESPACE_CHARS);
			if (wordLength == prefabNameLength)
			{
				if (!strnicmp(prefabTypes, prefabName, prefabNameLength))
				{
					match = true;
					break;
				}
			}

			prefabTypes += wordLength;
			prefabTypes += strspn(prefabTypes, WHITESPACE_CHARS);
		}

		if (!match)
			continue;

		const char* name = kv->GetName();

		return atoi(name);

	} while (kv = kv->GetNextTrueSubKey());

	return -1;
}
#pragma warning(pop)

void ItemSchema::LoadItemProperties(KeyValues* propsIn, ItemProperties& propsOut)
{
	// Load prefabs
	{
		const char* prefabs = propsIn->GetString("prefab");
		while (prefabs && prefabs[0])
		{
			// Search through the string-separated list of prefabs

			// Skip leading spaces
			auto leadingSpaces = strspn(prefabs, WHITESPACE_CHARS);
			prefabs += leadingSpaces;

			if (!prefabs[0])
				break;	// Trailing spaces

			const char* end = strchr(prefabs, ' ');
			size_t prefabLength = end ? end - prefabs : strlen(prefabs);

			char singlePrefab[128];
			strncpy_s(singlePrefab, prefabs, prefabLength);

			if (m_PrefabsLinked)
				propsOut.m_Prefabs.push_back(FindPrefab(singlePrefab));
			else
				propsOut.m_PrefabNames.push_back(singlePrefab);

			prefabs = end;
		}
	}

	// Load equip slot
	{
		const char* itemSlot = propsIn->GetString("item_slot", nullptr);
		if (!itemSlot || !itemSlot[0])
			propsOut.m_EquipSlot = ItemEquipSlot::Unspecified;
		else if (!stricmp(itemSlot, "primary"))
			propsOut.m_EquipSlot = ItemEquipSlot::Primary;
		else if (!stricmp(itemSlot, "secondary"))
			propsOut.m_EquipSlot = ItemEquipSlot::Secondary;
		else if (!stricmp(itemSlot, "melee"))
			propsOut.m_EquipSlot = ItemEquipSlot::Melee;
		else if (!stricmp(itemSlot, "PDA"))
			propsOut.m_EquipSlot = ItemEquipSlot::PDA;
		else if (!stricmp(itemSlot, "PDA2"))
			propsOut.m_EquipSlot = ItemEquipSlot::PDA2;
		else if (!stricmp(itemSlot, "building"))
			propsOut.m_EquipSlot = ItemEquipSlot::Building;
		else if (!stricmp(itemSlot, "action"))
			propsOut.m_EquipSlot = ItemEquipSlot::Action;
		else if (!stricmp(itemSlot, "utility"))
			propsOut.m_EquipSlot = ItemEquipSlot::Utility;
		else if (!stricmp(itemSlot, "taunt"))
			propsOut.m_EquipSlot = ItemEquipSlot::Taunt;
		else if (!stricmp(itemSlot, "head"))
			propsOut.m_EquipSlot = ItemEquipSlot::Head;
		else if (!stricmp(itemSlot, "misc"))
			propsOut.m_EquipSlot = ItemEquipSlot::Misc;
		else if (!stricmp(itemSlot, "quest"))
			propsOut.m_EquipSlot = ItemEquipSlot::Quest;
		else
			Assert(!"Unknown item_slot type");
	}

	propsOut.m_XifierClassRemap = propsIn->GetString("xifier_class_remap");
	propsOut.m_CraftClass = propsIn->GetString("craft_class");
	propsOut.m_IconName = propsIn->GetString("item_iconname");
	propsOut.m_PlayerModel = propsIn->GetString("model_player");
}

const std::string& ItemSchema::ItemProperties::GetXifierClassRemap() const
{
	if (!m_XifierClassRemap.empty())
		return m_XifierClassRemap;

	for (const auto& prefab : m_Prefabs)
	{
		const auto& potential = prefab->GetXifierClassRemap();
		if (!potential.empty())
			return potential;
	}

	// Return empty, didn't find any parent prefabs that had xifier_class_remap set
	return m_XifierClassRemap;
}

const std::string& ItemSchema::ItemProperties::GetCraftClass() const
{
	if (!m_CraftClass.empty())
		return m_CraftClass;

	for (const auto& prefab : m_Prefabs)
	{
		const auto& potential = prefab->GetCraftClass();
		if (!potential.empty())
			return potential;
	}

	return m_CraftClass;
}

const std::string& ItemSchema::ItemProperties::GetPlayerModel() const
{
	if (!m_PlayerModel.empty())
		return m_PlayerModel;

	for (const auto& prefab : m_Prefabs)
	{
		const auto& potential = prefab->GetPlayerModel();
		if (!potential.empty())
			return potential;
	}

	return m_PlayerModel;
}

const std::string& ItemSchema::ItemProperties::GetIconName() const
{
	if (!m_IconName.empty())
		return m_IconName;

	for (const auto& prefab : m_Prefabs)
	{
		const auto& potential = prefab->GetIconName();
		if (!potential.empty())
			return potential;
	}

	return m_IconName;
}

bool ItemSchema::ItemProperties::HasPrefab(const char* prefabName) const
{
	for (const auto& prefab : m_Prefabs)
	{
		if (!stricmp(prefab->m_Name.c_str(), prefabName))
			return true;

		if (prefab->HasPrefab(prefabName))
			return true;
	}

	return false;
}

ItemSchema::ItemEquipSlot ItemSchema::ItemProperties::GetEquipSlot() const
{
	if (m_EquipSlot != ItemEquipSlot::Unspecified)
		return m_EquipSlot;

	for (const auto& prefab : m_Prefabs)
	{
		const auto equipSlot = prefab->GetEquipSlot();
		if (equipSlot != ItemEquipSlot::Unspecified)
			return equipSlot;
	}

	return ItemEquipSlot::Unspecified;
}

bool ItemSchema::ItemRemap::FromMatch(const ItemEntry& entry) const
{
	const bool idMatch = m_FromModel.empty() && m_FromID == entry.m_ID;

	const auto& playerModel = entry.GetPlayerModel();
	const bool modelMatch = !m_FromModel.empty() && !playerModel.empty() && stristr(playerModel.c_str(), m_FromModel.c_str());

	return idMatch || modelMatch;
}

void ItemSchema::LoadUniqueItems(std::forward_list<ItemEntry>& items)
{
	m_UniqueItems.clear();

	// Loop through the remaps and create UniqueItemEntries for all of them
	for (auto& remap : m_Remaps)
	{
		bool foundExisting = false;
		for (auto& uniqueItem : m_UniqueItems)
		{
			if (uniqueItem.m_MasterEntry->m_ID == remap.m_ToID)
			{
				uniqueItem.m_Remaps.push_back(&remap);
				foundExisting = true;
				break;
			}
		}

		if (foundExisting)
			continue;

		// Couldn't find an existing UniqueItemEntry with this remap target, add a new UniqueItemEntry
		m_UniqueItems.emplace_front();
		auto& newUniqueItem = m_UniqueItems.front();

		if (remap.IsUnmap())
			newUniqueItem.m_IsForceUnmapped = true;
		else
			newUniqueItem.m_Remaps.push_back(&remap);

		// Find and move the master item into this UniqueItemEntry
		bool foundTarget = false;
		for (auto beforeIter = items.before_begin(); std::next(beforeIter) != items.end(); std::advance(beforeIter, 1))
		{
			auto iter = std::next(beforeIter);
			if (iter->m_ID == remap.m_ToID)
			{
				newUniqueItem.m_Entries.splice_after(newUniqueItem.m_Entries.before_begin(), items, beforeIter);
				newUniqueItem.m_MasterEntry = &newUniqueItem.m_Entries.front();
				foundTarget = true;
				break;
			}
		}

		assert(foundTarget);
	}

	// Auto-create UniqueItemEntrys for all remaining items based on their model
	while (!items.empty())
	{
		auto& item = items.front();
		const auto& model = item.GetPlayerModel();

		bool foundExisting = false;

		// Try to see if we already have a unique item matching this remap's
		for (auto& uniqueItem : m_UniqueItems)
		{
			bool match = false;

			// Check if we direct match the master entry
			if (!uniqueItem.m_IsForceUnmapped && !model.empty() &&
				!stricmp(uniqueItem.m_MasterEntry->GetPlayerModel().c_str(), model.c_str()))
			{
				match = true;
			}
			else
			{
				// Check if we partial match any of the remaps
				for (const auto& remap : uniqueItem.m_Remaps)
				{
					if (remap->FromMatch(item))
					{
						match = true;
						break;
					}
				}
			}

			if (match)
			{
				// Add ourselves to this UniqueItemEntry, removing ourselves from the global item list
				uniqueItem.m_Entries.splice_after(uniqueItem.m_Entries.before_begin(), items, items.before_begin());
				foundExisting = true;
				break;
			}
		}

		if (foundExisting)
			continue;

		// No remap or we don't have an existing unique item with this remap, add a new unique item
		m_UniqueItems.emplace_front();
		auto& newUniqueItem = m_UniqueItems.front();
		newUniqueItem.m_Entries.splice_after(newUniqueItem.m_Entries.before_begin(), items, items.before_begin());
		newUniqueItem.m_MasterEntry = &newUniqueItem.m_Entries.front();
	}
}

void ItemSchema::LoadOverrides(const std::forward_list<ItemEntry>& items)
{
	KeyValues::AutoDelete basekv(new KeyValues("none"));

	constexpr const char* FILENAME = "scripts/items/itemschema_overrides.vdf";
	if (!basekv->LoadFromFile(Interfaces::GetFileSystem(), FILENAME))
	{
		PluginWarning("Failed to load %s for module %s!\n", FILENAME, GetModuleName());
		return;
	}

	m_Remaps.clear();
	KeyValues* remap = basekv->GetFirstValue();

	do
	{
		ItemRemap newRemap;

		const char* name = remap->GetName();
		if (isdigit(name[0]))
			newRemap.m_FromID = atoi(name);
		else
			newRemap.m_FromModel = name;

		const char* value = remap->GetString();
		if (isdigit(value[0]))
			newRemap.m_ToID = atoi(value);
		else
		{
			// Try to find an item ID matching this text
			const std::string* foundString = nullptr;
			for (const auto& item : items)
			{
				const auto& playerModel = item.GetPlayerModel();
				if (stristr(playerModel.c_str(), value))
				{
					if (foundString && *foundString != playerModel)
					{
						PluginWarning("[Item Schema Overrides] Target weapon model \"%s\" matches 2 or more items. Try making the override more specific.\n", value);
						continue;
					}

					newRemap.m_ToID = item.m_ID;
					foundString = &playerModel;
				}
			}

			if (!foundString)
				PluginWarning("[Item Schema Overrides] Unable to find a target weapon using a model containing the string \"%s\".\n", value);
		}

		m_Remaps.emplace_back(std::move(newRemap));

	} while (remap = remap->GetNextValue());
}

unsigned int ItemSchema::UniqueItemEntry::GetLowestID() const
{
	unsigned int lowest = std::numeric_limits<unsigned int>::max();

	for (const auto& entry : m_Entries)
		lowest = std::min(lowest, entry.m_ID);

	return lowest;
}
