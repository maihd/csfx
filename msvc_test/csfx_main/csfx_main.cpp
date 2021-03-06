// csfx_main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define CSFX_IMPL_WITH_PDB_DELETE
#include "../../csfx.h"

#ifndef NDEBUG
#define SCRIPT_PATH "../csfx_script/Debug/csfx_script.dll"
#else
#define SCRIPT_PATH "../csfx_script/Release/csfx_script.dll"
#endif

int main()
{
    csfx::script_t script(SCRIPT_PATH);

    bool changed;
    int  testing;
    while (1)
    {                      
        changed = false;
        switch (csfx::script::update(script))
        {
        case CSFX_INIT:
            changed = true;
            printf("Initialize script...\n");
            break;

        case CSFX_RELOAD:
            changed = true;
            printf("Script reload\n");
            break;

        case CSFX_UNLOAD:
            printf("Script reload\n");
            break;

        case CSFX_QUIT:
            printf("Script quit\n");
            break;

        default:
            break;
        }

        if (changed)
        {
            int* __csfx_testing = csfx::script::symbol<int>(script, "__csfx_testing");
            if (!__csfx_testing || !*__csfx_testing)
            {
                break;
            }
        }

        Sleep(1000);
    }

    return 0;
}

