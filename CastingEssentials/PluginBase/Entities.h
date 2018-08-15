#pragma once

#include "PluginBase/EntityOffset.h"

#include <algorithm>
#include <map>
#include <set>
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
	using PropOffsetPair = std::pair<int, const std::set<const RecvTable*>*>;

public:
	static void Load();

	static PropOffsetPair RetrieveClassPropOffset(const RecvTable* table, const std::string_view& propertyString);
	static PropOffsetPair RetrieveClassPropOffset(const ClientClass* cc, const std::string_view& propertyString);
	static PropOffsetPair RetrieveClassPropOffset(const std::string_view& className, const std::string_view& propertyString);

	template<size_t size> static char* PropIndex(char(&buffer)[size], const char* base, int index)
	{
		sprintf_s(buffer, "%s.%03i", base, index);
		return buffer;
	}

	template<typename TValue> inline static EntityOffset<TValue>
	GetEntityProp(const ClientClass* cc, const char* propertyString)
	{
		auto found = RetrieveClassPropOffset(cc, propertyString);
		if (found.first < 0)
			throw invalid_class_prop(propertyString);

		return EntityOffset<TValue>(*found.second, found.first);
	}
	template<typename TValue> __forceinline static EntityOffset<TValue>
	GetEntityProp(IClientNetworkable* entity, const char* propertyString)
	{
		return GetEntityProp<TValue>((const ClientClass*)(entity->GetClientClass()), propertyString);
	}
	template<typename TValue> __forceinline static EntityOffset<TValue>
	GetEntityProp(const char* clientClassName, const char* propertyString)
	{
		return GetEntityProp<TValue>(GetClientClass(clientClassName), propertyString);
	}

	static EntityTypeChecker GetTypeChecker(const char* type);
	static EntityTypeChecker GetTypeChecker(const ClientClass* cc);
	static EntityTypeChecker GetTypeChecker(const RecvTable* cc);

	static ClientClass* GetClientClass(const char* className);
	static RecvProp* FindRecvProp(const char* className, const char* propName, bool recursive = true);
	static RecvProp* FindRecvProp(RecvTable* table, const char* propName, bool recursive = true);

	static const TFTeam* TryGetEntityTeam(const IClientNetworkable* entity);
	static TFTeam GetEntityTeamSafe(const IClientNetworkable* entity);

	static int GetItemDefinitionIndex(const IClientNetworkable* entity);

	static const RecvTable* FindRecvTable(const std::string_view& name);

private:
	Entities() = delete;
	~Entities() = delete;

	static PropOffsetPair RetrieveClassPropOffset(const RecvTable* table,
		const std::vector<std::string_view>& refPropertyTree, std::vector<std::string_view>& currentPropertyTree);

	static std::string ConvertTreeToString(const std::vector<std::string_view>& tree);
	static std::vector<std::string_view> ConvertStringToTree(const std::string_view& str);

	static void* GetEntityProp(IClientNetworkable* entity, const char* propertyString, bool throwifMissing = true);

	static std::map<const RecvTable*, std::set<const RecvTable*>> s_ContainingRecvTables;
	static void BuildContainingRecvTablesMap();
	static void AddChildTables(const RecvTable* parent, std::vector<const RecvTable*>& stack);

	static std::map<std::string_view, const RecvTable*> s_AllRecvTables;

#ifdef DEBUG
	static std::vector<const ClientClass*> s_DebugClientClasses;
#endif
};