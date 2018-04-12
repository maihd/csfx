/*******************************************
 * csfx - C scripting Framework
 *
 *******************************************/

#ifndef __CSFX_H__
#define __CSFX_H__


#ifndef __csfx__
#define __csfx__
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
# define __csfx_call__ __declspec(dllexport)
#else
# define __csfx_call__ extern
#endif

enum
{
    CSFX_NONE,
    CSFX_INIT,
    CSFX_QUIT,
    CSFX_UNLOAD,
    CSFX_RELOAD,
    CSFX_FAILED,
};

/** 
 *
 */
typedef struct
{
    int         version;
    long        libtime;
    void*       library;
    const char* filepath;
    void*       userdata;
} csfx_script_t;

typedef struct
{
    long        time;
    const char* path;
} csfx_filetime_t;

/** Hot reload library API **/

/**
 * Initialize script
 */
__csfx__ void  csfx_script_init(csfx_script_t* script, const char* path);

/**
 * Free memory usage by script, unload library and raise quit event
 */
__csfx__ void  csfx_script_free(csfx_script_t* script);

/**
 * Update script, check for changed library and reload
 */
__csfx__ int   csfx_script_update(csfx_script_t* script);

/**
 * Get an symbol address from script
 */
__csfx__ void* csfx_script_symbol(csfx_script_t* script, const char* name);

/**
 * Get error message of script
 */
__csfx__ const char* csfx_script_errmsg(csfx_script_t* lib);

/**
 * Watch for files or directories is changed
 * @note: if change the directory after received a changed event
 *        ensure call this again to update time to ignore change
 * @example: 
 *        csfx_filetime_t dir = { 0, "<dirpath>" };
 *        csfx_watch_files(&dir, 1); // Initialize
 *        ...
 *        if (csfx_watch_files(&dir, 1))
 *        {
 *            ... some operations on <dirpath>
 *            csfx_watch_files(&dir, 1); // Ignore change
 *        }
 */
__csfx__ int csfx_watch_files(csfx_filetime_t* files, int count);


/* --------------------------------------------- */
/* ----------------- CSFX_IMPL ----------------- */
/* --------------------------------------------- */

#ifdef CSFX_IMPL

/* Define dynamic library loading API */
#if defined(_MSC_VER)
# include <Windows.h>
# define csfx__dl_load(path)   (void*)LoadLibraryA(path)
# define csfx__dl_free(lib)    FreeLibrary((HMODULE)lib)
# define csfx__dl_symbol(l, n) GetProcAddress((HMODULE)l, n)

static const char* csfx__dl_errmsg(void)
{
    static int  error;
    static char buffer[256];

    int last_error = GetLastError();
    if (last_error != error)
    {
	error = last_error;
	FormatMessageA(
	    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, last_error,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
	    buffer, sizeof(buffer), NULL);
    }
    return buffer;
}
#elif (__unix__)
# include <dlfcn.h>
# define csfx__dl_load(path)   dlopen(path, RTLD_LAZY)
# define csfx__dl_free(lib)    dlclose(lib)
# define csfx__dl_symbol(l, n) dlsym(l, n)
# define csfx__dl_errmsg()     dlerror()
#else
# error "Unsupported platform"
#endif

/* Custom helper functions */
#if defined(_MSC_VER)
static long csfx__last_modify_time(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
    {
	return 0;
    }

    LARGE_INTEGER time;
    time.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    time.LowPart = fad.ftLastWriteTime.dwLowDateTime;

    return (long)(time.QuadPart / 10000000 - 11644473600LL);
}
#elif defined(__unix__)
#include <sys/stat.h>
#include <sys/types.h>

static long csfx__last_modify_time(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
	return 0;
    }

    return (long)st.st_mtime;
}
#else
#error "Unsupported platform"
#endif

typedef void* (*csfx_main_f)(void* userdata, int state);

static const char csfx_main_name[] = "csfx_main";

static int csfx__script_changed(csfx_script_t* script)
{
    time_t src = script->libtime;
    time_t cur = csfx__last_modify_time(script->filepath);
    return cur > src;
}

static int csfx__file_changed(csfx_filetime_t* ft)
{
    time_t src = ft->time;
    time_t cur = csfx__last_modify_time(ft->path);
    
    if (cur > src)
    {
	ft->time = cur;
	return 1;
    }
    else
    {
	return 0;
    }
}

void csfx_script_init(csfx_script_t* script, const char* path)
{
    script->version  = 0;
    script->libtime  = 0;
    script->library  = NULL;
    script->filepath = path;
    script->userdata = NULL;
}

void csfx_script_free(csfx_script_t* script)
{
    if (script->library)
    {
	void* library;
	csfx_main_f main;
	
	library = script->library;
	main = (csfx_main_f)csfx__dl_symbol(library, csfx_main_name);
	if (main)
	{
	    script->userdata = main(script->userdata, CSFX_QUIT);
	}
	csfx__dl_free(library);
    }

    script->version = 0;
    script->libtime = 0;
    script->library = NULL;
}

int csfx_script_update(csfx_script_t* script)
{
    if (csfx__script_changed(script))
    {
	int state;
	void* library;
	csfx_main_f main;

	/* Unload old version */
	library = script->library;
	if (library)
	{
	    main = (csfx_main_f)csfx__dl_symbol(library, csfx_main_name);
	    if (main)
	    {
		script->userdata = main(script->userdata, CSFX_UNLOAD);
	    }
	    csfx__dl_free(library);
	}

	/* Load new version */
	state   = CSFX_FAILED;
	library = csfx__dl_load(script->filepath); 
	if (library)
	{
	    state = !script->library ? CSFX_INIT : CSFX_RELOAD;
	    main  = (csfx_main_f)csfx__dl_symbol(library, csfx_main_name);
	    if (main)
	    {
		script->userdata = main(script->userdata, state);
	    }
	
	    script->library = library;
	    script->version = script->version + 1;
	    script->libtime = csfx__last_modify_time(script->filepath);
	}

	return state;
    }
    
    return CSFX_NONE;
}

void* csfx_script_symbol(csfx_script_t* script, const char* name)
{
    return csfx__dl_symbol(script->library, name);
}

const char* csfx_script_errmsg(csfx_script_t* script)
{
    (void)script;
    return csfx__dl_errmsg();
}

int csfx_watch_files(csfx_filetime_t* files, int count)
{
    int changed = 0;
    for (int i = 0; i < count; i++)
    {
	changed = csfx__file_changed(&files[i]) || changed;
    }
    return changed;
}

#endif /* CSFX_IMPL */

#endif /* __CSFX_H__ */
