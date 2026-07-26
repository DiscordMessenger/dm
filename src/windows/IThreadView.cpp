#include "IThreadView.hpp"
#include "ThreadView.hpp"
#include "ThreadViewOld.hpp"

IThreadView* IThreadView::CreateThreadView(HWND hWnd, LPRECT lprect)
{
	IThreadView* pThrView = ThreadView::Create(hWnd, lprect);
	
	//if (!pThrView)
	//	pThrView = ThreadViewOld::Create(hWnd, lprect);

	return pThrView;
}

void IThreadView::InitializeClasses()
{
	ThreadView::InitializeClass();
	//ThreadViewOld::InitializeClass();
}

ThreadListItem IThreadView::Simplify(const Channel& chan)
{
	ThreadListItem tli;
	tli.m_id = chan.m_snowflake;
	tli.m_ownerID = chan.m_ownerUser;
	tli.m_name = chan.m_name;
	tli.m_lastSentMsg = chan.m_lastSentMsg;
	tli.m_lastViewedMsg = chan.m_lastViewedMsg;
	return tli;
}
