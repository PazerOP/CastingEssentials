#pragma once

#include "PluginBase/EntityOffset.h"

#include <algorithm>
#include <map>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

class IClientNetworkable;
class RecvTable;
class RecvProp;
class ClientClass;
enum class TFTeam;

class Entities final
{
public:
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const RecvTable* table, const std::string_view& propertyString);
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const ClientClass* cc, const std::string_view& propertyString);
	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const std::string_view& className, const std::string_view& propertyString);

	template<size_t size> static char* PropIndex(char(&buffer)[size], const char* base, int index)
	{
		sprintf_s(buffer, "%s.%03i", base, index);
		return buffer;
	}

	template<typename TValue> inline static EntityOffset<TValue>
		GetEntityProp(const ClientClass* cc, const char* propertyString)
	{
		const auto found = RetrieveClassPropOffset(cc, propertyString);
		if (found.first < 0)
			throw invalid_class_prop(propertyString);

		return EntityOffset<TValue>(found.second, found.first);
	}
	template<typename TValue> __forceinline static EntityOffset<TValue>
		GetEntityProp(IClientNetworkable* entity, const char* propertyString)
	{
		return GetEntityProp<TValue>((const ClientClass*)(entity->GetClientClass()), propertyString);
	}
	template<typename TValue> inline static EntityOffset<TValue>
		GetEntityProp(const char* clientClassName, const char* propertyString)
	{
		return GetEntityProp<TValue>(GetClientClass(clientClassName), propertyString);
	}

	static bool CheckEntityBaseclass(IClientNetworkable* entity, const char* baseclass);
	static bool CheckTableBaseClass(const RecvTable* tableParent, const RecvTable* tableBase);

	static ClientClass* GetClientClass(const char* className);
	static RecvProp* FindRecvProp(const char* className, const char* propName, bool recursive = true);
	static RecvProp* FindRecvProp(RecvTable* table, const char* propName, bool recursive = true);

	static const TFTeam* TryGetEntityTeam(const IClientNetworkable* entity);
	static TFTeam GetEntityTeamSafe(const IClientNetworkable* entity);

private:
	Entities() = delete;
	~Entities() = delete;

	static bool CheckClassBaseclass(ClientClass *clientClass, const char* baseclass);
	static bool CheckTableBaseclass(RecvTable *sTable, const char* baseclass);

	static std::pair<int, const RecvTable*> RetrieveClassPropOffset(const RecvTable* table, const std::vector<std::string_view>& refPropertyTree,
		std::vector<std::string_view>& currentPropertyTree);

	static std::string ConvertTreeToString(const std::vector<std::string_view>& tree);
	static std::vector<std::string_view> ConvertStringToTree(const std::string_view& str);

	static bool GetSubProp(const RecvTable* table, const std::string_view& propName, const RecvProp*& prop, int& offset);
	static void* GetEntityProp(IClientNetworkable* entity, const char* propertyString, bool throwifMissing = true);

	struct ci_less
	{
	private:
		// We don't care about culture, the only place these strings are going to come from is our code.
		static constexpr const char FAST_TOUPPER[] = "\0\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17\20\21\22\23"
			"\24\25\26\27\30\31\32\33\34\35\36\37\40\41\42\43\44\45\46\47\50\51\52\53\54\55\56\57\60\61\62\63"
			"\64\65\66\67\70\71\72\73\74\75\76\77\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116"
			"\117\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137\140\101\102\103\104\105\106"
			"\107\110\111\112\113\114\115\116\117\120\121\122\123\124\125\126\127\130\131\132\173\174\175\176"
			"\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226"
			"\227\230\231\212\233\214\235\216\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256"
			"\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306"
			"\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336"
			"\337\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326"
			"\367\330\331\332\333\334\335\336\237";

	public:
		typedef int is_transparent;

		template<typename T1, typename T2> inline constexpr bool operator()(const T1& a, const T2& b) const
		{
			const auto min = std::min(a.size(), b.size());
			for (size_t i = 0; i < min; i++)
			{
				const auto c_a = FAST_TOUPPER[a[i]];
				const auto c_b = FAST_TOUPPER[b[i]];

				if (c_a < c_b)
					return true;
				else if (c_a > c_b)
					return false;
			}

			if (a.size() != b.size())
				return a.size() < b.size();
			else
				return false;	// Both strings are equal
		}
	};
};