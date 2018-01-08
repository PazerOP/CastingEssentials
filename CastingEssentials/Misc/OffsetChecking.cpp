#include <client/c_baseanimating.h>

#define OFFSET_CHECK(className, memberName, expectedOffset)														\
	static_assert(!(offsetof(className, memberName) > expectedOffset), "Actual offset greater than expected!");	\
	static_assert(!(offsetof(className, memberName) < expectedOffset), "Actual offset less than expected!");

// Don't trust intellisense... just use the actual errors the compiler emits and
// adjust your padding until you get it right
class OffsetChecking
{
	OFFSET_CHECK(C_BaseAnimating, m_nHitboxSet, 1376);
	OFFSET_CHECK(C_BaseAnimating, m_bDynamicModelPending, 2185);
	OFFSET_CHECK(C_BaseAnimating, m_pStudioHdr, 2192);

	OFFSET_CHECK(studiohdr_t, numbones, 156);
};