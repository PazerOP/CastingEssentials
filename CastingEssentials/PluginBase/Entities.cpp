#include "Entities.h"
#include "Interfaces.h"
#include "Exceptions.h"
#include "PluginBase/TFDefinitions.h"

#include <cdll_int.h>
#include <client_class.h>
#include <icliententity.h>
#include <vprof.h>

#include <sstream>

bool Entities::CheckEntityBaseclass(IClientNetworkable* entity, const char* baseclass)
{
	ClientClass *clientClass = entity->GetClientClass();
	if (clientClass)
		return CheckClassBaseclass(clientClass, baseclass);

	return false;
}

bool Entities::CheckTableBaseClass(const RecvTable* tableParent, const RecvTable* tableBase)
{
	if (tableParent == tableBase)
		return true;

	Assert(tableParent);
	Assert(tableBase);

	if (tableParent->m_nProps < 1)
		return false;

	const RecvProp* baseProp = &tableParent->m_pProps[0];
	if (baseProp->m_RecvType != DPT_DataTable || strcmp(baseProp->m_pVarName, "baseclass"))
		return false;

	return CheckTableBaseClass(baseProp->m_pDataTable, tableBase);
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

std::pair<int, const RecvTable*> Entities::RetrieveClassPropOffset(const std::string_view& className, const std::string_view& propertyString)
{
	return RetrieveClassPropOffset(GetClientClass(className.data()), propertyString);
}

std::pair<int, const RecvTable*> Entities::RetrieveClassPropOffset(const ClientClass* cc, const std::string_view& propertyString)
{
	if (!cc)
		return std::make_pair(-1, nullptr);

	return RetrieveClassPropOffset(cc->m_pRecvTable, propertyString);
}

std::pair<int, const RecvTable*> Entities::RetrieveClassPropOffset(const RecvTable* table, const std::string_view& propertyString)
{
	if (!table)
		return std::make_pair(-1, nullptr);

	std::vector<std::string_view> refPropertyTree;

	size_t found = propertyString.npos;
	do
	{
		size_t newFound = propertyString.find('.', found + 1);
		refPropertyTree.push_back(propertyString.substr(found + 1, newFound - (found + 1)));
		found = newFound;

	} while (found != propertyString.npos);

	std::vector<std::string_view> currentPropertyTree;

	return Entities::RetrieveClassPropOffset(table, refPropertyTree, currentPropertyTree);
}

std::pair<int, const RecvTable*> Entities::RetrieveClassPropOffset(const RecvTable* table,
	const std::vector<std::string_view>& refPropertyTree, std::vector<std::string_view>& currentPropertyTree)
{
	if (!table)
		return std::make_pair(-1, nullptr);

	for (int i = 0; i < table->m_nProps; i++)
	{
		const RecvProp* const prop = &table->m_pProps[i];

		if (prop->GetType() == DPT_DataTable)
		{
			currentPropertyTree.push_back(prop->GetName());

			if (auto found = RetrieveClassPropOffset(prop->m_pDataTable, refPropertyTree, currentPropertyTree); found.first != -1)
				return std::make_pair(prop->GetOffset() + found.first, found.second);

			currentPropertyTree.pop_back();
		}
		else
		{
			if (refPropertyTree.size() > (currentPropertyTree.size() + 1))
				continue;

			if (prop->GetName() != refPropertyTree.back())
				continue;

			bool match = true;

			// Try backwards-first search
			for (auto refIter = refPropertyTree.crbegin() + 1, curIter = currentPropertyTree.crbegin();
				refIter != refPropertyTree.crend();
				++refIter, ++curIter)
			{
				if (*refIter != *curIter)
				{
					match = false;
					break;
				}
			}

			if (!match)
				continue;

			Assert(prop->GetType() != DPT_DataTable);
			return std::make_pair(prop->GetOffset(), table);
		}
	}

	return std::make_pair(-1, nullptr);
}

const TFTeam* Entities::TryGetEntityTeam(const IClientNetworkable* entity)
{
	static const auto teamOffset = GetEntityProp<TFTeam>("CBaseEntity", "m_iTeamNum");
	return teamOffset.TryGetValue(entity);
}

TFTeam Entities::GetEntityTeamSafe(const IClientNetworkable* entity)
{
	if (auto team = TryGetEntityTeam(entity))
		return *team;
	else
		return TFTeam::Unassigned;
}

bool Entities::GetSubProp(const RecvTable* table, const std::string_view& propName, const RecvProp*& prop, int& offset)
{
	for (int i = 0; i < table->m_nProps; i++)
	{
		offset = 0;

		const RecvProp *currentProp = &table->m_pProps[i];

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

	auto offset = RetrieveClassPropOffset(className, propertyString);
	if (offset.first < 0)
	{
		if (throwifMissing)
			throw invalid_class_prop(entity->GetClientClass()->GetName());
		else
			return nullptr;
	}

	return (void *)(size_t(entity->GetDataTableBasePtr()) + size_t(offset.first));
}