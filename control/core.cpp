#include <Windows.h>
#include <memory>
#include <string>

class driver_t
{
	struct data_t
	{
		enum
		{
			get_base,
			get_peb,
			copy_memory
		};

		std::uint64_t from_process_id, to_process_id;
		const void *from_address, *to_address;
		std::size_t size;
	};

	std::unique_ptr< std::remove_pointer_t< HANDLE >, decltype( &CloseHandle ) > bedaisy{ CreateFileA( "\\\\.\\BattlEye", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ), &CloseHandle };

	const std::uint64_t from_process_id = GetCurrentProcessId( ), to_process_id;

public:
	explicit driver_t( const std::uint64_t to_process_id ) : to_process_id{ to_process_id } {}

	template < class type = std::uint8_t* >
	type read( const auto read_address )
	{
		type value;

		data_t data{ .from_process_id = to_process_id, .to_process_id = from_process_id, .from_address = reinterpret_cast< void* >( read_address ), .to_address = &value, .size = sizeof( type ) };
		if ( !DeviceIoControl( bedaisy.get( ), data_t::copy_memory, &data, 0xBE, nullptr, 0, nullptr, nullptr ) )
		{
			bedaisy = { CreateFileA( "\\\\.\\BattlEye", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ), &CloseHandle };
			return read< type >( read_address );
		}

		return value;
	}

	template < class type >
	void write( const auto write_address, const type& value )
	{
		data_t data{ .from_process_id = from_process_id, .to_process_id = to_process_id, .from_address = &value, .to_address = reinterpret_cast< void* >( write_address ), .size = sizeof( type ) };
		if ( !DeviceIoControl( bedaisy.get( ), data_t::copy_memory, &data, 0xBE, nullptr, 0, nullptr, nullptr ) )
		{
			bedaisy = { CreateFileA( "\\\\.\\BattlEye", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr ), &CloseHandle };
			write< type >( write_address, value );
		}
	}

	std::uint8_t* base( )
	{
		static std::uint8_t* base = nullptr;
		if ( !base )
		{
			data_t data{ .from_process_id = from_process_id, .to_process_id = to_process_id, .from_address = &base };
			DeviceIoControl( bedaisy.get( ), data_t::get_base, &data, 0xBE, nullptr, 0, nullptr, nullptr );
		}

		return base;
	}

	std::uint8_t* peb( )
	{
		static std::uint8_t* peb = nullptr;
		if ( !peb )
		{
			data_t data{ .from_process_id = from_process_id, .to_process_id = to_process_id, .from_address = &peb };
			DeviceIoControl( bedaisy.get( ), data_t::get_peb, &data, 0xBE, nullptr, 0, nullptr, nullptr );
		}

		return peb;
	}

	std::uint8_t* dll( const std::wstring& dll_name )
	{
		//if ( peb( ) )
		{
			auto v5 = read( peb( ) + 0x18 );

			if ( v5 )
			{
				auto v6 = read( v5 + 16 );
				if ( v6 )
				{
					while ( read( v6 + 0x30 ) )
					{
						auto length = read<USHORT>( v6 + 0x58 );

						auto start = read( v6 + 0x60 );

						std::wstring name{};

						name.reserve( length / 2 );

						for ( auto i = 0u; i < length / 2; ++i )
						{
							name.push_back( read< WCHAR>( start + i * 2 ) );
						}

						if ( name == dll_name )
							return read( v6 + 0x30 );

						v6 = read( v6 );
						if ( !v6 )
							return 0;
					}
				}
			}
		}
	}
};

int main( )
{
	return 0;
}
