#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if defined(_MSC) || defined(__TINYC__)
#include <Windows.h>
#define _sleep(ms) Sleep(ms)
#else
#include <unistd.h>
#define _sleep(ms) usleep((ms) * 1000)
#endif

#define CSFX_IMPL
#include "csfx.h"

#if !defined(__CYGWIN__) && !defined(_WIN32)
#define _LIBNAME "./csfx-temp.so"
#else
#define _LIBNAME "./csfx-temp.dll"
#endif

static struct
{
    int quit;
} app;

void _sighandler(int state)
{
    (void)state;
    if (state == SIGINT)
    {
	app.quit = 1;
    }
}

int main(int argc, char* argv[])
{

    signal(SIGINT, _sighandler);

    csfx_init();
    
    csfx_script_t script;
    csfx_script_init(&script, _LIBNAME);

    csfx_filetime_t files[] =
    {
	{ 0, "./csfx-temp.c" }
    };
    /* Initialize is not needed, 
     * 'cause I want to build csfx-temp when start
     * csfx_watch_files(files, sizeof(files) / sizeof(files[0]));
     */
    
    while (!app.quit)
    {
	if (csfx_watch_files(files, sizeof(files) / sizeof(files[0])))
	{
#if defined(__TINYC__)
	    system("tcc -shared -o " _LIBNAME " csfx-temp.c");
#else
	    system("gcc -shared -o " _LIBNAME " csfx-temp.c");
#endif
	    
	    /* Update folders time after build */
	    csfx_watch_files(files, sizeof(files) / sizeof(files[0]));
	}

	/* Reload module if has a newer library version */
	switch (csfx_script_update(&script))
	{
	case CSFX_FAILED:
	    fprintf(stderr, "Error: Library is failed to load\n");
	    fprintf(stderr, "       %s\n", csfx_script_errmsg(&script));
	    break;

	case CSFX_NONE:
	case CSFX_INIT:
	case CSFX_RELOAD:
	case CSFX_UNLOAD:
	    break;
	    
	default:
	    fprintf(stderr, "Error: csfx has an internal error occur\n");
	    exit(1);
	    break;
	}
	
	_sleep(1000);
    }

    csfx_script_free(&script);
    csfx_quit();
    return 0;
}
