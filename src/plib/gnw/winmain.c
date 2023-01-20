#include "plib/gnw/winmain.h"

#include <signal.h>

#include "game/main.h"
#include "plib/gnw/doscmdln.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"

static BOOL LoadDirectX();
static void UnloadDirectX(void);

// 0x53A280
HWND GNW95_hwnd = NULL;

// 0x53A284
HINSTANCE GNW95_hInstance = NULL;

// 0x53A288
LPSTR GNW95_lpszCmdLine = NULL;

// 0x53A28C
int GNW95_nCmdShow = 0;

// 0x53A290
BOOL GNW95_isActive = FALSE;

// 0x53A294
HANDLE GNW95_mutex = NULL;

// 0x53A298
HMODULE GNW95_hDDrawLib = NULL;

// 0x53A29C
HMODULE GNW95_hDInputLib = NULL;

// 0x53A2A0
HMODULE GNW95_hDSoundLib = NULL;

// 0x6B0760
char GNW95_title[256];

// 0x4C9C90
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    DOSCmdLine args;

    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() == ERROR_SUCCESS) {
        ShowCursor(FALSE);
        if (InitClass(hInstance)) {
            if (InitInstance(hInstance, nCmdShow)) {
                if (LoadDirectX()) {
                    GNW95_hInstance = hInstance;
                    GNW95_lpszCmdLine = lpszCmdLine;
                    GNW95_nCmdShow = nCmdShow;
                    DOSCmdLineInit(&args);
                    if (DOSCmdLineCreate(&args, lpszCmdLine)) {
                        signal(1, SignalHandler);
                        signal(3, SignalHandler);
                        signal(5, SignalHandler);
                        GNW95_isActive = TRUE;
                        gnw_main(args.numArgs, args.args);
                        DOSCmdLineDestroy(&args);
                        return 1;
                    }
                }
            }
        }
        CloseHandle(GNW95_mutex);
    }
    return 0;
}

// 0x4C9D84
BOOL InitClass(HINSTANCE hInstance)
{
    WNDCLASSA windowClass;
    windowClass.style = CS_VREDRAW | CS_HREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = NULL;
    windowClass.hCursor = NULL;
    windowClass.hbrBackground = NULL;
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = "GNW95 Class";

    return RegisterClassA(&windowClass);
}

// 0x4C9DF4
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    return DetectRequirements();
}

// 0x4C9DF4
BOOL DetectRequirements()
{
    OSVERSIONINFOA info;
    BOOL result;

    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

#pragma warning(suppress : 4996 28159)
    if (!GetVersionExA(&info)) {
        return TRUE;
    }

    result = TRUE;

    if (info.dwPlatformId == VER_PLATFORM_WIN32s || (info.dwPlatformId == VER_PLATFORM_WIN32_NT && info.dwMajorVersion < 4)) {
        result = FALSE;
    }

    if (!result) {
        MessageBoxA(NULL, "This program requires Windows 95 or Windows NT version 4.0 or greater.", "Wrong Windows Version", MB_ICONSTOP);
    }

    return result;
}

// 0x4C9E60
static BOOL LoadDirectX()
{
    GNW95_hDDrawLib = LoadLibraryA("DDRAW.DLL");
    if (GNW95_hDDrawLib == NULL) {
        goto err;
    }

    GNW95_DirectDrawCreate = (PFNDDRAWCREATE)GetProcAddress(GNW95_hDDrawLib, "DirectDrawCreate");
    if (GNW95_DirectDrawCreate == NULL) {
        goto err;
    }

    GNW95_hDInputLib = LoadLibraryA("DINPUT.DLL");
    if (GNW95_hDInputLib == NULL) {
        goto err;
    }

    GNW95_DirectInputCreate = (PFNDINPUTCREATE)GetProcAddress(GNW95_hDInputLib, "DirectInputCreateA");
    if (GNW95_DirectInputCreate == NULL) {
        goto err;
    }

    GNW95_hDSoundLib = LoadLibraryA("DSOUND.DLL");
    if (GNW95_hDSoundLib == NULL) {
        goto err;
    }

    GNW95_DirectSoundCreate = (PFNDSOUNDCREATE)GetProcAddress(GNW95_hDSoundLib, "DirectSoundCreate");
    if (GNW95_DirectSoundCreate == NULL) {
        goto err;
    }

    atexit(UnloadDirectX);

    return TRUE;

err:
    UnloadDirectX();

    MessageBoxA(NULL, "This program requires Windows 95 with DirectX 3.0a or later or Windows NT version 4.0 with Service Pack 3 or greater.", "Could not load DirectX", MB_ICONSTOP);

    return FALSE;
}

// 0x4C9F18
static void UnloadDirectX(void)
{
    if (GNW95_hDSoundLib != NULL) {
        FreeLibrary(GNW95_hDSoundLib);
        GNW95_hDSoundLib = NULL;
        GNW95_DirectDrawCreate = NULL;
    }

    if (GNW95_hDDrawLib != NULL) {
        FreeLibrary(GNW95_hDDrawLib);
        GNW95_hDDrawLib = NULL;
        GNW95_DirectSoundCreate = NULL;
    }

    if (GNW95_hDInputLib != NULL) {
        FreeLibrary(GNW95_hDInputLib);
        GNW95_hDInputLib = NULL;
        GNW95_DirectInputCreate = NULL;
    }
}

// 0x4C9F84
void SignalHandler(int signalID)
{
    win_exit();
}

// 0x4C9F8C
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT updateRect;
    Rect gnwUpdateRect;

    switch (uMsg) {
    case WM_DESTROY:
        exit(EXIT_SUCCESS);
    case WM_PAINT:
        if (GetUpdateRect(hWnd, &updateRect, FALSE)) {
            gnwUpdateRect.ulx = updateRect.left;
            gnwUpdateRect.uly = updateRect.top;
            gnwUpdateRect.lrx = updateRect.right - 1;
            gnwUpdateRect.lry = updateRect.bottom - 1;
            win_refresh_all(&gnwUpdateRect);
        }
        break;
    case WM_SETCURSOR:
        if ((HWND)wParam == GNW95_hwnd) {
            SetCursor(NULL);
            return 1;
        }
        break;
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            return 0;
        }
        break;
    case WM_ACTIVATEAPP:
        GNW95_isActive = wParam;
        if (wParam) {
            GNW95_hook_input(TRUE);
            win_refresh_all(&scr_size);
        } else {
            GNW95_hook_input(FALSE);
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
