#include "utilities.hpp"

PKLDR_DATA_TABLE_ENTRY utilities::get_driver( const wchar_t* const driver_name )
{
	for ( auto ldr_entry = PsLoadedModuleList->Flink; ldr_entry != PsLoadedModuleList; ldr_entry = ldr_entry->Flink )
	{
		const auto entry = reinterpret_cast< PKLDR_DATA_TABLE_ENTRY >( ldr_entry );

		if ( wcsstr( entry->BaseDllName.Buffer, driver_name ) )
			return entry;
	}

	return nullptr;
}

void utilities::force_copy( void* const destination, const void* const source, const size_t size )
{
	const auto mapped = MmMapIoSpaceEx( MmGetPhysicalAddress( destination ), size, PAGE_READWRITE );

	memcpy( mapped, source, size );

	MmUnmapIoSpace( mapped, size );
}