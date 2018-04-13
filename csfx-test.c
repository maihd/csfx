#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CSFX_IMPL
#include "csfx.h"

int main(int argc, char* argv[])
{
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
    const char* libname = "./csfx-temp.so";
#else
    const char* libname = "./csfx-temp.dll";
#endif
    
    csfx_script_t script;
    csfx_script_init(&script, libname);

    csfx_filetime_t files[] =
    {
	{ 0, "./csfx-temp.c" }
    };
    /* Initialize is not needed, 
     * 'cause I want to build csfx-temp when start
     * csfx_watch_files(files, sizeof(files) / sizeof(files[0]));
     */
    
    while (1)
    {
	if (csfx_watch_files(files, sizeof(files) / sizeof(files[0])))
	{
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
	    system("gcc -shared -o csfx-temp.so  csfx-temp.c");
#else
	    system("gcc -shared -o csfx-temp.dll csfx-temp.c");
#endif
	    
	    /* Update folders time after build */
	    csfx_watch_files(files, sizeof(files) / sizeof(files[0]));
	}

	/* Reload module if has a newer library version */
	switch (csfx_script_update(&script))
	{
#if UNUSED_CODE
	case CSFX_FAILED:
	    fprintf(stderr, "Error: Library is failed to load\n");
	    fprintf(stderr, "       %s", csfx_script_errmsg(&script));
	    break;
#endif

	case CSFX_NONE:
	case CSFX_INIT:
	case CSFX_RELOAD:
	    break;
	    
	default:
	    fprintf(stderr, "Error: csfx has an internal error occur\n");
	    exit(1);
	    break;
	}
	
	usleep(1000 * 1000);
    }

    csfx_script_free(&script);
    return 0;
}
