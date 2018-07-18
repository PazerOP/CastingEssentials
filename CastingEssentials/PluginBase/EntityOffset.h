#pragma once

#include "PluginBase/Exceptions.h"

#include <dt_recv.h>

#include <limits>
#include <memory>
#include <set>
#include <vector>

class IClientNetworkable;

#undef min

class EntityOffsetBase
{
public:
	__forceinline bool IsInit() const { return !m_ValidRecvTables.empty(); }

protected:
	inline EntityOffsetBase() {}
	inline EntityOffsetBase(const std::set<const RecvTable*>& validRecvTables) :
		m_ValidRecvTables(validRecvTables.begin(), validRecvTables.end())
	{

	}

	bool CheckForRecvTable(const RecvTable* table) const
	{
		const auto& end = m_ValidRecvTables.data() + m_ValidRecvTables.size();
		for (auto iter = m_ValidRecvTables.data(); iter < end; iter++)
		{
			if ((*iter) != table)
				continue;

			if (iter != m_ValidRecvTables.data())
				std::swap(*(iter - 1), *iter);    // Move up one

			return true;
		}

		return false;
	}

private:
	mutable std::vector<const RecvTable*> m_ValidRecvTables;
};

template<typename TValue>
class EntityOffset : EntityOffsetBase
{
public:
	inline constexpr EntityOffset() : m_Offset(std::numeric_limits<ptrdiff_t>::min())
	{
	}

	inline const TValue& GetValue(const IClientNetworkable* entity) const
	{
		if (auto table = entity->GetClientClass()->m_pRecvTable; !CheckForRecvTable(table))
			throw mismatching_entity_offset("FIXME", table->GetName());

		return *(TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue& GetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue&>(GetValue((const IClientNetworkable*)entity));
	}

	inline const TValue* TryGetValue(const IClientNetworkable* entity) const
	{
		if (auto table = entity->GetClientClass()->m_pRecvTable; !CheckForRecvTable(table))
			return nullptr;

		return (TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue* TryGetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue*>(TryGetValue((const IClientNetworkable*)entity));
	}

	using EntityOffsetBase::IsInit;
	__forceinline constexpr operator bool() const { return IsInit(); }

private:
	inline constexpr EntityOffset(const std::set<const RecvTable*>& validRecvTables, ptrdiff_t offset) :
		m_Offset(offset), EntityOffsetBase(validRecvTables)
	{
	}

	friend class Entities;

	ptrdiff_t m_Offset;
};