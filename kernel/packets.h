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

#define ETHERTYPE_IP 0x0800

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

#define ip_get_version(ip) (((ip)->verihl & 0xF0) >> 4)
#define ip_get_length(ip)  (((ip)->verihl & 0x0F) * 4)

#define IPPROTO_UDP 17

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

#define UDPPORT_DHCP_CLIENT 68
#define UDPPORT_DHCP_SERVER 67

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

#define BOOTREQUEST       1
#define BOOTREPLY         2
#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_HTYPE_ETHERNET 1

enum {
	/* clang-format: please allow tab indentation here */
	DHCPOPT_PAD = 0,
	DHCPOPT_SUBNET_MASK,
	DHCPOPT_TIME_OFFSET,
	DHCPOPT_ROUTER,
	DHCPOPT_TIME_SERVER,
	DHCPOPT_NAME_SERVER,
	DHCPOPT_DOMAIN_NAME_SERVER,
	DHCPOPT_LOG_SERVER,
	DHCPOPT_COOKIE_SERVER,
	DHCPOPT_LPR_SERVER,
	DHCPOPT_IMPRESS_SERVER,
	DHCPOPT_RESOURCE_LOCATION_SERVER,
	DHCPOPT_HOST_NAME,
	DHCPOPT_BOOT_FILE_SIZE,
	DHCPOPT_MERIT_DUMP_FILE,
	DHCPOPT_DOMAIN_NAME,
	DHCPOPT_SWAP_SERVER,
	DHCPOPT_ROOT_PATH,
	DHCPOPT_EXTENSIONS_PATH,
	DHCPOPT_IP_FORWARDING_ENABLE,
	DHCPOPT_IP_NONLOCAL_SOURCE_ROUTING,

	DHCPOPT_LEASE_TIME = 51,
	/* lots of seemingly less relevant options, trimming here */
	DHCPOPT_MSG_TYPE = 53,
	DHCPOPT_SERVER_IDENTIFIER,
	DHCPOPT_END = 255,
};

#define DHCPMTYPE_DHCPDISCOVER 1
#define DHCPMTYPE_DHCPOFFER    2
#define DHCPMTYPE_DHCPREQUEST  3
#define DHCPMTYPE_DHCPDECLINE  4
#define DHCPMTYPE_DHCPACK      5
#define DHCPMTYPE_DHCPNAK      6
#define DHCPMTYPE_DHCPRELEASE  7
