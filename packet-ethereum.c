#include "config.h"

#include <epan/packet.h>

#define ETHEREUM_PORT 30303

static int proto_ethereum = -1;
static int hf_ethereum_pdu_hash = -1;
static int hf_ethereum_pdu_signature = -1;
static int hf_ethereum_pdu_type = -1;
static int hf_ethereum_pdu_data = -1;
/* The following is for Ping message */
static int hf_ethereum_ping_version = -1;
static int hf_ethereum_ping_sender_ip = -1;
static int hf_ethereum_ping_sender_udp_port = -1;
static int hf_ethereum_ping_sender_tcp_port = -1;
static int hf_ethereum_ping_recipient_ip = -1;
static int hf_ethereum_ping_recipient_udp_port = -1;
static int hf_ethereum_ping_expiration = -1;

static gint ett_ethereum = -1;

static int dissect_ethereum(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree _U_, void *data _U_) {
    gint offset = 0;
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "ETHDEVP2PDISCO");
    col_clear(pinfo->cinfo, COL_INFO);

    proto_item *ti = proto_tree_add_item(tree, proto_ethereum, tvb, 0, -1, ENC_NA);

    proto_tree *ethereum_tree = proto_item_add_subtree(ti, ett_ethereum);
    proto_tree_add_item(ethereum_tree, hf_ethereum_pdu_hash, tvb, offset, 32, ENC_BIG_ENDIAN);
    offset += 32;
    proto_tree_add_item(ethereum_tree, hf_ethereum_pdu_signature, tvb, offset, 65, ENC_BIG_ENDIAN);
    offset += 65;
    proto_tree_add_item(ethereum_tree, hf_ethereum_pdu_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    //Check the version
    guint value;
    value = tvb_get_guint8(tvb, offset);
    offset += 1;
    if (value == 0x01) {
	offset += 1;	//Skip unknown packet 0xdc
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_version, tvb, offset, 1, ENC_BIG_ENDIAN);
	offset += 1;
	offset += 2;	//Skip unknown packet 0xcb84
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_sender_ip, tvb, offset, 4, ENC_BIG_ENDIAN);
	offset += 4;	
	offset += 1;	//Skip unknown packet 0x82
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_sender_udp_port, tvb, offset, 2, ENC_BIG_ENDIAN);
	offset += 2;
	offset += 1;	//Skip unknown packet 0x82
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_sender_tcp_port, tvb, offset, 2, ENC_BIG_ENDIAN);
	offset += 2;
	offset += 2;	//Skip unknown packet 0xc984
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_recipient_ip, tvb, offset, 4, ENC_BIG_ENDIAN);
	offset += 4;
	offset += 1;	//Skip unknown packet 0x82
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_recipient_udp_port, tvb, offset, 2, ENC_BIG_ENDIAN);
	offset += 2;
	proto_tree_add_item(ethereum_tree, hf_ethereum_ping_expiration, tvb, offset, -1, ENC_BIG_ENDIAN); 
    } else {
        proto_tree_add_item(ethereum_tree, hf_ethereum_pdu_data, tvb, offset, -1, ENC_BIG_ENDIAN);
    }

    return tvb_captured_length(tvb);
}

void proto_register_ethereum(void) {

    static hf_register_info hf[] = {
	{ &hf_ethereum_pdu_hash,
	    { "ETHEREUM DEVP2P Hash", "Ethdevp2p.hash",
	    FT_BYTES, SEP_SPACE,
	    NULL, 0X0,
	    NULL, HFILL }
	}, 

	{ &hf_ethereum_pdu_signature,
	    { "ETHEREUM DEVP2P Signature", "Ethdevp2p.signature",
	    FT_BYTES, SEP_SPACE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_pdu_type,
	    { "ETHEREUM DEVP2P Type", "Ethdevp2p.type",
	    FT_BYTES, SEP_SPACE,
	    NULL, 0X0,
	    NULL, HFILL }
	}, 

	{ &hf_ethereum_pdu_data,
	    { "ETHEREUM DEVP2P Data", "Ethdevp2p.data",
	    FT_BYTES, SEP_SPACE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_version,
	    { "ETHEREUM DEVP2P Ping Version", "Ethdevp2p.ping.version",
	    FT_UINT16, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_sender_ip,
	    { "ETHEREUM DEVP2P Ping Sender IP", "Ethdevp2p.ping.sender-ip",
	    FT_IPv4, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_sender_udp_port,
	    { "ETHEREUM DEVP2P Ping Sender UDP Port", "Ethdevp2p.ping.sender-udp-port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_sender_tcp_port,
	    { "ETHEREUM DEVP2P Ping Sender TCP Port", "Ethdevp2p.ping.sender-tcp-port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_recipient_ip,
	    { "ETHEREUM DEVP2P Ping Recipient IP", "Ethdevp2p.ping.recipient-ip",
	    FT_IPv4, BASE_NONE,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_recipient_udp_port,
	    { "ETHEREUM DEVP2P Ping Recipient UDP Port", "Ethdevp2p.ping.recipient-udp-port",
	    FT_UINT32, BASE_DEC,
	    NULL, 0X0,
	    NULL, HFILL }
	},

	{ &hf_ethereum_ping_expiration,
	    { "ETHEREUM DEVP2P Ping Expiration", "Ethdevp2p.ping.expiration",
	    FT_BYTES, SEP_SPACE,
	    NULL, 0X0,
	    NULL, HFILL }
	}
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
	&ett_ethereum
    };

    proto_ethereum = proto_register_protocol (
		    "Ethereum devp2p discovery protocol",
		    "ETHDEVP2PDISCO",
		    "ethdevp2pdisco"
		    );

    proto_register_field_array(proto_ethereum, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));    
}

void proto_reg_handoff_ethereum(void) {
    static dissector_handle_t ethereum_handle;

    ethereum_handle = create_dissector_handle(dissect_ethereum, proto_ethereum);
    dissector_add_uint("udp.port", ETHEREUM_PORT, ethereum_handle);
}