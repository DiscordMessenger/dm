#include "DoubleBufferingHelper.hpp"
#include "WinUtils.hpp"

DoubleBufferingHelper::DoubleBufferingHelper(HDC hdc, const RECT& paintRect)
{
	m_hdc = hdc;
	m_paintRect = paintRect;

	m_prWidth = paintRect.right - paintRect.left;
	m_prHeight = paintRect.bottom - paintRect.top;

	m_hBitmap = CreateCompatibleBitmap(hdc, m_prWidth, m_prHeight);
	m_hdcMem = CreateCompatibleDC(hdc);
	m_oldObj = SelectObject(m_hdcMem, m_hBitmap);

	SetViewportOrgEx(m_hdcMem, -paintRect.left, -paintRect.top, &m_oldOrg);
}

DoubleBufferingHelper::~DoubleBufferingHelper()
{
	BitBlt(m_hdc, m_paintRect.left, m_paintRect.top, m_prWidth, m_prHeight, m_hdcMem, m_paintRect.left, m_paintRect.top, SRCCOPY);
	SetViewportOrgEx(m_hdcMem, m_oldOrg.x, m_oldOrg.y, NULL);
	SelectObject(m_hdcMem, m_oldObj);
	DeleteDC(m_hdcMem);
	DeleteBitmap(m_hBitmap);
}

HRGN DoubleBufferingHelper::CreateRectRgn(HDC hdc, RECT rect)
{
	POINT pt;
	GetViewportOrgEx(hdc, &pt);
	OffsetRect(&rect, pt.x, pt.y);
	return CreateRectRgnIndirect(&rect);
}
