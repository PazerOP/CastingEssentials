#pragma once

#include "Entities.h"
#include "EntityOffset.h"

#include <dt_recv.h>

#include <vector>

template<typename TValue>
class EntityOffsetIterator
{
public:
	using self_type = EntityOffsetIterator<TValue>;
	using value_type = std::pair<std::string, EntityOffset<TValue>>;

	EntityOffsetIterator() : m_ExcludeTablesBegin(nullptr), m_ExcludeTablesEnd(nullptr) {}
	EntityOffsetIterator(const RecvTable* table, const RecvTable** excludeTablesBegin, const RecvTable** excludeTablesEnd) :
		m_ExcludeTablesBegin(excludeTablesBegin), m_ExcludeTablesEnd(excludeTablesEnd)
	{
		m_StateStack.emplace_back(table);
		if (m_StateStack.back().GetProp()->m_RecvType != GetSendPropType())
			Increment(true);
	}

	bool operator!=(const self_type& other) const { return !operator==(other); }
	bool operator==(const self_type& other) const { return m_StateStack == other.m_StateStack; }

	value_type operator*() const
	{
		const IteratorLevelState& state = m_StateStack.back();
		const RecvProp& prop = state.m_Table->m_pProps[state.m_Index];

		return value_type(m_NameBuf + prop.m_pVarName, EntityOffset<TValue>(Entities::GetTypeChecker(state.m_Table), m_BaseOffset + prop.m_Offset));
	}
	value_type operator->() const { return operator*(); }

	self_type& operator++() { return Increment(false); }

private:
	self_type& Increment(bool initial)
	{
		if (m_StateStack.empty())
			throw std::out_of_range("Attempted to increment EntityOffsetIterator beyond the end");

		while (!m_StateStack.empty())
		{
			if (!initial && !m_StateStack.back().TryAdvance())
			{
				if (auto found = m_NameBuf.find_last_of('.', m_NameBuf.size() - 2); found != m_NameBuf.npos)
					m_NameBuf.erase(found + 1);
				else
					m_NameBuf.clear();

				m_StateStack.pop_back();
				if (m_StateStack.empty())
					break;

				m_BaseOffset -= m_StateStack.back().GetProp()->m_Offset;
			}
			else if (m_StateStack.back().GetProp()->m_RecvType == DPT_DataTable)
			{
				do
				{
					const RecvProp* prop = m_StateStack.back().GetProp();
					const RecvTable* dt = prop->m_pDataTable;
					if (std::find(m_ExcludeTablesBegin, m_ExcludeTablesEnd, dt) != m_ExcludeTablesEnd)
						break;

					if (!dt->m_nProps)
						break;

					m_BaseOffset += prop->m_Offset;
					m_StateStack.emplace_back(dt);
					m_NameBuf += dt->m_pNetTableName;
					m_NameBuf += ".";

				} while (m_StateStack.back().GetProp()->m_RecvType == DPT_DataTable);
			}

			if (m_StateStack.back().GetProp()->m_RecvType == GetSendPropType())
				break;

			initial = false;
		}

		return *this;
	}

	struct IteratorLevelState
	{
		IteratorLevelState(const RecvTable* table) : m_Table(table) {}

		constexpr bool operator==(const IteratorLevelState& other) const { return m_Table == other.m_Table && m_Index == other.m_Index; }

		const RecvProp* GetProp() const { return &m_Table->m_pProps[m_Index]; }
		bool TryAdvance() { return ++m_Index < m_Table->m_nProps; }

		const RecvTable* m_Table;
		int m_Index = 0;
	};
	std::vector<IteratorLevelState> m_StateStack;
	std::string m_NameBuf;
	const RecvTable** m_ExcludeTablesBegin;
	const RecvTable** m_ExcludeTablesEnd;
	size_t m_BaseOffset = 0;

public:
	static constexpr SendPropType GetSendPropType()
	{
		if constexpr (std::is_integral_v<TValue>)
			return DPT_Int;
		else if constexpr (std::is_same_v<TValue, float>)
			return DPT_Float;
	}
};

template<typename TValue>
class EntityOffsetIterable
{
public:
	constexpr EntityOffsetIterable(const RecvTable* table) : m_Table(table), m_ExcludeTablesBegin(nullptr), m_ExcludeTablesEnd(nullptr) {}
	constexpr EntityOffsetIterable(const RecvTable* table, const RecvTable** excludeTablesBegin, const RecvTable** excludeTablesEnd) :
		m_Table(table), m_ExcludeTablesBegin(excludeTablesBegin), m_ExcludeTablesEnd(excludeTablesEnd) {}
	EntityOffsetIterable(const ClientClass* cc) : EntityOffsetIterable(cc->m_pRecvTable) {}
	EntityOffsetIterable(const ClientClass* cc, const RecvTable** excludeTablesBegin, const RecvTable** excludeTablesEnd) :
		EntityOffsetIterable(cc->m_pRecvTable, excludeTablesBegin, excludeTablesEnd) {}
	EntityOffsetIterable(const IClientNetworkable* networkable) : EntityOffsetIterable(networkable->GetClientClass()) {}
	EntityOffsetIterable(const IClientNetworkable* networkable, const RecvTable** excludeTablesBegin, const RecvTable** excludeTablesEnd) :
		EntityOffsetIterable(networkable->GetClientClass(), excludeTablesBegin, excludeTablesEnd) {}

	EntityOffsetIterator<TValue> begin() const { return EntityOffsetIterator<TValue>(m_Table, m_ExcludeTablesBegin, m_ExcludeTablesEnd); }
	EntityOffsetIterator<TValue> end() const { return EntityOffsetIterator<TValue>(); }

private:
	const RecvTable* m_Table;
	const RecvTable** m_ExcludeTablesBegin;
	const RecvTable** m_ExcludeTablesEnd;
};