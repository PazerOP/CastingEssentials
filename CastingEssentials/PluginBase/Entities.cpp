#include "Entities.h"
#include "Interfaces.h"
#include "Exceptions.h"
#include <sstream>
#include <cdll_int.h>
#include <client_class.h>
#include <icliententity.h>

std::unordered_map<std::string, std::unordered_map<std::string, int>> Entities::s_ClassPropOffsets;

std::string Entities::ConvertTreeToString(const std::vector<std::string>& tree)
{
	std::stringstream ss;
	for (std::string branch : tree)
		ss << '>' << branch;

	return ss.str();
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