#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class IClientEntity;
class RecvTable;
class RecvProp;
class ClientClass;
enum class TFTeam;

class Entities final
{
public:
	static bool RetrieveClassPropOffset(const std::string& className, const std::string& propertyString, const std::vector<std::string>& propertyTree);
	__forceinline static bool RetrieveClassPropOffset(const std::string& className, const std::vector<std::string>& propertyTree)
	{
		return RetrieveClassPropOffset(className, ConvertTreeToString(propertyTree), propertyTree);
	}

	template<typename T> __forceinline static T GetEntityProp(IClientEntity* entity, const std::vector<std::string>& propertyTree)
	{
		return reinterpret_cast<T>(GetEntityProp(entity, propertyTree));
	}

	static bool CheckEntityBaseclass(IClientEntity* entity, const char* baseclass);

	static ClientClass* GetClientClass(const char* className);

	__forceinline static TFTeam* GetEntityTeam(IClientEntity* entity) { return GetEntityProp<TFTeam*>(entity, { "m_iTeamNum" }); }

private:
	Entities() = delete;
	~Entities() = delete;

	static bool CheckClassBaseclass(ClientClass *clientClass, const char* baseclass);
	static bool CheckTableBaseclass(RecvTable *sTable, const char* baseclass);

	static std::string ConvertTreeToString(const std::vector<std::string>& tree);

	static bool GetSubProp(RecvTable* table, const char* propName, RecvProp*& prop, int& offset);
	static void* GetEntityProp(IClientEntity* entity, const std::vector<std::string>& propertyTree);

	static std::unordered_map<std::string, std::unordered_map<std::string, int>> s_ClassPropOffsets;
};