#pragma once

#include <commctrl.h>

#ifndef LVN_HOTTRACK
#define LVN_HOTTRACK (LVN_FIRST - 21)
#endif // LVN_HOTTRACK

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif // LVS_EX_DOUBLEBUFFER

#ifndef COLOR_MENUBAR
#define COLOR_MENUBAR 30
#endif // COLOR_MENUBAR

#ifndef SS_REALSIZECONTROL
#define SS_REALSIZECONTROL 0x00000040
#endif // SS_REALSIZECONTROL

// NOTE: This was added in Windows Vista, so it won't work on XP and earlier. but who cares
#ifndef SMTO_ERRORONEXIT
#define SMTO_ERRORONEXIT 0x20
#endif

#ifdef MINGW_SPECIFIC_HACKS

#if(WINVER >= 0x0500)

#define LV_VIEW_ICON            0x0000
#define LV_VIEW_DETAILS         0x0001
#define LV_VIEW_SMALLICON       0x0002
#define LV_VIEW_LIST            0x0003
#define LV_VIEW_TILE            0x0004
#define LV_VIEW_MAX             0x0004

#define LVGF_NONE           0x00000000
#define LVGF_HEADER         0x00000001
#define LVGF_FOOTER         0x00000002
#define LVGF_STATE          0x00000004
#define LVGF_ALIGN          0x00000008
#define LVGF_GROUPID        0x00000010
#if _WIN32_WINNT >= 0x0600
#define LVGF_SUBTITLE           0x00000100
#define LVGF_TASK               0x00000200
#define LVGF_DESCRIPTIONTOP     0x00000400
#define LVGF_DESCRIPTIONBOTTOM  0x00000800
#define LVGF_TITLEIMAGE         0x00001000
#define LVGF_EXTENDEDIMAGE      0x00002000
#define LVGF_ITEMS              0x00004000
#define LVGF_SUBSET             0x00008000
#define LVGF_SUBSETITEMS        0x00010000
#endif

#define LVGS_NORMAL             0x00000000
#define LVGS_COLLAPSED          0x00000001
#define LVGS_HIDDEN             0x00000002
#define LVGS_NOHEADER           0x00000004
#define LVGS_COLLAPSIBLE        0x00000008
#define LVGS_FOCUSED            0x00000010
#define LVGS_SELECTED           0x00000020
#define LVGS_SUBSETED           0x00000040
#define LVGS_SUBSETLINKFOCUSED  0x00000080

#define LVGA_HEADER_LEFT    0x00000001
#define LVGA_HEADER_CENTER  0x00000002
#define LVGA_HEADER_RIGHT   0x00000004
#define LVGA_FOOTER_LEFT    0x00000008
#define LVGA_FOOTER_CENTER  0x00000010
#define LVGA_FOOTER_RIGHT   0x00000020

typedef struct tagLVGROUP
{
	UINT    cbSize;
	UINT    mask;
	LPWSTR  pszHeader;
	int     cchHeader;

	LPWSTR  pszFooter;
	int     cchFooter;

	int     iGroupId;

	UINT    stateMask;
	UINT    state;
	UINT    uAlign;
#if _WIN32_WINNT >= 0x0600
	LPWSTR  pszSubtitle;
	UINT    cchSubtitle;
	LPWSTR  pszTask;
	UINT    cchTask;
	LPWSTR  pszDescriptionTop;
	UINT    cchDescriptionTop;
	LPWSTR  pszDescriptionBottom;
	UINT    cchDescriptionBottom;
	int     iTitleImage;
	int     iExtendedImage;
	int     iFirstItem;
	UINT    cItems;
	LPWSTR  pszSubsetTitle;
	UINT    cchSubsetTitle;

#endif
} LVGROUP, *PLVGROUP;

#ifndef NIF_REALTIME
#define NIF_REALTIME 0x40
#endif
#ifndef NIF_SHOWTIP
#define NIF_SHOWTIP  0x80
#endif
#ifndef NIIF_USER
#define NIIF_USER 0x4
#endif

#ifndef NIN_BALLOONSHOW
#define NIN_BALLOONSHOW (WM_USER + 2)
#endif
#ifndef NIN_BALLOONHIDE
#define NIN_BALLOONHIDE (WM_USER + 3)
#endif
#ifndef NIN_BALLOONTIMEOUT
#define NIN_BALLOONTIMEOUT (WM_USER + 4)
#endif
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif

#endif // WINVER >= 0x0500

#endif // MINGW_SPECIFIC_HACKS
