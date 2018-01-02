#include "Entities.h"
#include "Interfaces.h"
#include "Exceptions.h"
#include "PluginBase/TFDefinitions.h"

#include <cdll_int.h>
#include <client_class.h>
#include <icliententity.h>
#include <vprof.h>

#include <sstream>

std::unordered_map<const char*, std::unordered_map<const char*, int>> Entities::s_ClassPropOffsets;

bool Entities::CheckEntityBaseclass(IClientNetworkable* entity, const char* baseclass)
{
	ClientClass *clientClass = entity->GetClientClass();
	if (clientClass)
		return CheckClassBaseclass(clientClass, baseclass);

	return false;
}

bool Entities::CheckClassBaseclass(ClientClass* clientClass, const char* baseclass)
{
	RecvTable *sTable = clientClass->m_pRecvTable;
	if (sTable)
		return CheckTableBaseclass(sTable, baseclass);

	return false;
}

bool Entities::CheckTableBaseclass(RecvTable* sTable, const char* baseclass)
{
	const char* tName = sTable->GetName();

	// We're operating on the assumption that network table classes will start with "DT_"
	if (tName && tName[0] && tName[1] && tName[2] && !strcmp(tName + 3, baseclass))
		return true;

	for (int i = 0; i < sTable->GetNumProps(); i++)
	{
		const RecvProp& sProp = sTable->m_pProps[i];

		RecvTable* sChildTable = sProp.GetDataTable();
		if (!sChildTable || strcmp(sProp.GetName(), "baseclass"))
			continue;

		return CheckTableBaseclass(sChildTable, baseclass);
	}

	return false;
}

std::string Entities::ConvertTreeToString(const std::vector<const char*>& tree)
{
	Assert(tree.size() > 0);
	if (tree.size() < 1)
		return std::string();

	if (tree.size() < 2)
		return tree[0];

	std::stringstream ss;
	ss << tree[0];
	for (size_t i = 1; i < tree.size(); i++)
		ss << '.' << tree[i];

	return ss.str();
}

std::vector<std::string> Entities::ConvertStringToTree(const char* str)
{
	std::vector<std::string> retVal;

	std::string buffer;
	while (*str != '\0')
	{
		if (*str == '.')
		{
			retVal.push_back(buffer);
			buffer.clear();
		}
		else
		{
			buffer.push_back(*str);
		}

		str++;
	}

	if (buffer.size() > 0)
		retVal.push_back(buffer);

	return retVal;
}

ClientClass* Entities::GetClientClass(const char* className)
{
	ClientClass *cc = Interfaces::GetClientDLL()->GetAllClasses();
	while (cc)
	{
		if (!stricmp(className, cc->GetName()))
			return cc;

		cc = cc->m_pNext;
	}

	return nullptr;
}

int Entities::FindExistingPropOffset(const char* className, const char* propertyString, bool bThrow)
{
	for (const auto& pair1 : s_ClassPropOffsets)
	{
		if (strcmp(pair1.first, className))
			continue;

		for (const auto& pair2 : pair1.second)
		{
			if (strcmp(pair2.first, propertyString))
				continue;

			return pair2.second;
		}

		break;
	}

	if (bThrow)
		throw invalid_class_prop("Unable to find existing prop");
	else
		return -1;
}

void Entities::AddPropOffset(const char* className, const char* propertyString, int offset)
{
	bool added = false;

	for (const auto& pair1 : s_ClassPropOffsets)
	{
		if (strcmp(pair1.first, className))
			continue;

		className = pair1.first;

		for (const auto& pair2 : pair1.second)
		{
			const bool match = !strcmp(pair2.first, propertyString);
			Assert(!match);
			if (match)
				return;
		}

		propertyString = strdup(propertyString);
		s_ClassPropOffsets[className][propertyString] = offset;
		return;
	}

	if (!added)
	{
		className = strdup(className);
		propertyString = strdup(propertyString);
		s_ClassPropOffsets[className][propertyString] = offset;
	}
}

int Entities::RetrieveClassPropOffset(const char* className, const char* propertyString)
{
	const auto existing = FindExistingPropOffset(className, propertyString, false);
	if (existing >= 0)
		return existing;

	ClientClass *cc = Interfaces::GetClientDLL()->GetAllClasses();

	const std::vector<std::string>& propertyTree = ConvertStringToTree(propertyString);

	while (cc)
	{
		if (!strcmp(className, cc->GetName()))
		{
			RecvTable *table = cc->m_pRecvTable;

			int offset = -1;
			RecvProp *prop = nullptr;

			if (table)
			{
				for (const auto& propertyName : propertyTree)
				{
					int subOffset = 0;

					if (prop && prop->GetType() == DPT_DataTable)
						table = prop->GetDataTable();

					if (!table)
						return false;

					if (GetSubProp(table, propertyName.c_str(), prop, subOffset))
					{
						Assert(subOffset >= 0);
						if (offset < 0)
							offset = 0;

						offset += subOffset;
					}
					else
						return false;

					table = nullptr;
				}

				Assert(offset >= 0);
				AddPropOffset(className, propertyString, offset);
				return offset;
			}
		}
		cc = cc->m_pNext;
	}

	return -1;
}

bool Entities::GetSubProp(RecvTable* table, const char* propName, RecvProp*& prop, int& offset)
{
	for (int i = 0; i < table->GetNumProps(); i++)
	{
		offset = 0;

		RecvProp *currentProp = table->GetProp(i);

		if (strcmp(currentProp->GetName(), propName) == 0)
		{
			prop = currentProp;
			offset = currentProp->GetOffset();
			return true;
		}

		if (currentProp->GetType() == DPT_DataTable)
		{
			if (GetSubProp(currentProp->GetDataTable(), propName, prop, offset))
			{
				offset += currentProp->GetOffset();
				return true;
			}
		}
	}

	return false;
}

void* Entities::GetEntityProp(IClientNetworkable* entity, const char* propertyString)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	auto const className = entity->GetClientClass()->GetName();

	int offset = RetrieveClassPropOffset(className, propertyString);
	if (offset < 0)
		throw invalid_class_prop(entity->GetClientClass()->GetName());

	return (void *)(size_t(entity->GetDataTableBasePtr()) + size_t(offset));
}