#include "Entities.h"
#include "Interfaces.h"
#include "Exceptions.h"
#include "PluginBase/TFDefinitions.h"

#include <cdll_int.h>
#include <client_class.h>
#include <icliententity.h>

#include <sstream>

std::unordered_map<std::string, std::unordered_map<std::string, int>> Entities::s_ClassPropOffsets;

bool Entities::CheckEntityBaseclass(IClientEntity * entity, const char * baseclass)
{
	ClientClass *clientClass = entity->GetClientClass();
	if (clientClass)
		return CheckClassBaseclass(clientClass, baseclass);

	return false;
}

bool Entities::CheckClassBaseclass(ClientClass * clientClass, const char * baseclass)
{
	RecvTable *sTable = clientClass->m_pRecvTable;
	if (sTable)
		return CheckTableBaseclass(sTable, baseclass);

	return false;
}

bool Entities::CheckTableBaseclass(RecvTable * sTable, const char * baseclass)
{
	const char* tName = sTable->GetName();

	// We're operating on the assumption that network table classes will start with "DT_"
	if (tName && tName[0] && tName[1] && tName[2] && !strcmp(tName + 3, baseclass))
		return true;

	// Micro-optimization smh
	{
		RecvProp* sProp;
		RecvTable* sChildTable;
		for (int i = 0; i < sTable->GetNumProps(); i++)
		{
			sProp = sTable->GetProp(i);

			sChildTable = sProp->GetDataTable();
			if (!sChildTable || strcmp(sProp->GetName(), "baseclass"))
				continue;

			return CheckTableBaseclass(sChildTable, baseclass);
		}
	}

	return false;
}

std::string Entities::ConvertTreeToString(const std::vector<std::string>& tree)
{
	std::stringstream ss;
	for (std::string branch : tree)
		ss << '>' << branch;

	return ss.str();
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

bool Entities::RetrieveClassPropOffset(const std::string& className, const std::string& propertyString, const std::vector<std::string>& propertyTree)
{
	if (s_ClassPropOffsets[className].find(propertyString) != s_ClassPropOffsets[className].end())
		return true;

	ClientClass *cc = Interfaces::GetClientDLL()->GetAllClasses();

	while (cc)
	{
		if (className.compare(cc->GetName()) == 0)
		{
			RecvTable *table = cc->m_pRecvTable;

			int offset = 0;
			RecvProp *prop = nullptr;

			if (table)
			{
				for (std::string propertyName : propertyTree)
				{
					int subOffset = 0;

					if (prop && prop->GetType() == DPT_DataTable)
						table = prop->GetDataTable();

					if (!table)
						return false;

					if (GetSubProp(table, propertyName.c_str(), prop, subOffset))
						offset += subOffset;
					else
						return false;

					table = nullptr;
				}

				s_ClassPropOffsets[className][propertyString] = offset;

				return true;
			}
		}
		cc = cc->m_pNext;
	}

	return false;
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

void* Entities::GetEntityProp(IClientEntity* entity, const std::vector<std::string>& propertyTree)
{
	const std::string className = entity->GetClientClass()->GetName();
	const std::string propertyString = ConvertTreeToString(propertyTree);

	if (!RetrieveClassPropOffset(className, propertyString, propertyTree))
		throw invalid_class_prop(entity->GetClientClass()->GetName());

	return (void *)((unsigned long)(entity)+(unsigned long)(s_ClassPropOffsets[className][propertyString]));
}