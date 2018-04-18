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

__csfx__ int   csfx_init(void);
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
#elif (__unix__)
# include <signal.h>
# include <setjmp.h>
# define csfx_try(s)							\
    (s)->errcode = sigsetjmp(csfx__jmpenv, 0);				\
    if ((s)->errcode == 0) (s)->errcode = CSFX_ERROR_NONE;		\
    if ((s)->errcode == CSFX_ERROR_NONE)

# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   else
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE && (s)->errcode <= CSFX_ERROR_MEMORY

extern __thread sigjmp_buf csfx__jmpenv;
#else
# include <signal.h>
# include <setjmp.h>
# define csfx_try(s)						\
    (s)->errcode = setjmp(csfx__jmpenv);			\
    if ((s)->errcode == 0) (s)->errcode = CSFX_ERROR_NONE;	\
    if ((s)->errcode == CSFX_ERROR_NONE)

# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   else
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE && (s)->errcode <= CSFX_ERROR_MEMORY

extern
# if defined(__MINGW32__)
__thread
# endif
jmp_buf csfx__jmpenv;
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
#if defined(_WIN32) || defined(__CYGWIN__)
__declspec(dllexport)
#else
extern
#endif
void* csfx_main(void* userdata, int old_state, int new_state);

/* --------------------------------------------- */
/* ----------------- CSFX_IMPL ----------------- */
/* --------------------------------------------- */

#ifdef CSFX_IMPL_WITH_PDB_UNLOCK
# define CSFX_IMPL
# define CSFX_PDB_UNLOCK
#endif

#ifdef CSFX_IMPL
/* BEGIN OF CSFX_IMPL */

#include <stdio.h>
#include <stdlib.h>

/* Define dynamic library loading API */
#if defined(_WIN32)
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
#if defined(_WIN32)

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

# if defined(_MSC_VER)

#  if defined(CSFX_PDB_UNLOCK)
#   include <winternl.h>
#   include <RestartManager.h> 
#   pragma comment(lib, "ntdll.lib")
#   pragma comment(lib, "rstrtmgr.lib")

#   define NTSTATUS_SUCCESS              ((NTSTATUS)0x00000000L)
#   define NTSTATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)

// Undocumented SYSTEM_INFORMATION_CLASS: SystemHandleInformation
#   define SystemHandleInformation ((SYSTEM_INFORMATION_CLASS)16)

typedef struct
{
    ULONG       ProcessId;
    BYTE        ObjectTypeNumber;
    BYTE        Flags;
    USHORT      Value;
    PVOID       Address;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE;

typedef struct
{
    ULONG         HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION;

typedef struct
{
    UNICODE_STRING Name;
} OBJECT_INFORMATION;

static void csfx__unlock_file_from_process(HANDLE heap, SYSTEM_HANDLE_INFORMATION* sys_info, ULONG pid, const WCHAR* file)
{ 
    HANDLE hCurProcess = GetCurrentProcess();
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
    {
        return;
    }

    int i;
    int handles = 0;
    for (i = 0; i < sys_info->HandleCount; i++)
    {
        SYSTEM_HANDLE* handle_info = &sys_info->Handles[i];
        HANDLE         handle      = (HANDLE)handle_info->Value;      

        if (handle_info->ProcessId == pid)
        {
            handles++;

            HANDLE hCopy; // Duplicate the handle in the current process
            if (!DuplicateHandle(hProcess, handle, hCurProcess, &hCopy, MAXIMUM_ALLOWED, FALSE, 0))
            {
                continue;
            }

            ULONG ObjectInformationLength = sizeof(OBJECT_INFORMATION) + 512;
            OBJECT_INFORMATION* pobj = (OBJECT_INFORMATION*)HeapAlloc(heap, 0, ObjectInformationLength);

            if (pobj == NULL)
            {
                continue;
            }

            ULONG ReturnLength;
            if (NtQueryObject(hCopy, (OBJECT_INFORMATION_CLASS)1, pobj, ObjectInformationLength, &ReturnLength) != NTSTATUS_SUCCESS)
            {
                HeapFree(heap, 0, pobj); 
                continue;
            }                  

            const WCHAR prefix[] = L"\\Device\\HarddiskVolume";
            const ULONG prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1;
            if (pobj->Name.Buffer && wcsncmp(pobj->Name.Buffer, prefix, prefix_length) == 0)
            {                          
                int volume;                   
                WCHAR path0[MAX_PATH];
                WCHAR path1[MAX_PATH];

                swscanf(pobj->Name.Buffer + prefix_length, L"%d\\%s", &volume, path0);
                wsprintf(path1, L"%c:\\%s", 'A' + volume - 1, path0);

                if (wcscmp(path1, file) == 0)
                {
                    HANDLE hForClose;
                    DuplicateHandle(hProcess, handle, hCurProcess, &hForClose, MAXIMUM_ALLOWED, FALSE, DUPLICATE_CLOSE_SOURCE);
                    CloseHandle(hForClose);
                }
            }


            CloseHandle(hCopy);
            HeapFree(heap, 0, pobj);
        }
    }

    CloseHandle(hProcess);
}

static SYSTEM_HANDLE_INFORMATION* csfx__create_system_info(void)
{                          
    size_t sys_info_size = 0;
    HANDLE heap_handle   = GetProcessHeap();
    SYSTEM_HANDLE_INFORMATION* sys_info = NULL;
    {
        ULONG res;
        NTSTATUS status;
        do
        {
            HeapFree(heap_handle, 0, sys_info);
            sys_info_size += 4096;
            sys_info = (SYSTEM_HANDLE_INFORMATION*)HeapAlloc(heap_handle, 0, sys_info_size);
            status = NtQuerySystemInformation(SystemHandleInformation, sys_info, sys_info_size, &res);
        } while (status == NTSTATUS_INFO_LENGTH_MISMATCH);
    }

    return sys_info;
}

static DWORD WINAPI csfx__unlock_pdb_file_routine(void* data)
{
    HANDLE heap_handle  = GetProcessHeap();
    const WCHAR* szFile = (const WCHAR*)data;

    int   i;
    UINT  nProcInfoNeeded;
    UINT  nProcInfo = 10;    
    DWORD dwError;                                  
    DWORD dwReason;
    DWORD dwSession;     
    RM_PROCESS_INFO rgpi[10];
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    /* Create system info */
    SYSTEM_HANDLE_INFORMATION* sys_info = NULL;

    dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError == ERROR_SUCCESS)
    {
        dwError = RmRegisterResources(dwSession, 1, (const WCHAR**)&szFile, 0, NULL, 0, NULL);
        if (dwError == ERROR_SUCCESS)
        {

            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (i = 0; i < nProcInfo; i++)
                {                                            
                    if (!sys_info) sys_info = csfx__create_system_info();
                    csfx__unlock_file_from_process(heap_handle, sys_info, rgpi[i].Process.dwProcessId, szFile);
                }
            }
        }
        RmEndSession(dwSession);
    }            

    /* Clean up */
    HeapFree(heap_handle, 0, sys_info);
    HeapFree(heap_handle, 0, (void*)szFile);

    return 0;
}

#undef NTSTATUS_SUCCESS             
#undef NTSTATUS_INFO_LENGTH_MISMATCH 
#undef SystemHandleInformation
static void csfx__unlock_pdb_file(const char* file)
{
    HANDLE heap_handle = GetProcessHeap();

    int i, chr;
    char drv[8];
    char dir[MAX_PATH];
    char name[MAX_PATH];
    char path[MAX_PATH];
    GetFullPathNameA(file, MAX_PATH, path, NULL);
    _splitpath(path, drv, dir, name, NULL);
    _sprintf_p(path, MAX_PATH, "%s%s%s.pdb", drv, dir, name);

    file = path;
    WCHAR* szFile = (WCHAR*)HeapAlloc(heap_handle, 0, MAX_PATH);
    for (i = 0, chr = *file++; chr; chr = *file++, i++)
    {
        szFile[i] = (WCHAR)chr;
    }
    szFile[i] = 0;

#if !defined(CSFX_SINGLE_THREAD)
    HANDLE thread = CreateThread(NULL, 0, csfx__unlock_pdb_file_routine, (void*)szFile, 0, NULL);
    (void)thread;
#else
    csfx__unlock_pdb_file_routine((void*)szFile);
#endif

    /* Clean up szFile must be done in csfx__unlock_pdb_file_routine */
}
#  endif /* CSFX_PDB_UNLOCK */

int  csfx_init(void)
{
    return -1;
}

void csfx_quit(void)
{
    /* NULL */
}
# else

#  if defined(__MINGW32__)
__thread
#  else
__declspec(thread)
#  endif
jmp_buf csfx__jmpenv;

static void csfx__sighandler(int code)
{
    int errcode;
    switch (code)
    {
    case SIGILL:
	errcode = CSFX_ERROR_INSTR;
	break;

	/*
    case SIGBUS:
	errcode = CSFX_ERROR_ALIGN;
	break;
	*/

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
    longjmp(csfx__jmpenv, errcode);
}

int  csfx_init(void)
{
    int idx;
    int signals[] = { SIGILL, SIGSEGV, SIGABRT };
    for (idx = 0; idx < sizeof(signals) / sizeof(signals[0]); idx++)
    {
	if (signal(signals[idx], csfx__sighandler) != 0)
	{
	    return -1;
	}
    }

    return 0;
}

void csfx_quit(void)
{
    int idx;
    int signals[] = { SIGILL, SIGSEGV, SIGABRT };
    for (idx = 0; idx < sizeof(signals) / sizeof(signals[0]); idx++)
    {
	if (signal(signals[idx], SIG_DFL) != 0)
	{
	    break;
	}
    }
}
# endif /* _MSC_VER */

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

    (void)info;
    (void)context;
    
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


int  csfx_init(void)
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
	    return -1;
	}
    }

    return 0;
}

void csfx_quit(void)
{    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags     = SA_NODEFER;
    sa.sa_handler   = SIG_DFL;
    sa.sa_sigaction = NULL;

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
    typedef void* (*csfx_main_f)(void*, int, int);

    const char* name = "csfx_main";
    csfx_main_f func = (csfx_main_f)csfx__dlib_symbol(library, name);

    if (func)
    {
	csfx_try (script)
	{
	    script->userdata = func(script->userdata, script->state, state);
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
    script->errcode  = CSFX_ERROR_NONE;
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
		state = state == CSFX_NONE ? CSFX_INIT : CSFX_RELOAD;
		csfx__call_main(script, library, state);

		script->library = library;
		script->libtime = csfx__last_modify_time(script->filepath);

		#if defined(_MSC_VER) && defined(CSFX_PDB_UNLOCK)
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

	return script->state;
    }
    else
    {
	if (script->state != CSFX_FAILED)
	{
	    script->state = CSFX_NONE;
	}

	return CSFX_NONE;
    }
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

/* END OF CSFX_IMPL */
#endif /* CSFX_IMPL */

/* END OF EXTERN "C" */
CSFX_EXTERN_C_END;

#endif /* __CSFX_H__ */
