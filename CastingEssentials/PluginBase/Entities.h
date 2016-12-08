#pragma once

#include <array>
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
	static int RetrieveClassPropOffset(const char* className, const char* propertyString);
	__forceinline static int RetrieveClassPropOffset(const char* className, const std::vector<const char*>& propertyTree)
	{
		return RetrieveClassPropOffset(className, ConvertTreeToString(propertyTree).c_str());
	}

	template<size_t size> static void PropIndex(char(&buffer)[size], const char* base, int index)
	{
		sprintf_s(buffer, "%s.%03i", base, index);
	}

	template<typename T> __forceinline static T GetEntityProp(IClientEntity* entity, const char* propertyString)
	{
		return reinterpret_cast<T>(GetEntityProp(entity, propertyString));
	}
	template<typename T, class... Args> inline static T GetEntityProp(IClientEntity* entity, Args... propertyNames)
	{
		static_assert(sizeof...(Args) > 0, "Must have at least 1 value in propertyTree");

		std::string propertyNamesArray[] = { std::to_string(propertyNames)... };

		std::string finalBuffer;
		for (size_t i = 0; i < sizeof...(Args); i++)
			finalBuffer += propertyNamesArray[i];

		return GetEntityProp<T>(entity, finalBuffer.c_str());
	}
	template<typename T> [[deprecated("deprecated due to performance issues")]]
		__forceinline static T GetEntityProp(IClientEntity* entity, std::vector<const char*> propertyTree)
	{
		static_assert(false, "deprecated due to performance issues");
	}

	static bool CheckEntityBaseclass(IClientEntity* entity, const char* baseclass);

	static ClientClass* GetClientClass(const char* className);

	__forceinline static TFTeam* GetEntityTeam(IClientEntity* entity) { return GetEntityProp<TFTeam*>(entity, "m_iTeamNum"); }

private:
	Entities() = delete;
	~Entities() = delete;

	static bool CheckClassBaseclass(ClientClass *clientClass, const char* baseclass);
	static bool CheckTableBaseclass(RecvTable *sTable, const char* baseclass);

	static std::string ConvertTreeToString(const std::vector<const char*>& tree);
	static std::vector<std::string> ConvertStringToTree(const char* str);

	static bool GetSubProp(RecvTable* table, const char* propName, RecvProp*& prop, int& offset);
	static void* GetEntityProp(IClientEntity* entity, const char* propertyString);
	static void* GetEntityProp(IClientEntity* entity, const std::vector<const char*>& propertyTree);

	static std::unordered_map<const char*, std::unordered_map<const char*, int>> s_ClassPropOffsets;
	static int FindExistingPropOffset(const char* className, const char* propertyString, bool bThrow = true);
	static void AddPropOffset(const char* className, const char* propertyString, int offset);
};