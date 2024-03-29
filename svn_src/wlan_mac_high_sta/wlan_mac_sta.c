/** @file wlan_mac_sta.c
 *  @brief Station
 *
 *  This contains code for the 802.11 Station.
 *
 *  @copyright Copyright 2013-2017, Mango Communications. All rights reserved.
 *          Distributed under the Mango Communications Reference Design License
 *              See LICENSE.txt included in the design archive or
 *              at http://mangocomm.com/802.11/license
 *
 *  This file is part of the Mango 802.11 Reference Design (https://mangocomm.com/802.11)
 */

/***************************** Include Files *********************************/
#include "wlan_mac_high_sw_config.h"

// Xilinx SDK includes
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "xil_cache.h"

// WLAN includes
#include "wlan_platform_common.h"
#include "wlan_platform_high.h"
#include "wlan_mac_802_11_defs.h"
#include "wlan_mac_queue.h"
#include "wlan_mac_event_log.h"
#include "wlan_mac_entries.h"
#include "wlan_mac_ltg.h"
#include "wlan_mac_high.h"
#include "wlan_mac_packet_types.h"
#include "wlan_mac_eth_util.h"
#include "ascii_characters.h"
#include "wlan_mac_schedule.h"
#include "wlan_mac_dl_list.h"
#include "wlan_mac_network_info.h"
#include "wlan_mac_station_info.h"
#include "wlan_mac_scan.h"
#include "wlan_mac_sta_join.h"
#include "wlan_mac_sta.h"
#include "wlan_mac_common.h"
#include "wlan_mac_pkt_buf_util.h"


// WLAN Exp includes
#include "wlan_exp.h"
#include "wlan_exp_common.h"
#include "wlan_exp_node.h"
#include "wlan_exp_node_sta.h"
#include "wlan_exp_transport.h"
#include "wlan_exp_user.h"


/*************************** Constant Definitions ****************************/

#define WLAN_EXP_ETH TRANSPORT_ETH_B


#define WLAN_DEFAULT_CHANNEL 6
#define WLAN_DEFAULT_TX_PWR 15
#define WLAN_DEFAULT_TX_ANTENNA TX_ANTMODE_SISO_ANTA
#define WLAN_DEFAULT_RX_ANTENNA RX_ANTMODE_SISO_ANTA


// The WLAN_DEFAULT_USE_HT define will set the default unicast TX phy mode
// to:  1 --> HTMF  or  0 --> NONHT.
#define WLAN_DEFAULT_USE_HT 1


/*********************** Global Variable Definitions *************************/


/*************************** Variable Definitions ****************************/

// If you want this station to try to associate to a known AP at boot, type
//   the string here. Otherwise, let it be an empty string.
static char access_point_ssid[SSID_LEN_MAX + 1] = "MANGO-AP";
// static char access_point_ssid[SSID_LEN_MAX + 1] = "";

// Common TX header for 802.11 packets
mac_header_80211_common tx_header_common;

// Access point information
u8 my_aid;
network_info_t* active_network_info;

// Tx queue variables;
static u32 max_queue_size;
volatile u8 pause_data_queue;

// MAC address
static u8 wlan_mac_addr[MAC_ADDR_LEN];

// Beacon configuration
static beacon_txrx_config_t gl_beacon_txrx_config;


/*************************** Functions Prototypes ****************************/

#if WLAN_SW_CONFIG_ENABLE_WLAN_EXP
int  wlan_exp_process_user_cmd(u32 cmd_id, int socket_index, void* from, cmd_resp* command, cmd_resp* response, u32 max_resp_len);
#endif

void sta_set_beacon_ts_update_mode(u32 enable);


/******************************** Functions **********************************/


int main() {
	// Initialize Microblaze --
	//  these functions should be called before anything
	//  else is executed
	Xil_DCacheDisable();
	Xil_ICacheDisable();
	microblaze_enable_exceptions();

	// This list of channels will be used by the active scan state machine. The STA will scan
	//  each channel looking for a network with the default SSID
	//

	volatile join_parameters_t*             join_parameters;
	compilation_details_t					compilation_details;
	bzero(&compilation_details, sizeof(compilation_details_t));

	// Print initial message to UART
	xil_printf("\f");
	xil_printf("----- Mango 802.11 Reference Design -----\n");
	xil_printf("----- v1.7.1 ----------------------------\n");
	xil_printf("----- wlan_mac_sta ----------------------\n");
	xil_printf("Compiled %s %s\n\n", __DATE__, __TIME__);
	strncpy(compilation_details.compilation_date, __DATE__, 12);
	strncpy(compilation_details.compilation_time, __TIME__, 9);

	wlan_mac_common_malloc_init();

	// Initialize the maximum TX queue size
	max_queue_size = MAX_TX_QUEUE_LEN;

	// Unpause the queue
	pause_data_queue = 0;

	// Initialize AID / Beacons configuration (not associated with an AP)
	my_aid = 0;

	gl_beacon_txrx_config.ts_update_mode = ALWAYS_UPDATE;
	bzero(gl_beacon_txrx_config.bssid_match, MAC_ADDR_LEN);
	gl_beacon_txrx_config.beacon_tx_mode = NO_BEACON_TX;
	gl_beacon_txrx_config.beacon_interval_tu = 0;

	// Initialize the utility library
	wlan_mac_high_init();

	// STA is not currently a member of a BSS
	configure_bss(NULL, 0);

	// Initialize hex display to "No BSS"
	wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, 0xFF);

	// Initialize the join state machine
	wlan_mac_sta_join_init();

	// Set default Tx params
	// Set sane default Tx params. These will be overwritten by the user application
	tx_params_t	tx_params = { .phy = { .mcs = 3, .phy_mode = PHY_MODE_NONHT, .antenna_mode = WLAN_DEFAULT_TX_ANTENNA, .power = WLAN_DEFAULT_TX_PWR },
							  .mac = { .flags = 0 } };

	if(WLAN_DEFAULT_USE_HT) tx_params.phy.phy_mode = PHY_MODE_HTMF;

	wlan_mac_set_default_tx_params(unicast_data, &tx_params);

	tx_params.phy.mcs = 0;
	tx_params.phy.phy_mode = PHY_MODE_NONHT;

	wlan_mac_set_default_tx_params(unicast_mgmt, &tx_params);
	wlan_mac_set_default_tx_params(mcast_data, &tx_params);
	wlan_mac_set_default_tx_params(mcast_mgmt, &tx_params);

	// Re-apply the defaults to any existing station_info_t structs that this AP
	// knows about
	wlan_mac_reapply_default_tx_params();


	// Initialize callbacks
#if WLAN_SW_CONFIG_ENABLE_ETH_BRIDGE
	wlan_mac_util_set_eth_rx_callback((void*) ethernet_receive);
#endif
	wlan_mac_high_set_mpdu_rx_callback((void*) mpdu_rx_process);
	wlan_mac_high_set_uart_rx_callback((void*) uart_rx);
	wlan_mac_high_set_poll_tx_queues_callback((void*) poll_tx_queues);
#if WLAN_SW_CONFIG_ENABLE_LTG
	wlan_mac_ltg_sched_set_callback((void*) ltg_event);
#endif //WLAN_SW_CONFIG_ENABLE_LTG
	wlan_mac_scan_set_tx_probe_request_callback((void*) send_probe_req);
	wlan_mac_scan_set_state_change_callback((void*) process_scan_state_change);

#if WLAN_SW_CONFIG_ENABLE_ETH_BRIDGE
	// Set the Ethernet ecapsulation mode
	wlan_mac_util_set_eth_encap_mode(APPLICATION_ROLE_STA);
#endif

    wlan_mac_hw_info_t* hw_info;
    // Get the hardware info that has been collected from CPU low
    hw_info = get_mac_hw_info();

#if WLAN_SW_CONFIG_ENABLE_WLAN_EXP

    // NOTE:  To use the WLAN Experiments Framework, it must be initialized after
    //        CPU low has populated the hw_info structure in the MAC High framework.

    // Initialize WLAN Exp
	wlan_exp_node_init(hw_info->serial_number, hw_info->fpga_dna,
			   WLAN_EXP_ETH, hw_info->hw_addr_wlan_exp, hw_info->hw_addr_wlan);

    // Set WLAN Exp callbacks
    wlan_exp_set_process_node_cmd_callback((void*) wlan_exp_process_node_cmd);
    wlan_exp_set_purge_all_data_tx_queue_callback((void*) purge_all_data_tx_queue);
    //   - wlan_exp_set_tx_cmd_add_association_callback() should not be used by the STA
    wlan_exp_set_process_user_cmd_callback((void*) wlan_exp_process_user_cmd);
    wlan_exp_set_beacon_ts_update_mode_callback((void*) sta_set_beacon_ts_update_mode);
    wlan_exp_set_process_config_bss_callback((void*) configure_bss);
    wlan_exp_set_active_network_info_getter_callback((void*) active_network_info_getter);
    //   - wlan_exp_set_beacon_tx_param_update_callback() should not be used by the STA

    // Set CPU_HIGH Type in wlan_exp's node_info struct;
    wlan_exp_node_set_type_high(APPLICATION_ROLE_STA, &compilation_details);
#endif

	// CPU Low will pass HW information to CPU High as part of the boot process
	//   - Get necessary HW information
	memcpy((void*) &(wlan_mac_addr[0]), (void*) get_mac_hw_addr_wlan(), MAC_ADDR_LEN);

    // Set Header information
	tx_header_common.address_2 = &(wlan_mac_addr[0]);

	// Set the at-boot MAC Time to 0 usec
	set_mac_time_usec(0);

	wlan_mac_high_set_radio_channel(WLAN_DEFAULT_CHANNEL);
	wlan_mac_high_set_rx_ant_mode(WLAN_DEFAULT_RX_ANTENNA);
	wlan_mac_high_set_tx_ctrl_power(WLAN_DEFAULT_TX_PWR);
	wlan_mac_high_set_radio_tx_power(WLAN_DEFAULT_TX_PWR);

	wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_APPLICATION_ROLE, APPLICATION_ROLE_STA);

	// Schedule all events
	//     None at this time

#if WLAN_SW_CONFIG_ENABLE_LOGGING
	// Reset the event log
	event_log_reset();
#endif

	// Print Station information to the terminal
    xil_printf("------------------------\n");
    xil_printf("WLAN MAC Station boot complete: \n");
    xil_printf("  Serial Number : %s-%05d\n", hw_info->serial_number_prefix, hw_info->serial_number);
    xil_printf("  Default SSID  : %s \n", access_point_ssid);
	xil_printf("  MAC Addr      : %02x:%02x:%02x:%02x:%02x:%02x\n\n", wlan_mac_addr[0], wlan_mac_addr[1], wlan_mac_addr[2], wlan_mac_addr[3], wlan_mac_addr[4], wlan_mac_addr[5]);

#ifdef WLAN_USE_UART_MENU
	xil_printf("\nPress the Esc key in your terminal to access the UART menu\n");
#endif

	// Start the interrupts
	wlan_mac_high_interrupt_restore_state(INTERRUPTS_ENABLED);

	// If there is a default SSID and the DIP switch allows it, initiate a probe request
	if ((strlen(access_point_ssid) > 0) && ((wlan_platform_userio_get_state() & USERIO_INPUT_MASK_SW_3) == 0)) {
		// Get current join parameters
		join_parameters = wlan_mac_sta_get_join_parameters();

		// Set join parameters
		//     - Zero out BSSID / Channel so the node performs a scan before joining
		join_parameters->channel = 0;
		bzero((void*)join_parameters->bssid, MAC_ADDR_LEN);

		wlan_mac_high_free(join_parameters->ssid);
		join_parameters->ssid = strndup(access_point_ssid, SSID_LEN_MAX);

		// Join the default SSID
		wlan_mac_sta_join();
	}

	while(1){
#if WLAN_SW_CONFIG_ENABLE_WLAN_EXP
		// The wlan_exp Ethernet handling is not interrupt based. Periodic polls of the wlan_exp
		//     transport are required to service new commands. All other node activity (wired/wireless Tx/Rx,
		//     scheduled events, user interaction, etc) are handled via interrupt service routines
		transport_poll(WLAN_EXP_ETH);
#endif
	}

	// Unreachable, but non-void return keeps the compiler happy
	return -1;
}



/*****************************************************************************/
/**
 * @brief Send probe requet
 *
 * This function is part of the scan infrastructure and will be called whenever
 * the node needs to send a probe request.
 *
 * @param   None
 * @return  None
 *
 *****************************************************************************/
void send_probe_req(){
	u16 tx_length;
	dl_entry* curr_tx_queue_element;
	tx_queue_buffer_t* curr_tx_queue_buffer;
	volatile scan_parameters_t* scan_parameters = wlan_mac_scan_get_parameters();

	// Check out queue element for packet
	curr_tx_queue_element = queue_checkout();

	// Create probe request
	if(curr_tx_queue_element != NULL){
		curr_tx_queue_buffer = (tx_queue_buffer_t*)(curr_tx_queue_element->data);

		// Setup the TX header
		wlan_mac_high_setup_tx_header(&tx_header_common, (u8 *)bcast_addr, (u8 *)bcast_addr);

		// Fill in the data
		tx_length = wlan_create_probe_req_frame((void*)(curr_tx_queue_buffer->frame), &tx_header_common, scan_parameters->ssid);

		// Fill in metadata
		curr_tx_queue_buffer->flags = 0;
		curr_tx_queue_buffer->length = tx_length;
		curr_tx_queue_buffer->station_info = station_info_create((u8*)bcast_addr);

		// Put the packet in the queue
		enqueue_after_tail(MANAGEMENT_QID, curr_tx_queue_element);

		// Poll the TX queues to possibly send the packet
		poll_tx_queues(PKT_BUF_GROUP_GENERAL);
	}
}



/*****************************************************************************/
/**
 * @brief Handle state change in the network scanner
 *
 * This function is part of the scan infrastructure and will be called whenever
 * the scanner is started, stopped, paused, or resumed. This allows the STA
 * to revert the channel to a known-good state when the scanner has stopped and
 * also serves as notification to the project that it should stop dequeueing data
 * frames since it might be on a different channel than its intended recipient.
 *
 * @param   None
 * @return  None
 *
 *****************************************************************************/
void process_scan_state_change(scan_state_t scan_state){

	// ------------------------------------------------------------------------
	// Note on scanning:
	//
	//   Currently, scanning should only be done with active_bss_info = NULL, ie the
	// node is not currently in a BSS.  This is to avoid any corner cases.  The
	// STA needs to do the following things to make scanning safe when active_bss_info
	// is not NULL:
	//
	//     - Send a NULL data packet indicating that the station is entering a
	//       DOZE state when the scan is started or resumed.
	//     - Send a NULL data packet indicating that the station is entering an
	//       AWAKE state when the scan is paused or stopped.
	//
	// Note: The STA doesn't need full power savings functionality to enact this
	// behavior (e.g. listening for DTIMS at particular intervals, watching the TIM
	// bitmap for our AID, etc).  The decision to doze or wake isn't a function of
	// what the AP is up to, it's a function of whether the STA decides to start
	// scanning.
	// ------------------------------------------------------------------------

	switch(scan_state){
		case SCAN_IDLE:
		case SCAN_PAUSED:
			pause_data_queue = 0;
			if(active_network_info != NULL){
				wlan_mac_high_set_radio_channel(
						wlan_mac_high_bss_channel_spec_to_radio_chan(active_network_info->bss_config.chan_spec));
			}
		break;
		case SCAN_RUNNING:
			pause_data_queue = 1;
		break;
	}
}



/*****************************************************************************/
/**
 * @brief Poll Tx queues to select next available packet to transmit
 *
 * This function will attempt to completely fill all Tx packet buffers in the
 * PKT_BUF_GROUP_GENERAL group.
 *
 * The reference implementation uses a simple queue prioritization scheme:
 *   - Two queues are defined: Management (MANAGEMENT_QID) and Data (UNICAST_QID)
 *     - The Management queue is for all management traffic
 *     - The Data queue is for all data to the associated AP
 *   - The code alternates its polling between queues
 *
 *****************************************************************************/
void poll_tx_queues(){
	u32 i;
	int num_pkt_bufs_avail;
	int poll_loop_cnt;
	dl_entry* tx_queue_buffer_entry;

	#define MAX_NUM_QUEUE 2

	num_pkt_bufs_avail = wlan_mac_num_tx_pkt_buf_available(PKT_BUF_GROUP_GENERAL);


	// Are we pausing transmissions?
	if (pause_data_queue == 0) {
		static u32 queue_index = 0;

		// This loop will (at most) check every queue twice
		//  This handles the case of a single non-empty queue needing to supply packets
		//  for both GENERAL packet buffers
		poll_loop_cnt = 0;
		while((num_pkt_bufs_avail > 0) && (poll_loop_cnt < (2*MAX_NUM_QUEUE))) {
			poll_loop_cnt++;

			queue_index = (queue_index + 1) % MAX_NUM_QUEUE;

			switch(queue_index){
				case 0:
					tx_queue_buffer_entry = dequeue_from_head(MANAGEMENT_QID);
				break;
				case 1:
					tx_queue_buffer_entry = dequeue_from_head(UNICAST_QID);
				break;
			}
			if(tx_queue_buffer_entry) {
				// Update the packet buffer group
				((tx_queue_buffer_t*)(tx_queue_buffer_entry->data))->queue_info.pkt_buf_group = PKT_BUF_GROUP_GENERAL;
				// Successfully dequeued a management packet - transmit and checkin
				transmit_checkin(tx_queue_buffer_entry);
				//continue the loop
				num_pkt_bufs_avail--;
				continue;
			}
		}
	} else {
		// We are only currently allowed to send management frames. Typically this is caused by an ongoing
		// active scan
		for(i=0; i<num_pkt_bufs_avail; i++) {
			// Dequeue packets until there are no more packet buffers or no more packets
			tx_queue_buffer_entry = dequeue_from_head(MANAGEMENT_QID);
			if(tx_queue_buffer_entry) {
				// Update the packet buffer group
				((tx_queue_buffer_t*)(tx_queue_buffer_entry->data))->queue_info.pkt_buf_group = PKT_BUF_GROUP_GENERAL;
				// Successfully dequeued a management packet - transmit and checkin
				transmit_checkin(tx_queue_buffer_entry);
			} else {
				// Queue is empty - end this loop
				break;
			}
		}
	}
}



/*****************************************************************************/
/**
 * @brief Purges all packets from all Tx queues
 *
 * This function discards all currently en-queued packets awaiting transmission and returns all
 * queue entries to the free pool.
 *
 * This function does not discard packets already submitted to the lower-level MAC for transmission
 *
 * @param None
 * @return None
 *****************************************************************************/
void purge_all_data_tx_queue(){
	// Purge all data transmit queues
	purge_queue(MCAST_QID);           // Broadcast Queue
	purge_queue(UNICAST_QID);         // Unicast Queue
}

/*****************************************************************************/
/**
 * @brief Callback to handle insertion of an Ethernet reception into the corresponding wireless Tx queue
 *
 * This function is called when a new Ethernet packet is received that must be transmitted via the wireless interface.
 * The packet must be encapsulated before it is passed to this function. Ethernet encapsulation is implemented in the mac_high framework.
 *
 * The tx_queue_list argument is a DL list, but must contain exactly one queue entry which contains the encapsulated packet
 * A list container is used here to ease merging of the list with the target queue.
 *
 * @param dl_entry* curr_tx_queue_element
 *  - A single queue element containing the packet to transmit
 * @param u8* eth_dest
 *  - 6-byte destination address from original Ethernet packet
 * @param u8* eth_src
 *  - 6-byte source address from original Ethernet packet
 * @param u16 tx_length
 *  - Length (in bytes) of the packet payload
 * @return 1 for successful enqueuing of the packet, 0 otherwise
 *****************************************************************************/
int ethernet_receive(dl_entry* curr_tx_queue_element, u8* eth_dest, u8* eth_src, u16 tx_length){
	tx_queue_buffer_t* curr_tx_queue_buffer;
	station_info_t* ap_station_info;


	// Check associations
	//     Is there an AP to send the packet to?
	if(active_network_info != NULL){
		ap_station_info = (station_info_t*)((active_network_info->members.first)->data);

		// Send the packet to the AP
		if(queue_num_queued(UNICAST_QID) < max_queue_size){

			// Send the pre-encapsulated Ethernet frame over the wireless interface
			//     NOTE:  The queue element has already been provided, so we do not need to check if it is NULL
			curr_tx_queue_buffer = (tx_queue_buffer_t*)(curr_tx_queue_element->data);

			// Setup the TX header
			wlan_mac_high_setup_tx_header( &tx_header_common, ap_station_info->addr,(u8*)(&(eth_dest[0])));

			// Fill in the data
			wlan_create_data_frame((void*)(curr_tx_queue_buffer->frame), &tx_header_common, MAC_FRAME_CTRL2_FLAG_TO_DS);

			// Fill in metadata
			curr_tx_queue_buffer->flags = TX_QUEUE_BUFFER_FLAGS_FILL_DURATION;
			curr_tx_queue_buffer->length = tx_length;
			curr_tx_queue_buffer->station_info = ap_station_info;

			// Put the packet in the queue
			enqueue_after_tail(UNICAST_QID, curr_tx_queue_element);



		} else {
			// Packet was not successfully enqueued
			return 0;
		}

		// Packet was successfully enqueued
		return 1;
	} else {

		// STA is not currently associated, so we won't send any eth frames
		return 0;
	}
}



/*****************************************************************************/
/**
 * @brief Process received MPDUs
 *
 * This callback function will process all the received MPDUs.
 *
 * This function must implement the state machine that will allow a station to join the AP.
 *
 * @param  void* pkt_buf_addr
 *     - Packet buffer address;  Contains the contents of the MPDU as well as other packet information from CPU low
 * @param  station_info_t * station_info
 *     - Pointer to metadata about the station from which this frame was received
 * @param  rx_common_entry* rx_event_log_entry
 * 	   - Pointer to the log entry created for this reception by the MAC High Framework
 * @return u32 flags
 *
 *****************************************************************************/
u32 mpdu_rx_process(void* pkt_buf_addr, station_info_t* station_info, rx_common_entry* rx_event_log_entry) {

	rx_frame_info_t* rx_frame_info = (rx_frame_info_t*)pkt_buf_addr;
	void* mac_payload = (u8*)pkt_buf_addr + PHY_RX_PKT_BUF_MPDU_OFFSET;
	u8* mac_payload_ptr_u8 = (u8*)mac_payload;
	mac_header_80211* rx_80211_header = (mac_header_80211*)((void *)mac_payload_ptr_u8);

	u16 rx_seq;
	u8 unicast_to_me;
	u8 to_multicast;
	u8 is_associated = 0;
	network_info_entry_t* network_info_entry;
	network_info_t* curr_network_info;
	volatile network_info_t* attempt_network_info;
#if WLAN_SW_CONFIG_ENABLE_ETH_BRIDGE
	u8 pre_llc_offset = 0;
#endif
	u32 return_val = 0;
	u16 length = rx_frame_info->phy_details.length;


	// If this function was passed a CTRL frame (e.g., CTS, ACK), then we should just quit.
	// The only reason this occured was so that it could be logged in the line above.
	if((rx_80211_header->frame_control_1 & 0xF) == MAC_FRAME_CTRL1_TYPE_CTRL){
		goto mpdu_rx_process_end;
	}

	// Determine destination of packet
	unicast_to_me = wlan_addr_eq(rx_80211_header->address_1, wlan_mac_addr);
	to_multicast  = wlan_addr_mcast(rx_80211_header->address_1);

    // If the packet is good (ie good FCS) and it is destined for me, then process it
	if( (rx_frame_info->flags & RX_FRAME_INFO_FLAGS_FCS_GOOD)){


		// Sequence number is 12 MSB of seq_control field
		rx_seq         = ((rx_80211_header->sequence_control) >> 4) & 0xFFF;

		// Check if this was a duplicate reception
		//   - Packet is unicast and directed towards me
		//	 - Packet has the RETRY bit set to 1 in the second frame control byte
		//   - Received seq num matched previously received seq num for this STA
		if( (station_info != NULL) && unicast_to_me ){
			if( ((rx_80211_header->frame_control_2) & MAC_FRAME_CTRL2_FLAG_RETRY) && (station_info->latest_rx_seq == rx_seq) ) {
				if(rx_event_log_entry != NULL){
					rx_event_log_entry->flags |= RX_FLAGS_DUPLICATE;
					return_val |= MAC_RX_CALLBACK_RETURN_FLAG_DUP;
				}
			} else {
				station_info->latest_rx_seq = rx_seq;
			}
		}

		if( active_network_info != NULL && station_info_is_member(&active_network_info->members, station_info) ) {

			is_associated  = 1;

			if( return_val & MAC_RX_CALLBACK_RETURN_FLAG_DUP ) {
				// Finish the function
				goto mpdu_rx_process_end;
			}
		}

		if(unicast_to_me || to_multicast){
			// Process the packet
			switch(rx_80211_header->frame_control_1) {
				//---------------------------------------------------------------------
				case (MAC_FRAME_CTRL1_SUBTYPE_BEACON):
				case (MAC_FRAME_CTRL1_SUBTYPE_PROBE_RESP):
					//TODO: Log a MAC time change
				break;

				//---------------------------------------------------------------------
				case MAC_FRAME_CTRL1_SUBTYPE_QOSDATA:
#if WLAN_SW_CONFIG_ENABLE_ETH_BRIDGE
					pre_llc_offset = sizeof(qos_control);
#endif
				case (MAC_FRAME_CTRL1_SUBTYPE_DATA):
					// Data packet
					//   - If the STA is associated with the AP and this is from the DS, then transmit over the wired network
					//
					if(is_associated){
						if((rx_80211_header->frame_control_2) & MAC_FRAME_CTRL2_FLAG_FROM_DS) {
							// MPDU is flagged as destined to the DS - send it for de-encapsulation and Ethernet Tx (if appropriate)
#if WLAN_SW_CONFIG_ENABLE_ETH_BRIDGE
							wlan_mpdu_eth_send(mac_payload,length, pre_llc_offset);
#endif
						}
					}
				break;


				//---------------------------------------------------------------------
				case (MAC_FRAME_CTRL1_SUBTYPE_ASSOC_RESP):
					// Association response
					//   - If we are in the correct association state and the response was success, then associate with the AP
					//
					mac_payload_ptr_u8 += sizeof(mac_header_80211);

					if(wlan_addr_eq(rx_80211_header->address_1, wlan_mac_addr) && ((association_response_frame*)mac_payload_ptr_u8)->status_code == STATUS_SUCCESS){
						// AP is authenticating us. Update BSS_info.
						network_info_entry = wlan_mac_high_find_network_info_BSSID(rx_80211_header->address_3);

						if(network_info_entry != NULL){
							curr_network_info = network_info_entry->data;
							wlan_mac_sta_successfully_associated(curr_network_info->bss_config.bssid,
																(((association_response_frame*)mac_payload_ptr_u8)->association_id)&~0xC000);

						}
					} else {
						// AP is rejecting association request
						//     - Check that the response was from a known BSS
						network_info_entry = wlan_mac_high_find_network_info_BSSID(rx_80211_header->address_3);

						if (network_info_entry != NULL) {
							curr_network_info = network_info_entry->data;
							attempt_network_info = wlan_mac_sta_get_attempt_network_info();
							if(attempt_network_info && wlan_addr_eq(curr_network_info->bss_config.bssid, attempt_network_info->bss_config.bssid)) wlan_mac_sta_join_return_to_idle();
							xil_printf("Join process association failed for BSS %s\n", network_info_entry->data->bss_config.ssid);
						}

						xil_printf("Association failed, reason code %d\n", ((association_response_frame*)mac_payload_ptr_u8)->status_code);
					}
				break;


				//---------------------------------------------------------------------
				case (MAC_FRAME_CTRL1_SUBTYPE_AUTH):
					// Authentication Response

					if( wlan_addr_eq(rx_80211_header->address_1, wlan_mac_addr)) {

						// Move the packet pointer to after the header
						mac_payload_ptr_u8 += sizeof(mac_header_80211);

						// Check the authentication algorithm
						switch(((authentication_frame*)mac_payload_ptr_u8)->auth_algorithm){

							case AUTH_ALGO_OPEN_SYSTEM:
								// Check that this was a successful authentication response
								if(((authentication_frame*)mac_payload_ptr_u8)->auth_sequence == AUTH_SEQ_RESP){

									if(((authentication_frame*)mac_payload_ptr_u8)->status_code == STATUS_SUCCESS){
										// AP is authenticating us. Update BSS_info.
										network_info_entry = wlan_mac_high_find_network_info_BSSID(rx_80211_header->address_3);
										if(network_info_entry != NULL){
											curr_network_info = network_info_entry->data;
											wlan_mac_sta_successfully_authenticated(curr_network_info->bss_config.bssid);
										}
									}

									// Finish the function
									goto mpdu_rx_process_end;
								}
							break;

							default:
								// STA cannot support authentication request
								//     - Check that the response was from a known BSS
								network_info_entry = wlan_mac_high_find_network_info_BSSID(rx_80211_header->address_3);

								if (network_info_entry != NULL) {
									curr_network_info = (network_info_t*)(network_info_entry->data);
									attempt_network_info = wlan_mac_sta_get_attempt_network_info();
									if(attempt_network_info && wlan_addr_eq(curr_network_info->bss_config.bssid, attempt_network_info->bss_config.bssid)) wlan_mac_sta_join_return_to_idle();
									xil_printf("Join process authentication failed for BSS %s\n", network_info_entry->data->bss_config.ssid);
								}

								xil_printf("Authentication failed.  AP uses authentication algorithm %d which is not support by the 802.11 reference design.\n", ((authentication_frame*)mac_payload_ptr_u8)->auth_algorithm);
							break;
						}
					}
				break;


				//---------------------------------------------------------------------
				case (MAC_FRAME_CTRL1_SUBTYPE_DEAUTH):
					// De-authentication
					//   - If we are being de-authenticated, then log and update the association state
					//   - Start and active scan to find the AP if an SSID is defined
					//
					if(active_network_info != NULL){
						if(wlan_addr_eq(rx_80211_header->address_1, wlan_mac_addr) && is_associated){

							//
							// TODO:  (Optional) Log association state change
							//

							// Stop any on-going join
							if (wlan_mac_sta_is_joining()) { wlan_mac_sta_join_return_to_idle(); }

							// Purge all packets for the AP
							purge_queue(UNICAST_QID);

							// Update the hex display to show that we are no longer associated
							wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, 0);

							// Remove the association
							curr_network_info = active_network_info;
							configure_bss(NULL, 0);

							//
							// Note:  This is the place in the code where it would be easy to add new
							//     "just de-authenticated" behaviors, such as an auto-re-join protocol.
							//     The comments below show a simple "re-join the same AP" protocol using
							//     the curr_bss_info variable assigned above.
							//
							// Try to re-join the AP
							// volatile join_parameters_t*  join_parameters = wlan_mac_sta_get_join_parameters();
							// bzero((void*)join_parameters->bssid, MAC_ADDR_LEN);
							// wlan_mac_high_free(join_parameters->ssid);
							// join_parameters->ssid = strndup(curr_bss_info->ssid, SSID_LEN_MAX);
							// wlan_mac_sta_join();
						}
					}
				break;

				//---------------------------------------------------------------------
				default:
					//This should be left as a verbose print. It occurs often when communicating with mobile devices since they tend to send
					//null data frames (type: DATA, subtype: 0x4) for power management reasons.
					wlan_printf(PL_VERBOSE, "Received unknown frame control type/subtype %x\n",rx_80211_header->frame_control_1);
				break;
			}
		}

		// Finish the function
		goto mpdu_rx_process_end;
	} else {
		// Process any Bad FCS packets
		goto mpdu_rx_process_end;
	}


	// Finish any processing for the RX MPDU process
	mpdu_rx_process_end:

    return return_val;
}

#if WLAN_SW_CONFIG_ENABLE_LTG
/*****************************************************************************/
/**
 * @brief Callback to handle new Local Traffic Generator event
 *
 * This function is called when the LTG scheduler determines a traffic generator should create a new packet. The
 * behavior of this function depends entirely on the LTG payload parameters.
 *
 * The reference implementation defines 3 LTG payload types:
 *  - LTG_PYLD_TYPE_FIXED: generate 1 fixed-length packet to single destination; callback_arg is pointer to ltg_pyld_fixed struct
 *  - LTG_PYLD_TYPE_UNIFORM_RAND: generate 1 random-length packet to signle destimation; callback_arg is pointer to ltg_pyld_uniform_rand struct
 *  - LTG_PYLD_TYPE_ALL_ASSOC_FIXED: generate 1 fixed-length packet to each associated station; callback_arg is poitner to ltg_pyld_all_assoc_fixed struct
 *
 * @param u32 id
 *  - Unique ID of the previously created LTG
 * @param void* callback_arg
 *  - Callback argument provided at LTG creation time; interpretation depends on LTG type
 * @return None
 *****************************************************************************/
void ltg_event(u32 id, void* callback_arg){

	u32 payload_length;
	u32 min_ltg_payload_length;
	u8* addr_da;
	station_info_t* ap_station_info;
	dl_entry* curr_tx_queue_element = NULL;
	tx_queue_buffer_t* curr_tx_queue_buffer = NULL;

	if(active_network_info != NULL){
		switch(((ltg_pyld_hdr*)callback_arg)->type){
			case LTG_PYLD_TYPE_FIXED:
				addr_da = ((ltg_pyld_fixed*)callback_arg)->addr_da;
				payload_length = ((ltg_pyld_fixed*)callback_arg)->length;
			break;
			case LTG_PYLD_TYPE_UNIFORM_RAND:
				addr_da = ((ltg_pyld_uniform_rand*)callback_arg)->addr_da;
				payload_length = (rand()%(((ltg_pyld_uniform_rand*)(callback_arg))->max_length - ((ltg_pyld_uniform_rand*)(callback_arg))->min_length))+((ltg_pyld_uniform_rand*)(callback_arg))->min_length;
			break;
			default:
				xil_printf("ERROR ltg_event: Unknown LTG Payload Type! (%d)\n", ((ltg_pyld_hdr*)callback_arg)->type);
				return;
			break;
		}

		ap_station_info = (station_info_t*)((active_network_info->members.first)->data);

		// Send a Data packet to AP
		if(queue_num_queued(UNICAST_QID) < max_queue_size){
			// Checkout 1 element from the queue;
			curr_tx_queue_element = queue_checkout();
			if(curr_tx_queue_element != NULL){
				// Create LTG packet
				curr_tx_queue_buffer = ((tx_queue_buffer_t*)(curr_tx_queue_element->data));

				// Setup the MAC header
				wlan_mac_high_setup_tx_header( &tx_header_common, ap_station_info->addr, addr_da );

				min_ltg_payload_length = wlan_create_ltg_frame((void*)(curr_tx_queue_buffer->frame), &tx_header_common, MAC_FRAME_CTRL2_FLAG_TO_DS, id);
				payload_length = max(payload_length+sizeof(mac_header_80211)+WLAN_PHY_FCS_NBYTES, min_ltg_payload_length);

				// Fill in metadata
				curr_tx_queue_buffer->flags = TX_QUEUE_BUFFER_FLAGS_FILL_DURATION | TX_QUEUE_BUFFER_FLAGS_FILL_UNIQ_SEQ;
				curr_tx_queue_buffer->length = payload_length;
				curr_tx_queue_buffer->station_info = ap_station_info;

				// Submit the new packet to the appropriate queue
				enqueue_after_tail(UNICAST_QID, curr_tx_queue_element);
			}
		}
	}
}
#endif //WLAN_SW_CONFIG_ENABLE_LTG

/*****************************************************************************/
/**
 * @brief Disassociate the STA from the associated AP
 *
 * This function will disassociate the STA from the AP if it is associated.  Otherwise,
 * it will do nothing.  It also logs any association state change
 *
 * @param  None
 * @return int
 *  -  0 if successful
 *  - -1 if unsuccessful
 *
 *  @note This function uses global variables:  association_state, association_table
 *****************************************************************************/
int sta_disassociate( void ) {
	int status = 0;
	station_info_entry_t* ap_station_info_entry;
	dl_entry* curr_tx_queue_element;
	tx_queue_buffer_t* curr_tx_queue_buffer;
	u32 tx_length;

	// If the STA is currently associated, remove the association; otherwise do nothing
	if(active_network_info != NULL){

		// Get the AP station info
		ap_station_info_entry = (station_info_entry_t*)(active_network_info->members.first);

		//
		// TODO:  (Optional) Log association state change
		//

		// ---------------------------------------------------------------
		// Send de-authentication message to tell AP that the STA is leaving

		// Jump to BSS channel before sending.  No need to change back.
		wlan_mac_high_set_radio_channel(wlan_mac_high_bss_channel_spec_to_radio_chan(active_network_info->bss_config.chan_spec));

		// Send disassociation packet
		curr_tx_queue_element = queue_checkout();

		if(curr_tx_queue_element != NULL){
			curr_tx_queue_buffer = (tx_queue_buffer_t*)(curr_tx_queue_element->data);

			// Setup the TX header
			wlan_mac_high_setup_tx_header(&tx_header_common, ap_station_info_entry->addr, wlan_mac_addr );

			// Fill in the data
			tx_length = wlan_create_disassoc_frame((void*)(curr_tx_queue_buffer->frame), &tx_header_common, DISASSOC_REASON_STA_IS_LEAVING);

			// Fill in metadata
			curr_tx_queue_buffer->flags = TX_QUEUE_BUFFER_FLAGS_FILL_DURATION;
			curr_tx_queue_buffer->length = tx_length;
			curr_tx_queue_buffer->station_info = ap_station_info_entry->data;

			// Put the packet in the queue
			enqueue_after_tail(MANAGEMENT_QID, curr_tx_queue_element);

			// Purge any data for the AP
			purge_queue(UNICAST_QID);
		}

		// Set BSS to NULL
		configure_bss(NULL, 0);
	}

	return status;
}

/*****************************************************************************/
/**
 *
 *****************************************************************************/
u32	configure_bss(bss_config_t* bss_config, u32 update_mask){
	u32 return_status = 0;
	u8 send_channel_switch_to_low = 0;
	u8 send_beacon_config_to_low = 0;

	network_info_t* local_network_info;
	interrupt_state_t curr_interrupt_state;
	station_info_t* curr_station_info;
	station_info_entry_t* curr_station_info_entry;
	station_info_t* ap_station_info = NULL;

	//---------------------------------------------------------
	// 1. Check for any invalid inputs or combination of inputs
	//      First verify the requested update to the BSS configuration before
	//      modifying anything. This will prevent a partial update of BSS
	//      configuration with valid parameters before discovering an invalid
	//      parameter.

	if (bss_config != NULL) {
		if (update_mask & BSS_FIELD_MASK_BSSID) {
			if (wlan_addr_eq(bss_config->bssid, zero_addr) == 0) {
				if ((active_network_info != NULL) && wlan_addr_eq(bss_config->bssid, active_network_info->bss_config.bssid)) {
					// The caller of this function claimed that it was updating the BSSID,
					// but the new BSSID matches the one already specified in active_bss_info.
					// Complete the rest of this function as if that bit in the update mask
					// were not set
					update_mask &= ~BSS_FIELD_MASK_BSSID;
				} else {
					// Changing the BSSID, perform necessary argument checks
					if ((bss_config->bssid[0] & MAC_ADDR_MSB_MASK_LOCAL) == 1) {
						// In the STA implementation, the BSSID provided must not
						// be locally generated.
						return_status |= BSS_CONFIG_FAILURE_BSSID_INVALID;
					}
					if (((update_mask & BSS_FIELD_MASK_SSID) == 0) ||
						((update_mask & BSS_FIELD_MASK_CHAN) == 0)) {
						return_status |= BSS_CONFIG_FAILURE_BSSID_INSUFFICIENT_ARGUMENTS;
					}
				}
			}
		} else {
			if (active_network_info == NULL) {
				// Cannot update BSS without specifying BSSID
				return_status |= BSS_CONFIG_FAILURE_BSSID_INSUFFICIENT_ARGUMENTS;
			}
		}
		if (update_mask & BSS_FIELD_MASK_CHAN) {
			if (wlan_verify_channel(
					wlan_mac_high_bss_channel_spec_to_radio_chan(bss_config->chan_spec)) != XST_SUCCESS) {
				return_status |= BSS_CONFIG_FAILURE_CHANNEL_INVALID;
			}
		}
		if (update_mask & BSS_FIELD_MASK_BEACON_INTERVAL) {
			// There is no error condition for setting the beacon interval
			// at the STA since a STA is incapable of sending beacons
		}
		if (update_mask & BSS_FIELD_MASK_HT_CAPABLE) {
			if (bss_config->ht_capable > 1) {
				return_status |= BSS_CONFIG_FAILURE_HT_CAPABLE_INVALID;
			}
		}
		if (update_mask & BSS_FIELD_MASK_DTIM_PERIOD) {
			// For a STA, there is no invalid selection of dtim_period so we never return
			// BSS_CONFIG_FAILURE_DTIM_PERIOD_INVALID. An AP that does not contain a TIM
			// MGMT tag in its beacon will have a value of 0 in the dtim_period field of
			// the corresponding network_info_t.
		}
	}

	if (return_status == 0) {
		//---------------------------------------------------------
		// 2. Apply BSS configuration changes
		//      Now that the provided bss_config_t struct is valid, apply the changes.

		// Disable interrupts around these modifications to prevent state
		// changing out from underneath this context while the new BSS
		// configuration parameters are only partially updated.
		curr_interrupt_state = wlan_mac_high_interrupt_stop();

		if ((bss_config == NULL) || (update_mask & BSS_FIELD_MASK_BSSID)) {
			// Adopting a new BSSID. This could mean either
			//    1) Shutting the BSS down
			// or 2) Shutting the BSS down and then starting a new BSS.
			//
			// In either case, first remove any station_info structs
			// that are members of the current active_bss_info and return to
			// a NULL active_bss_info state.
			//
			// This will not result in any OTA transmissions to the stations.

			if (active_network_info != NULL) {

				if ((bss_config == NULL) ||
						((update_mask & BSS_FIELD_MASK_BSSID) && wlan_addr_eq(bss_config->bssid, zero_addr)) ) {
					xil_printf("Leaving BSS\n");
				}

				curr_station_info_entry = (station_info_entry_t*)(active_network_info->members.first);
				curr_station_info = curr_station_info_entry->data;

				// Purge any data for the AP
				purge_queue(UNICAST_QID);

				// Lower the KEEP flag
				curr_station_info->flags &= ~STATION_INFO_FLAG_KEEP;

				// Remove the association
				station_info_remove( &active_network_info->members, curr_station_info_entry->addr );

				// Update the hex display to show STA is not currently associated
				wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, 0);

				// Inform the MAC High Framework to no longer will keep this BSS Info. This will
				// allow it to be overwritten in the future to make space for new BSS Infos.
				active_network_info->flags &= ~NETWORK_FLAGS_KEEP;

				// Set "active_network_info" to NULL
				//     - All functions must be able to handle active_bss_info = NULL
				active_network_info = NULL;

				// Disable beacon processing immediately
				bzero(gl_beacon_txrx_config.bssid_match, MAC_ADDR_LEN);
				wlan_mac_high_config_txrx_beacon(&gl_beacon_txrx_config);

				// Set hex display to "No BSS"
				wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, 0xFF);
			}

			// Pause the data queue, if un-paused
			//     Since interrupts are disabled, this does not need to be done before the purge_queue()
			if (pause_data_queue == 0) {
				pause_data_queue = 1;
			}

			// (bss_config_update == NULL) is one way to remove the BSS state of the node. This operation
			// was executed just above.  Rather that continuing to check non-NULLness of bss_config
			// throughout the rest of this function, just re-enable interrupts and return early.

			if (bss_config == NULL) {
				wlan_mac_high_interrupt_restore_state(curr_interrupt_state);
				return return_status;
			}

			// active_bss_info is guaranteed to be NULL at this point in the code
			// bss_config is guaranteed to be non-NULL at this point in the code

			// Update BSS
			//     - BSSID must not be zero_addr (reserved address)
			if (wlan_addr_eq(bss_config->bssid, zero_addr) == 0) {
				// Stop the join state machine if it is running
				if (wlan_mac_sta_is_joining()) {
					wlan_mac_sta_join_return_to_idle();
				}

				// Stop the scan state machine if it is running
				if (wlan_mac_scan_is_scanning()) {
					wlan_mac_scan_stop();
				}

				// Create a new bss_info or overwrite an existing one with matching BSSID.
				//     Note:  The wildcard SSID and 0-valued channel arguments are temporary. Because
				//         of the error checking at the top of this function, the bss_config will
				//         contain a valid SSID as well as channel. These fields will be updated
				//         in step 3).
				local_network_info = wlan_mac_high_create_network_info(bss_config->bssid, "", 0);

				if (local_network_info != NULL) {
					local_network_info->flags |= NETWORK_FLAGS_KEEP;

					local_network_info->capabilities = (BSS_CAPABILITIES_ESS);

					active_network_info = local_network_info;

					// Add AP to association table
					//     - Set ht_capable argument to the HT_CAPABLE capability of the BSS.  Given that the STA does not know
					//       the HT capabilities of the AP, it is reasonable to assume that they are the same as the BSS.
					//
					ap_station_info = station_info_add(&(active_network_info->members), active_network_info->bss_config.bssid, 0, active_network_info->bss_config.ht_capable);

					if (ap_station_info != NULL) {

						ap_station_info->flags |= STATION_INFO_FLAG_KEEP;

						time_hr_min_sec_t time_hr_min_sec = wlan_mac_time_to_hr_min_sec(get_system_time_usec());
						xil_printf("*%dh:%02dm:%02ds* Now associated with AP 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n",
								time_hr_min_sec.hr, time_hr_min_sec.min, time_hr_min_sec.sec,
								active_network_info->bss_config.bssid[0], active_network_info->bss_config.bssid[1], active_network_info->bss_config.bssid[2],
								active_network_info->bss_config.bssid[3], active_network_info->bss_config.bssid[4], active_network_info->bss_config.bssid[5]);

						//
						// TODO:  (Optional) Log association state change
						//
					}
				}

				// Set hex display
				wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, my_aid);
			}
		}

		//---------------------------------------------------------
		// 3. Clean up
		//      Now that active_bss_info has been updated, CPU_HIGH can communicate
		//      the changes to CPU_LOW so that the node is tuned to the correct channel,
		//      send beacons at the correct interval, and update the beacon
		//      template packet buffer.
		if (active_network_info != NULL) {

			if (update_mask & BSS_FIELD_MASK_CHAN) {
				active_network_info->bss_config.chan_spec = bss_config->chan_spec;
				send_channel_switch_to_low = 1;
			}
			if (update_mask & BSS_FIELD_MASK_SSID) {
				strncpy(active_network_info->bss_config.ssid, bss_config->ssid, SSID_LEN_MAX);
			}
			if (update_mask & BSS_FIELD_MASK_BEACON_INTERVAL) {
				active_network_info->bss_config.beacon_interval = bss_config->beacon_interval;
				send_beacon_config_to_low = 1;
			}
			if (update_mask & BSS_FIELD_MASK_HT_CAPABLE) {
				active_network_info->bss_config.ht_capable = bss_config->ht_capable;
			}

			// Update the channel
			if (send_channel_switch_to_low) {
				wlan_mac_high_set_radio_channel(
						wlan_mac_high_bss_channel_spec_to_radio_chan(active_network_info->bss_config.chan_spec));
			}

			// Update Beacon configuration
			if (send_beacon_config_to_low) {
				memcpy(gl_beacon_txrx_config.bssid_match, active_network_info->bss_config.bssid, MAC_ADDR_LEN);

				gl_beacon_txrx_config.beacon_interval_tu = active_network_info->bss_config.beacon_interval;        // CPU_LOW does not need this parameter for the STA project
				gl_beacon_txrx_config.beacon_template_pkt_buf = TX_PKT_BUF_BEACON;              // CPU_LOW does not need this parameter for the STA project

				wlan_mac_high_config_txrx_beacon(&gl_beacon_txrx_config);
			}

			// Unpause the queue, if paused
			if (pause_data_queue) {
				pause_data_queue = 0;
			}

			// Update the hex diplay with the current AID
			wlan_platform_high_userio_disp_status(USERIO_DISP_STATUS_MEMBER_LIST_UPDATE, my_aid);

			// Print new BSS information
			xil_printf("BSS Details: \n");
			xil_printf("  BSSID           : %02x-%02x-%02x-%02x-%02x-%02x\n",active_network_info->bss_config.bssid[0],active_network_info->bss_config.bssid[1],
																			 active_network_info->bss_config.bssid[2],active_network_info->bss_config.bssid[3],
																			 active_network_info->bss_config.bssid[4],active_network_info->bss_config.bssid[5]);
			xil_printf("   SSID           : %s\n", active_network_info->bss_config.ssid);
			xil_printf("   Channel        : %d\n", wlan_mac_high_bss_channel_spec_to_radio_chan(active_network_info->bss_config.chan_spec));
			if (active_network_info->bss_config.beacon_interval == BEACON_INTERVAL_NO_BEACON_TX) {
				xil_printf("   Beacon Interval: No Beacon Tx\n");
			} else if (active_network_info->bss_config.beacon_interval == BEACON_INTERVAL_UNKNOWN) {
				xil_printf("   Beacon Interval: Unknown\n");
			} else {
				xil_printf("   Beacon Interval: %d TU (%d us)\n", active_network_info->bss_config.beacon_interval, active_network_info->bss_config.beacon_interval*1024);
			}
		}

		// Restore interrupts after all BSS changes
		wlan_mac_high_interrupt_restore_state(curr_interrupt_state);
	}

	return return_status;
}



/*****************************************************************************/
/**
 * @brief Callback to handle beacon MAC time update mode
 *
 * @param  u32 enable        - Enable / Disable MAC time update from beacons
 * @return None
 *
 *****************************************************************************/
void sta_set_beacon_ts_update_mode(u32 enable){
    if (enable) {
        gl_beacon_txrx_config.ts_update_mode = ALWAYS_UPDATE;
    } else {
        gl_beacon_txrx_config.ts_update_mode = NEVER_UPDATE;
    }

    // Push beacon configuration to CPU_LOW
    wlan_mac_high_config_txrx_beacon(&gl_beacon_txrx_config);
}

/*****************************************************************************/
/**
 * @brief Accessor methods for global variables
 *
 * These functions will return pointers to global variables
 *
 * @param  None
 * @return None
 *****************************************************************************/
dl_list* get_network_member_list(){
	if(active_network_info != NULL){
		return &(active_network_info->members);
	} else {
		return NULL;
	}
}

network_info_t* active_network_info_getter(){ return active_network_info; }




#if WLAN_SW_CONFIG_ENABLE_WLAN_EXP

// ****************************************************************************
// Define MAC Specific User Commands
//
// NOTE:  All User Command IDs (CMDID_*) must be a 24 bit unique number
//

//-----------------------------------------------
// MAC Specific User Commands
//
// #define CMDID_USER_<COMMAND_NAME>                       0x100000


//-----------------------------------------------
// MAC Specific User Command Parameters
//
// #define CMD_PARAM_USER_<PARAMETER_NAME>                 0x00000000



/*****************************************************************************/
/**
 * Process User Commands
 *
 * This function is part of the WLAN Exp framework and will process the framework-
 * level user commands.  This function intentionally does not implement any user
 * commands and it is left to the user to implement any needed functionality.   By
 * default, any commands not processed in this function will print an error to the
 * UART.
 *
 * @param   socket_index     - Index of the socket on which to send message
 * @param   from             - Pointer to socket address structure (struct sockaddr *) where command is from
 * @param   command          - Pointer to Command
 * @param   response         - Pointer to Response
 * @param   max_resp_len     - Maximum number of u32 words allowed in response
 *
 * @return  int              - Status of the command:
 *                                 NO_RESP_SENT - No response has been sent
 *                                 RESP_SENT    - A response has been sent
 *
 * @note    See on-line documentation for more information:
 *          https://warpproject.org/trac/wiki/802.11/wlan_exp/Extending
 *
 *****************************************************************************/
int wlan_exp_process_user_cmd(u32 cmd_id, int socket_index, void* from, cmd_resp* command, cmd_resp* response, u32 max_resp_len) {

    //
    // IMPORTANT ENDIAN NOTES:
    //     - command
    //         - header - Already endian swapped by the framework (safe to access directly)
    //         - args   - Must be endian swapped as necessary by code (framework does not know the contents of the command)
    //     - response
    //         - header - Will be endian swapped by the framework (safe to write directly)
    //         - args   - Must be endian swapped as necessary by code (framework does not know the contents of the response)
    //

    // Standard variables
    //
    // Used for accessing command arguments and constructing the command response header/payload
    //
    // NOTE:  Some of the standard variables below have been commented out.  This was to remove
    //     compiler warnings for "unused variables" since the default implementation is empty.  As
    //     you add commands, you should un-comment the standard variables.
    //
    u32 resp_sent = NO_RESP_SENT;

#if 0
    cmd_resp_hdr* cmd_hdr = command->header;
    cmd_resp_hdr* resp_hdr = response->header;

    u32* cmd_args_32 = command->args;
    u32* resp_args_32 = response->args;

    u32 resp_index = 0;
#endif

    switch(cmd_id){

//-----------------------------------------------------------------------------
// MAC Specific User Commands
//-----------------------------------------------------------------------------

        // Template framework for a Command
        //
        // NOTE:  The WLAN Exp framework assumes that the Over-the-Wire format of the data is
        //     big endian.  However, the node processes data using little endian.  Therefore,
        //     any data received from the host must be properly endian-swapped and similarly,
        //     any data sent to the host must be properly endian-swapped.  The built-in Xilinx
        //     functions:  Xil_Ntohl() (Network to Host) and Xil_Htonl() (Host to Network) are
        //     used for this.
        //
#if 0
        //---------------------------------------------------------------------
        case CMDID_USER_<COMMAND_NAME>: {
            // Command Description
            //
            // Message format:
            //     cmd_args_32[0:N]    Document command arguments from the host
            //
            // Response format:
            //     resp_args_32[0:M]   Document response arguments from the node
            //
            // NOTE:  Variables are declared above.
            // NOTE:  Please take care of the endianness of the arguments (see comment above)
            //

            // Variables for template command
            int status;
            u32 arg_0;
            interrupt_state_t curr_interrupt_state;

            // Initialize variables
            status = CMD_PARAM_SUCCESS;
            arg_0 = Xil_Ntohl(cmd_args_32[0]);              // Swap endianness of command argument

            // Do something with argument(s)
            xil_printf("Command argument 0: 0x%08x\n", arg_0);

            // If necessary, disable interrupts before processing the command
            //  Interrupts must be disabled if the command implementation relies on any state that might
            //  change during an interrupt service routine. See the user guide for more details
            //  https://warpproject.org/trac/wiki/802.11/wlan_exp/Extending
            curr_interrupt_state = wlan_mac_high_interrupt_stop();

            // Process command arguments and generate any response payload
            //     NOTE:  If interrupts were disabled above, take care to avoid any long-running code in this
            //         block (i.e. avoid xil_printf()). When interrupts are disabled, CPU High is unable to
            //         respond to CPU Low (ie CPU High will not send / receive packets) and execute scheduled
            //         tasks, such as LTGs.

            // Re-enable interrupts before returning (only do this if wlan_mac_high_interrupt_stop() is called above)
            wlan_mac_high_interrupt_restore_state(curr_interrupt_state);

            // Send response
            //   NOTE:  It is good practice to send a status as the first argument of the response.
            //       This way it is easy to determine if the other data in the response is valid.
            //       Predefined status values are:  CMD_PARAM_SUCCESS, CMD_PARAM_ERROR
            //
            resp_args_32[resp_index++] = Xil_Htonl(status);       // Swap endianness of response arguments

            resp_hdr->length  += (resp_index * sizeof(u32));
            resp_hdr->num_args = resp_index;
        }
        break;
#endif


        //---------------------------------------------------------------------
        default: {
            wlan_exp_printf(WLAN_EXP_PRINT_ERROR, print_type_node, "Unknown STA user command: 0x%x\n", cmd_id);
        }
        break;
    }

    return resp_sent;
}


#endif

