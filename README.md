# CSFX - C scripting framework

## Example
1. Application:
```C
#define CSFX_IMPL
#define CSFX_PDB_UNLOCK
#define CSFX_IMPL_WITH_PDB_UNLOCK /* Equal 2 lines above */
#include "csfx.h"

/* Initialize is required for exception handling */
if (csfx_init() != 0)
{
    /* Error */
}

csfx_script_t script;
void (*on_update)(void*);

csfx_script_init(&script, "temp.dll"); /* or .so */
while (1)
{
    switch (csfx_script_update(&script))
    {
    case CSFX_INIT:
    	 on_update = csfx_script_symbol(&script, "on_update");
    	 break;

    case CSFX_QUIT:
    	 on_update = NULL;
     	 break;

    case CSFX_RELOAD:
    	 on_update = csfx_script_symbol(&script, "on_update");
    	 break;

    case CSFX_UNLOAD:
    	 on_update = NULL;
    	 break;

    case CSFX_FAILED:
    	 /* an error occur when call csfx_main */
    	 break;
    }

    if (on_update)
    {
	on_update(script.userdata);
    }
}

/* Clean up */
csfx_script_free(&script);
csfx_quit();
```
2. Script library:
```C
#include "csfx.h"

void* csfx_main(void* userdata, int old_state, int state)
{
    switch (state)
    {
    case CSFX_INIT:
    	 /* create userdata */
    	 break;

    case CSFX_QUIT:
    	 /* delete userdata */
     	 break;

    case CSFX_RELOAD:
    	 /* storing userdata, create local stuffs */
    	 break;

    case CSFX_UNLOAD:
    	 /* garbage collect userdata, delete local stuffs*/
    	 break;

    case CSFX_FAILED:
    	 /* never reached here */
    	 break;
    }

    return userdata; /* userdata is default NULL */
}

void on_update(void* userdata)
{
    /* @todo: update stuffs */
}
```

## Compitability
1. Visual C++ - Test passed, unlock .pdb, reload .pdb when reload .dll
2. GCC        - Test passed with Cygwin
3. MinGW      - Test passed
4. TinyCC     - Test passed

## Meta
Copyright: 2018, MaiHD @ ${HOME}