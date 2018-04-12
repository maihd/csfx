#include <stdio.h>
#include "csfx.h"

__csfx_call__
void* csfx_main(void* userdata, int state)
{
    switch (state)
    {
    case CSFX_INIT:
	printf("Script is initialize\n");
	break;

    case CSFX_QUIT:
	printf("Script is quit\n");
	break;

    case CSFX_RELOAD:
	printf("Script is reload\n");
	break;

    case CSFX_UNLOAD:
	printf("Script is unload\n");
	break;

    default:
	/* Cannot reached here */
	break;
    }
    
    return userdata;
}
