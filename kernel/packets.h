/*
 * packets.h: protocol header and body structs
 */
#pragma once

/*
 * Ethernet Frame Header. A CRC-32 usually goes after the data.
 * https://en.wikipedia.org/wiki/Ethernet_frame
 */
struct etherframe {
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	uint16_t ethertype;
} __attribute__((packed));

/*
 * IP Header
 * https://tools.ietf.org/html/rfc791#page-11
 */
struct iphdr {
	uint8_t verihl;
	uint8_t tos;
	uint16_t len;
	uint16_t id;
	uint16_t flags_foffset;
	uint8_t ttl;
	uint8_t proto;
	uint16_t csum;
	uint32_t src;
	uint32_t dst;
	uint8_t options[0];
} __attribute__((packed));

/*
 * UDP Header
 * https://tools.ietf.org/html/rfc768
 */
struct udphdr {
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t len;
	uint16_t csum;
} __attribute__((packed));

struct dhcp_option {
	uint8_t tag;
	uint8_t len;
	uint8_t data[0];
} __attribute__((packed));

/*
 * DHCP Packet
 * https://tools.ietf.org/html/rfc2131#page-9
 */
struct dhcp {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint32_t chaddr[4];
	uint8_t sname[64];
	uint8_t file[128];
	uint32_t cookie;
	uint8_t options[0];
} __attribute__((packed));

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCPOPT_MSG_TYPE 53

#define DHCPMTYPE_DHCPDISCOVER 1
#define DHCPMTYPE_DHCPOFFER 2
#define DHCPMTYPE_DHCPREQUEST 3
#define DHCPMTYPE_DHCPDECLINE 4
#define DHCPMTYPE_DHCPACK 5
#define DHCPMTYPE_DHCPNAK 6
#define DHCPMTYPE_DHCPRELEASE 7
