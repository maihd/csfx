// csfx_script.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

extern "C" 
{
    __declspec(dllexport) int __csfx_testing = 1;
}