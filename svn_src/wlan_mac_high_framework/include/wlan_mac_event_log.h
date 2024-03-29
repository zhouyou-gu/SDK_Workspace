/** @file wlan_mac_event_log.h
 *  @brief MAC Event Log Framework
 *
 *  Contains code for logging MAC events in DRAM.
 *
 *  @copyright Copyright 2013-2017, Mango Communications. All rights reserved.
 *          Distributed under the Mango Communications Reference Design License
 *              See LICENSE.txt included in the design archive or
 *              at http://mangocomm.com/802.11/license
 *
 *  This file is part of the Mango 802.11 Reference Design (https://mangocomm.com/802.11)
 */




/*************************** Constant Definitions ****************************/
#ifndef WLAN_MAC_EVENT_LOG_H_
#define WLAN_MAC_EVENT_LOG_H_

/***************************** Include Files *********************************/
#include "xil_types.h"




// ****************************************************************************
// Define Event Log Constants
//

// Define default event logging state
//   '0' - Event logging is disabled on boot
//   '1' - Event logging is enabled on boot
//
#define EVENT_LOG_DEFAULT_LOGGING                          1


// Define event logging enable / disable
#define EVENT_LOG_LOGGING_ENABLE                           1
#define EVENT_LOG_LOGGING_DISABLE                          2


// Define event wrapping enable / disable
#define EVENT_LOG_WRAP_ENABLE                              1
#define EVENT_LOG_WRAP_DISABLE                             2


// Define event wrap buffer
//   This is the number of additional bytes that the head address will be incremented
//   on each request.  Currently, when the log wraps, the head address will stay just
//   ahead of the next address which will add overhead to the logging calls.  This
//   will allow the head address to be incremented by an additional buffer so that
//   the head pointer will not have to increment as often.  By default this is zero.
//
#define EVENT_LOG_WRAPPING_BUFFER                          0


// Maximum number of events to store in log
//   A maximum event length of -1 is used to signal that the entire DRAM after the queue
//   should be used for logging events. If MAX_EVENT_LOG > 0, then that number of events
//   will be the maximum.
//
#define MAX_EVENT_LOG                                     -1


// Define magic number that will indicate the start of an event
//   - Magic number was chose so that:
//       - It would not be in DDR address space
//       - It is human readable
//
#define EVENT_LOG_MAGIC_NUMBER                             0xACED0000


// Define constants for function flags
//   NOTE:  the transmit flag is defined in wlan_exp_common.h since it is used in multiple places
#define EVENT_LOG_NO_COUNTS                                0
#define EVENT_LOG_COUNTS                                   1


/*********************** Global Structure Definitions ************************/

//-----------------------------------------------
// Log Entry Header
//   - This is used by the event log but is not exposed to the user
//
typedef struct entry_header{
    u32 entry_id;
    u16 entry_type;
    u16 entry_length;
} entry_header;



/*************************** Function Prototypes *****************************/

void      event_log_init( char* start_address, u32 size );

void      event_log_reset();

int       event_log_config_wrap( u32 enable );
int       event_log_config_logging( u32 enable );

u32       event_log_get_data(u32 start_index, u32 size, void* buffer, u8 copy_data);
u32       event_log_get_size( u32 start_index );
u32       event_log_get_total_size( void );
u32       event_log_get_capacity( void );
u32       event_log_get_next_entry_index( void );
u32       event_log_get_oldest_entry_index( void );
u32       event_log_get_num_wraps( void );
u32       event_log_get_flags( void );
void*     event_log_get_next_empty_entry( u16 entry_type, u16 entry_size );

void      print_event_log( u32 num_events );
void      print_event_log_size();

#endif /* WLAN_MAC_EVENT_LOG_H_ */
