#include <stdint.h>
#include "utilities/utilities.hpp"

extern "C"
{
	#include "dependencies/hk/hk.h"
}

extern "C" __declspec( dllimport ) NTSTATUS
ObReferenceObjectByName(
	__in PUNICODE_STRING ObjectName,
	__in ULONG Attributes,
	__in_opt PACCESS_STATE AccessState,
	__in_opt ACCESS_MASK DesiredAccess,
	__in POBJECT_TYPE ObjectType,
	__in KPROCESSOR_MODE AccessMode,
	__inout_opt PVOID ParseContext,
	__out PVOID* Object
);

extern "C" __declspec( dllimport ) POBJECT_TYPE* IoDriverObjectType;

#define DbgPrint 

void* allocation;

uint8_t* load_image_location;
void( *load_image_original )( const UNICODE_STRING&, HANDLE, const IMAGE_INFO& );

// https://github.com/ALEHACKsp/kernel_library/tree/master/kernel_library
template < size_t N >
UNICODE_STRING make_unicode( const wchar_t( &str )[ N ] )
{
	UNICODE_STRING result{};

	result.Buffer = const_cast< wchar_t* >( str );
	result.Length = ( N - 1 ) * 2; /* size of the string, multiplied by the size of 1 wide character. */
	result.MaximumLength = result.Length + 2;

	return result;
}

template < typename function_t, size_t N >
function_t get_export( const wchar_t( &export_name )[ N ] )
{
	auto string = make_unicode( export_name );
	return static_cast< function_t >( MmGetSystemRoutineAddress( &string ) );
}

struct imports_t
{
	using ps_lookup_process_by_process_id_t = decltype( &PsLookupProcessByProcessId );
	ps_lookup_process_by_process_id_t ps_lookup_process_by_process_id = get_export< ps_lookup_process_by_process_id_t >( L"PsLookupProcessByProcessId" );

	using obf_dereference_object_t = decltype( &ObfDereferenceObject );
	obf_dereference_object_t obf_dereference_object = get_export< obf_dereference_object_t >( L"ObfDereferenceObject" );
	
	using mm_copy_virtual_memory_t = decltype( &MmCopyVirtualMemory );
	mm_copy_virtual_memory_t mm_copy_virtual_memory = get_export< mm_copy_virtual_memory_t >( L"MmCopyVirtualMemory" );

	using iof_complete_request_t = decltype( &IofCompleteRequest );
	iof_complete_request_t iof_complete_request = get_export< iof_complete_request_t >( L"IofCompleteRequest" );

	uint8_t* original;
};

constexpr auto stub_size = 275;
NTSTATUS stub( PDEVICE_OBJECT device_object, PIRP irp )
{
	imports_t* imports = reinterpret_cast< imports_t* >( 0x1122334455667788 );

	struct data_t
	{
		enum
		{
			get_base,
			get_peb,
			copy_memory
		};

		HANDLE from_process_id, to_process_id;
		void *from_address, *to_address;
		size_t size;
	};

	const auto stack_location = IoGetCurrentIrpStackLocation( irp );

	if ( stack_location->Parameters.DeviceIoControl.InputBufferLength != 0xBE )
		return reinterpret_cast< decltype( &stub ) >( imports->original )( device_object, irp );
	
	const auto data = static_cast< data_t* >( irp->AssociatedIrp.SystemBuffer );

	PEPROCESS from_process;
	imports->ps_lookup_process_by_process_id( data->from_process_id, &from_process );

	PEPROCESS to_process;
	imports->ps_lookup_process_by_process_id( data->to_process_id, &to_process );

	switch ( const auto command = stack_location->Parameters.DeviceIoControl.IoControlCode; command )
	{
		case data_t::get_peb:
		case data_t::get_base:
		{	
			*reinterpret_cast< void** >( data->from_address ) = *reinterpret_cast< void** >( reinterpret_cast< uintptr_t >( to_process ) + ( command == data_t::get_peb ? 0x550 : 0x520 ) );

			break;
		}
		case data_t::copy_memory:
		{
			size_t out_size;
			imports->mm_copy_virtual_memory( from_process, data->from_address, to_process, data->to_address, data->size, UserMode, &out_size );

			break;
		}
	}

	if ( from_process )
		imports->obf_dereference_object( from_process );

	if ( to_process )
		imports->obf_dereference_object( to_process );

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_SUCCESS;
	imports->iof_complete_request( irp, IO_NO_INCREMENT );
	
	return STATUS_SUCCESS;
}

void fix_imports( uint8_t* original )
{
	imports_t temp_imports{ .original = original };
	const auto imports = memcpy( ExAllocatePool( NonPagedPoolNx, sizeof( imports_t ) ), &temp_imports, sizeof( imports_t ) );

	auto stub = reinterpret_cast< uint8_t* >( &::stub );

	for ( auto i = stub; i < stub + stub_size; ++i )
	{
		if ( const auto read = *reinterpret_cast< uintptr_t* >( i ); read >= 0x1122334455667788 && read <= 0x1122334455667788 + 40 )
		{
			*reinterpret_cast< uintptr_t* >( i ) = reinterpret_cast< uintptr_t >( imports ) + ( read - 0x1122334455667788 );
		}
	}
}

void load_image_hook( const UNICODE_STRING& image_name, const HANDLE process_id, const IMAGE_INFO& image_info )
{
	if ( wcsstr( image_name.Buffer, L"BEDaisy.sys" ) )
	{
		load_image_original( image_name, process_id, image_info );
		HkRestoreFunction( load_image_location, load_image_original );

		const auto code_cave = static_cast< uint8_t* >( image_info.ImageBase ) + 0x35D5; 

		const auto ioctl_dispatcher = static_cast< uint8_t* >( image_info.ImageBase ) + 0x32DE2B;

		uint8_t jump[ ] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
		*reinterpret_cast< uintptr_t* >( jump + 1 ) = ( ioctl_dispatcher + 5 ) - ( code_cave + stub_size + 10 ) - 5;

		utilities::force_copy( code_cave + stub_size + 5, ioctl_dispatcher, 5 );
		utilities::force_copy( code_cave + stub_size + 10, jump, sizeof( jump ) );
		
		fix_imports( code_cave + stub_size + 5 );

		utilities::force_copy( code_cave, &stub, stub_size );

		*reinterpret_cast< uintptr_t* >( jump + 1 ) = code_cave - ioctl_dispatcher - 5;
		utilities::force_copy( ioctl_dispatcher, jump, sizeof( jump ) );

		ExFreePool( allocation );
		
		return;
	}

	load_image_original( image_name, process_id, image_info );
}

NTSTATUS core( void* const allocation )
{
	::allocation = allocation;

	const auto ahcache = utilities::get_driver( L"ahcache.sys" );

	load_image_location = ahcache->DllBase + 0x1000;
	do
		++load_image_location;
	while ( memcmp( load_image_location, "\x48\x85\xC9\x0F\x84\x72\x01\x00\x00\x4C\x8B\xDC\x55\x41\x56\x41\x57\x48\x83\xEC\x60\x45\x33\xFF", 24 ) );

	HkDetourFunction( load_image_location, &load_image_hook, 15, reinterpret_cast< void** >( &load_image_original ) );

	UNICODE_STRING disk_str = RTL_CONSTANT_STRING( L"\\Driver\\Disk" );
	PDRIVER_OBJECT disk;
	ObReferenceObjectByName( &disk_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, 0, *IoDriverObjectType, KernelMode, nullptr, reinterpret_cast< void** >( &disk ) );

	auto class_dispatch_unimplemented = reinterpret_cast< uint8_t* >( disk->MajorFunction[ IRP_MJ_DEVICE_CONTROL ] );
	do
		++class_dispatch_unimplemented;
	while ( memcmp( class_dispatch_unimplemented, "\x40\x53\x48\x83\xEC\x20\x48\x8B\x82", 9 ) );

	disk->MajorFunction[ IRP_MJ_DEVICE_CONTROL ] = reinterpret_cast< PDRIVER_DISPATCH >( class_dispatch_unimplemented );

	return STATUS_SUCCESS;
}
