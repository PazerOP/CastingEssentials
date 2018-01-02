#pragma once
#include <refcount.h>

// Who designed this reference counting system?
template<class T>
class CRefPtrFix : public CRefPtr<T>
{
	typedef CRefPtr<T> BaseClass;

public:
	CRefPtrFix() = default;
	CRefPtrFix(T* pInit) : BaseClass(InlineAddRef(pInit)) { }
	CRefPtrFix(const CRefPtr<T>& from) : BaseClass(InlineAddRef(pInit)) { }

	T* operator=(T* p) { AssignAddRef(p); }

private:
	int operator=(int i) { Assert(!"Terrible design"); return 0; }
};

#define CRefPtr CRefPtrFix