/** @file wlan_mac_packet_types.h
 *  @brief Packet Constructors
 *
 *  This contains code for constructing a variety of different types of MPDUs.
 *
 *  @copyright Copyright 2013-2017, Mango Communications. All rights reserved.
 *          Distributed under the Mango Communications Reference Design License
 *              See LICENSE.txt included in the design archive or
 *              at http://mangocomm.com/802.11/license
 *
 *  This file is part of the Mango 802.11 Reference Design (https://mangocomm.com/802.11)
 */


#ifndef WLAN_MAC_PACKET_TYPES_H_
#define WLAN_MAC_PACKET_TYPES_H_

#include "xil_types.h"

//Forward declarations
struct mac_header_80211_common;
struct network_info_t;

typedef struct mac_header_80211_common{
	u8* address_1;
	u8* address_2;
	u8* address_3;
	u8 frag_num;
	u8 reserved;
} mac_header_80211_common;

typedef struct authentication_frame{
	u16 auth_algorithm;
	u16 auth_sequence;
	u16 status_code;
} authentication_frame;

typedef struct deauthentication_frame{
	u16 reason_code;
} deauthentication_frame;

typedef struct association_response_frame{
	u16 capabilities;
	u16 status_code;
	u16 association_id;
} association_response_frame;

typedef struct association_request_frame{
	u16 capabilities;
	u16 listen_interval;
} association_request_frame;

typedef struct channel_switch_announcement_frame{
	u8 category;
	u8 action;

	// Channel Switch Announcement Element (Section 8.4.2.21)
	u8 element_id;                 // Set to 37 (Table 8-54 - Section 8.4.2.1)
	u8 length;                     // Set to 3
	u8 chan_switch_mode;           // Set to 0 - No restrictions on transmission until a channel switch
	u8 new_chan_num;
	u8 chan_switch_count;          // Set to 0 - Switch occurs any time after the frame is transmitted

} channel_switch_announcement_frame;

typedef struct measurement_common_frame{
	u8 category;
	u8 action;
	u8 dialog_token;
	u8 element_id;
	u8 length;
	u8 measurement_token;
	u8 request_mode;
	u8 measurement_type;
	///Note, technically measurement action frames can follow this comment with additional fields of unknown length
	///But currently, the three types of measurements are all the same so for ease we'll hardcode that structure here
	u8 channel;
	u8 start_time[8];
	u8 duration[2];
} measurement_common_frame;

//#define MEASUREMENT_REQ_MODE_ENABLE  0x40
//#define MEASUREMENT_REQ_MODE_REQUEST 0x20
//#define MEASUREMENT_REQ_MODE_REPORT  0x10

#define MEASUREMENT_REQ_MODE_PARALLEL    0x01
#define MEASUREMENT_REQ_MODE_ENABLE	     0x02
#define MEASUREMENT_REQ_MODE_REPORTS     0x04
#define MEASUREMENT_REQ_MODE_AUTONOMOUS  0x08

#define MEASUREMENT_TYPE_BASIC 0
#define MEASUREMENT_TYPE_CCA 1
#define MEASUREMENT_TYPE_RPA 2




#define AUTH_ALGO_OPEN_SYSTEM 0x00

#define AUTH_SEQ_REQ 0x01
#define AUTH_SEQ_RESP 0x02

// Reason Codes as per IEEE 802.11-2012 standard.(table 8.36)
#define DEAUTH_REASON_STA_IS_LEAVING                       3
#define DEAUTH_REASON_INACTIVITY                           4
#define DEAUTH_REASON_NONASSOCIATED_STA                    7
#define DISASSOC_REASON_STA_IS_LEAVING                     8

// Status Codes: Table 7-23 in 802.11-2007
#define STATUS_SUCCESS 0
#define STATUS_AUTH_REJECT_UNSPECIFIED 1
#define STATUS_AUTH_REJECT_OUTSIDE_SCOPE 12
#define STATUS_AUTH_REJECT_CHALLENGE_FAILURE 15
#define STATUS_REJECT_TOO_MANY_ASSOCIATIONS 17

#define wlan_create_beacon_frame(a,b,c) wlan_create_beacon_probe_resp_frame(MAC_FRAME_CTRL1_SUBTYPE_BEACON, a, b, c)
#define wlan_create_probe_resp_frame(a,b,c) wlan_create_beacon_probe_resp_frame(MAC_FRAME_CTRL1_SUBTYPE_PROBE_RESP, a, b, c)

int wlan_create_beacon_probe_resp_frame(u8 frame_control_1, void* pkt_buf, struct mac_header_80211_common* common, struct network_info_t* network_info);
int wlan_create_probe_req_frame(void* pkt_buf, struct mac_header_80211_common* common, char* ssid);
int wlan_create_auth_frame(void* pkt_buf, struct mac_header_80211_common* common, u16 auth_algorithm,  u16 auth_seq, u16 status_code);

#define wlan_create_deauth_frame(pkt_buf, common, attempt_network_info)   wlan_create_deauth_disassoc_frame(pkt_buf, MAC_FRAME_CTRL1_SUBTYPE_DEAUTH,   common, attempt_network_info)
#define wlan_create_disassoc_frame(pkt_buf, common, attempt_network_info) wlan_create_deauth_disassoc_frame(pkt_buf, MAC_FRAME_CTRL1_SUBTYPE_DISASSOC, common, attempt_network_info)

int wlan_create_deauth_disassoc_frame(void* pkt_buf, u8 frame_control_1, struct mac_header_80211_common* common, u16 reason_code);
int wlan_create_association_response_frame(void* pkt_buf, struct mac_header_80211_common* common, u16 status, u16 AID, struct network_info_t* network_info);

#define wlan_create_association_req_frame(pkt_buf, common, attempt_network_info) wlan_create_reassoc_assoc_req_frame(pkt_buf, MAC_FRAME_CTRL1_SUBTYPE_ASSOC_REQ, common, attempt_network_info)
#define wlan_create_reassociation_req_frame(pkt_buf, common, attempt_network_info) wlan_create_reassoc_assoc_req_frame(pkt_buf, MAC_FRAME_CTRL1_SUBTYPE_REASSOC_REQ, common, attempt_network_info)

int wlan_create_reassoc_assoc_req_frame(void* pkt_buf, u8 frame_control_1, struct mac_header_80211_common* common, struct network_info_t* network_info);
int wlan_create_data_frame(void* pkt_buf, struct mac_header_80211_common* common, u8 flags);
int wlan_create_rts_frame(void* pkt_buf_addr, u8* address_ra, u8* address_ta, u16 duration);
int wlan_create_cts_frame(void* pkt_buf_addr, u8* address_ra, u16 duration);
int wlan_create_ack_frame(void* pkt_buf_addr, u8* address_ra);

#endif /* WLAN_MAC_PACKET_TYPES_H_ */
