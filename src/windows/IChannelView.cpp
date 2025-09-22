#include "IChannelView.hpp"
#include "ChannelView.hpp"
#include "ChannelViewOld.hpp"

IChannelView* IChannelView::CreateChannelView(ChatWindow* parent, LPRECT rect)
{
	IChannelView* pChView = ChannelView::Create(parent, rect);

	if (!pChView)
		pChView = ChannelViewOld::Create(parent, rect);

	return pChView;
}

void IChannelView::InitializeClasses()
{
	ChannelView::InitializeClass();
	ChannelViewOld::InitializeClass();
}
