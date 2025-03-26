#include "IMemberList.hpp"
#include "MemberList.hpp"
#include "MemberListOld.hpp"

IMemberList* IMemberList::CreateMemberList(HWND hwnd, LPRECT lprect)
{
	IMemberList* pMList = MemberList::Create(hwnd, lprect);

	if (!pMList)
		pMList = MemberListOld::Create(hwnd, lprect);

	return pMList;
}

void IMemberList::InitializeClasses()
{
	MemberList::InitializeClass();
	MemberListOld::InitializeClass();
}
