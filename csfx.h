/*******************************************
 * csfx - C scripting Framework
 *
 *******************************************/

#ifndef __CSFX_H__
#define __CSFX_H__

#ifndef __csfx__
#define __csfx__
#endif

#if defined(__cplusplus)
# define CSFX_EXTERN_C     extern "C" {
# define CSFX_EXTERN_C_END }
#else
# define CSFX_EXTERN_C
# define CSFX_EXTERN_C_END
#endif

CSFX_EXTERN_C;
/* BEGIN EXTERN "C" */

/**
 * CSFX script state
 */
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
 * CSFX script error code
 */
enum
{
    CSFX_ERROR_NONE,
    CSFX_ERROR_ABORT,
    CSFX_ERROR_ALIGN,
    CSFX_ERROR_INSTR,
    CSFX_ERROR_MEMORY,
};

/** 
 * Script data structure
 */
typedef struct
{
    int         state;
    int         errcode;
    long        libtime;
    void*       library;
    void*       userdata;
    const char* filepath;
    const char* temppath;
} csfx_script_t;

/**
 * File data structure
 */
typedef struct
{
    long        time;
    const char* path;
} csfx_filetime_t;

/** Hot reload library API **/

__csfx__ void  csfx_init(void);
__csfx__ void  csfx_quit(void);

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
__csfx__ const char* csfx_script_errmsg(csfx_script_t* script);

#if defined(_MSC_VER)
# define csfx_try(s)    __try
# define csfx_except(s) __except(csfx__seh_filter(s, GetExceptionCode()))
# define csfx_finally   __finally
__csfx__ int csfx__seh_filter(csfx_script_t* script, unsigned long code);
#elif (__unix__) && !defined(__MINGW32__)
# include <signal.h>
# include <setjmp.h>
# define csfx_try(s)						\
    (s)->errcode = sigsetjmp(csfx__jmpenv, CSFX_ERROR_NONE);	\
    if ((s)->errcode == CSFX_ERROR_NONE)

# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   else
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE && (s)->errcode <= CSFX_ERROR_MEMORY

extern __thread sigjmp_buf csfx__jmpenv;
#else
# define csfx_try(s)    if (((s)->errcode = CSFX_ERROR_NONE) || 1)
# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   else
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE && (s)->errcode <= CSFX_ERROR_MEMORY
#endif

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

/**
 * Script main function
 */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
__declspec(dllexport)
#else
extern
#endif
void* csfx_main(void* userdata, int state);

/* --------------------------------------------- */
/* ----------------- CSFX_IMPL ----------------- */
/* --------------------------------------------- */

#ifdef CSFX_IMPL

#include <stdio.h>
#include <stdlib.h>

/* Define dynamic library loading API */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(_WIN32)
# include <Windows.h>
# define csfx__dlib_load(path)   (void*)LoadLibraryA(path)
# define csfx__dlib_free(lib)    FreeLibrary((HMODULE)lib)
# define csfx__dlib_symbol(l, n) (void*)GetProcAddress((HMODULE)l, n)

static const char* csfx__dlib_errmsg(void)
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
# define csfx__dlib_load(path)   dlopen(path, RTLD_LAZY)
# define csfx__dlib_free(lib)    dlclose(lib)
# define csfx__dlib_symbol(l, n) dlsym(l, n)
# define csfx__dlib_errmsg()     dlerror()
#else
# error "Unsupported platform"
#endif


/** Custom helper functions **/
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(_WIN32)

# define CSFX__PATH_LENGTH MAX_PATH

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

    return (long)(time.QuadPart / 10000000L - 11644473600L);
}

static int csfx__copy_file(const char* from_path, const char* to_path)
{
    if (CopyFileA(from_path, to_path, FALSE))
    {
	return 1;
    }
    else
    {
	return 0;
    }
}

int csfx__seh_filter(csfx_script_t* script, unsigned long code)
{
    int errcode = CSFX_ERROR_NONE;
    
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
	errcode = CSFX_ERROR_MEMORY;
	break;
	
    case EXCEPTION_ILLEGAL_INSTRUCTION:
	errcode = CSFX_ERROR_INSTR;
	break;
	
    case EXCEPTION_DATATYPE_MISALIGNMENT:
	errcode = CSFX_ERROR_ALIGN;
	break;
	
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	errcode = CSFX_ERROR_MEMORY;
	break;
	
    case EXCEPTION_STACK_OVERFLOW:
	errcode = CSFX_ERROR_MEMORY;
	break;

    default:
	break;
    }

    script->errcode = errcode;
    if (errcode == CSFX_ERROR_NONE)
    {
	return EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
	return EXCEPTION_EXECUTE_HANDLER;
    }
}

static int csfx__remove_file(const char* path);
static char* csfx__get_temp_path(const char* path);

#if defined(_MSC_VER)
static int csfx__unlock_pdb_file(const char* dllpath)
{
    char drive[12];
    char dir[CSFX__PATH_LENGTH];
    char name[CSFX__PATH_LENGTH];
    char path[CSFX__PATH_LENGTH];
    char scmd[CSFX__PATH_LENGTH];
    
    GetFullPathNameA(dllpath, sizeof(path), path, NULL);
    _splitpath(path, drive, dir, name, NULL);
    sprintf_s(path, CSFX__PATH_LENGTH, "%s%s%s.pdb", drive, dir, name);

    sprintf_s(scmd, CSFX__PATH_LENGTH, "del /Q %s", path);
    return system(scmd);
}
#endif

void csfx_init(void)
{
    /* NULL */
}

void csfx_quit(void)
{
    /* NULL */
}

#elif defined(__unix__)
# include <sys/stat.h>
# include <sys/types.h>
# define CSFX__PATH_LENGTH PATH_MAX

__thread sigjmp_buf csfx__jmpenv;

static long csfx__last_modify_time(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
	return 0;
    }

    return (long)st.st_mtime;
}

static int csfx__copy_file(const char* from_path, const char* to_path)
{
    char scmd[3 * PATH_MAX]; /* 2 path and command */
    sprintf(scmd, "\\cp -fR %s %s", from_path, to_path);
    if (system(scmd) != 0)
    {
	/* Has an error occur */
	return 0;
    }
    else
    {
	return 1;
    }
}

static void csfx__sighandler(int code, siginfo_t* info, void* context)
{
    int errcode;
    switch (code)
    {
    case SIGILL:
	errcode = CSFX_ERROR_INSTR;
	break;

    case SIGBUS:
	errcode = CSFX_ERROR_ALIGN;
	break;

    case SIGABRT:
	errcode = CSFX_ERROR_ABORT;
	break;

    case SIGSEGV:
	errcode = CSFX_ERROR_MEMORY;
	break;
	
    default:
	errcode = CSFX_ERROR_NONE;
	break;
    }
    siglongjmp(csfx__jmpenv, errcode);
}


void csfx_init(void)
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_handler   = NULL;
    sa.sa_sigaction = csfx__sighandler;

    int idx;
    int signals[] = { SIGBUS, SIGILL, SIGSEGV, SIGABRT };
    for (idx = 0; idx < sizeof(signals) / sizeof(signals[0]); idx++)
    {
	if (sigaction(signals[idx], &sa, NULL) != 0)
	{
	    break;
	}
    }
}

void csfx_quit(void)
{
    
}
#else
#error "Unsupported platform"
#endif

static int csfx__remove_file(const char* path)
{
    return remove(path);
}

static char* csfx__get_temp_path(const char* path)
{
    char* res = NULL;
    
    if (path)
    {
	res = (char*)malloc(CSFX__PATH_LENGTH);
	    
	int version = 0;
	while (1)
	{
	    sprintf(res, "%s.%d", path, version++);

	    FILE* file = fopen(res, "r");
	    if (file)
	    {
		fclose(file);
	    }
	    else
	    {
		break;
	    }
	}
    }

    return res;
}

static int csfx__script_changed(csfx_script_t* script)
{
    long src = script->libtime;
    long cur = csfx__last_modify_time(script->filepath);
    return cur > src;
}

static int csfx__file_changed(csfx_filetime_t* ft)
{
    long src = ft->time;
    long cur = csfx__last_modify_time(ft->path);
    
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

static int csfx__call_main(csfx_script_t* script, void* library, int state)
{
    typedef void* (*csfx_main_f)(void*, int);

    const char* name = "csfx_main";
    csfx_main_f func = (csfx_main_f)csfx__dlib_symbol(library, name);

    if (func)
    {
	csfx_try (script)
	{
	    script->userdata = func(script->userdata, state);
	}
	csfx_except (script)
	{
	    return -1;
	}
    }

    return 0;
}

void csfx_script_init(csfx_script_t* script, const char* path)
{
    script->state    = CSFX_NONE;
    script->libtime  = 0;
    script->library  = NULL;
    script->userdata = NULL;
    script->filepath = path;
    script->temppath = csfx__get_temp_path(path);
}

void csfx_script_free(csfx_script_t* script)
{
    /* Raise quit event */
    if (script->library)
    {
	void* library;
	
	library = script->library;
	csfx__call_main(script, library, (script->state = CSFX_QUIT));
	csfx__dlib_free(library);
	
	/* Remove temp library */
	csfx__remove_file(script->temppath); /* Ignore error code */
    }

    /* script->temppath is in heap */
    free((void*)script->temppath);
    
    script->libtime  = 0;
    script->library  = NULL;
    script->filepath = NULL;
    script->temppath = NULL;
}

int csfx_script_update(csfx_script_t* script)
{
    if (csfx__script_changed(script))
    {
	void* library;

	/* Unload old version */
	library = script->library;
	if (library)
	{
	    /* Raise unload event */
	    script->state = CSFX_UNLOAD;
	    csfx__call_main(script, library, script->state);

	    /* Collect garbage */
	    csfx__dlib_free(library);
	    script->library = NULL;

	    if (script->errcode != CSFX_ERROR_NONE)
	    {
		script->state = CSFX_FAILED;
		return script->state;
	    }
	    else
	    {
		return script->state;
	    }
	}

	/* Create and load new temp version */
	csfx__remove_file(script->temppath); /* Remove temp library */
	if (csfx__copy_file(script->filepath, script->temppath))
	{
	    library = csfx__dlib_load(script->temppath); 
	    if (library)
	    {
		int state = script->state; /* new state */
		state = state != CSFX_UNLOAD ? CSFX_INIT : CSFX_RELOAD;
		csfx__call_main(script, library, state);

		script->library = library;
		script->libtime = csfx__last_modify_time(script->filepath);

		#ifdef _MSC_VER
		csfx__unlock_pdb_file(script->filepath);
		#endif

		if (script->errcode != CSFX_ERROR_NONE)
		{
		    csfx__dlib_free(script->library);
		    
		    script->state = CSFX_FAILED;
		    script->library = NULL;
		}
		else
		{
		    script->state = state;
		}
	    }
	}
    }
    else
    {
	script->state = CSFX_NONE;
    }

    return script->state;
}

void* csfx_script_symbol(csfx_script_t* script, const char* name)
{
    return csfx__dlib_symbol(script->library, name);
}

const char* csfx_script_errmsg(csfx_script_t* script)
{
    (void)script;
    return csfx__dlib_errmsg();
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

/* END OF EXTERN "C" */
CSFX_EXTERN_C_END;

#endif /* __CSFX_H__ */
