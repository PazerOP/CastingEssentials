#pragma once

namespace Hooking{ namespace Internal
{
	template<class Type, class RetVal, class... Args> using MemberFnPtr_Const = RetVal(Type::*)(Args...) const;
	template<class Type, class RetVal, class... Args> using MemberFnPtr = RetVal(Type::*)(Args...);
	template<class Type, class RetVal, class... Args> using MemFnVaArgsPtr = RetVal(__cdecl Type::*)(Args..., ...);
	template<class Type, class RetVal, class... Args> using MemFnVaArgsPtr_Const = RetVal(__cdecl Type::*)(Args..., ...) const;

	// Hard cast from __thiscall to __fastcall, this pointer in ECX, garbage in EDX, arguments on stack
	template<class Type, class RetVal, class... Args> using LocalDetourFnPtr = RetVal(__fastcall*)(Type*, void*, Args...);
	template<class RetVal, class... Args> using GlobalDetourFnPtr = RetVal(*)(Args...);
	template<class Type, class RetVal, class... Args> using LocalFnPtr = RetVal(__thiscall*)(Type*, Args...);

	template<class RetVal, class... Args> using GlobalVaArgsFnPtr = RetVal(__cdecl*)(Args..., ...);
	template<class Type, class RetVal, class... Args> using LocalVaArgsFnPtr = RetVal(__cdecl*)(Type*, Args..., ...);

	template<class RetVal, class... Args> using GlobalFunctionalType = std::function<RetVal(Args...)>;
	template<class Type, class RetVal, class... Args> using LocalFunctionalType = std::function<RetVal(Type*, Args...)>;
} }