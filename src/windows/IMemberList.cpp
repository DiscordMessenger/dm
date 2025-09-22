#include "IMemberList.hpp"
#include "MemberList.hpp"
#include "MemberListOld.hpp"

IMemberList* IMemberList::CreateMemberList(ChatWindow* parent, LPRECT lprect)
{
	IMemberList* pMList = MemberList::Create(parent, lprect);

	if (!pMList)
		pMList = MemberListOld::Create(parent, lprect);

	return pMList;
}

void IMemberList::InitializeClasses()
{
	MemberList::InitializeClass();
	MemberListOld::InitializeClass();
}
