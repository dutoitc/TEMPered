#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "hidapi.h"

#include "tempered.h"

#include "temper_type.h"

struct tempered_device_ {
	hid_device *hid_dev;
	struct temper_type const *type;
};

/** This define is used for convenience to set the error message only when the
 * pointer to it is not NULL, and depends on the parameter being named "error".
 */
#define set_error( error_msg ) do { \
		if ( error != NULL ) { *error = error_msg; } \
	} while (false)

/** Initialize the TEMPered library. */
bool tempered_init( char **error )
{
	if ( hid_init() != 0 )
	{
		set_error( "Could not initialize the HID API." );
		return false;
	}
	return true;
}

/** Finalize the TEMPered library. */
bool tempered_exit( char **error )
{
	if ( hid_exit() != 0 )
	{
		set_error( "Error shutting down the HID API." );
		return false;
	}
	return true;
}

/** Enumerate the TEMPer devices. */
struct tempered_device_list* tempered_enumerate( char **error )
{
	struct tempered_device_list *list = NULL, *current = NULL;
	struct hid_device_info *devs, *info;
	devs = hid_enumerate( 0, 0 );
	if ( devs == NULL )
	{
		set_error( "No HID devices were found." );
		return NULL;
	}
	for ( info = devs; info; info = info->next )
	{
		temper_type* type = get_temper_type( info );
		if ( type != NULL && !type->ignored )
		{
			printf(
				"Device %04hx:%04hx if %d rel %4hx | %s | %ls %ls\n",
				info->vendor_id, info->product_id,
				info->interface_number, info->release_number,
				info->path,
				info->manufacturer_string, info->product_string
			);
			struct tempered_device_list *next = malloc(
				sizeof( struct tempered_device_list )
			);
			if ( next == NULL )
			{
				tempered_free_device_list( list );
				set_error( "Unable to allocate memory for list." );
				return NULL;
			}
			
			next->next = NULL;
			next->path = strdup( info->path );
			next->internal_data = type;
			next->type_name = type->name;
			next->vendor_id = info->vendor_id;
			next->product_id = info->product_id;
			next->interface_number = info->interface_number;
			
			if ( next->path == NULL )
			{
				tempered_free_device_list( list );
				set_error( "Unable to allocate memory for path." );
				return NULL;
			}
			
			if ( current == NULL )
			{
				list = next;
				current = list;
			}
			else
			{
				current->next = next;
				current = current->next;
			}
		}
	}
	hid_free_enumeration( devs );
	return list;
}

/** Free the memory used by the given device list. */
void tempered_free_device_list( struct tempered_device_list *list )
{
	while ( list != NULL )
	{
		struct tempered_device_list *next = list->next;
		free( list->path );
		free( list );
		list = next;
	}
}

/** Open a given device from the list. */
tempered_device* tempered_open( struct tempered_device_list *list, char **error )
{
	// TODO: move this into temper_type->open()
	tempered_device *device = malloc( sizeof( tempered_device ) );
	if ( device == NULL )
	{
		set_error( "Could not allocate memory for the device." );
		return NULL;
	}
	device->type = (temper_type *)list->internal_data;
	device->hid_dev = hid_open_path( list->path );
	if ( !device->hid_dev )
	{
		set_error( "Could not open the HID device." );
		free( device );
		return NULL;
	}
	return device;
}

/** Close an open device. */
void tempered_close( tempered_device *device )
{
	if ( device == NULL )
	{
		return;
	}
	// TODO: move this into temper_type->close()
	hid_close( device->hid_dev );
	free( device );
}

/** Get the type name of the given device. */
char const * tempered_get_type_name( tempered_device *device, char **error )
{
	if ( device == NULL || device->type == NULL || device->type->name == NULL )
	{
		set_error( "Invalid device handle." );
		return NULL;
	}
	return device->type->name;
}
