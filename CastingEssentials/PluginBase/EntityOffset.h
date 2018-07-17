#pragma once

#include "PluginBase/Exceptions.h"

#include <dt_recv.h>

#include <limits>

class IClientNetworkable;

#undef min

template<typename TValue>
class EntityOffset
{
public:
	constexpr EntityOffset() : m_RecvTable(nullptr), m_Offset(std::numeric_limits<ptrdiff_t>::min())
	{
	}

	inline const TValue& GetValue(const IClientNetworkable* entity) const
	{
		auto table = entity->GetClientClass()->m_pRecvTable;
		if (table != m_RecvTable)
		{
			if (!FindContainedDataTable(table, m_RecvTable))
				throw mismatching_entity_offset(m_RecvTable->GetName(), table->GetName());
		}

		return *(TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue& GetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue&>(GetValue((const IClientNetworkable*)entity));
	}
	//__forceinline const TValue& GetValue(const IClientNetworkable& entity) const { return GetValue(&entity); }
	//__forceinline TValue& GetValue(IClientNetworkable& entity) const { return GetValue(&entity); }

	inline const TValue* TryGetValue(const IClientNetworkable* entity) const
	{
		auto table = entity->GetClientClass()->m_pRecvTable;
		if (table != m_RecvTable)
		{
			if (!FindContainedDataTable(table, m_RecvTable))
				return nullptr;
		}

		return (TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue* TryGetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue*>(TryGetValue((const IClientNetworkable*)entity));
	}
	//__forceinline const TValue* TryGetValue(const IClientNetworkable& entity) const { return TryGetValue(&entity); }
	//__forceinline TValue* TryGetValue(IClientNetworkable& entity) const { return TryGetValue(&entity); }

	constexpr bool IsInit() const
	{
		Assert((m_RecvTable != nullptr) == (m_Offset != std::numeric_limits<ptrdiff_t>::min()));
		return m_RecvTable != nullptr && m_Offset != std::numeric_limits<ptrdiff_t>::min();
	}

	__forceinline constexpr operator bool() const { return IsInit(); }

private:
	constexpr EntityOffset(const RecvTable* recvTable, ptrdiff_t offset) : m_RecvTable(recvTable), m_Offset(offset)
	{
	}

	static bool FindContainedDataTable(const RecvTable* tableParent, const RecvTable* tableChild)
	{
		Assert(tableParent);
		Assert(tableChild);

		for (int i = 0; i < tableParent->m_nProps; i++)
		{
			const RecvProp* baseProp = &tableParent->m_pProps[i];
			if (baseProp->m_RecvType != DPT_DataTable)
				continue;

			if (baseProp->m_pDataTable == tableChild)
				return true;

			if (FindContainedDataTable(baseProp->m_pDataTable, tableChild))
				return true;
		}

		return false;
	}

	friend class Entities;

	ptrdiff_t m_Offset;
	const RecvTable* m_RecvTable;
};