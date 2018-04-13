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
 * CSFX loading state
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
 *
 */
typedef struct
{
    long        libtime;
    void*       library;
    void*       userdata;
    const char* filepath;
    const char* temppath;
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
__csfx__ const char* csfx_script_errmsg(csfx_script_t* script);

#if defined(_MSC_VER)
# define csfx_try(s)   __try
# define csfx_catch(s) __except(csfx__seh_filter(s, GetExceptionCode()))

__csfx__ int csfx__seh_filter(csfx_script_t* script, unsigned long code);
#else
# define csfx_try(s)   if (1)
# define csfx_catch(s) else
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
#if defined(_MSC_VER)
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

static int csfx__remove_file(const char* path)
{
    return remove(path);
}

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

    return (long)(time.QuadPart / 10000000L - 11644473600L);
}

static const char* csfx__get_temp_path(const char* filepath)
{
    char path[MAX_PATH];
    char name[MAX_PATH];
    
    char* tmp = (char*)malloc(MAX_PATH);

    _splitpath(filepath, NULL, NULL, name, NULL);
    
    GetTempPathA(MAX_PATH, path);
    sprintf_s(tmp, MAX_PATH, "%s%s.dll", path, name);
    //GetTempFileNameA(path, "__csfx_temp_", 0, tmp);
    return tmp;
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

static int csfx__temp_pdb_file(const char* dllpath)
{
    char pdbpath[MAX_PATH];
    char dirname[MAX_PATH];
    char filename[MAX_PATH];
    char temppath[MAX_PATH];
    char temp_pdbpath[MAX_PATH];
	
    GetTempPathA(MAX_PATH, temppath);
    _splitpath(dllpath, NULL, dirname, filename, NULL);

    sprintf_s(pdbpath, MAX_PATH, "%s%s.pdb", dirname, filename);
    sprintf_s(temp_pdbpath, MAX_PATH, "%s%s.pdb", temppath, filename);

    int res = csfx__copy_file(pdbpath, temp_pdbpath);
    csfx__remove_file(pdbpath);
    return res;
}

static int csfx__restore_pdb_file(const char* dllpath)
{
    char pdbpath[MAX_PATH];
    char dirname[MAX_PATH];
    char filename[MAX_PATH];
    char temppath[MAX_PATH];
    char temp_pdbpath[MAX_PATH];
	
    GetTempPathA(MAX_PATH, temppath);
    _splitpath(dllpath, NULL, dirname, filename, NULL);

    sprintf_s(pdbpath, MAX_PATH, "%s%s.pdb", dirname, filename);
    sprintf_s(temp_pdbpath, MAX_PATH, "%s%s.pdb", temppath, filename);

    int res = csfx__copy_file(temp_pdbpath, pdbpath);
    csfx__remove_file(temp_pdbpath);
    return res;
}

int csfx__seh_filter(csfx_script_t* script, unsigned long code)
{
    (void)script;
    
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_STACK_OVERFLOW:
	return EXCEPTION_EXECUTE_HANDLER;

    default:
	break;
    }

    return EXCEPTION_CONTINUE_SEARCH;
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

static const char* csfx__get_temp_path(const char* filepath)
{
    (void)filepath;
    return tmpnam(NULL);
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
#else
#error "Unsupported platform"
#endif

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
    csfx_main_f main = (csfx_main_f)csfx__dlib_symbol(library, name);

    if (main)
    {
	csfx_try (script)
	{
	    script->userdata = main(script->userdata, state);
	}
	csfx_catch (script)
	{
	    return -1;
	}
    }

    return 0;
}

void csfx_script_init(csfx_script_t* script, const char* path)
{
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
	csfx__call_main(script, library, CSFX_QUIT);
	csfx__dlib_free(library);

	#ifdef _MSC_VER
	csfx__restore_pdb_file(script->filepath);
	#endif
    }

    /* Return back temppath to the system */
    free((void*)script->temppath);

    script->libtime  = 0;
    script->library  = NULL;
    script->temppath = NULL;
    script->filepath = NULL;
}

int csfx_script_update(csfx_script_t* script)
{
    if (csfx__script_changed(script))
    {
	int state;
	void* library;

	/* Intialize state */
	state = CSFX_FAILED;
	
	/* Unload old version */
	library = script->library;
	if (library)
	{
	    /* Raise unload event */
	    state = CSFX_UNLOAD;
	    csfx__call_main(script, library, state);

	    /* Collect garbage */
	    csfx__dlib_free(library);
	    script->library = NULL;
	    
	    /* Remove temp library */
	    csfx__remove_file(script->temppath); /* Ignore error code */
	}

	/* Create new temp version */
	csfx__copy_file(script->filepath, script->temppath);
#ifdef _MSC_VER
	csfx__temp_pdb_file(script->filepath);
#endif

	/* Load new version */
	library = csfx__dlib_load(script->temppath); 
	if (library)
	{
	    state = state != CSFX_UNLOAD ? CSFX_INIT : CSFX_RELOAD;
	    csfx__call_main(script, library, state);
	
	    script->library = library;
	    script->libtime = csfx__last_modify_time(script->filepath);
	}

	return state;
    }
    
    return CSFX_NONE;
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
