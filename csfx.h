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
    CSFX_ERROR_ILLCODE,
    CSFX_ERROR_SYSCALL,
    CSFX_ERROR_MISALIGN,
    CSFX_ERROR_SEGFAULT,
    CSFX_ERROR_OUTBOUNDS,
    CSFX_ERROR_STACKOVERFLOW,
};

/** 
 * Script data structure
 */
typedef struct
{
    int   state;
    int   errcode;
    void* userdata;
    char  internal[sizeof(void*)];
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
__csfx__ const char* csfx_script_errmsg(const csfx_script_t* script);

#if defined(_WIN32)
/* Undocumented, should not call by hand */
__csfx__ int csfx__seh_filter(csfx_script_t* script, unsigned long code);
#endif

#if defined(_MSC_VER)
# define csfx_try(s)    __try
# define csfx_except(s) __except(csfx__seh_filter(s, GetExceptionCode()))
# define csfx_finally   __finally
#elif (__unix__)
# include <signal.h>
# include <setjmp.h>
# define csfx_try(s)							\
    (s)->errcode = sigsetjmp(csfx__jmpenv, 0);				\
    if ((s)->errcode == 0) (s)->errcode = CSFX_ERROR_NONE;		\
    if ((s)->errcode == CSFX_ERROR_NONE)

# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE					\
    && (s)->errcode <= CSFX_ERROR_STACKOVERFLOW

extern __thread sigjmp_buf csfx__jmpenv;
#else
# include <signal.h>
# include <setjmp.h>
# define csfx_try(s)						\
    (s)->errcode = setjmp(csfx__jmpenv);			\
    if ((s)->errcode == 0) (s)->errcode = CSFX_ERROR_NONE;	\
    if ((s)->errcode == CSFX_ERROR_NONE)

# define csfx_except(s) else if (csfx__errcode_filter(s))
# define csfx_finally   
# define csfx__errcode_filter(s)					\
    (s)->errcode > CSFX_ERROR_NONE					\
    && (s)->errcode <= CSFX_ERROR_STACKOVERFLOW

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

/* END OF EXTERN "C" */
CSFX_EXTERN_C_END;

/**
 * @region: C++ API wrapper
 */
#ifdef __cplusplus
namespace csfx
{
    typedef ::csfx_filetime_t filetime_t;

    union script_t
    {
    public:
        struct
        {
            int   state;
            int   errcode;
            void* userdata;
        };

    public:
        inline script_t(const char* path)
        {
            ::csfx_script_init(&internal, path);
        }

        inline ~script_t(void)
        {
            ::csfx_script_free(&internal);
        }

    public:
        inline operator ::csfx_script_t* ()
        {
            return &internal;
        }

        inline operator const ::csfx_script_t* () const
        {
            return &internal;
        }

    private:
        ::csfx_script_t internal;
    };

    namespace script
    {
        inline int update(script_t& script)
        {
            return ::csfx_script_update(script);
        }

        inline void* symbol(script_t& script, const char* name)
        {
            return ::csfx_script_symbol(script, name);
        }

        template <typename type_t>
        inline type_t* symbol(script_t& script, const char* name)
        {
            return (type_t*)::csfx_script_symbol(script, name);
        }

        inline const char* errmsg(const script_t& script)
        {
            return ::csfx_script_errmsg(script);
        }

        inline int update(script_t* script)
        {
            return ::csfx_script_update(*script);
        }

        inline void* symbol(script_t* script, const char* name)
        {
            return ::csfx_script_symbol(*script, name);
        }

        template <typename type_t>
        inline type_t* symbol(script_t* script, const char* name)
        {
            return (type_t*)::csfx_script_symbol(*script, name);
        }

        inline const char* errmsg(const script_t* script)
        {
            return ::csfx_script_errmsg(*script);
        }
    }
    
    inline bool watch_files(filetime_t* files, int count)
    {
        return ::csfx_watch_files(files, count);
    }

    template <int count>
    inline bool watch_files(filetime_t files[count])
    {
        return ::csfx_watch_files(files, count);
    }
}
#endif
/* @endregion: C++ API... */

#endif /* __CSFX_H__ */


/* --------------------------------------------- */
/* ----------------- CSFX_IMPL ----------------- */
/* --------------------------------------------- */

#ifdef CSFX_IMPL_WITH_PDB_UNLOCK
#  define CSFX_IMPL
#  define CSFX_PDB_UNLOCK
#endif

#ifdef CSFX_IMPL_WITH_PDB_DELETE
#  define CSFX_IMPL
#  define CSFX_PDB_DELETE
#endif

#ifdef CSFX_PDB_DELETE
#  define CSFX_PDB_UNLOCK
#endif

#ifdef CSFX_IMPL
/* BEGIN OF CSFX_IMPL */

#define CSFX__MAX_PATH 256

typedef struct
{
    void* library;

#if defined(_MSC_VER) && defined(CSFX_PDB_UNLOCK)
    int   delpdb;
    long  libtime;
    long  pdbtime;
    
    char  librpath[CSFX__MAX_PATH];
    char  libtpath[CSFX__MAX_PATH];
    char  pdbrpath[CSFX__MAX_PATH];
    char  pdbtpath[CSFX__MAX_PATH];
#else
    long  libtime;
    
    char  librpath[CSFX__MAX_PATH];
    char  libtpath[CSFX__MAX_PATH];
#endif
} csfx__script_data_t;

#include <stdio.h>
#include <stdlib.h>

/* Define dynamic library loading API */
#if defined(_WIN32)
#  include <Windows.h>
#  define csfx__dlib_load(path)   (void*)LoadLibraryA(path)
#  define csfx__dlib_free(lib)    FreeLibrary((HMODULE)lib)
#  define csfx__dlib_symbol(l, n) (void*)GetProcAddress((HMODULE)l, n)

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
#  include <dlfcn.h>
#  define csfx__dlib_load(path)   dlopen(path, RTLD_LAZY)
#  define csfx__dlib_free(lib)    dlclose(lib)
#  define csfx__dlib_symbol(l, n) dlsym(l, n)
#  define csfx__dlib_errmsg()     dlerror()
#else
#  error "Unsupported platform"
#endif


/** Custom helper functions **/
#if defined(_WIN32)

static long csfx__last_modify_time(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
    {
        return 0;
    }

    LARGE_INTEGER time;
    time.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    time.HighPart = fad.ftLastWriteTime.dwHighDateTime;

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
        errcode = CSFX_ERROR_SEGFAULT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        errcode = CSFX_ERROR_ILLCODE;
        break;
	
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        errcode = CSFX_ERROR_MISALIGN;
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        errcode = CSFX_ERROR_OUTBOUNDS;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        errcode = CSFX_ERROR_STACKOVERFLOW;
        break;

    default:
        break;
    }

    if (script) script->errcode = errcode;
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

#if defined(_MSC_VER)

#  if defined(CSFX_PDB_UNLOCK) || defined(CSFX_PDB_DELETE)
#    include <winternl.h>
#    include <RestartManager.h> 
#    pragma comment(lib, "ntdll.lib")
#    pragma comment(lib, "rstrtmgr.lib")

#    define NTSTATUS_SUCCESS              ((NTSTATUS)0x00000000L)
#    define NTSTATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)

// Undocumented SYSTEM_INFORMATION_CLASS: SystemHandleInformation
#    define SystemHandleInformation ((SYSTEM_INFORMATION_CLASS)16)

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

static BOOL csfx__path_compare(LPCWSTR path0, LPCWSTR path1)
{
    //Get file handles
    HANDLE handle1 = CreateFileW(path0, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE handle2 = CreateFileW(path1, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    BOOL result = FALSE;

    //if we could open both paths...
    if (handle1 != INVALID_HANDLE_VALUE && handle2 != INVALID_HANDLE_VALUE)
    {
        BY_HANDLE_FILE_INFORMATION fileInfo1;
        BY_HANDLE_FILE_INFORMATION fileInfo2;
        if (GetFileInformationByHandle(handle1, &fileInfo1) && GetFileInformationByHandle(handle2, &fileInfo2))
        {
            //the paths are the same if they refer to the same file (fileindex) on the same volume (volume serial number)
            result = 
                fileInfo1.dwVolumeSerialNumber == fileInfo2.dwVolumeSerialNumber &&
                fileInfo1.nFileIndexHigh == fileInfo2.nFileIndexHigh &&
                fileInfo1.nFileIndexLow == fileInfo2.nFileIndexLow;
        }
    }

    // free the handles
    CloseHandle(handle1);
    CloseHandle(handle2);

    return result;
}

static void csfx__unlock_file_from_process(HANDLE heap, SYSTEM_HANDLE_INFORMATION* sys_info, ULONG pid, const WCHAR* file)
{ 
    // Make sure the process is valid
    HANDLE hCurProcess = GetCurrentProcess();
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
    {
        return;
    }

    int i;
    for (i = 0; i < sys_info->HandleCount; i++)
    {
        SYSTEM_HANDLE* handle_info = &sys_info->Handles[i];
        HANDLE         handle      = (HANDLE)handle_info->Value;      

        if (handle_info->ProcessId == pid)
        {
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

        #if 0
            const WCHAR prefix[] = L"\\Device\\HarddiskVolume";
            const ULONG prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1;
            if (pobj->Name.Buffer && wcsncmp(pobj->Name.Buffer, prefix, prefix_length) == 0)
            {                          
                int volume;                   
                WCHAR path0[MAX_PATH];
                WCHAR path1[MAX_PATH];

            #if _MSC_VER >= 1200
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%d\\%s", &volume, path0, _countof(path0));
            #else
                swscanf(pobj->Name.Buffer + prefix_length, L"%d\\%s", &volume, path0);
            #endif

                WCHAR fullpath[MAX_PATH];
                GetVolumePathNameW(file, fullpath, MAX_PATH);
                wsprintfW(path1, L"%c:\\%s", 'A' + volume - 1, path0);

                if (wcscmp(pobj->Name.Buffer, file) == 0)
                {
                    HANDLE hForClose;
                    DuplicateHandle(hProcess, handle, hCurProcess, &hForClose, MAXIMUM_ALLOWED, FALSE, DUPLICATE_CLOSE_SOURCE);
                    CloseHandle(hForClose);
                }
            }
        #endif


            const WCHAR prefix[] = L"\\Device\\HarddiskVolume";
            const ULONG prefix_length = sizeof(prefix) / sizeof(prefix[0]) - 1;
            if (pobj->Name.Buffer && wcsncmp(pobj->Name.Buffer, prefix, prefix_length) == 0)
            {
                WCHAR temp0[MAX_PATH];
                WCHAR temp1[MAX_PATH];

                int volume;
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%d", &volume);
                wsprintfW(temp0, L"\\Device\\HarddiskVolume%d", volume);
                if (QueryDosDevice(temp0, temp1, MAX_PATH) == 0)
                {
                    volume = 0;
                    DWORD error = GetLastError();
                    if (error == ERROR_INSUFFICIENT_BUFFER)
                    {
                        error = 0;
                    }
                }

                WCHAR path0[MAX_PATH];
                WCHAR path1[MAX_PATH];

            #if _MSC_VER >= 1200
                swscanf_s(pobj->Name.Buffer + prefix_length, L"%s", path0, _countof(path0));
            #else
                swscanf(pobj->Name.Buffer + prefix_length, L"%s", path0);
            #endif                                            

                wsprintfW(path1, L"\\?\\HarddiskVolume%s", path0);

                if (csfx__path_compare(path1, file))
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

static DWORD WINAPI csfx__unlock_pdb_file_routine(void* userdata)
{
    HANDLE heap_handle = GetProcessHeap();
    const struct data_s
    {
        csfx__script_data_t* script_data;
        WCHAR                szFile[1];
    }* data = (const struct data_s*)userdata;

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
        LPCWSTR szFile = data->szFile;
        dwError = RmRegisterResources(dwSession, 1, &szFile, 0, NULL, 0, NULL);
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

    /* Remove the .pdb file if required */
#if defined(CSFX_PDB_DELETE)
    //data->script_data->delpdb = TRUE;
    if (csfx__remove_file(data->script_data->pdbrpath))
    {
        data->script_data->delpdb = TRUE;
    }
    else
    {
        data->script_data->delpdb = TRUE;
    }

    HANDLE handle = CreateFileA(data->script_data->pdbrpath,
                                0,
                                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                                NULL);
    if (handle)
    {
        CloseHandle(handle);
    }
#endif

    /* Clean up */
    HeapFree(heap_handle, 0, sys_info);
    HeapFree(heap_handle, 0, (void*)data);

    return 0;
}

static void csfx__unlock_pdb_file(csfx__script_data_t* script, const char* file)
{
    HANDLE heap_handle = GetProcessHeap();
    struct data_s
    {
        csfx__script_data_t* script_data;
        WCHAR                szFile[1];
    }* data = (struct data_s*)HeapAlloc(heap_handle, 0, sizeof(struct data_s) + MAX_PATH);

    data->script_data = script;
    MultiByteToWideChar(CP_UTF8, 0, file, -1, data->szFile, MAX_PATH);

#if !defined(CSFX_SINGLE_THREAD)
    HANDLE threadHandle = CreateThread(NULL, 0, csfx__unlock_pdb_file_routine, (void*)data, 0, NULL);
    (void)threadHandle;
#else
    csfx__unlock_pdb_file_routine((void*)data);
#endif

    /* Clean up szFile must be done in csfx__unlock_pdb_file_routine */
}   

static int csfx__get_pdb_path(const char* libpath, char* buf, int len)
{
    int i, chr;
    char drv[8];
    char dir[MAX_PATH];
    char name[MAX_PATH];
    char ext[32];

    GetFullPathNameA(libpath, len, buf, NULL);

#if _MSC_VER >= 1200
    _splitpath_s(buf, drv, sizeof(drv), dir, sizeof(dir), name, sizeof(name), NULL, 0);
#else
    _splitpath(buf, drv, dir, name, NULL);
#endif

    return snprintf(buf, len, "%s%s%s.pdb", drv, dir, name);
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

static LONG WINAPI csfx__sighandler(EXCEPTION_POINTERS* info)
{
    int excode  = info->ExceptionRecord->ExceptionCode;
    int errcode = csfx__seh_filter(NULL, excode);
    longjmp(csfx__jmpenv, errcode);
    return EXCEPTION_CONTINUE_EXECUTION;
}

int  csfx_init(void)
{
    SetUnhandledExceptionFilter(csfx__sighandler);
    return 0;
}

void csfx_quit(void)
{
    SetUnhandledExceptionFilter(NULL);
}
# endif /* _MSC_VER */

#elif defined(__unix__)
# include <string.h>
# include <sys/stat.h>
# include <sys/types.h>
# define CSFX__PATH_LENGTH PATH_MAX
# define csfx__countof(x) (sizeof(x) / sizeof((x)[0]))

__thread sigjmp_buf csfx__jmpenv;
const int csfx__signals[] = { SIGBUS, SIGSYS, SIGILL, SIGSEGV, SIGABRT };

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
	errcode = CSFX_ERROR_ILLCODE;
	break;

    case SIGBUS:
	errcode = CSFX_ERROR_MISALIGN;
	break;

    case SIGSYS:
	errcode = CSFX_ERROR_SYSCALL;
	break;

    case SIGABRT:
	errcode = CSFX_ERROR_ABORT;
	break;

    case SIGSEGV:
	errcode = CSFX_ERROR_SEGFAULT;
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
    sa.sa_flags     = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    sa.sa_handler   = NULL;
    sa.sa_sigaction = csfx__sighandler;

    int idx;
    for (idx = 0; idx < csfx__countof(csfx__signals); idx++)
    {
	if (sigaction(csfx__signals[idx], &sa, NULL) != 0)
	{
	    return -1;
	}
    }

    return 0;
}

void csfx_quit(void)
{
    int idx;
    for (idx = 0; idx < csfx__countof(csfx__signals); idx++)
    {
	if (signal(csfx__signals[idx], SIG_DFL) != 0)
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
    return remove(path) == 0;
}

static int csfx__get_temp_path(const char* path, char* buffer, int length)
{
    int res;
    
    if (buffer)
    { 
        int version = 0;
        while (1)
        {
            res = snprintf(buffer, length, "%s.%d", path, version++);

        #if defined(_MSC_VER) && _MSC_VER >= 1200
            FILE* file;
            if (fopen_s(&file, buffer, "r") != 0)
            {
                file = NULL;
            }
        #else
            FILE* file = fopen(buffer, "r");
        #endif

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
    typedef csfx__script_data_t data_t;

    data_t** dptr = (data_t**)(&script->internal);
    data_t*  data = *dptr;
    
    long src = data->libtime;
    long cur = csfx__last_modify_time(data->librpath);
    int  res = cur > src;
#if defined(_MSC_VER) && defined(CSFX_PDB_UNLOCK)
    if (res)
    {  
        src = data->pdbtime;
        cur = csfx__last_modify_time(data->pdbrpath);
        res = (cur == src && cur == 0) || cur > src;
    }
#endif
    return res;
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

/* @impl: csfx_script_init */
void csfx_script_init(csfx_script_t* script, const char* libpath)
{
    typedef csfx__script_data_t data_t;
    
    script->state    = CSFX_NONE;
    script->errcode  = CSFX_ERROR_NONE;
    script->userdata = NULL;

    data_t** dptr = (data_t**)(&script->internal);
    data_t*  data = (data_t*)(malloc(sizeof(data_t)));

    if (data)
    {
        *dptr = data;

        data->libtime = 0;
        data->library = NULL;
        csfx__get_temp_path(libpath, data->libtpath, CSFX__MAX_PATH);
    
    #if defined(_MSC_VER) && _MSC_VER >= 1200
        strncpy_s(data->librpath, libpath, CSFX__MAX_PATH);
    #else
        strncpy(data->librpath, libpath, CSFX__MAX_PATH);
    #endif
    
    #if defined(_MSC_VER) && defined(CSFX_PDB_UNLOCK)
        data->delpdb  = FALSE;
        data->pdbtime = 0;
        csfx__get_pdb_path(libpath, data->pdbrpath, CSFX__MAX_PATH);
        csfx__get_temp_path(data->pdbrpath, data->pdbtpath, CSFX__MAX_PATH);
    #endif
    }
}

void csfx_script_free(csfx_script_t* script)
{
    typedef csfx__script_data_t data_t;
    
    if (!script) return;

    data_t** dptr = (data_t**)(&script->internal);
    data_t*  data = *dptr;

    /* Raise quit event */
    if (data->library)
    {
        script->state = CSFX_QUIT;
        csfx__call_main(script, data->library, script->state);
        csfx__dlib_free(data->library);

        /* Remove temp library */
        csfx__remove_file(data->libtpath); /* Ignore error code */
	
#if defined(_MSC_VER) && defined(CSFX_PDB_DELETE)
        csfx__copy_file(data->pdbtpath, data->pdbrpath);
        csfx__remove_file(data->pdbtpath);
#endif
    }

    /* Clean up */
    free(data);
    *dptr = NULL;
}

int csfx_script_update(csfx_script_t* script)
{
    typedef csfx__script_data_t data_t;
    
    data_t** dptr = (data_t**)(&script->internal);
    data_t*  data = *dptr;

#if 0
    if (data->delpdb)
    {
        HANDLE fileHandle = CreateFileA(
            data->pdbrpath,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_DELETE_ON_CLOSE,
            NULL);
        if (fileHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(fileHandle);
        }

        if (csfx__remove_file(data->pdbrpath))
        {
            int a = 0;
            int b = 1;

            a = a + b;
        }
        else
        {
            int a = 0;
            int b = 1;

            a = a + b;
        }
    }
#endif
    
    if (csfx__script_changed(script))
    {
        void* library;

        /* Unload old version */
        library = data->library;
        if (library)
        {
            /* Raise unload event */
            script->state = CSFX_UNLOAD;
            csfx__call_main(script, library, script->state);

            /* Collect garbage */
            csfx__dlib_free(library);
            data->library = NULL;

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
        csfx__remove_file(data->libtpath); /* Remove temp library */
        if (csfx__copy_file(data->librpath, data->libtpath))
        {
            library = csfx__dlib_load(data->libtpath);
            if (library)
            {
                int state = script->state; /* new state */
                state = state == CSFX_NONE ? CSFX_INIT : CSFX_RELOAD;
                csfx__call_main(script, library, state);

                data->library = library;
                data->libtime = csfx__last_modify_time(data->librpath);

                if (script->errcode != CSFX_ERROR_NONE)
                {
                    csfx__dlib_free(data->library);

                    data->library = NULL;
                    script->state = CSFX_FAILED;
                }
                else
                {
                    script->state = state;

                #if defined(_MSC_VER) && defined(CSFX_PDB_UNLOCK)
                # if defined(CSFX_PDB_DELETE)
                    csfx__remove_file(data->pdbtpath);
                    csfx__copy_file(data->pdbrpath, data->pdbtpath);
                # endif
                    csfx__unlock_pdb_file(data, data->pdbrpath);
                    data->pdbtime = csfx__last_modify_time(data->pdbrpath);
                #endif
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
    typedef csfx__script_data_t data_t;

    data_t** dptr = (data_t**)(&script->internal);
    data_t*  data = *dptr;
    return csfx__dlib_symbol(data->library, name);
}

const char* csfx_script_errmsg(const csfx_script_t* script)
{
    (void)script;
    return csfx__dlib_errmsg();
}

/* @impl: csfx_watch_files */
int csfx_watch_files(csfx_filetime_t* files, int count)
{
    int changed = 0;
    for (int i = 0; i < count; i++)
    {
        if (csfx__file_changed(&files[i]))
        {
            changed = 1;
        }
    }
    return changed;
}

/* END OF CSFX_IMPL */
#endif /* CSFX_IMPL */
