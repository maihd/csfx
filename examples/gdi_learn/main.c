#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <Windows.h>

#include "app.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static void execute_and_wait(const char* cmd)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    
    si.cb = sizeof(si);
    
    if (CreateProcessA(NULL,
		       (char*)cmd,
		       NULL, NULL, FALSE, 0,
		       NULL, NULL, &si, &pi))
    {
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		     LPSTR cmdLine, int cmdShow)
{   
    printf("= Windows GDI experiment application =\n");
    printf("======================================\n\n");
    
    app_create_window(hInstance);
    
    if (csfx_init() != 0)
    {
	fprintf(stderr, "error: csfx_init() failed!\n");
	return 1;
    }

    csfx_script_t script;
    csfx_script_init(&script, "script.dll");

    csfx_filetime_t files[] = {
	{ 0, "script.c" },
    };
    csfx_watch_files(files, countof(files));
    
    while (!app_isquit())
    {
	app_update(&script);
	
	if (csfx_watch_files(files, countof(files)))
	{
	    execute_and_wait("build_script.bat");
	}
	
        app_usleep(1000);
    }

    printf("\n");
    printf("======================================\n");
    
    csfx_quit();
    csfx_script_free(&script);
    return 0;
}
