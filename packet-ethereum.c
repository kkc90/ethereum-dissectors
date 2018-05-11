#include "config.h"

#include <epan/packet.h>
#include <epan/proto_data.h>
#include <epan/tap.h>
#include <epan/stats_tree.h>
#include <epan/conversation.h>
#include <epan/srt_table.h>
#include <epan/conversation_table.h>

#define MIN_ETHDEVP2PDISCO_LEN 98
#define MAX_ETHDEVP2PDISCO_LEN 1280

static dissector_handle_t ethdevp2pdisco_handle;

/* Sub tree */
static int proto_ethdevp2p = -1;
static gint ett_ethdevp2p = -1;
static gint ett_ethdevp2p_packet = -1;
static gint ett_ethdevp2p_node = -1;

static const value_string packettypenames[] = {
    { 0x01, "Ping" },
    { 0x02, "Pong" },
    { 0x03, "FindNode" },
    { 0x04, "Neighbors" },
	{ 0x00, NULL }
};

static int hf_ethdevp2p_hash = -1;
static int hf_ethdevp2p_signature = -1;
static int hf_ethdevp2p_packet = -1;
static int hf_ethdevp2p_packet_type = -1;
/* For Ping Message */
static int hf_ethdevp2p_ping_version = -1;
static int hf_ethdevp2p_ping_sender_ipv4 = -1;
static int hf_ethdevp2p_ping_sender_ipv6 = -1;
static int hf_ethdevp2p_ping_sender_udp_port = -1;
static int hf_ethdevp2p_ping_sender_tcp_port = -1;
static int hf_ethdevp2p_ping_recipient_ipv4 = -1;
static int hf_ethdevp2p_ping_recipient_ipv6 = -1;
static int hf_ethdevp2p_ping_recipient_udp_port = -1;
static int hf_ethdevp2p_ping_recipient_tcp_port = -1;
static int hf_ethdevp2p_ping_expiration = -1;
/* For Pong Message */
static int hf_ethdevp2p_pong_recipient_ipv4 = -1;
static int hf_ethdevp2p_pong_recipient_ipv6 = -1;
static int hf_ethdevp2p_pong_recipient_udp_port = -1;
static int hf_ethdevp2p_pong_recipient_tcp_port = -1;
static int hf_ethdevp2p_pong_ping_hash = -1;
static int hf_ethdevp2p_pong_expiration = -1;
/* For FindNode Message */
static int hf_ethdevp2p_findNode_target = -1;
static int hf_ethdevp2p_findNode_expiration = -1;
/* For Neighbors Message */
static int hf_ethdevp2p_neighbors_node = -1;
static int hf_ethdevp2p_neighbors_nodes_ipv4 = -1;
static int hf_ethdevp2p_neighbors_nodes_ipv6 = -1;
static int hf_ethdevp2p_neighbors_nodes_udp_port = -1;
static int hf_ethdevp2p_neighbors_nodes_tcp_port = -1;
static int hf_ethdevp2p_neighbors_nodes_id = -1;
static int hf_ethdevp2p_neighbors_expiration = -1;
static int hf_ethdevp2p_neighbors_nodes_number = -1;

//For RLP decoding
struct UnitData {
	gint offset;
	gint length;
};

struct PacketContent {
	gint dataCount;
	struct UnitData *data_list;
};

//For Tap
static int ethdevp2p_tap = -1;

struct Ethdevp2pTap {
    gint packet_type;
	gint node_number;
	gint ping_count;
	gint pong_count;
	gint findNode_count;
	gint neighbours_count;
	gint pp_time_flag;
	gint fn_time_flag;
	nstime_t pp_time;
	nstime_t fn_time;
};

//For conversation
static int hf_ethdevp2p_conv_id = -1;
static int hf_ethdevp2p_conv_ping_count = -1;
static int hf_ethdevp2p_conv_pong_count = -1;
static int hf_ethdevp2p_conv_findNode_count = -1;
static int hf_ethdevp2p_conv_neighbours_count = -1;
static int hf_ethdevp2p_conv_ping_index = -1;
static int hf_ethdevp2p_conv_pong_index = -1;
static int hf_ethdevp2p_conv_findNode_index = -1;
static int hf_ethdevp2p_conv_neighbours_index = -1;
static int hf_ethdevp2p_conv_ping_frame = -1;
static int hf_ethdevp2p_conv_pong_frame = -1;
static int hf_ethdevp2p_conv_pp_time = -1;
static int hf_ethdevp2p_conv_findNode_frame = -1;
static int hf_ethdevp2p_conv_neighbours_frame = -1;
static int hf_ethdevp2p_conv_fn_time = -1;
typedef struct _eth_conv_info_t {
	wmem_map_t *pdus;
	wmem_map_t *fdus;
	guint32 conv_id;
	guint32 ping_count;
	guint32 pong_count;
	guint32 findNode_count;
	guint32 neighbours_count;
} eth_conv_info_t;
typedef struct _eth_conv_ping_index_t {
	guint32 ping_index;
} eth_conv_ping_index_t;
typedef struct _eth_conv_pong_index_t {
	guint32 pong_index;
} eth_conv_pong_index_t;
typedef struct _eth_conv_findNode_index_t {
	guint32 findNode_index;
} eth_conv_findNode_index_t;
typedef struct _eth_conv_neighbours_index_t {
	guint32 neighbours_index;
} eth_conv_neighbours_index_t;
typedef struct _eth_conv_pp_transaction_t {
	guint32 ping_frame;
	guint32 pong_frame;
	nstime_t pp_time;
} eth_conv_pp_transaction_t;
typedef struct _eth_conv_fn_transaction_t {
	guint32 findNode_frame;
	guint32 neighbours_frame;
	nstime_t fn_time;
} eth_conv_fn_transaction_t;

static const guint8* st_str_packets = "Total Packets";
static const guint8* st_str_packet_types = "Ethdevp2p Packet Types";
static const guint8* st_str_packet_neighbours = "Number of Nodes exchanged";
static const guint8* st_str_packet_ping_frequency = "Frequency of Ping per node";
static const guint8* st_str_packet_pong_frequency = "Frequency of Pong per node";
static const guint8* st_str_packet_findNode_frequency = "Frequency of findNode per node";
static const guint8* st_str_packet_neighbours_frequency = "Frequency of neighbours per node";
static int st_node_packets = -1;
static int st_node_packet_types = -1;
static int st_node_packet_neighbours = -1;
static int st_node_packet_ping_frequency = -1;
static int st_node_packet_pong_frequency = -1;
static int st_node_packet_findNode_frequency = -1;
static int st_node_packet_neighbours_frequency = -1;

static int rlp_decode(tvbuff_t *tvb, struct PacketContent *packet_content) {
	gint offset = 0;
	offset += 97;	//Skip Hash and Signature, checks packet data only
	gint length = tvb_captured_length(tvb);
	gint prefix;
	gint flag;	//Trigger to indicates if a new unit data is detected
	while (offset < length) {
		flag = 0;
		prefix = tvb_get_guint8(tvb, offset);
		if (prefix >= 0x00 && prefix <= 0x7f) {
			//data is prefix it self
			packet_content->data_list[packet_content->dataCount].offset = offset;
			packet_content->data_list[packet_content->dataCount].length = 1;
			offset += 1;	//Skip the packet
			flag = 1;
		}
		else if (prefix >= 0x80 && prefix <= 0xb7) {
			//A string less than 55 bytes long
			offset += 1;	//Skip the prefix
			packet_content->data_list[packet_content->dataCount].offset = offset;
			packet_content->data_list[packet_content->dataCount].length = prefix - 0x80;
			offset += prefix - 0x80;	//Skip the packet
			flag = 1;
		}
		else if (prefix >= 0xb8 && prefix <= 0xbf) {
			//A string more than 55 bytes long
			offset += 1;	//Skip the prefix
			switch (prefix - 0xb7) {
			case 1:
				packet_content->data_list[packet_content->dataCount].length = tvb_get_guint8(tvb, offset);
				offset += 1;
				break;
			case 2:
				packet_content->data_list[packet_content->dataCount].length = tvb_get_guint16(tvb, offset, ENC_BIG_ENDIAN);
				offset += 2;
				break;
			case 3:
				packet_content->data_list[packet_content->dataCount].length = tvb_get_guint24(tvb, offset, ENC_BIG_ENDIAN);
				offset += 3;
				break;
			case 4:
				packet_content->data_list[packet_content->dataCount].length = tvb_get_guint32(tvb, offset, ENC_BIG_ENDIAN);
				offset += 4;
				break;
			case 5:
				packet_content->data_list[packet_content->dataCount].length = (guint32)tvb_get_guint40(tvb, offset, ENC_BIG_ENDIAN);
				offset += 5;
				break;
			case 6:
				packet_content->data_list[packet_content->dataCount].length = (guint32)tvb_get_guint48(tvb, offset, ENC_BIG_ENDIAN);
				offset += 6;
				break;
			case 7:
				packet_content->data_list[packet_content->dataCount].length = (guint32)tvb_get_guint56(tvb, offset, ENC_BIG_ENDIAN);
				offset += 7;
				break;
			case 8:
				packet_content->data_list[packet_content->dataCount].length = (guint32)tvb_get_guint64(tvb, offset, ENC_BIG_ENDIAN);
				offset += 8;
				break;
			}
			packet_content->data_list[packet_content->dataCount].offset = offset;
			offset += packet_content->data_list[packet_content->dataCount].length;	//Skip the packet
			flag = 1;
		}
		else if (prefix >= 0xc0 && prefix <= 0xf7) {
			//A list less than 55 bytes long
			offset += 1;	//Skip the prefix
		}
		else if (prefix >= 0xf8 && prefix <= 0xff) {
			//A list more than 55 bytes long
			offset += 1;	//Skip the prefix
			offset += prefix - 0xf7;	//Skip the length
		}
		else {
			//Not RLP encoded messsage
			return 1;
		}
		if (flag) {
			packet_content->dataCount++;
			packet_content->data_list = wmem_realloc(wmem_packet_scope(), packet_content->data_list,
				sizeof(struct UnitData) * (packet_content->dataCount + 1));
		}
	}
	return 0;
}

static int dissect_ethdevp2p(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree _U_, void *data _U_, struct PacketContent *packet_content) {
	//For tap
	struct Ethdevp2pTap *ethdevp2pInfo;
	ethdevp2pInfo = wmem_alloc(wmem_packet_scope(), sizeof(struct Ethdevp2pTap));
	ethdevp2pInfo->packet_type = -1;
	ethdevp2pInfo->node_number = -1;
	ethdevp2pInfo->ping_count = -1;
	ethdevp2pInfo->pong_count = -1;
	ethdevp2pInfo->findNode_count = -1;
	ethdevp2pInfo->neighbours_count = -1;
	ethdevp2pInfo->pp_time_flag = 0;
	ethdevp2pInfo->fn_time_flag = 0;
	ethdevp2pInfo->pp_time = pinfo->fd->abs_ts;
	ethdevp2pInfo->fn_time = pinfo->fd->abs_ts;
	gint offset = 0;
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "ETHDEVP2PDISCO");
	col_clear(pinfo->cinfo, COL_INFO);

	proto_item *ti = proto_tree_add_item(tree, proto_ethdevp2p, tvb, 0, -1, ENC_NA);
	proto_tree *ethdevp2p_tree = proto_item_add_subtree(ti, ett_ethdevp2p);
	/* Add Header */
	proto_tree_add_item(ethdevp2p_tree, hf_ethdevp2p_hash, tvb, offset, 32, ENC_BIG_ENDIAN);
	offset += 32;
	proto_tree_add_item(ethdevp2p_tree, hf_ethdevp2p_signature, tvb, offset, 65, ENC_BIG_ENDIAN);
	offset += 65;

	/* Add Packet Sub Tree */
	proto_item *tiPacket = proto_tree_add_item(ethdevp2p_tree, hf_ethdevp2p_packet, tvb, offset, -1, ENC_NA);
	proto_tree *ethdevp2p_packet = proto_item_add_subtree(tiPacket, ett_ethdevp2p_packet);

	/* Packet Type */
	/* Get the Packet Type */
	guint value;
	value = tvb_get_guint8(tvb, offset);
	ethdevp2pInfo->packet_type = value;
	/* Add the Packet Type to the Sub Tree */
	proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_packet_type, tvb, offset, 1, ENC_BIG_ENDIAN);
	gint currentData = 1;

	//This is a Ping message
	if (value == 0x01) {
		ethdevp2pInfo.packet_type = 0x01;
		ethdevp2pInfo.numbnodes = -1;
		currentData = 1;
		//Get Ping version
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 1) {
				//It's Ping version
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_version, tvb,
					packet_content->data_list[currentData].offset, 1, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
		}
		//Get sender IP address
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's IPv4
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_sender_ipv4, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_BIG_ENDIAN);
				currentData++;
			} 
			else if (packet_content->data_list[currentData].length == 16) {
				//It's IPv6
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_sender_ipv6, tvb,
					packet_content->data_list[currentData].offset, 16, ENC_BIG_ENDIAN);
				currentData++;
			} 
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get sender UDP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 2) {
				//It's sender UDP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_sender_udp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			} 
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get sender TCP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 0) {
				//Optional TCP port missed
				currentData++;
			} 
			else if (packet_content->data_list[currentData].length == 2) {
				//It's sender TCP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_sender_tcp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get recipient IP address
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's IPv4
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_recipient_ipv4, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_BIG_ENDIAN);
				currentData++;
			}
			else if (packet_content->data_list[currentData].length == 16) {
				//It's IPv6
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_recipient_ipv6, tvb,
					packet_content->data_list[currentData].offset, 16, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get recipient UDP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 2) {
				//It's recipient UDP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_recipient_udp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			} 
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get recipient TCP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 0) {
				//Optional TCP port missed
				currentData++;
			} 
			else if (packet_content->data_list[currentData].length == 2) {
				//It's recipient TCP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_recipient_tcp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			} 
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
		//Get expiration
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's expiration
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_ping_expiration, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_TIME_SECS | ENC_BIG_ENDIAN);
				currentData++;
			} 
			else {
				//Not valid packet
				return 1;
			}
		} 
		else {
			//Not valid packet
			return 1;
		}
	}
	//This is a Pong message	
	else if (value == 0x02) {
		//Get recipient IP address
		ethdevp2pInfo.packet_type = 0x02;
		ethdevp2pInfo.numbnodes = -1;
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's IPv4
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_recipient_ipv4, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_BIG_ENDIAN);
				currentData++;
			}
			else if (packet_content->data_list[currentData].length == 16) {
				//It's IPv6
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_recipient_ipv6, tvb,
					packet_content->data_list[currentData].offset, 16, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
		//Get recipient UDP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 2) {
				//It's recipient UDP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_recipient_udp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
		//Get recipient TCP port
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 0) {
				//Optional TCP port missed
				currentData++;
			}
			else if (packet_content->data_list[currentData].length == 2) {
				//It's recipient TCP port
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_recipient_tcp_port, tvb,
					packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
		//Get hash
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 32) {
				//It's hash
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_ping_hash, tvb,
					packet_content->data_list[currentData].offset, 32, ENC_TIME_SECS | ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
		//Get expiration
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's expiration
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_pong_expiration, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_TIME_SECS | ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
	}
	//This is a FindNode message
	else if (value == 0x03) {
		ethdevp2pInfo.packet_type = 0x03;
		ethdevp2pInfo.numbnodes = -1;
		//Get public key
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 64) {
				//It's Public key
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_findNode_target, tvb,
					packet_content->data_list[currentData].offset, 64, ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
		//Get expiration
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's expiration
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_findNode_expiration, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_TIME_SECS | ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
	}
	//This is a Neighbour message
	else if (value == 0x04) {
		//Skip Prefix
		ethdevp2pInfo.packet_type = 0x04;
		ethdevp2pInfo.numbnodes = (tvb_captured_length(tvb) - 151) / 79;
		hf_ethdevp2p_node_lenght = (tvb_captured_length(tvb) - 151) / 79;
		test = tvb_get_guint8(tvb, offset);	//Get the length of the Overall List bytes length
		offset += 1;
		offset += (test - 0xf7);	//Skip the Overall List length byte(s)
		test = tvb_get_guint8(tvb, offset);	//Get the length of the Node List bytes length
		offset += 1;
		offset += (test - 0xf7);	//Skip the Node List length byte(s)
		proto_item *tiNode;
		proto_tree *ethdevp2p_node;
		//Get a list of nodes
		//Variable to get node numbers
		gint nodeNumber = 0;
		for (currentData = 1; currentData < packet_content->dataCount - 1; currentData++) {
			//Add a Node Sub Tree
			//At least one node exists
			if (packet_content->dataCount > currentData + 4) {
				tiNode = proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_neighbors_node, tvb,
					packet_content->data_list[currentData].offset - 1,
					packet_content->data_list[currentData + 4].offset - packet_content->data_list[currentData].offset, ENC_NA);
				ethdevp2p_node = proto_item_add_subtree(tiNode, ett_ethdevp2p_packet);
				//Get neighbor node IP address
				if (packet_content->dataCount > currentData) {
					if (packet_content->data_list[currentData].length == 4) {
						//It's IPv4
						proto_tree_add_item(ethdevp2p_node, hf_ethdevp2p_neighbors_nodes_ipv4, tvb,
							packet_content->data_list[currentData].offset, 4, ENC_BIG_ENDIAN);
						currentData++;
					}
					else if (packet_content->data_list[currentData].length == 16) {
						//It's IPv6
						proto_tree_add_item(ethdevp2p_node, hf_ethdevp2p_neighbors_nodes_ipv6, tvb,
							packet_content->data_list[currentData].offset, 16, ENC_BIG_ENDIAN);
						currentData++;
					}
					else {
						//Not valid packet
						return 1;
					}
				}
				else {
					//Not valid packet
					return 1;
				}
				//Get neighbor node UDP port
				if (packet_content->dataCount > currentData) {
					if (packet_content->data_list[currentData].length == 2) {
						//It's node UDP port
						proto_tree_add_item(ethdevp2p_node, hf_ethdevp2p_neighbors_nodes_udp_port, tvb,
							packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
						currentData++;
					}
					else {
						//Not valid packet
						return 1;
					}
				}
				else {
					//Not valid packet
					return 1;
				}
				//Get neighbor node TCP port
				if (packet_content->dataCount > currentData) {
					if (packet_content->data_list[currentData].length == 0) {
						//Optional TCP port missed
						currentData++;
					}
					else if (packet_content->data_list[currentData].length == 2) {
						//It's node TCP port
						proto_tree_add_item(ethdevp2p_node, hf_ethdevp2p_neighbors_nodes_tcp_port, tvb,
							packet_content->data_list[currentData].offset, 2, ENC_BIG_ENDIAN);
						currentData++;
					}
					else {
						//Not valid packet
						return 1;
					}
				}
				else {
					//Not valid packet
					return 1;
				}
				//Get neighbor node Public key
				if (packet_content->dataCount > currentData) {
					if (packet_content->data_list[currentData].length == 64) {
						//It's public key
						proto_tree_add_item(ethdevp2p_node, hf_ethdevp2p_neighbors_nodes_id, tvb,
							packet_content->data_list[currentData].offset, 64, ENC_BIG_ENDIAN);
					}
					else {
						//Not valid packet
						return 1;
					}
				}
				else {
					//Not valid packet
					return 1;
				}
				nodeNumber++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		//Add node number
		proto_tree_add_uint(ethdevp2p_packet, hf_ethdevp2p_neighbors_nodes_number, tvb,
			packet_content->data_list[0].offset + 1,
			packet_content->data_list[currentData].offset - packet_content->data_list[0].offset - 2,
			nodeNumber);
		ethdevp2pInfo->node_number = nodeNumber;
		//Get expiration
		if (packet_content->dataCount > currentData) {
			if (packet_content->data_list[currentData].length == 4) {
				//It's expiration
				proto_tree_add_item(ethdevp2p_packet, hf_ethdevp2p_neighbors_expiration, tvb,
					packet_content->data_list[currentData].offset, 4, ENC_TIME_SECS | ENC_BIG_ENDIAN);
				currentData++;
			}
			else {
				//Not valid packet
				return 1;
			}
		}
		else {
			//Not valid packet
			return 1;
		}
	}
	//This is not a valid message	
	else {
		//Not valid packet
		return 1;
	}
	//For conversation
	proto_item *it;
	conversation_t *conversation;
	conversation = find_or_create_conversation(pinfo);
	eth_conv_info_t *eth_conv_info;
	eth_conv_ping_index_t *eth_conv_ping_index;
	eth_conv_pong_index_t *eth_conv_pong_index;
	eth_conv_findNode_index_t *eth_conv_findNode_index;
	eth_conv_neighbours_index_t *eth_conv_neighbours_index;
	eth_conv_pp_transaction_t *eth_conv_pp_trans;
	eth_conv_fn_transaction_t *eth_conv_fn_trans;
	eth_conv_info = (eth_conv_info_t *)conversation_get_proto_data(conversation, proto_ethdevp2p);
	if (!eth_conv_info) {
		//No conversation found
		eth_conv_info = wmem_new(wmem_file_scope(), eth_conv_info_t);
		eth_conv_info->pdus = wmem_map_new(wmem_file_scope(), g_direct_hash, g_direct_equal);
		eth_conv_info->fdus = wmem_map_new(wmem_file_scope(), g_direct_hash, g_direct_equal);
		eth_conv_info->conv_id = conversation->conv_index;
		eth_conv_info->ping_count = 0;
		eth_conv_info->pong_count = 0;
		eth_conv_info->findNode_count = 0;
		eth_conv_info->neighbours_count = 0;
		conversation_add_proto_data(conversation, proto_ethdevp2p, eth_conv_info);
	} 
	if (!PINFO_FD_VISITED(pinfo)) {
		//update conversation info based on type
		switch (value) {
		case 0x01:
			//This is a Ping request
			eth_conv_ping_index = wmem_new(wmem_file_scope(), eth_conv_ping_index_t);
			eth_conv_ping_index->ping_index = eth_conv_info->ping_count;
			p_add_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num, eth_conv_ping_index);
			eth_conv_info->ping_count++;
			eth_conv_pp_trans = wmem_new(wmem_file_scope(), eth_conv_pp_transaction_t);
			eth_conv_pp_trans->ping_frame = pinfo->num;
			eth_conv_pp_trans->pong_frame = 0;
			eth_conv_pp_trans->pp_time = pinfo->fd->abs_ts;
			wmem_map_insert(eth_conv_info->pdus, GUINT_TO_POINTER(eth_conv_ping_index->ping_index), (void *)eth_conv_pp_trans);
			break;
		case 0x02:
			//This a Pong response to Ping request
			eth_conv_pong_index = wmem_new(wmem_file_scope(), eth_conv_pong_index_t);
			eth_conv_pong_index->pong_index = eth_conv_info->pong_count;
			p_add_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num, eth_conv_pong_index);
			eth_conv_info->pong_count++;
			eth_conv_pp_trans = (eth_conv_pp_transaction_t *)wmem_map_lookup(eth_conv_info->pdus, GUINT_TO_POINTER(eth_conv_pong_index->pong_index));
			if (eth_conv_pp_trans) {
				eth_conv_pp_trans->pong_frame = pinfo->num;
			}
			break;
		case 0x03:
			//This is a findNode request
			eth_conv_findNode_index = wmem_new(wmem_file_scope(), eth_conv_findNode_index_t);
			eth_conv_findNode_index->findNode_index = eth_conv_info->findNode_count;
			p_add_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num, eth_conv_findNode_index);
			eth_conv_info->findNode_count++;
			eth_conv_fn_trans = wmem_new(wmem_file_scope(), eth_conv_fn_transaction_t);
			eth_conv_fn_trans->findNode_frame = pinfo->num;
			eth_conv_fn_trans->neighbours_frame = 0;
			eth_conv_fn_trans->fn_time = pinfo->fd->abs_ts;
			wmem_map_insert(eth_conv_info->fdus, GUINT_TO_POINTER(eth_conv_findNode_index->findNode_index), (void *)eth_conv_fn_trans);
			break;
		case 0x04:
			//This is a Neighbours reponse to findNode request
			eth_conv_neighbours_index = wmem_new(wmem_file_scope(), eth_conv_neighbours_index_t);
			eth_conv_neighbours_index->neighbours_index = eth_conv_info->neighbours_count;
			p_add_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num, eth_conv_neighbours_index);
			eth_conv_info->neighbours_count++;
			//Choose the first neighbours msg
			if (!(eth_conv_neighbours_index->neighbours_index % 2)) {
				eth_conv_fn_trans = (eth_conv_fn_transaction_t *)wmem_map_lookup(eth_conv_info->fdus, GUINT_TO_POINTER(eth_conv_neighbours_index->neighbours_index / 2));
				if (eth_conv_fn_trans) {
					eth_conv_fn_trans->neighbours_frame = pinfo->num;
				}
			}
			break;
		}
	}
	else {
		//Print state track in the tree
		switch (value) {
			case 0x01:
				eth_conv_ping_index = (eth_conv_ping_index_t *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num);
				if (eth_conv_ping_index) {
					it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_ping_index, tvb, 0, 0, eth_conv_ping_index->ping_index);
					PROTO_ITEM_SET_GENERATED(it);
				}
				eth_conv_pp_trans = (eth_conv_pp_transaction_t *)wmem_map_lookup(eth_conv_info->pdus, GUINT_TO_POINTER(eth_conv_ping_index->ping_index));
				break;
			case 0x02:
				eth_conv_pong_index = (eth_conv_pong_index_t *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num);
				if (eth_conv_pong_index) {
					it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_pong_index, tvb, 0, 0, eth_conv_pong_index->pong_index);
					PROTO_ITEM_SET_GENERATED(it);
				}
				eth_conv_pp_trans = (eth_conv_pp_transaction_t *)wmem_map_lookup(eth_conv_info->pdus, GUINT_TO_POINTER(eth_conv_pong_index->pong_index));
				break;
			case 0x03:
				eth_conv_findNode_index = (eth_conv_findNode_index_t *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num);
				if (eth_conv_findNode_index) {
					it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_findNode_index, tvb, 0, 0, eth_conv_findNode_index->findNode_index);
					PROTO_ITEM_SET_GENERATED(it);
				}
				eth_conv_fn_trans = (eth_conv_fn_transaction_t *)wmem_map_lookup(eth_conv_info->fdus, GUINT_TO_POINTER(eth_conv_findNode_index->findNode_index));
				break;
			case 0x04:
				eth_conv_neighbours_index = (eth_conv_neighbours_index_t *)p_get_proto_data(wmem_file_scope(), pinfo, proto_ethdevp2p, pinfo->num);
				if (eth_conv_neighbours_index) {
					it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_neighbours_index, tvb, 0, 0, eth_conv_neighbours_index->neighbours_index);
					PROTO_ITEM_SET_GENERATED(it);
				}
				if (!(eth_conv_neighbours_index->neighbours_index % 2)) {
					eth_conv_fn_trans = (eth_conv_fn_transaction_t *)wmem_map_lookup(eth_conv_info->fdus, GUINT_TO_POINTER(eth_conv_neighbours_index->neighbours_index / 2));
				}
				break;
		}
		it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_id, tvb, 0, 0, eth_conv_info->conv_id);
		PROTO_ITEM_SET_GENERATED(it);
		it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_ping_count, tvb, 0, 0, eth_conv_info->ping_count);
		PROTO_ITEM_SET_GENERATED(it);
		ethdevp2pInfo->ping_count = eth_conv_info->ping_count;
		it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_pong_count, tvb, 0, 0, eth_conv_info->pong_count);
		PROTO_ITEM_SET_GENERATED(it);
		ethdevp2pInfo->pong_count = eth_conv_info->pong_count;
		it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_findNode_count, tvb, 0, 0, eth_conv_info->findNode_count);
		PROTO_ITEM_SET_GENERATED(it);
		ethdevp2pInfo->findNode_count = eth_conv_info->findNode_count;
		it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_neighbours_count, tvb, 0, 0, eth_conv_info->neighbours_count);
		PROTO_ITEM_SET_GENERATED(it);
		ethdevp2pInfo->neighbours_count = eth_conv_info->neighbours_count;
	}
	if (!eth_conv_pp_trans) {
		/* Create a fake transaction */
		eth_conv_pp_trans = wmem_new(wmem_packet_scope(), eth_conv_pp_transaction_t);
		eth_conv_pp_trans->ping_frame = 0;
		eth_conv_pp_trans->pong_frame = 0;
		eth_conv_pp_trans->pp_time = pinfo->fd->abs_ts;
	}
	if (!eth_conv_fn_trans) {
		/* Create a fake transaction */
		eth_conv_fn_trans = wmem_new(wmem_packet_scope(), eth_conv_fn_transaction_t);
		eth_conv_fn_trans->findNode_frame = 0;
		eth_conv_fn_trans->neighbours_frame = 0;
		eth_conv_fn_trans->fn_time = pinfo->fd->abs_ts;
	}
	switch (value) {
	case 0x01:
		if (eth_conv_pp_trans->pong_frame && eth_conv_pp_trans->ping_frame) {
			it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_pong_frame, tvb, 0, 0, eth_conv_pp_trans->pong_frame);
			PROTO_ITEM_SET_GENERATED(it);
		}
		break;
	case 0x02:
		if (eth_conv_pp_trans->ping_frame && eth_conv_pp_trans->pong_frame) {
			nstime_t ns;
			it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_ping_frame, tvb, 0, 0, eth_conv_pp_trans->ping_frame);
			PROTO_ITEM_SET_GENERATED(it);
			nstime_delta(&ns, &pinfo->fd->abs_ts, &eth_conv_pp_trans->pp_time);
			it = proto_tree_add_time(ethdevp2p_tree, hf_ethdevp2p_conv_pp_time, tvb, 0, 0, &ns);
			PROTO_ITEM_SET_GENERATED(it);
			ethdevp2pInfo->pp_time = eth_conv_pp_trans->pp_time;
			ethdevp2pInfo->pp_time_flag = 1;
		}
		break;
	case 0x03:
		if (eth_conv_fn_trans->neighbours_frame && eth_conv_fn_trans->findNode_frame) {
			it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_neighbours_frame, tvb, 0, 0, eth_conv_fn_trans->neighbours_frame);
			PROTO_ITEM_SET_GENERATED(it);
		}
		break;
	case 0x04:
		if (!(eth_conv_neighbours_index->neighbours_index % 2)) {
			if (eth_conv_fn_trans->findNode_frame && eth_conv_fn_trans->neighbours_frame) {
				nstime_t ns;
				it = proto_tree_add_uint(ethdevp2p_tree, hf_ethdevp2p_conv_findNode_frame, tvb, 0, 0, eth_conv_fn_trans->findNode_frame);
				PROTO_ITEM_SET_GENERATED(it);
				nstime_delta(&ns, &pinfo->fd->abs_ts, &eth_conv_fn_trans->fn_time);
				it = proto_tree_add_time(ethdevp2p_tree, hf_ethdevp2p_conv_fn_time, tvb, 0, 0, &ns);
				PROTO_ITEM_SET_GENERATED(it);
				ethdevp2pInfo->fn_time = eth_conv_fn_trans->fn_time;
				ethdevp2pInfo->fn_time_flag = 1;
			}
		}
		break;
	}
	//For stats
	tap_queue_packet(ethdevp2p_tap, pinfo, ethdevp2pInfo);
	wmem_free(wmem_packet_scope(), ethdevp2pInfo);
	return 0;
}

static gboolean dissect_ethdevp2p_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
	// First, make sure we have enough data to do the check.
	if (tvb_captured_length(tvb) < MIN_ETHDEVP2PDISCO_LEN || tvb_captured_length(tvb) > MAX_ETHDEVP2PDISCO_LEN) {
		  return FALSE;
	}
	// Check if it is rlp encoded
	struct PacketContent *packet_content;
	packet_content = wmem_alloc(wmem_packet_scope(), sizeof(struct PacketContent));
	packet_content->dataCount = 0;
	packet_content->data_list = wmem_alloc(wmem_packet_scope(), sizeof(struct UnitData));
	if (rlp_decode(tvb, packet_content)) {
		wmem_free(wmem_packet_scope(), packet_content->data_list);
		wmem_free(wmem_packet_scope(), packet_content);
		return FALSE;
	}
	if (dissect_ethdevp2p(tvb, pinfo, tree, data _U_, packet_content) == 1) {
		//This is not a valid message
		wmem_free(wmem_packet_scope(), packet_content->data_list);
		wmem_free(wmem_packet_scope(), packet_content);
		return FALSE;
	}
	wmem_free(wmem_packet_scope(), packet_content->data_list);
	wmem_free(wmem_packet_scope(), packet_content);
	return TRUE;
}

// register all http trees

static void ethdevp2p_stats_tree_init(stats_tree *st) {
	st_node_packets = stats_tree_create_node(st, st_str_packets, 0, TRUE);
	st_node_packet_types = stats_tree_create_pivot(st, st_str_packet_types, st_node_packets);
	st_node_packet_neighbours = stats_tree_create_range_node(st, st_str_packet_neighbours, 0,
		"0-5", "6-10", "11-", NULL);
	st_node_packet_ping_frequency = stats_tree_create_range_node(st, st_str_packet_ping_frequency, 0,
		"0-", NULL);
	st_node_packet_pong_frequency = stats_tree_create_range_node(st, st_str_packet_pong_frequency, 0,
		"0-", NULL);
	st_node_packet_findNode_frequency = stats_tree_create_range_node(st, st_str_packet_findNode_frequency, 0,
		"0-", NULL);
	st_node_packet_neighbours_frequency = stats_tree_create_range_node(st, st_str_packet_neighbours_frequency, 0,
		"0-", NULL);
}

static int ethdevp2p_stats_tree_packet(stats_tree* st, packet_info* pinfo, epan_dissect_t* edt, const void* p) {
	struct Ethdevp2pTap *pi = (struct Ethdevp2pTap *)p;
	tick_stat_node(st, st_str_packets, 0, FALSE);
	stats_tree_tick_pivot(st, st_node_packet_types,
		val_to_str(pi->packet_type, packettypenames, "Unknown packet type (%d)"));
	stats_tree_tick_range(st, st_str_packet_neighbours, 0, pi->node_number);
	stats_tree_tick_range(st, st_str_packet_ping_frequency, 0, pi->ping_count);
	stats_tree_tick_range(st, st_str_packet_pong_frequency, 0, pi->pong_count);
	stats_tree_tick_range(st, st_str_packet_findNode_frequency, 0, pi->findNode_count);
	stats_tree_tick_range(st, st_str_packet_neighbours_frequency, 0, pi->neighbours_count);
	return 1;
}

static void register_ethdevp2p_stat_trees(void) {
    stats_tree_register_plugin("ethdevp2pdisco", "ethdevp2pdisco", "Ethdevp2p/Packet Stats", 0,
        ethdevp2p_stats_tree_packet, ethdevp2p_stats_tree_init, NULL);
}

WS_DLL_PUBLIC_DEF const gchar version[] = "0.0";
static void ethdevp2p_srt_table_init(struct register_srt* srt _U_, GArray* srt_array,
	srt_gui_init_cb gui_callback, void* gui_data) {
	srt_stat_table *eth_srt_table;
	eth_srt_table = init_srt_table("Ethdevp2p packets service response time", NULL, srt_array, 2,
		NULL, "eth.srt.packets", gui_callback, gui_data, NULL);
	init_srt_table_row(eth_srt_table, 0, "P->P response time");
	init_srt_table_row(eth_srt_table, 1, "F->N response time");
}

static int ethdevp2p_srt_table_packet(void *pss, packet_info *pinfo, epan_dissect_t *edt _U_, const void *prv) {
	srt_stat_table *eth_srt_table;
	srt_data_t *data = (srt_data_t *)pss;
	const struct Ethdevp2pTap *ethdevp2pInfo = (const struct Ethdevp2pTap *)prv;
	if (!ethdevp2pInfo) {
		return 0;
	}
	eth_srt_table = g_array_index(data->srt_array, srt_stat_table*, 0);
	if (ethdevp2pInfo->pp_time_flag) {
		add_srt_table_data(eth_srt_table, 0, &ethdevp2pInfo->pp_time, pinfo);
	}
	if (ethdevp2pInfo->fn_time_flag) {
		add_srt_table_data(eth_srt_table, 1, &ethdevp2pInfo->fn_time, pinfo);
	}
	return 1;
}

static void register_ethdevp2p_srt_table(void) {
	register_srt_table(proto_ethdevp2p, "ethdevp2pdisco", 1, ethdevp2p_srt_table_packet, ethdevp2p_srt_table_init, NULL);
}

static const char* ethdevp2p_conv_get_filter_type(conv_item_t* conv, conv_filter_type_e filter) {
	return CONV_FILTER_INVALID;
}

static const char* ethdevp2p_host_get_filter_type(hostlist_talker_t* host, conv_filter_type_e filter) {
	return CONV_FILTER_INVALID;
}

static ct_dissector_info_t ethdevp2p_ct_dissector_info = {&ethdevp2p_conv_get_filter_type};

static hostlist_dissector_info_t ethdevp2p_host_dissector_info = {&ethdevp2p_host_get_filter_type};

static int ethdevp2p_hostlist_packet(void *pit, packet_info *pinfo,
	epan_dissect_t *edt _U_, const void *vip _U_) {
	conv_hash_t *hash = (conv_hash_t*)pit;

	add_hostlist_table_data(hash, &pinfo->dl_src, 0, TRUE, 1, pinfo->fd->pkt_len, &ethdevp2p_host_dissector_info, ENDPOINT_NONE);
	add_hostlist_table_data(hash, &pinfo->dl_dst, 0, FALSE, 1, pinfo->fd->pkt_len, &ethdevp2p_host_dissector_info, ENDPOINT_NONE);

	return 1;
}

static int ethdevp2p_conversation_packet(void *pct, packet_info *pinfo,
	epan_dissect_t *edt _U_, const void *vip _U_) {
	conv_hash_t *hash = (conv_hash_t*)pct;
	add_conversation_table_data(hash, &pinfo->dl_src, &pinfo->dl_dst, 0, 0, 1,
		pinfo->fd->pkt_len, &pinfo->rel_ts, &pinfo->abs_ts,
		&ethdevp2p_ct_dissector_info, ENDPOINT_NONE);
	return 1;
}

static void register_ethdevp2p_conversation_table(void) {
	register_conversation_table(proto_ethdevp2p, TRUE, ethdevp2p_conversation_packet, ethdevp2p_hostlist_packet);
}

void proto_register_ethdevp2p(void) {

    static hf_register_info hf[] = {
	{ &hf_ethdevp2p_hash,
	    {"Ethereum Devp2p Hash", "ethdevp2pdisco.hash",
	    FT_BYTES, BASE_NONE,
	    NULL, 0x0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_signature,
	    {"Ethereum Devp2p Signature", "ethdevp2pdisco.signature",
	    FT_BYTES, BASE_NONE,
	    NULL, 0x0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_packet,
		{ "Ethereum Devp2p Packet", "ethdevp2pdisco.packet",
		FT_NONE, BASE_NONE,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_packet_type,
	    {"Ethereum Devp2p Packet Type", "ethdevp2pdisco.packet_type",
	    FT_UINT8, BASE_DEC,
	    VALS(packettypenames), 0x0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_version,
	    {"Ping Version", "ethdevp2pdisco.ping.version",
	    FT_UINT8, BASE_DEC,
	    NULL, 0x0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_sender_ipv4,
	    {"Ping Sender IPv4", "ethdevp2pdisco.ping.sender_ipv4",
	    FT_IPv4, BASE_NONE,
	    NULL, 0x0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_sender_ipv6,
		{ "Ping Sender IPv6", "ethdevp2pdisco.ping.sender_ipv6",
		FT_IPv6, BASE_NONE,
		NULL, 0x0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_sender_udp_port,
	    { "Ping Sender UDP Port", "ethdevp2pdisco.ping.sender_udp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_sender_tcp_port,
	    { "Ping Sender TCP Port", "ethdevp2pdisco.ping.sender_tcp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_recipient_ipv4,
	    { "Ping Recipient IPv4", "ethdevp2pdisco.ping.recipient_ipv4",
	    FT_IPv4, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_recipient_ipv6,
		{ "Ping Recipient IPv6", "ethdevp2pdisco.ping.recipient_ipv6",
		FT_IPv6, BASE_NONE,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_recipient_udp_port,
	    { "Ping Recipient UDP Port", "ethdevp2pdisco.ping.recipient_udp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_recipient_tcp_port,
		{ "Ping Recipient TCP Port", "ethdevp2pdisco.ping.recipient_tcp_port",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_ping_expiration,
	    { "Ping Expiration", "ethdevp2pdisco.ping.expiration",
	    FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_recipient_ipv4,
	    { "Pong Recipient IPv4", "ethdevp2pdisco.pong.recipient_ipv4",
	    FT_IPv4, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_recipient_ipv6,
		{ "Pong Recipient IPv6", "ethdevp2pdisco.pong.recipient_ipv6",
		FT_IPv6, BASE_NONE,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_recipient_udp_port,
	    { "Pong Recipient UDP Port", "ethdevp2pdisco.pong.recipient_udp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_recipient_tcp_port,
	    { "Pong Recipient TCP Port", "ethdevp2pdisco.pong.recipient_tcp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_ping_hash,
	    { "Pong Ping Hash", "ethdevp2pdisco.pong.ping_hash",
	    FT_BYTES, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_pong_expiration,
	    { "Pong Expiration", "ethdevp2pdisco.pong.expiration",
	    FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_findNode_target,
	    { "FindNode Target Public Key", "ethdevp2pdisco.findNode.target",
	    FT_BYTES, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_findNode_expiration,
	    { "FindNode Expiration", "ethdevp2pdisco.findNode.expiration",
	    FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_node,
		{ "Neighbors Node", "ethdevp2pdisco.neighbors.node",
		FT_NONE, BASE_NONE,
		NULL, 0X0,
		NULL, HFILL }
	},
    
	{ &hf_ethdevp2p_neighbors_nodes_ipv4,
	    { "Neighbors Nodes IPv4", "ethdevp2pdisco.neighbors.node.ipv4",
	    FT_IPv4, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_nodes_ipv6,
		{ "Neighbors Nodes IPv6", "ethdevp2pdisco.neighbors.node.ipv6",
		FT_IPv6, BASE_NONE,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_nodes_udp_port,
	    { "Neighbors Nodes UDP Port", "ethdevp2pdisco.neighbors.node.udp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_nodes_tcp_port,
	    { "Neighbors Nodes TCP Port", "ethdevp2pdisco.neighbors.node.tcp_port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_nodes_id,
	    { "Neighbors Nodes ID", "ethdevp2pdisco.neighbors.node.id",
	    FT_BYTES, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_expiration,
	    { "Neighbors Expiration", "ethdevp2pdisco.neighbors.expiration",
	    FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethdevp2p_neighbors_nodes_number,
		{ "Neighbors Node number", "ethdevp2pdisco.neighbors.node_number",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_id,
		{ "Conversation id", "ethdevp2pdisco.conv.id",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_ping_count,
		{ "Number of Ping exchanged with peer", "ethdevp2pdisco.conv.ping_count",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_pong_count,
		{ "Number of Piong exchanged with peer", "ethdevp2pdisco.conv.pong_count",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_findNode_count,
		{ "Number of findNode exchanged with peer", "ethdevp2pdisco.conv.findNode_count",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_neighbours_count,
		{ "Number of Neighbours exchanged with peer", "ethdevp2pdisco.conv.neighbours_count",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_ping_index,
		{ "Index of this Ping in this conversation", "ethdevp2pdisco.conv.ping_index",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_pong_index,
		{ "Index of this Pong in this conversation", "ethdevp2pdisco.conv.pong_index",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_findNode_index,
		{ "Index of this findNode in this conversation", "ethdevp2pdisco.conv.findNode_index",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_neighbours_index,
		{ "Index of this Neighbours in this conversation", "ethdevp2pdisco.conv.neighbours_index",
		FT_UINT32, BASE_DEC,
		NULL, 0X0,
		NULL, HFILL }
	},

	{ &hf_ethdevp2p_conv_ping_frame,
		{ "Ping in", "ethdevp2pdisco.conv.ping_frame",
		FT_FRAMENUM, BASE_NONE,
		FRAMENUM_TYPE(FT_FRAMENUM_REQUEST), 0X0,
		"This is a response to the Ping request in this frame", HFILL }
	},

	{ &hf_ethdevp2p_conv_pong_frame,
		{ "Pong in", "ethdevp2pdisco.conv.pong_frame",
		FT_FRAMENUM, BASE_NONE,
		FRAMENUM_TYPE(FT_FRAMENUM_RESPONSE), 0X0,
		"The response to this Ping request is in this frame", HFILL }
	},

	{ &hf_ethdevp2p_conv_pp_time,
		{ "P->P Response time", "ethdevp2pdisco.conv.pp_time",
		FT_RELATIVE_TIME, BASE_NONE,
		NULL, 0X0,
		"The time between the Ping and the Pong", HFILL }
	},

	{ &hf_ethdevp2p_conv_findNode_frame,
		{ "findNode in", "ethdevp2pdisco.conv.findNode_frame",
		FT_FRAMENUM, BASE_NONE,
		FRAMENUM_TYPE(FT_FRAMENUM_REQUEST), 0X0,
		"This is a response(1st) to the findNode request in this frame", HFILL }
	},

	{ &hf_ethdevp2p_conv_neighbours_frame,
		{ "Neighbours in", "ethdevp2pdisco.conv.neighbours_frame",
		FT_FRAMENUM, BASE_NONE,
		FRAMENUM_TYPE(FT_FRAMENUM_RESPONSE), 0X0,
		"The response(1st) to this findNode request is in this frame", HFILL }
	},

	{ &hf_ethdevp2p_conv_fn_time,
		{ "F->N Response time", "ethdevp2pdisco.conv.fn_time",
		FT_RELATIVE_TIME, BASE_NONE,
		NULL, 0X0,
		"The time between the findNode and the 1st Neighbours", HFILL }
	}
	
    };


    /* Setup protocol subtree array */
    static gint *ett[] = {
	&ett_ethdevp2p,
	&ett_ethdevp2p_packet,
	&ett_ethdevp2p_node
    };

    proto_ethdevp2p = proto_register_protocol (
	    "Ethereum Devp2p Protocol",
	    "ETHDEVP2PDISCO",
	    "ethdevp2pdisco"
	);

	ethdevp2pdisco_handle = create_dissector_handle(dissect_ethdevp2p_heur, proto_ethdevp2p);
	proto_register_field_array(proto_ethdevp2p, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
	ethdevp2p_tap = register_tap("ethdevp2pdisco");
	register_ethdevp2p_stat_trees();
	register_ethdevp2p_srt_table();
	register_ethdevp2p_conversation_table();
}

void proto_reg_handoff_ethdevp2p(void) {
	heur_dissector_add("udp", dissect_ethdevp2p_heur, "Ethdevp2p disco Over Udp", "Ethdevp2p_udp", proto_ethdevp2p, HEURISTIC_ENABLE);
}