#include "app.h"
#include <csfx.h>
#include <stdio.h>
#include <Windows.h>

void* csfx_main(void* userdata, int old_state, int state)
{
    switch (state)
    {
    case CSFX_INIT:
	break;

    case CSFX_QUIT:
	break;

    case CSFX_RELOAD:
	break;

    case CSFX_UNLOAD:
	break;
    }
    return userdata;
}

__declspec(dllexport)
void on_paint(HDC hdc, int width, int height)
{
    SelectObject(hdc, GetStockObject(GRAY_BRUSH));
    Ellipse(hdc, 200, 200, 200 + 200, 200 + 200);
}
