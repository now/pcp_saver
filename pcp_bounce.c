#define WIN32_LEAN_AND_MEAN
#define STRICT

#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <windowsx.h>
#pragma hdrstop

#include "resource.h"

typedef struct tagBALL
{
	POINT pos;
	POINT pos_d;
} BALL, *PBALL;

LRESULT CALLBACK main_wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);
BOOL main_register(HINSTANCE instance);
HWND create_saver(HINSTANCE instace, HWND parent);

BOOL CALLBACK config_wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);
BOOL verify_password(void);
BOOL do_config(HWND parent);
BOOL do_change_password(HWND parent);
UINT do_saver(HWND parent);
void saver_update(void);
void pos_update(PBALL ball);
BOOL split_args(unsigned int *argc, char* argv, char* args);
char *argn(char *args, int n);

typedef VOID (WINAPI *PWDCHANGEPASSWORD)(LPCSTR lpcRegkeyname, HWND hwnd, UINT uiReserved1, UINT uiReserved2);
typedef BOOL (WINAPI *VERIFYSCREENSAVEPWD)(HWND hwnd);

static char g_appname[] = "PCPBounce by pcppopper";
static HWND g_main_wnd;
static HINSTANCE g_main_instance;

#define	CLICKS_NO_CLOSE 3
#define DELTA_VALUE 1
#define UPDATE_INTERVAL 25

struct
{
	HBITMAP bmp;
	HBITMAP mask;
	HBITMAP old_bmp;
	HDC dc_bmp;
	HDC dc_mask;
	HDC dc_tmp;
	HDC dc_bk;
	BITMAP bmpinfo;
	PBALL *balls;
	int c_balls;
	UINT clicks;
	DWORD counter;
	POINT screen_dim;
	BOOL is_preview;
} saver_info;

BOOL split_args(unsigned int *argc, char *argv, char *args)
{
	UINT i = 0;
	OSVERSIONINFO os;

	*argc = 0;

	if (args == NULL || args[0] == '\0')
		return FALSE;

	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&os);

	for (; args[i] != '\0'; i++)
	{
		if (args[i] == ' ' || (os.dwPlatformId == VER_PLATFORM_WIN32_NT && args[i] == ':'))
			argv[i] = '\0';
		else
			argv[i] = args[i];
	}

	argv[i] = '\0';
	argv[++i] = '\0';

	for (i = 0; argv[i] != '\0'; i++)
	{
		while (argv[i] != 0)
			i++;

		(*argc)++;
	}

	return (TRUE);
}

char *argn(char *args, int n)
{
	char *arg = args;
	int i;

	for (i = 0; i < n; i++)
	{
		while(arg[0] != '\0')
			arg++;

		arg++;

		if (arg[0] == '\0')
			return 0;
	}

	return (arg);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev_inst, LPSTR params, int cmd_show)
{
	HWND parent = 0;
	UINT ret_val;
	UINT argc = 0;
	char *argv = (char *)malloc(strlen(params) + 1);

	g_main_instance = inst;

	split_args(&argc, argv, params);

	if (argc > 1)
	{
		parent = (HWND)atoi(argn(argv, 1));

		if (!IsWindow(parent))
		{
			parent = NULL;
			saver_info.is_preview = FALSE;
		}
		else
		{
			saver_info.is_preview = TRUE;
		}
	}

	if (argc)
	{
		if (!stricmp(argn(argv, 0), "/s") || !stricmp(argn(argv, 0), "-s") ||
			!stricmp(argn(argv, 0), "/p") || !stricmp(argn(argv, 0), "-p") ||
			!stricmp(argn(argv, 0), "/l") || !stricmp(argn(argv, 0), "-l"))
		{
			ret_val = do_saver(parent);
		}
		else if (!stricmp(argn(argv, 0), "/a") || !stricmp(argn(argv, 0), "-a"))
		{
			ret_val = do_change_password(parent);
		}
		else
		{
			if (!stricmp(argn(argv, 0), "/c") || !stricmp(argn(argv, 0), "-c"))
			{
				if (parent == NULL)
					parent = GetForegroundWindow();
			}

			if (!IsWindow(parent))
				parent = NULL;

			ret_val = do_config(parent);
		}
	}
	else
	{
		ret_val = do_config(parent);
	}

	free(argv);

	return (ret_val);
}

BOOL main_register(HINSTANCE inst)
{
	WNDCLASSEX class_ex;

	memset(&class_ex, '\0', sizeof(class_ex));

	class_ex.cbSize			= sizeof(WNDCLASSEX);
	class_ex.lpfnWndProc	= main_wndproc;
	class_ex.hInstance		= inst;
	class_ex.hIcon			= LoadIcon(inst, MAKEINTRESOURCE(IDI_APP));
	class_ex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	class_ex.lpszClassName	= g_appname;
	class_ex.hIconSm		= LoadIcon(inst, MAKEINTRESOURCE(IDI_APP));

	if (!RegisterClassEx(&class_ex))
		return (FALSE);
	else
		return (TRUE);
}

HWND create_saver(HINSTANCE inst, HWND parent)
{
	DWORD style		= WS_VISIBLE;
	DWORD style_ex	= 0;
	HWND hwnd;

	if (parent == NULL)
	{
		saver_info.screen_dim.x = GetSystemMetrics(SM_CXSCREEN);
		saver_info.screen_dim.y = GetSystemMetrics(SM_CYSCREEN);

		style |= WS_POPUP;
		style_ex |= WS_EX_TOPMOST;
	}
	else
	{
		RECT wnd_rc;
		GetWindowRect(parent, &wnd_rc);

		saver_info.screen_dim.x = wnd_rc.right - wnd_rc.left;
		saver_info.screen_dim.y = wnd_rc.bottom - wnd_rc.top;

		style |= WS_CHILD;
	}

	hwnd = CreateWindowEx(style_ex, g_appname, g_appname, style,
								0, 0, saver_info.screen_dim.x, saver_info.screen_dim.y,
								parent, NULL, g_main_instance, NULL);

	if (hwnd == NULL)
		return (NULL);

	ShowWindow(hwnd, SW_NORMAL);
	
	return (hwnd);
}

UINT do_saver(HWND parent)
{
	MSG msg;

	if (!main_register(g_main_instance))
		return (0);

	g_main_wnd = create_saver(g_main_instance, parent);

	if (g_main_wnd == NULL)
		return (0);

	while (TRUE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			saver_update();
		}
	} 

	return (msg.wParam);
}

BOOL do_config(HWND parent)
{
	return (DialogBox(g_main_instance, MAKEINTRESOURCE(IDD_ABOUT), parent, config_wndproc));
}

BOOL CALLBACK config_wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   switch (msg)
   {
      case WM_INITDIALOG:
      return (TRUE);
      case WM_COMMAND:
         switch(wparam)
         {
            case IDOK:
               EndDialog(wnd, 0);
            break;
         }
      break;
   }

   return (FALSE);
}

BOOL do_change_password(HWND parent)
{
	HINSTANCE mpr = LoadLibrary("MPR.DLL");
	PWDCHANGEPASSWORD PwdChangePassword;
	if (mpr == NULL)
	{
		MessageBox(parent, "Couldn't load MRP.DLL", "Error", MB_OK | MB_ICONEXCLAMATION);

		return (FALSE);
	}

	PwdChangePassword = (PWDCHANGEPASSWORD)GetProcAddress(mpr, "PwdChangePasswordA");

	if(PwdChangePassword == NULL)
	{
		MessageBox(parent, "Couldn't get PwdChangePassword", "Error", MB_OK | MB_ICONEXCLAMATION);
		FreeLibrary(mpr);

		return (FALSE);
	}

	PwdChangePassword("SCRSAVE", parent, 0, 0);
	FreeLibrary(mpr);

	return (TRUE);
}

BOOL verify_password(void)
{
	UINT usepassword;
	DWORD usepasswordsize = sizeof(usepassword);
	HKEY key;
	long ret;
	VERIFYSCREENSAVEPWD VerifyScreenSavePwd;
	HINSTANCE pass_cpl;

	ret = RegOpenKey(HKEY_CURRENT_USER, "Control Panel\\Desktop", &key);

	if (ret != ERROR_SUCCESS)
		return (TRUE);

	ret = RegQueryValueEx(key, "ScreenSaveUsePassword", NULL, NULL, (LPBYTE)&usepassword, &usepasswordsize);

	if (ret != ERROR_SUCCESS)
	{
		RegCloseKey(key);

		return (TRUE);
	}

	RegCloseKey(key);

	if (!usepassword)
		return (TRUE);

	pass_cpl = LoadLibrary("PASSWORD.CPL");

	if (pass_cpl == NULL)
		return (TRUE);

	VerifyScreenSavePwd = (VERIFYSCREENSAVEPWD)GetProcAddress(pass_cpl, "VerifyScreenSavePwd");
	
	if (VerifyScreenSavePwd == NULL)
	{
		FreeLibrary(pass_cpl);

		return (TRUE);
	}

	ret = VerifyScreenSavePwd(g_main_wnd);
	FreeLibrary(pass_cpl);

	return (ret);
}

LRESULT CALLBACK main_wndproc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DWORD dw;

	switch (msg)
	{
		case WM_CREATE:
			{
				int i;

				srand((unsigned)time(NULL));

				saver_info.c_balls = (rand() % 12) + 1;
				saver_info.clicks = 0;
				saver_info.counter = 0;
				saver_info.bmp	= LoadBitmap(g_main_instance, MAKEINTRESOURCE(IDB_PCP));
				saver_info.mask	= LoadBitmap(g_main_instance, MAKEINTRESOURCE(IDB_PCP_MASK));

				if (saver_info.bmp == NULL || saver_info.mask == NULL)
				{
					MessageBox(wnd, "Failed to load bitmaps.", NULL, MB_OK | MB_ICONEXCLAMATION);
					DestroyWindow(wnd);
					
					return (FALSE);	
				}
				
				saver_info.dc_bmp	= CreateCompatibleDC(NULL);
				saver_info.dc_mask	= CreateCompatibleDC(NULL);

				if (saver_info.dc_bmp == NULL || saver_info.dc_mask == NULL)
				{
					MessageBox(wnd, "Failed to create dcs.", NULL, MB_OK | MB_ICONEXCLAMATION);
					DestroyWindow(wnd);
					
					return (FALSE);	
				}
				
				GetObject(saver_info.bmp, sizeof(saver_info.bmpinfo), &saver_info.bmpinfo);
				SelectObject(saver_info.dc_bmp, saver_info.bmp);
				SelectObject(saver_info.dc_mask, saver_info.mask);

				saver_info.balls = (PBALL *)malloc(sizeof (PBALL) * saver_info.c_balls);

				for (i = 0; i < saver_info.c_balls; i++)
				{
					saver_info.balls[i]		= (PBALL)malloc(sizeof (BALL));
					saver_info.balls[i]->pos.x	= rand() % (saver_info.screen_dim.x - saver_info.bmpinfo.bmWidth);
					saver_info.balls[i]->pos.y	= rand() % (saver_info.screen_dim.y - saver_info.bmpinfo.bmHeight);
					saver_info.balls[i]->pos_d.x	= (rand() % 2 == 1) ? DELTA_VALUE : -DELTA_VALUE;
					saver_info.balls[i]->pos_d.y	= (rand() % 2 == 1) ? DELTA_VALUE : -DELTA_VALUE;
				}

				saver_info.dc_tmp = CreateCompatibleDC(saver_info.dc_bmp);
				saver_info.old_bmp = SelectObject(saver_info.dc_tmp,
										CreateCompatibleBitmap(saver_info.dc_bmp, saver_info.screen_dim.x,
											saver_info.screen_dim.y));

				saver_info.dc_bk	= CreateCompatibleDC(NULL);
				SelectObject(saver_info.dc_bk, CreateCompatibleBitmap(saver_info.dc_tmp,
													saver_info.screen_dim.x, saver_info.screen_dim.y));
				BitBlt(saver_info.dc_bk, 0, 0, saver_info.screen_dim.x, saver_info.screen_dim.y,
						GetDC(NULL), 0, 0, SRCCOPY);

				if (!saver_info.is_preview)
				{
					SetCursor(NULL);
					SystemParametersInfo(SPI_SCREENSAVERRUNNING, TRUE, &dw, 0);
				}
			}
		return (TRUE);
		case WM_CLOSE:
			if (saver_info.is_preview)
				DestroyWindow(wnd);
			else if (verify_password())
				DestroyWindow(wnd);
		break;
		case WM_DESTROY:
			DeleteObject(saver_info.bmp);
			DeleteDC(saver_info.dc_bmp);
			DeleteObject(saver_info.mask);
			DeleteDC(saver_info.dc_mask);
			DeleteObject(SelectObject(saver_info.dc_tmp, saver_info.old_bmp));
			DeleteDC(saver_info.dc_tmp);

			if (!saver_info.is_preview)
			{
				SetCursor(LoadCursor(NULL, IDC_ARROW));
				SystemParametersInfo(SPI_SCREENSAVERRUNNING, FALSE, &dw, 0);
			}

			PostQuitMessage(0);
		break;
		case WM_SYSCOMMAND:
			switch (wparam)
			{
				case SC_CLOSE:
				case SC_SCREENSAVE:
					return (FALSE);
			}
		break;
		case WM_MOUSEMOVE:
		case WM_KEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			if (!saver_info.is_preview)
			{
				if (SetCursor(NULL))
				{
					saver_info.clicks = 0;

					return (0);
				}
				else if (saver_info.clicks > CLICKS_NO_CLOSE)
				{
					PostMessage(wnd, WM_CLOSE, 0, 0);
				}
			}
		break;
		default:
		return (DefWindowProc(wnd, msg, wparam, lparam));
	}
	return 0;
}

void saver_update(void)
{
	if ((GetTickCount() - saver_info.counter) > UPDATE_INTERVAL)
	{
		HDC dc_win = GetDC(g_main_wnd);
//		RECT rc = { 0, 0, saver_info.screen_dim.x, saver_info.screen_dim.y };
		int i;

//		FillRect(saver_info.dc_tmp, &rc, GetStockBrush(BLACK_BRUSH));
		BitBlt(saver_info.dc_tmp, 0, 0, saver_info.screen_dim.x, saver_info.screen_dim.y, saver_info.dc_bk, 0, 0, SRCCOPY);
		
		for (i = 0; i < saver_info.c_balls; i++)
		{
			PBALL ball		= saver_info.balls[i];

			pos_update(ball);

			BitBlt(saver_info.dc_tmp, ball->pos.x, ball->pos.y, saver_info.bmpinfo.bmWidth, saver_info.bmpinfo.bmHeight,
							saver_info.dc_mask, 0, 0, SRCAND);

			BitBlt(saver_info.dc_tmp, ball->pos.x, ball->pos.y, saver_info.bmpinfo.bmWidth, saver_info.bmpinfo.bmHeight,
							saver_info.dc_bmp, 0, 0, SRCPAINT);
		}

		BitBlt(dc_win, 0, 0, saver_info.screen_dim.x, saver_info.screen_dim.y, saver_info.dc_tmp, 0, 0, SRCCOPY);

		ReleaseDC(g_main_wnd, dc_win);

		saver_info.clicks++;
		saver_info.counter = GetTickCount();
	}
}

void pos_update(PBALL ball)
{
	ball->pos.x			+= ball->pos_d.x;
	ball->pos.y			+= ball->pos_d.y;

	if (ball->pos.x <= 0)
		ball->pos_d.x = DELTA_VALUE;	
	else if ((ball->pos.x + saver_info.bmpinfo.bmWidth) >= saver_info.screen_dim.x)
		ball->pos_d.x = -DELTA_VALUE;

	if (ball->pos.y <= 0)
		ball->pos_d.y = DELTA_VALUE;
	else if ((ball->pos.y + saver_info.bmpinfo.bmHeight) >= saver_info.screen_dim.y)
		ball->pos_d.y = -DELTA_VALUE;
}
