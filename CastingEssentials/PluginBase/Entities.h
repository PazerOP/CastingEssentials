#pragma once

#include "PluginBase/EntityOffset.h"

#include <algorithm>
#include <map>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

class IClientNetworkable;
class RecvTable;
class RecvProp;
class ClientClass;
enum class TFTeam;

class Entities final
{
public:
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const RecvTable* table, const std::string_view& propertyString);
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const ClientClass* cc, const std::string_view& propertyString);
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const std::string_view& className, const std::string_view& propertyString);

	template<size_t size> static char* PropIndex(char(&buffer)[size], const char* base, int index)
	{
		sprintf_s(buffer, "%s.%03i", base, index);
		return buffer;
	}

	template<typename TValue> inline static EntityOffset<TValue>
		GetEntityProp(const ClientClass* cc, const char* propertyString)
	{
		const auto found = RetrieveClassPropOffset(cc, propertyString);
		if (found.first < 0)
			throw invalid_class_prop(propertyString);

		return EntityOffset<TValue>(found.second, found.first);
	}
	template<typename TValue> __forceinline static EntityOffset<TValue>
		GetEntityProp(IClientNetworkable* entity, const char* propertyString)
	{
		return GetEntityProp<TValue>((const ClientClass*)(entity->GetClientClass()), propertyString);
	}
	template<typename TValue> inline static EntityOffset<TValue>
		GetEntityProp(const char* clientClassName, const char* propertyString)
	{
		return GetEntityProp<TValue>(GetClientClass(clientClassName), propertyString);
	}

	static bool CheckEntityBaseclass(IClientNetworkable* entity, const char* baseclass);
	static bool CheckTableBaseClass(const RecvTable* tableParent, const RecvTable* tableBase);

	static ClientClass* GetClientClass(const char* className);
	static RecvProp* FindRecvProp(const char* className, const char* propName, bool recursive = true);
	static RecvProp* FindRecvProp(RecvTable* table, const char* propName, bool recursive = true);

	static const TFTeam* TryGetEntityTeam(const IClientNetworkable* entity);
	static TFTeam GetEntityTeamSafe(const IClientNetworkable* entity);

	static int GetItemDefinitionIndex(const IClientNetworkable* entity);

private:
	Entities() = delete;
	~Entities() = delete;

	static bool CheckClassBaseclass(ClientClass *clientClass, const char* baseclass);
	static bool CheckTableBaseclass(RecvTable *sTable, const char* baseclass);

	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const RecvTable* table, const std::vector<std::string_view>& refPropertyTree,
		std::vector<std::string_view>& currentPropertyTree);

	static std::string ConvertTreeToString(const std::vector<std::string_view>& tree);
	static std::vector<std::string_view> ConvertStringToTree(const std::string_view& str);

	static bool GetSubProp(const RecvTable* table, const std::string_view& propName, const RecvProp*& prop, int& offset);
	static void* GetEntityProp(IClientNetworkable* entity, const char* propertyString, bool throwifMissing = true);
};