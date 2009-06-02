/*
 * CLCL
 *
 * fmt_bitmap_view.c
 *
 * Copyright (C) 1996-2005 by Nakashima Tomoaki. All rights reserved.
 *		http://www.nakka.com/
 *		nakka@nakka.com
 */

/* Include Files */
#define _INC_OLE
#include <windows.h>
#undef  _INC_OLE
#ifdef OP_XP_STYLE
#include <uxtheme.h>
#include <tmschema.h>
#endif	// OP_XP_STYLE

#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Message.h"
#include "Ini.h"
#include "Data.h"
#include "Bitmap.h"
#include "fmt_bitmap_view.h"

#include "resource.h"

/* Define */
#define WINDOW_CLASS					TEXT("CLCLBitmapView")

#define WM_SET_STRETCH_BITMAP			(WM_APP + 100)
#define WM_SET_SCROLLBAR				(WM_APP + 101)
#define WM_SHOW_MENU					(WM_APP + 102)

/* �z�C�[�����b�Z�[�W */
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL					0x020A
#endif
#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED					0x031A
#endif

#define SCROLL_LINE						30

/* Global Variables */
typedef struct _BUFFER {
	HBITMAP hbmp;

	HDC draw_dc;
	HBITMAP draw_bmp;
	HBITMAP draw_ret_bmp;
	HBRUSH draw_brush;

	BOOL stretch_mode;
	BOOL free;

#ifdef OP_XP_STYLE
	// XP
	HTHEME hTheme;
#endif	// OP_XP_STYLE
} BUFFER;

// �I�v�V����
extern OPTION_INFO option;

/* Local Function Prototypes */

/*
 * binview_proc - �E�B���h�E�̃v���V�[�W��
 */
static LRESULT CALLBACK binview_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	SCROLLINFO si;
	RECT window_rect;
	HDC hdc, mdc;
	BITMAP bmp;
	HBITMAP hRetBmp;
	BUFFER *bf;
	DATA_INFO *di;
	BYTE *mem;
	OSVERSIONINFO osvi;
	int max, min, pos;
	int i;

	switch (msg) {
	case WM_CREATE:
		// �E�B���h�E�쐬
		if ((bf = mem_calloc(sizeof(BUFFER))) == NULL) {
			return -1;
		}

#ifdef OP_XP_STYLE
		// XP
		bf->hTheme = theme_open(hWnd);
#endif	// OP_XP_STYLE

		// �`��p���
		hdc = GetDC(hWnd);
		GetClientRect(hWnd, &window_rect);
		bf->draw_dc = CreateCompatibleDC(hdc);
		bf->draw_bmp = CreateCompatibleBitmap(hdc, window_rect.right, window_rect.bottom);
		bf->draw_ret_bmp = SelectObject(bf->draw_dc, bf->draw_bmp);
		ReleaseDC(hWnd, hdc);
		// �w�i�u���V
		bf->draw_brush = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));
		// option
		bf->stretch_mode = option.fmt_bmp_stretch_mode;

		SetWindowLong(hWnd, GWL_USERDATA, (LPARAM)bf);
		ImmAssociateContext(hWnd, (HIMC)NULL);
		break;

	case WM_CLOSE:
		// �E�B���h�E�����
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) != NULL) {
			SetWindowLong(hWnd, GWL_USERDATA, (LPARAM)0);

#ifdef OP_XP_STYLE
			// XP
			theme_close(bf->hTheme);
#endif	// OP_XP_STYLE
			if (bf->hbmp != NULL && bf->free == TRUE) {
				DeleteObject(bf->hbmp);
			}
			SelectObject(bf->draw_dc, bf->draw_ret_bmp);
			DeleteObject(bf->draw_bmp);
			DeleteDC(bf->draw_dc);
			DeleteObject(bf->draw_brush);

			option.fmt_bmp_stretch_mode = bf->stretch_mode;
			mem_free(&bf);
		}
		// �E�B���h�E�̔j��
		return DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_SIZE:
		// �T�C�Y�ύX
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
			break;
		}

		// �`����̍X�V
		GetClientRect(hWnd, &window_rect);
		hdc = GetDC(hWnd);
		SelectObject(bf->draw_dc, bf->draw_ret_bmp);
		DeleteObject(bf->draw_bmp);
		bf->draw_bmp = CreateCompatibleBitmap(hdc, window_rect.right, window_rect.bottom);
		bf->draw_ret_bmp = SelectObject(bf->draw_dc, bf->draw_bmp);
		ReleaseDC(hWnd, hdc);

		SendMessage(hWnd, WM_SET_STRETCH_BITMAP, 0, 0);
		SendMessage(hWnd, WM_SET_SCROLLBAR, 0, 0);
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_EXITSIZEMOVE:
		// �T�C�Y�ύX����
		break;

	case WM_SETFOCUS:
		break;

	case WM_KILLFOCUS:
		break;

	case WM_GETDLGCODE:
		return DLGC_WANTALLKEYS;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
		SetFocus(hWnd);
		break;

	case WM_RBUTTONUP:
		SendMessage(hWnd, WM_SHOW_MENU, 0, 0);
		break;

	case WM_PAINT:
		// �`��
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
			break;
		}
		hdc = BeginPaint(hWnd, &ps);
		if (bf->stretch_mode == FALSE) {
			// �w�i�h��Ԃ�
			FillRect(bf->draw_dc, &ps.rcPaint, bf->draw_brush);
			if (bf->hbmp != NULL) {
				mdc = CreateCompatibleDC(hdc);
				hRetBmp = SelectObject(mdc, bf->hbmp);

				// BITMAP �̕`��
				BitBlt(bf->draw_dc,
					ps.rcPaint.left,
					ps.rcPaint.top,
					ps.rcPaint.right,
					ps.rcPaint.bottom,
					mdc,
					ps.rcPaint.left + GetScrollPos(hWnd, SB_HORZ),
					ps.rcPaint.top + GetScrollPos(hWnd, SB_VERT),
					SRCCOPY);
				SelectObject(mdc, hRetBmp);
				DeleteDC(mdc);
			}
		}
		// �`��
		BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom,
			bf->draw_dc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

		EndPaint(hWnd, &ps);
		break;

	case WM_ERASEBKGND:
		return 1;

#ifdef OP_XP_STYLE
	case WM_NCPAINT:
		// XP�p�̔w�i�`��
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL ||
			theme_draw(hWnd, (HRGN)wParam, bf->hTheme) == FALSE) {
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
		break;

	case WM_THEMECHANGED:
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
			break;
		}
		// XP�e�[�}�̕ύX
		theme_close(bf->hTheme);
		bf->hTheme = theme_open(hWnd);
		break;
#endif	// OP_XP_STYLE

	case WM_HSCROLL:
		GetScrollRange(hWnd, SB_HORZ, &min, &max);
		if (max == 1) {
			break;
		}
		i = pos = GetScrollPos(hWnd, SB_HORZ);
		GetClientRect(hWnd, &window_rect);

		switch (LOWORD(wParam)) {
		case SB_TOP:
			pos = 0;
			break;

		case SB_BOTTOM:
			pos = max;
			break;

		case SB_LINELEFT:
			pos -= SCROLL_LINE;
			break;

		case SB_LINERIGHT:
			pos += SCROLL_LINE;
			break;

		case SB_PAGELEFT:
			pos -= window_rect.right;
			break;

		case SB_PAGERIGHT:
			pos += window_rect.right;
			break;

		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_HORZ, &si);
			pos = si.nTrackPos;
			break;
		}
		SetScrollPos(hWnd, SB_HORZ, pos, TRUE);

		pos = GetScrollPos(hWnd, SB_HORZ);
		ScrollWindowEx(hWnd, (i - pos), 0, NULL, &window_rect, NULL, NULL, SW_INVALIDATE | SW_ERASE);
		break;

	case WM_VSCROLL:
		GetScrollRange(hWnd, SB_VERT, &min, &max);
		if (max == 1) {
			break;
		}
		i = pos = GetScrollPos(hWnd, SB_VERT);
		GetClientRect(hWnd, &window_rect);

		switch (LOWORD(wParam)) {
		case SB_TOP:
			pos = 0;
			break;

		case SB_BOTTOM:
			pos = max;
			break;

		case SB_LINEUP:
			pos -= SCROLL_LINE;
			break;

		case SB_LINEDOWN:
			pos += SCROLL_LINE;
			break;

		case SB_PAGEUP:
			pos -= window_rect.bottom;
			break;

		case SB_PAGEDOWN:
			pos += window_rect.bottom;
			break;

		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_VERT, &si);
			pos = si.nTrackPos;
			break;
		}
		SetScrollPos(hWnd, SB_VERT, pos, TRUE);

		pos = GetScrollPos(hWnd, SB_VERT);
		ScrollWindowEx(hWnd, 0, (i - pos), NULL, &window_rect, NULL, NULL, SW_INVALIDATE | SW_ERASE);
		break;

	case WM_MOUSEWHEEL:
		for (i = 0; i < 3; i++) {
			SendMessage(hWnd, WM_VSCROLL, ((short)HIWORD(wParam) > 0) ? SB_LINEUP : SB_LINEDOWN, 0);
		}
		break;

	case WM_KEYDOWN:
		switch ((int)wParam) {
		case VK_APPS:
			SendMessage(hWnd, WM_SHOW_MENU, 0, 0);
			break;

		case VK_TAB:
			SetFocus(GetParent(hWnd));
			break;

		case VK_LEFT:
			SendMessage(hWnd, WM_HSCROLL, SB_LINELEFT, 0);
			break;

		case VK_RIGHT:
			SendMessage(hWnd, WM_HSCROLL, SB_LINERIGHT, 0);
			break;

		case VK_UP:
			SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
			break;

		case VK_DOWN:
			SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
			break;

		case VK_HOME:
			if (GetAsyncKeyState(VK_SHIFT) >= 0) {
				SendMessage(hWnd, WM_VSCROLL, SB_TOP, 0);
			} else {
				SendMessage(hWnd, WM_HSCROLL, SB_TOP, 0);
			}
			break;

		case VK_END:
			if (GetAsyncKeyState(VK_SHIFT) >= 0) {
				SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, 0);
			} else {
				SendMessage(hWnd, WM_HSCROLL, SB_BOTTOM, 0);
			}
			break;

		case VK_NEXT:
			if (GetAsyncKeyState(VK_SHIFT) >= 0) {
				SendMessage(hWnd, WM_VSCROLL, SB_PAGEDOWN, 0);
			} else {
				SendMessage(hWnd, WM_HSCROLL, SB_PAGERIGHT, 0);
			}
			break;

		case VK_PRIOR:
			if (GetAsyncKeyState(VK_SHIFT) >= 0) {
				SendMessage(hWnd, WM_VSCROLL, SB_PAGEUP, 0);
			} else {
				SendMessage(hWnd, WM_HSCROLL, SB_PAGELEFT, 0);
			}
			break;
		}
		break;

	case WM_SET_BMPDATA:
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
			break;
		}
		if (bf->hbmp != NULL && bf->free == TRUE) {
			DeleteObject(bf->hbmp);
		}
		bf->hbmp = NULL;
		bf->free = FALSE;

		// �f�[�^�ݒ�
		if ((di = (DATA_INFO *)lParam) != NULL && di->data != NULL) {
			if (lstrcmpi(di->format_name, TEXT("BITMAP")) != 0) {
				if ((mem = GlobalLock(di->data)) == NULL) {
					return FALSE;
				}
				bf->hbmp = dib_to_bitmap(mem);
				GlobalUnlock(di->data);
				bf->free = TRUE;
			} else {
				bf->hbmp = di->data;
				bf->free = FALSE;
			}
			SendMessage(hWnd, WM_SET_SCROLLBAR, 0, 0);

		} else {
			EnableScrollBar(hWnd, SB_HORZ, ESB_DISABLE_BOTH);
			EnableScrollBar(hWnd, SB_VERT, ESB_DISABLE_BOTH);

			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
			si.nMax = (bf->stretch_mode == FALSE) ? 1 : 0;
			SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
			SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
		}
		SendMessage(hWnd, WM_SET_STRETCH_BITMAP, 0, 0);
		break;

	case WM_SET_STRETCH_MODE:
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
			break;
		}
		bf->stretch_mode = (BOOL)lParam;

		SendMessage(hWnd, WM_SET_SCROLLBAR, 0, 0);
		SendMessage(hWnd, WM_SET_STRETCH_BITMAP, 0, 0);
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_SET_STRETCH_BITMAP:
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL || bf->stretch_mode == FALSE) {
			break;
		}
		GetClientRect(hWnd, &window_rect);
		if (bf->hbmp == NULL) {
			// �w�i�h��Ԃ�
			FillRect(bf->draw_dc, &window_rect, bf->draw_brush);
			break;
		}
		// �摜���擾
		GetObject(bf->hbmp, sizeof(BITMAP), &bmp);

		mdc = CreateCompatibleDC(bf->draw_dc);
		hRetBmp = SelectObject(mdc, bf->hbmp);
		//OS�o�[�W�����̃`�F�b�N
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&osvi);
		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			SetStretchBltMode(bf->draw_dc, HALFTONE);
			SetBrushOrgEx(bf->draw_dc, 0, 0, NULL);
		} else {
			SetStretchBltMode(bf->draw_dc, COLORONCOLOR);
		}
		// �L�k�摜�̕`��
		SetBrushOrgEx(bf->draw_dc, 0, 0, NULL);
		StretchBlt(bf->draw_dc, 0, 0, window_rect.right, window_rect.bottom,
			mdc, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);
		SelectObject(mdc, hRetBmp);
		DeleteDC(mdc);
		break;

	case WM_SET_SCROLLBAR:
		// �X�N���[���o�[�ݒ�
		if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL || bf->hbmp == NULL) {
			break;
		}
		if (GetObject(bf->hbmp, sizeof(BITMAP), &bmp) == 0) {
			break;
		}

		GetClientRect(hWnd, &window_rect);
		if (bf->stretch_mode == FALSE && window_rect.right < bmp.bmWidth - 1) {
			EnableScrollBar(hWnd, SB_HORZ, ESB_ENABLE_BOTH);

			pos = GetScrollPos(hWnd, SB_HORZ);
			pos = (pos < bmp.bmWidth - window_rect.right) ? pos : bmp.bmWidth - window_rect.right;

			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
			si.nMin = 0;
			si.nMax = bmp.bmWidth - 1;
			si.nPage = window_rect.right;
			si.nPos = pos;
			si.nTrackPos = 0;
			SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
		} else {
			EnableScrollBar(hWnd, SB_HORZ, ESB_DISABLE_BOTH);

			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
			si.nMax = (bf->stretch_mode == FALSE) ? 1 : 0;
			SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
		}

		GetClientRect(hWnd, &window_rect);
		if (bf->stretch_mode == FALSE && window_rect.bottom < bmp.bmHeight - 1) {
			EnableScrollBar(hWnd, SB_VERT, ESB_ENABLE_BOTH);

			pos = GetScrollPos(hWnd, SB_VERT);
			pos = (pos < bmp.bmHeight - window_rect.bottom) ? pos : bmp.bmHeight - window_rect.bottom;

			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
			si.nMin = 0;
			si.nMax = bmp.bmHeight - 1;
			si.nPage = window_rect.bottom;
			si.nPos = pos;
			si.nTrackPos = 0;
			SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
		} else {
			EnableScrollBar(hWnd, SB_VERT, ESB_DISABLE_BOTH);

			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
			si.nMax = (bf->stretch_mode == FALSE) ? 1 : 0;
			SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
		}
		break;

	case WM_SHOW_MENU:
		{
			HMENU hMenu;
			POINT apos;

			if ((bf = (BUFFER *)GetWindowLong(hWnd, GWL_USERDATA)) == NULL) {
				break;
			}
			// ���j���[�̍쐬
			hMenu = CreatePopupMenu();
			AppendMenu(hMenu, MF_STRING | (bf->stretch_mode == 1) ? MF_CHECKED : 0,
				1, message_get_res(IDS_BITMAP_MENU_STRETCH));
			// ���j���[�̕\��
			GetCursorPos((LPPOINT)&apos);
			i = TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_RETURNCMD, apos.x, apos.y, 0, hWnd, NULL);
			DestroyMenu(hMenu);
			if (i <= 0) {
				break;
			}
			switch (i) {
			case 1:
				SendMessage(hWnd, WM_SET_STRETCH_MODE, 0, !bf->stretch_mode);
				break;
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

/*
 * bmpview_regist - �E�B���h�E�N���X�̓o�^
 */
BOOL bmpview_regist(const HINSTANCE hInstance)
{
	WNDCLASS wc;

	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)binview_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNSHADOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WINDOW_CLASS;
	// �E�B���h�E�N���X�̓o�^
	return RegisterClass(&wc);
}

/*
 * bmpview_create - �r�b�g�}�b�v�r���[�A�̍쐬
 */
HWND bmpview_create(const HINSTANCE hInstance, const HWND pWnd, int id)
{
	HWND hWnd;

	// �E�B���h�E�̍쐬
	hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, WINDOW_CLASS,
		TEXT(""),
		WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
		0, 0, 0, 0, pWnd, (HMENU)id, hInstance, NULL);
	return hWnd;
}
/* End of source */
