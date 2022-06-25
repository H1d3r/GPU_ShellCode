#pragma once
#include <Windows.h>

// from GpuMemoryAbuse.h : check if a nividia gpu is present on the system, to do the thing
BOOL IsNvidiaGraphicsCardPresent();

// hook.c, this function does the following:
//	1- run functions that will initialize the cuda api struct and run other api to get us started ...
//	2- install 2 hooks, on sleep and on virtualalloc, using minhook library
//	3- start the vector exception handler
BOOL InitializeMemToGpu ();

