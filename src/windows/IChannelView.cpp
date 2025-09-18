#include "IChannelView.hpp"
#include "ChannelView.hpp"
#include "ChannelViewOld.hpp"

IChannelView* IChannelView::CreateChannelView(HWND hwnd, LPRECT rect)
{
	IChannelView* pChView = nullptr;// ChannelView::Create(hwnd, rect);

	if (!pChView)
		pChView = ChannelViewOld::Create(hwnd, rect);

	return pChView;
}

void IChannelView::InitializeClasses()
{
	ChannelView::InitializeClass();
	ChannelViewOld::InitializeClass();
}
