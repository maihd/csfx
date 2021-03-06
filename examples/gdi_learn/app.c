#define APP_EXPORT
#include "app.h"

#define CSFX_IMPL
#include <csfx.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>

typedef void (*paint_f)(HDC hdc, int width, int height);

/**
 * Application state
 */
typedef struct
{
    /* Misc */
    int     quit;
    HWND    hwnd;
    paint_f on_paint;

    /* Drawing fields */
    HDC hdc;
    int width;
    int height;
    struct
    {
	HDC     hdc;
	HBITMAP bmp;
    } backbuf;
} app_t;

static app_t app;

void app_sighandler(int sig)
{
    if (sig == SIGINT)
    {
	app.quit = 1;
    }
}

int app_isquit(void)
{
    return app.quit;
}

void app_quit(void);
void app_init(HWND hwnd)
{
    HDC  hdc;
    RECT rect;
    int  width;
    int  height;
    
    hdc = GetDC(hwnd);
	
    GetClientRect(hwnd, &rect);
    width  = rect.right - rect.left;
    height = rect.bottom - rect.top; 
	
    app.hdc         = hdc;
    app.hwnd        = hwnd;
    app.width       = width;
    app.height      = height;
    app.backbuf.hdc = CreateCompatibleDC(hdc);
    app.backbuf.bmp = CreateCompatibleBitmap(hdc, width, height);

    SaveDC(app.backbuf.hdc);
    SelectObject(app.backbuf.hdc, app.backbuf.bmp);
    
    signal(SIGINT, app_sighandler);

    /* Listen exit event */
    atexit(app_quit);
}

void app_quit(void)
{
    ReleaseDC(app.hwnd, app.hdc);
    ReleaseDC(app.hwnd, app.backbuf.hdc);
    DeleteObject(app.backbuf.bmp);
    DestroyWindow(app.hwnd);
    
    signal(SIGINT, SIG_DFL);

    ZeroMemory(&app, sizeof(app));
    app.quit = 1;
}

void app_resize(void)
{
    RECT rect;
    int  width;
    int  height;
    
    DeleteObject(app.backbuf.bmp);

    GetClientRect(app.hwnd, &rect);
    width  = rect.right - rect.left;
    height = rect.bottom - rect.top;

    app.width       = width;
    app.height      = height;
    app.backbuf.bmp = CreateCompatibleBitmap(app.hdc, width, height);
    SelectObject(app.backbuf.hdc, app.backbuf.bmp);
}

void app_clear(void)
{
    int mode = BLACKNESS;
    BitBlt(app.backbuf.hdc, 0, 0, app.width, app.height, 0, 0, 0, mode);
}

void app_present(void)
{
    int mode   = SRCCOPY;
    int width  = app.width;
    int height = app.height;
    BitBlt(app.hdc, 0, 0, width, height, app.backbuf.hdc, 0, 0, mode);
}

static LRESULT CALLBACK app_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_CLOSE:
    case WM_DESTROY:
	PostQuitMessage(0);
	break;

    case WM_QUIT:
	app.quit = 1;
	break;

    case WM_PAINT:
	break;

    case WM_CREATE:
        app_init(hwnd);
	break;

    case WM_SIZE:
	app_resize();
	break;
    }
    
    return DefWindowProc(hwnd, msg, w, l);
}

void app_usleep(long us)
{
    /* Actually return NTSTATUS, and 'typedef LONG NTSTATUS;' =)) */
    typedef LONG (__stdcall *NtDelayExecution_f)(BOOL, PLARGE_INTEGER);
    static NtDelayExecution_f NtDelayExecution;
    
    if (!NtDelayExecution)
    {
	HMODULE module   = GetModuleHandle("ntdll.dll");
	const char* func = "NtDelayExecution";
	NtDelayExecution = (NtDelayExecution_f)GetProcAddress(module, func);
	if (!NtDelayExecution)
	{
	    return;
	}
    }
    
    LARGE_INTEGER times;
    times.QuadPart = -us * 10;
    NtDelayExecution(FALSE, &times);
}

void app_msgbox(const char* title, const char* message, int mode)
{
    MessageBox(app.hwnd, message, title, mode);
}

void app_error(const char* fmt, ...)
{
    char msg[1024];
    char fnl_fmt[1024];
	
    sprintf(fnl_fmt, "Error: %s\n", fmt);
    
    va_list ap;
    va_start(ap, fmt);
    vsprintf(msg, fnl_fmt, ap);
    va_end(ap);

    app_msgbox(APP_TITLE, msg, MB_OK);
    app_quit();
    exit(1);
}

void app_loginfo(const char* fmt, ...)
{
    char final_fmt[1024];

    sprintf(final_fmt, "=> %s\n", fmt);

    va_list ap;
    va_start(ap, fmt);
    vprintf(final_fmt, ap);
    va_end(ap);
}

void app_logerror(const char* fmt, ...)
{
    char final_fmt[1024];
    
    sprintf(final_fmt, "=> Error: %s\n", fmt);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, final_fmt, ap);
    va_end(ap);
}

void app_create_window(HINSTANCE hInstance)
{
    WNDCLASS wndclass;
    ZeroMemory(&wndclass, sizeof(wndclass));

    wndclass.hInstance     = hInstance;
    wndclass.lpfnWndProc   = app_wndproc;
    wndclass.lpszClassName = "__wndclass__";
    wndclass.style         = CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW;
    //wndclass.hbrBackground = GetStockObject(BLACK_BRUSH);
    wndclass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClass(&wndclass))
    {
        app_error("error: create window class failed\n");
    }

    HWND hwnd = CreateWindow(
        wndclass.lpszClassName,
	APP_TITLE,
	WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
	NULL,
	NULL,
	hInstance,
	NULL);
    if (!hwnd)
    {
	app_error("error: create window failed\n");
    }
}

void app_update(csfx_script_t* script)
{
    /* Update application state */
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
	TranslateMessage(&msg); 
	DispatchMessage(&msg);
    }

    if (msg.message == WM_QUIT)
    {
	app.quit = 1;
    }

    /*******************/
    /***** Drawing *****/
    /*******************/
    app_clear(); /* clear application backbuffer */

    /* Call on_paint */
    if (app.on_paint)
    {
	csfx_try (script)
	{
	    app.on_paint(app.backbuf.hdc, app.width, app.height);
	}
	csfx_except (script)
	{
	    app.on_paint = NULL;
	    switch (script->errcode)
	    {
	    case CSFX_ERROR_SEGFAULT:
		app_logerror("Segment fault");
		break;
		
	    case CSFX_ERROR_NONE:
	    default:
		app_logerror("An error occur but received CSFX_ERROR_NONE");
		break;
	    }
	}
    }

    /* Present the drawed bitmap in backbuffer */
    app_present();

    
    /* Reloading script */
    switch (csfx_script_update(script))
    {
    case CSFX_INIT:
    case CSFX_RELOAD:
	app.on_paint = (paint_f)csfx_script_symbol(script, "on_paint");
	app_loginfo("Load on_paint() function: 0x%p", app.on_paint);
	break;

    case CSFX_QUIT:
    case CSFX_UNLOAD:
	app.on_paint = NULL;
	break;
	    
    default:
	break;
    }
}
