#pragma once

#include "PluginBase/Exceptions.h"

#include <client_class.h>
#include <dt_recv.h>
#include <iclientnetworkable.h>

#include <limits>
#include <set>
#include <vector>

class IClientNetworkable;

#undef min

class EntityTypeChecker final
{
public:
	EntityTypeChecker() = default;

	__forceinline bool IsInit() const { return !m_ValidRecvTables.empty(); }

	__forceinline bool Match(const IClientNetworkable* ent) const { Assert(ent); return Match(ent->GetClientClass()); }
	__forceinline bool Match(const ClientClass* cc) const { Assert(cc); return Match(cc->m_pRecvTable); }
	bool Match(const RecvTable* table) const
	{
		Assert(table);
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
	inline EntityTypeChecker(const std::set<const RecvTable*>& validRecvTables) :
		m_ValidRecvTables(validRecvTables.begin(), validRecvTables.end())
	{
	}

	friend class Entities;
	mutable std::vector<const RecvTable*> m_ValidRecvTables;
};

template<typename TValue>
class EntityOffset final
{
public:
	inline constexpr EntityOffset() : m_Offset(std::numeric_limits<ptrdiff_t>::min())
	{
	}

	inline const TValue& GetValue(const IClientNetworkable* entity) const
	{
		if (!m_ValidTypes.Match(entity))
		{
			const char* clientClass;
			if (!entity)
				clientClass = "(null entity)";
			else if (auto cc = entity->GetClientClass(); !cc)
				clientClass = "(null ClientClass)";
			else
				clientClass = cc->GetName();

			throw mismatching_entity_offset("FIXME", clientClass);
		}

		return *(TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue& GetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue&>(GetValue((const IClientNetworkable*)entity));
	}

	inline const TValue* TryGetValue(const IClientNetworkable* entity) const
	{
		if (!m_ValidTypes.Match(entity))
			return nullptr;

		return (TValue*)(((std::byte*)entity->GetDataTableBasePtr()) + m_Offset);
	}
	__forceinline TValue* TryGetValue(IClientNetworkable* entity) const
	{
		return const_cast<TValue*>(TryGetValue((const IClientNetworkable*)entity));
	}

	__forceinline ptrdiff_t GetOffset(const IClientNetworkable* entity) const
	{
		if (!m_ValidTypes.Match(entity))
			throw mismatching_entity_offset("FIXME", "FIXME2");

		return m_Offset;
	}

	__forceinline bool IsInit() const { return m_ValidTypes.IsInit(); }

private:
	inline constexpr EntityOffset(EntityTypeChecker&& validRecvTables, ptrdiff_t offset) :
		m_Offset(offset), m_ValidTypes(std::move(validRecvTables))
	{
	}

	friend class Entities;

	ptrdiff_t m_Offset;
	EntityTypeChecker m_ValidTypes;
};