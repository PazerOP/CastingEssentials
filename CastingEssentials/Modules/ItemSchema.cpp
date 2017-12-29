#include "ItemSchema.h"
#include "PluginBase/Interfaces.h"

#include <filesystem.h>
#include <KeyValues.h>

ItemSchema::ItemSchema()
{
	LoadItemSchema();

	PluginWarning("asdf");
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
	if (specializedID < 0 || (size_t)specializedID > m_MappingsSize)
		return specializedID;

	return m_Mappings.get()[specializedID];
}

void ItemSchema::LoadItemSchema()
{
	KeyValues::AutoDelete basekv(new KeyValues("none"));

	constexpr const char* FILENAME = "scripts/items/items_game.txt";
	if (!basekv->LoadFromFile(Interfaces::GetFileSystem(), FILENAME))
	{
		PluginWarning("Failed to load %s for module %s!\n", FILENAME, GetModuleName());
		return;
	}

	KeyValues* items = basekv->FindKey("items");

	KeyValues* item = items->GetFirstTrueSubKey();
	if (!item)
	{
		PluginWarning("No items found in the item schema???\n");
		return;
	}

	size_t mappingCount = 0;

	std::map<int, std::vector<int>> mappings;
	int maxItemID = 0;

	do
	{
		const char* name = item->GetName();
		if (!isdigit(name[0]))
			continue;

		const int itemID = atoi(name);

		const char* prefabTypes = item->GetString("prefab", nullptr);
		if (!prefabTypes)
			continue;

		char firstPrefabType[128];
		GetFirstPrefabType(item->GetString("prefab"), firstPrefabType, sizeof(firstPrefabType));

		const int firstFoundPrefab = GetFirstItemUsingPrefab(items, firstPrefabType);

		if (firstFoundPrefab < itemID)
		{
			mappings[firstFoundPrefab].push_back(itemID);
			maxItemID = max(maxItemID, itemID);
			mappingCount++;
		}

	} while (item = item->GetNextTrueSubKey());

	PluginMsg("Generated item schema mappings: %zu down to %zu.\n", mappingCount, mappings.size());

	// Just allocate an array for O(1) lookup
	m_MappingsSize = maxItemID + 1;
	m_Mappings.reset(new int[m_MappingsSize]);

	// Assign them to their index
	for (size_t i = 0; i < m_MappingsSize; i++)
		m_Mappings.get()[i] = (int)i;

	for (const auto& mapping : mappings)
	{
		for (const auto& child : mapping.second)
		{
			Assert(child < m_MappingsSize);
			m_Mappings.get()[child] = mapping.first;
		}
	}
}

void ItemSchema::GetFirstPrefabType(const char* prefabsGroup, char* prefabOut, size_t maxOutSize)
{
	while (maxOutSize-- && *prefabsGroup && !isspace(*prefabsGroup))
		*prefabOut++ = *prefabsGroup++;

	Assert(maxOutSize != (size_t)-1 && maxOutSize > 0);

	*prefabOut = '\0';
}

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
