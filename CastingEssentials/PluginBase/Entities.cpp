#include "Entities.h"
#include "Interfaces.h"
#include "Exceptions.h"
#include "PluginBase/TFDefinitions.h"

#include <cdll_int.h>
#include <client_class.h>
#include <icliententity.h>
#include <vprof.h>

#include <sstream>

std::map<std::string, std::map<std::string, int, Entities::ci_less>, Entities::ci_less> Entities::s_ClassPropOffsets;

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

std::string Entities::ConvertTreeToString(const std::vector<std::string_view>& tree)
{
	Assert(tree.size() > 0);
	if (tree.size() < 1)
		return std::string();

	if (tree.size() < 2)
		return std::string(tree[0]);

	std::stringstream ss;
	ss << tree[0];
	for (size_t i = 1; i < tree.size(); i++)
		ss << '.' << tree[i];

	return ss.str();
}

std::vector<std::string_view> Entities::ConvertStringToTree(const std::string_view& str)
{
	std::vector<std::string_view> retVal;

	auto lastElement = str.begin();
	for (auto iter = str.begin(); iter < str.end(); ++iter)
	{
		if (*iter == '.')
		{
			retVal.push_back(std::string_view(&*lastElement, iter - lastElement));
			lastElement = ++iter;
		}
	}

	if (lastElement < str.end())
		retVal.push_back(std::string_view(&*lastElement, str.end() - lastElement));

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

RecvProp* Entities::FindRecvProp(const char* className, const char* propName, bool recursive)
{
	auto cc = GetClientClass(className);
	if (!cc)
		return nullptr;

	return FindRecvProp(cc->m_pRecvTable, propName, recursive);
}

RecvProp* Entities::FindRecvProp(RecvTable* table, const char* propName, bool recursive)
{
	for (int i = 0; i < table->m_nProps; i++)
	{
		auto& prop = table->m_pProps[i];
		if (!strcmp(prop.m_pVarName, propName))
			return &prop;

		if (prop.m_RecvType == SendPropType::DPT_DataTable)
		{
			if (auto found = FindRecvProp(prop.m_pDataTable, propName))
				return found;
		}
	}

	return nullptr;
}

int Entities::FindExistingPropOffset(const std::string_view& className, const std::string_view& propertyString, bool bThrow)
{
	if (auto found = s_ClassPropOffsets.find(className); found != s_ClassPropOffsets.end())
	{
		if (auto found2 = found->second.find(propertyString); found2 != found->second.end())
		{
			return found2->second;
		}
	}

	if (bThrow)
		throw invalid_class_prop("Unable to find existing prop");
	else
		return -1;
}

void Entities::AddPropOffset(const std::string_view& className, const std::string_view& propertyString, int offset)
{
	bool added = false;

	if (auto found = s_ClassPropOffsets.find(className); found != s_ClassPropOffsets.end())
	{
		found->second[std::string(propertyString)] = offset;
	}
	else
		s_ClassPropOffsets[std::string(className)][std::string(propertyString)] = offset;
}

int Entities::RetrieveClassPropOffset(const std::string_view& className, const std::string_view& propertyString)
{
	const auto existing = FindExistingPropOffset(className, propertyString, false);
	if (existing >= 0)
		return existing;

	const std::vector<std::string_view>& propertyTree = ConvertStringToTree(propertyString);

	for (auto cc = Interfaces::GetClientDLL()->GetAllClasses(); cc; cc = cc->m_pNext)
	{
		if (className != cc->GetName())
			continue;

		RecvTable *table = cc->m_pRecvTable;
		if (!table)
			break;

		int offset = -1;
		RecvProp *prop = nullptr;

		for (const auto& propertyName : propertyTree)
		{
			int subOffset = 0;

			if (prop && prop->GetType() == DPT_DataTable)
				table = prop->GetDataTable();

			if (!table)
				return -1;

			if (GetSubProp(table, propertyName, prop, subOffset))
			{
				Assert(subOffset >= 0);
				if (offset < 0)
					offset = 0;

				offset += subOffset;
			}
			else
				return -1;

			table = nullptr;
		}

		Assert(offset > 0);
		AddPropOffset(className, propertyString, offset);
		return offset;
	}

	return -1;
}

bool Entities::GetSubProp(RecvTable* table, const std::string_view& propName, RecvProp*& prop, int& offset)
{
	for (int i = 0; i < table->GetNumProps(); i++)
	{
		offset = 0;

		RecvProp *currentProp = table->GetProp(i);

		if (propName == currentProp->GetName())
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

void* Entities::GetEntityProp(IClientNetworkable* entity, const char* propertyString, bool throwifMissing)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	auto const className = entity->GetClientClass()->GetName();

	int offset = RetrieveClassPropOffset(className, propertyString);
	if (offset < 0)
	{
		if (throwifMissing)
			throw invalid_class_prop(entity->GetClientClass()->GetName());
		else
			return nullptr;
	}

	return (void *)(size_t(entity->GetDataTableBasePtr()) + size_t(offset));
}