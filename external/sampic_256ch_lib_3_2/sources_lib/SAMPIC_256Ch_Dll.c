//==============================================================================
//
// Title:       sampic_lib
// Purpose:     A short description of the library.
//
// Created on:  01/07/2024  by Maalmi
// Modified on: 27/05/2025
// Copyright:   LAL. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files


 #ifdef _WINDOWS
 #include "windows.h"
#else
 #include <malloc.h>  
#endif

#include <utility.h>

#include "lpDevC.h"

#include "SAMPIC_256Ch_Type.h" 
#include "SAMPIC_256Ch_lib.h"

	 

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

/// HIFN  What does your function do?
/// HIPAR x/What inputs does your function expect?
/// HIRET What does your function return?
int Your_Functions_Here (int x)
{
    return x;
}

//==============================================================================
// DLL main entry-point functions

int __stdcall DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            if (InitCVIRTE (hinstDLL, 0, 0) == 0)

				return 0;     /* out of memory */
			
            break;
        case DLL_PROCESS_DETACH:
            CloseCVIRTE ();
            break;
    }
    
    return 1;
}

/*
int __stdcall DllEntryPoint (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    // Included for compatibility with Borland 

    return DllMain (hinstDLL, fdwReason, lpvReserved);
} */
