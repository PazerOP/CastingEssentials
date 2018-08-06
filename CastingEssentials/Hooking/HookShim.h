#pragma once
#include "Hooking/BaseGroupHook.h"

class HookManager;
namespace Hooking
{
	enum ShimType;
	template<class HookType, class... Args> class HookShim final :
		public BaseGroupHook<ShimType, (ShimType)HookType::HOOK_ID, typename HookType::Functional, typename HookType::RetVal, Args...>
	{
		friend class ::HookManager;

	public:
		using Functional = typename HookType::Functional;

		~HookShim()
		{
			if (m_InnerHook)
				DetachHook();

			m_HooksTable.clear();
		}

		Functional GetOriginal() override { return m_InnerHook->GetOriginal(); }
		Hooking::HookType GetType() const override { return m_InnerHook->GetType(); }

		int AddHook(const Functional& newHook) override
		{
			const auto retVal = BaseGroupHookType::AddHook(newHook);
			AddInnerHook(retVal, newHook);
			return retVal;
		}
		bool RemoveHook(int hookID, const char* funcName) override
		{
			bool retVal = BaseGroupHookType::RemoveHook(hookID, funcName);
			RemoveInnerHook(hookID, funcName);
			return retVal;
		}

		void SetState(Hooking::HookAction action) override { Assert(m_InnerHook); m_InnerHook->SetState(action); }

		void InitHook() override {}
		int GetUniqueHookID() const { return (int)HOOK_ID; }

		typedef HookType Inner;
		void AttachHook(const std::shared_ptr<HookType>& innerHook)
		{
			Assert(!m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			m_InnerHook = innerHook;

			std::lock_guard<decltype(m_HooksTableMutex)> hooksTableLock(m_HooksTableMutex);
			for (auto hooks : m_HooksTable)
				AddInnerHook(hooks.first, hooks.second);
		}

	private:
		void DetachHook()
		{
			Assert(m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			for (auto hookID : m_ActiveHooks)
				m_InnerHook->RemoveHook(hookID.second, __FUNCSIG__);

			m_ActiveHooks.clear();
			m_InnerHook.reset();
		}

		void AddInnerHook(uint64 fakeHookID, const Functional& newHook)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto current = m_InnerHook->AddHook(newHook);
				m_ActiveHooks.insert(std::make_pair(fakeHookID, current));
			}
		}
		void RemoveInnerHook(uint64 hookID, const char* funcName)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto& link = m_ActiveHooks.find(hookID);
				if (link != m_ActiveHooks.end())
				{
					m_InnerHook->RemoveHook(link->second, funcName);
					m_ActiveHooks.erase(link);
				}
			}
		}

		std::recursive_mutex m_Mutex;

		std::shared_ptr<HookType> m_InnerHook;
		std::map<uint64, uint64> m_ActiveHooks;
	};
}