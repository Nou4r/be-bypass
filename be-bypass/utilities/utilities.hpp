#pragma once

#include <ntifs.h>

typedef struct _KLDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY		InLoadOrderLinks;
	void* ExceptionTable;
	unsigned int	 ExceptionTableSize;
	void* GpValue;
	void* NonPagedDebugInfo;
	unsigned char* DllBase;
	void* EntryPoint;
	unsigned int	 SizeOfImage;
	UNICODE_STRING	 FullDllName;
	UNICODE_STRING	 BaseDllName;
	unsigned int	 Flags;
	unsigned __int16 LoadCount;
	unsigned __int16 u1;
	void* SectionPointer;
	unsigned int	 CheckSum;
	unsigned int	 CoverageSectionSize;
	void* CoverageSection;
	void* LoadedImports;
	void* Spare;
	unsigned int	 SizeOfImageNotRounded;
	unsigned int	 TimeDateStamp;
} KLDR_DATA_TABLE_ENTRY, * PKLDR_DATA_TABLE_ENTRY;

extern "C"
{
	__declspec(dllimport) PLIST_ENTRY PsLoadedModuleList;

	NTSTATUS NTAPI MmCopyVirtualMemory
	(
		PEPROCESS SourceProcess,
		PVOID SourceAddress,
		PEPROCESS TargetProcess,
		PVOID TargetAddress,
		SIZE_T BufferSize,
		KPROCESSOR_MODE PreviousMode,
		PSIZE_T ReturnSize
	);
}

namespace utilities
{
	PKLDR_DATA_TABLE_ENTRY get_driver( const wchar_t* const driver_name );

	void force_copy( void* const destination, const void* const source, const size_t size );
}