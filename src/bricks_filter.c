/*
 * Copyright (c) 2014, Asim Jamshed, Robin Sommer, Seth Hall
 * and the International Computer Science Institute. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * (1) Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * (2) Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------*/
/* for prints etc */
#include "bricks_log.h"
/* for string functions */
#include <string.h>
/* for io modules */
#include "io_module.h"
/* for function prototypes */
#include "backend.h"
/* for requests/responses */
#include "bricks_interface.h"
/* for install_filter */
#include "netmap_module.h"
/* for filter functions */
#include "bricks_filter.h"
/* for types */
#include <sys/types.h>
/* for [n/h]to[h/n][ls] */
#include <netinet/in.h>
/* iphdr */
#include <netinet/ip.h>
/* ipv6hdr */
#include <netinet/ip6.h>

#if linux
#define IPV6_VERSION 0x60
#endif

/* tcphdr */
#include <netinet/tcp.h>
/* udphdr */
#include <netinet/udp.h>
/* eth hdr */
#include <net/ethernet.h>
/* for inet_ntoa() */
#include <sys/socket.h>
/* for inet_ntoa() */
#include <arpa/inet.h>
#ifdef ENABLE_BROKER
/* for broker comm. */
#include <broker/broker.h>
#endif
/*---------------------------------------------------------------------*/
#define CONN_SUBNET_CHECK(faddr, a)			\
	((a.s_addr & faddr.ip_mask) == faddr.ip_masked)
#define CONN_SUBNET6_CHECK(faddr, a)			\
	(!memcmp(a.s6_addr, faddr.addr32, 16))
#define FLOW_SUBNET_CHECK(faddr, a)					\
	(faddr.addr32 == 0 || (a.s_addr & faddr.ip_mask) == faddr.ip_masked)
#define FLOW_PORT_CHECK(p_a, p_b)		\
	(p_a == 0 || (p_a == p_b))
/*---------------------------------------------------------------------*/
static inline int32_t
HandleConnectionFilterIPv4Tcp(Filter *f, struct ip *iph, struct tcphdr *tcph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPVERSION &&
		((CONN_SUBNET_CHECK(f->conn.sip4addr, iph->ip_src) &&
		 CONN_SUBNET_CHECK(f->conn.dip4addr, iph->ip_dst) &&
		 f->conn.sport == tcph->th_sport &&
		 f->conn.dport == tcph->th_dport) ||
		(CONN_SUBNET_CHECK(f->conn.sip4addr, iph->ip_dst) &&
		 CONN_SUBNET_CHECK(f->conn.dip4addr, iph->ip_src) &&
		 f->conn.sport == tcph->th_dport &&
		 f->conn.dport == tcph->th_sport))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleConnectionFilterIPv6Tcp(Filter *f, struct ip6_hdr *iph, struct tcphdr *tcph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPV6_VERSION &&
		((CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_src) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_dst) &&
		 f->conn.sport == tcph->th_sport &&
		 f->conn.dport == tcph->th_dport) ||
		(CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_dst) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_src) &&
		 f->conn.sport == tcph->th_dport &&
		 f->conn.dport == tcph->th_sport))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleConnectionFilterIPv4Udp(Filter *f, struct ip *iph, struct udphdr *udph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPVERSION &&
		((CONN_SUBNET_CHECK(f->conn.sip4addr, iph->ip_src) &&
		 CONN_SUBNET_CHECK(f->conn.dip4addr, iph->ip_dst) &&
		 f->conn.sport == udph->uh_sport &&
		 f->conn.dport == udph->uh_dport) ||
		(CONN_SUBNET_CHECK(f->conn.sip4addr, iph->ip_dst) &&
		 CONN_SUBNET_CHECK(f->conn.dip4addr, iph->ip_src) &&
		 f->conn.sport == udph->uh_dport &&
		 f->conn.dport == udph->uh_sport))
		) ? 0 : 1;
	return 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleConnectionFilterIPv6Udp(Filter *f, struct ip6_hdr *iph, struct udphdr *udph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPV6_VERSION &&
		((CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_src) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_dst) &&
		 f->conn.sport == udph->uh_sport &&
		 f->conn.dport == udph->uh_dport) ||
		(CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_dst) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_src) &&
		 f->conn.sport == udph->uh_dport &&
		 f->conn.dport == udph->uh_sport))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleFlowFilterIPv4Tcp(Filter *f, struct ip *iph, struct tcphdr *tcph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPVERSION &&
		((FLOW_SUBNET_CHECK(f->conn.sip4addr, iph->ip_src) &&
		 FLOW_SUBNET_CHECK(f->conn.dip4addr, iph->ip_dst) &&
		 FLOW_PORT_CHECK(f->conn.sport, tcph->th_sport) &&
		 FLOW_PORT_CHECK(f->conn.dport, tcph->th_dport))
		||
		(FLOW_SUBNET_CHECK(f->conn.sip4addr, iph->ip_dst) &&
		 FLOW_SUBNET_CHECK(f->conn.dip4addr, iph->ip_src) &&
		 FLOW_PORT_CHECK(f->conn.sport, tcph->th_dport) &&
		 FLOW_PORT_CHECK(f->conn.dport, tcph->th_sport)))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleFlowFilterIPv6Tcp(Filter *f, struct ip6_hdr *iph, struct tcphdr *tcph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPV6_VERSION &&
		((CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_src) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_dst) &&
		 f->conn.sport == tcph->th_sport &&
		 f->conn.dport == tcph->th_dport) ||
		(CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_dst) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_src) &&
		 f->conn.sport == tcph->th_dport &&
		 f->conn.dport == tcph->th_sport))
		) ? 0 : 1;	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleFlowFilterIPv4Udp(Filter *f, struct ip *iph, struct udphdr *udph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPVERSION &&
		((FLOW_SUBNET_CHECK(f->conn.sip4addr, iph->ip_src) &&
		 FLOW_SUBNET_CHECK(f->conn.dip4addr, iph->ip_dst) &&
		 FLOW_PORT_CHECK(f->conn.sport, udph->uh_sport) &&
		 FLOW_PORT_CHECK(f->conn.dport, udph->uh_dport))
		||
		(FLOW_SUBNET_CHECK(f->conn.sip4addr, iph->ip_dst) &&
		 FLOW_SUBNET_CHECK(f->conn.dip4addr, iph->ip_src) &&
		 FLOW_PORT_CHECK(f->conn.sport, udph->uh_dport) &&
		 FLOW_PORT_CHECK(f->conn.dport, udph->uh_sport)))
		) ? 0 : 1;
	return 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleFlowFilterIPv6Udp(Filter *f, struct ip6_hdr *iph, struct udphdr *udph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPV6_VERSION &&
		((CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_src) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_dst) &&
		 f->conn.sport == udph->uh_sport &&
		 f->conn.dport == udph->uh_dport) ||
		(CONN_SUBNET6_CHECK(f->conn.sip6addr, iph->ip6_dst) &&
		 CONN_SUBNET6_CHECK(f->conn.dip6addr, iph->ip6_src) &&
		 f->conn.sport == udph->uh_dport &&
		 f->conn.dport == udph->uh_sport))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleIPv4Filter(Filter *f, struct ip *iph)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPVERSION &&
		(f->ip4addr.addr32 == iph->ip_src.s_addr ||
		 f->ip4addr.addr32 == iph->ip_dst.s_addr)
		) ? 0 : 1;	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleIPv6Filter(Filter *f, struct ip6_hdr *ip6h)
{
	TRACE_FILTER_FUNC_START();
	return (f->proto == IPV6_VERSION &&
		(!memcmp(f->ip6addr.addr32, ip6h->ip6_src.s6_addr, 16) ||
		 !memcmp(f->ip6addr.addr32, ip6h->ip6_dst.s6_addr, 16)))
		? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline int32_t
HandleMACFilter(Filter *f, struct ether_header *ethh)
{
	TRACE_FILTER_FUNC_START();
	return ((!memcmp(f->ethaddr.addr8, ethh->ether_dhost,
			 sizeof(ethh->ether_dhost))) ||
		(!memcmp(f->ethaddr.addr8, ethh->ether_shost,
			 sizeof(ethh->ether_shost)))
		) ? 0 : 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
/* Under construction.. */
int
analyze_packet(unsigned char *buf, FilterContext *cn, time_t current_time)	
{	
	TRACE_FILTER_FUNC_START();
	struct ether_header *ethh = NULL;
	struct ip *iph = NULL;
	struct ip6_hdr *ip6h = NULL;
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	int rc = 1;
	Filter *f = NULL;
	Filter *f_prev = NULL;

	ethh = (struct ether_header *)buf;
	switch (ntohs(ethh->ether_type)) {
	case ETHERTYPE_IP:
		/* IPv4 case */
		iph = ((struct ip *)(ethh + 1));
		break;
	case ETHERTYPE_IPV6:
		/* IPv6 case */
		ip6h = ((struct ip6_hdr *)(ethh + 1));
		break;
	default:
		TRACE_FILTER_FUNC_END();
		TRACE_DEBUG_LOG("Failed to recognize L3 protocol\n");
		return 1;
	}

 locate_l4:
	if (iph != NULL) {
		switch (iph->ip_p) {
		case IPPROTO_TCP:
			tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ip_hl<<2));
			break;
		case IPPROTO_UDP:
			udph = (struct udphdr *)((uint8_t *)iph + (iph->ip_hl<<2));
			break;
		case IPPROTO_IPIP:
			/* tunneling case to be filled */
			iph = ((struct ip *)((uint8_t *)iph + (iph->ip_hl<<2)));
			ip6h = NULL;
			goto locate_l4;
		default:
			TRACE_FILTER_FUNC_END();
			TRACE_DEBUG_LOG("Failed to recognize L4 protocol\n");
			return 1;
		}
	}

 locate_l46:
	if (ip6h != NULL) {
		switch (ntohs(ip6h->ip6_ctlun.ip6_un1.ip6_un1_nxt)) {
		case IPPROTO_TCP:
			tcph = (struct tcphdr *)(ip6h + 1);
			break;
		case IPPROTO_UDP:
			udph = (struct udphdr *)(ip6h + 1);
			break;
		case IPPROTO_IPIP:
			iph = (struct ip *)(ip6h + 1);
			ip6h = NULL;
			goto locate_l4;
			break;
		case IPPROTO_IPV6:
			ip6h = (struct ip6_hdr *)(ip6h + 1);
			iph = NULL;
			goto locate_l46;
			break;
		}
	}
	TAILQ_FOREACH_SAFE(f, &cn->filter_list, entry, f_prev) {
		if (unlikely((f->filt_time_period >= 0) &&
			     (current_time - f->filt_start_time >
			      f->filt_time_period))) {
			/* filter has expired, delete entry */
			if (f->filter_type_flag != BRICKS_NO_FILTER) {
				f->filter_type_flag = BRICKS_NO_FILTER;
				TRACE_LOG("Disabling filter: current_time: %d, filt_start_time: %d, filt_time_period: %d\n",
					  (int)current_time, (int)f->filt_start_time, (int)f->filt_time_period);
			}
			TAILQ_REMOVE(&cn->filter_list, f, entry);
			free(f);
			continue;
		}

		switch (f->filter_type_flag) {
		case BRICKS_CONNECTION_FILTER:
			if (iph != NULL) {
				if (tcph != NULL)
					rc = HandleConnectionFilterIPv4Tcp(f, iph, tcph);
				else if (udph != NULL)
					rc = HandleConnectionFilterIPv4Udp(f, iph, udph);
			} else if (ip6h != NULL) {
				if (tcph != NULL)
					rc = HandleConnectionFilterIPv6Tcp(f, ip6h, tcph);
				else if (udph != NULL)
					rc = HandleConnectionFilterIPv6Udp(f, ip6h, udph);
			}
			TRACE_DEBUG_LOG("Connection filter\n");
			break;
		case BRICKS_FLOW_FILTER:
			if (iph != NULL) {
				if (tcph != NULL)
					rc = HandleFlowFilterIPv4Tcp(f, iph, tcph);
				else if (udph != NULL)
					rc = HandleFlowFilterIPv4Udp(f, iph, udph);
			} else if (ip6h != NULL) {
				if (tcph != NULL)
					rc = HandleFlowFilterIPv6Tcp(f, ip6h, tcph);
				else if (udph != NULL)
					rc = HandleFlowFilterIPv6Udp(f, ip6h, udph);
			}
			TRACE_DEBUG_LOG("Flow filter\n");
			break;
		case BRICKS_IP_FILTER:
			if (iph != NULL) {
				rc = HandleIPv4Filter(f, iph);
			} else if (ip6h != NULL) {
				rc = HandleIPv6Filter(f, ip6h);
			}
			TRACE_DEBUG_LOG("IP filter\n");
			break;			
		case BRICKS_MAC_FILTER:
			rc = HandleMACFilter(f, ethh);
			TRACE_DEBUG_LOG("MAC filter\n");
			break;
		default:
			break;
		}

		if (rc == 0 && f->tgt == WHITELIST)
			return 1;
	}

	TRACE_FILTER_FUNC_END();
	return rc;

	UNUSED(tcph);
	UNUSED(udph);
}
/*---------------------------------------------------------------------*/
static inline uint32_t
MaskFromPrefix(int prefix)
{
	TRACE_FILTER_FUNC_START();
	uint32_t mask = 0;
	uint8_t *mask_t = (uint8_t *)&mask;
	int i, j;

	for (i = 0; i <= prefix / 8 && i < 4; i++) {
		for (j = 0; j < (prefix - i * 8) && j < 8; j++) {
			mask_t[i] |= (1 << (7 - j));
		}
	}
	
	return mask;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
static inline void
adjustMasks(Filter *f)
{
	TRACE_FILTER_FUNC_START();
	switch (f->filter_type_flag) {
	case BRICKS_CONNECTION_FILTER:
	case BRICKS_FLOW_FILTER:
		f->conn.sip4addr.ip_mask = MaskFromPrefix(f->conn.sip4addr.mask);
		f->conn.dip4addr.ip_mask = MaskFromPrefix(f->conn.dip4addr.mask);
		f->conn.sip4addr.ip_masked = f->conn.sip4addr.addr32 & f->conn.sip4addr.ip_mask;
		f->conn.dip4addr.ip_masked = f->conn.dip4addr.addr32 & f->conn.dip4addr.ip_mask;		
		break;
	case BRICKS_IP_FILTER:
		f->ip4addr.ip_mask = MaskFromPrefix(f->ip4addr.mask);
		f->ip4addr.ip_masked = f->ip4addr.addr32 & f->ip4addr.ip_mask;
		break;
	default:
		/* do nothing */
		break;
	}
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
int
apply_filter(FilterContext *cn, Filter *fin)
{
	TRACE_FILTER_FUNC_START();
	Filter *f = (Filter *)calloc(1, sizeof(Filter));
	if (f == NULL) {
		TRACE_LOG("Could not allocate memory for a new filter!\n");
		return 0;
	}
	memcpy(f, fin, sizeof(Filter));
	/* set the filter duration */
	f->filt_start_time = time(NULL) + fin->filt_start_time;
	/* set mask settings */
	adjustMasks(f);

	TRACE_LOG("Applying filter with time period: %d, and start_time: %d\n",
		  (int)f->filt_time_period, (int)f->filt_start_time);
	
	TAILQ_INSERT_TAIL(&cn->filter_list, f, entry);
	return 1;
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
#ifdef DEBUG
void
printFilter(Filter *f)
{
	TRACE_FILTER_FUNC_START();
	struct in_addr addr;
	struct in6_addr addr6;
	char str_addr[INET6_ADDRSTRLEN];       
	
	TRACE_LOG("Printing current filter...\n");
	
	switch (f->filter_type_flag) {
	case BRICKS_NO_FILTER:
		TRACE_LOG("No filter!\n");
		break;
	case BRICKS_CONNECTION_FILTER:
		TRACE_LOG("It's a connection filter\n");
		if (f->proto == IPVERSION) {		
			addr.s_addr = f->conn.sip4addr.addr32;
			TRACE_LOG("Sip: %s\n", inet_ntoa(addr));
			addr.s_addr = f->conn.dip4addr.addr32;
			TRACE_LOG("Dip: %s\n", inet_ntoa(addr));
		} else /* f->proto == IPV6_VERSION */ {
			memcpy(addr6.s6_addr, f->conn.sip6addr.addr32, 16);
			inet_ntop(AF_INET6, &addr6, str_addr, 16);
			TRACE_LOG("Sip: %s\n", str_addr);
			memcpy(addr6.s6_addr, f->conn.dip6addr.addr32, 16);
			inet_ntop(AF_INET6, &addr6, str_addr, 16);
			TRACE_LOG("Dip: %s\n", str_addr);			
		}
		break;
	case BRICKS_FLOW_FILTER:
		TRACE_LOG("It's a flow filter\n");
		if (f->proto == IPVERSION) {
			addr.s_addr = f->conn.sip4addr.addr32;
			TRACE_LOG("Sip: %s\n", inet_ntoa(addr));
			TRACE_LOG("Mask: %d\n", f->conn.sip4addr.mask);
			addr.s_addr = f->conn.dip4addr.addr32;
			TRACE_LOG("Dip: %s\n", inet_ntoa(addr));
			TRACE_LOG("Mask: %d\n", f->conn.dip4addr.mask);
		} else /* f->proto == IPV6_VERSION */ {
			memcpy(addr6.s6_addr, f->conn.sip6addr.addr32, 16);
			inet_ntop(AF_INET6, &addr6, str_addr, 16);
			TRACE_LOG("Sip: %s\n", str_addr);
			memcpy(addr6.s6_addr, f->conn.dip6addr.addr32, 16);
			inet_ntop(AF_INET6, &addr6, str_addr, 16);
			TRACE_LOG("Dip: %s\n", str_addr);			
		}
		break;		
	default:
		TRACE_LOG("Filter type is unrecognized!\n");
		break;
	}
	TRACE_FILTER_FUNC_END();
}
#endif /* !DEBUG */
/*---------------------------------------------------------------------*/
#ifdef ENABLE_BROKER
void
accept_node_name(Filter *f, char *name)
{
	TRACE_FILTER_FUNC_START();

	if (strstr(name, "}") != NULL)
		strcpy(f->node_name, name);
	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
#define parse_request(x, f)		parse_record(x, f)
void
parse_record(broker_data *v, Filter *f)
{
	TRACE_FILTER_FUNC_START();
	
	char *str = NULL;
	float duration = 0;
	broker_record *inner_r = broker_data_as_record(v);
	broker_record_iterator *inner_it = broker_record_iterator_create(inner_r);
	broker_subnet *bsub = NULL;
	broker_address *baddr = NULL;
	char str_addr[INET6_ADDRSTRLEN];
	struct in_addr addr;
	struct in6_addr addr6;
	uint8_t mask = INET_MASK;
	char *needle = NULL;
	
	TRACE_DEBUG_LOG( "Got a record\n");
	while (!broker_record_iterator_at_last(inner_r, inner_it)) {
		broker_data *inner_d = broker_record_iterator_value(inner_it);
		if (inner_d != NULL) {
			broker_data_type bdt = broker_data_which(inner_d);
			switch (bdt) {
			case broker_data_type_bool:
				TRACE_DEBUG_LOG( "Got a bool: %d\n",
						 broker_bool_true(broker_data_as_bool(inner_d)));
				break;
			case broker_data_type_count:
				TRACE_DEBUG_LOG( "Got a count: %lu\n",
						 *broker_data_as_count(inner_d));
				break;
			case broker_data_type_integer:
				TRACE_DEBUG_LOG( "Got an integer: %ld\n",
						 *broker_data_as_integer(inner_d));
				break;
			case broker_data_type_real:
				TRACE_DEBUG_LOG( "Got a real: %f\n",
						 *broker_data_as_real(inner_d));
				break;
			case broker_data_type_string:
				TRACE_DEBUG_LOG( "Got a string: %s\n",
						 broker_string_data(broker_data_as_string(inner_d)));
				accept_node_name(f, (char *)broker_string_data(broker_data_as_string(inner_d)));
				break;
			case broker_data_type_address:
				baddr = broker_data_as_address(inner_d);
				TRACE_DEBUG_LOG( "Got an address: %s\n",
						 broker_string_data(broker_address_to_string(baddr)));
				if (f->filter_type_flag == BRICKS_IP_FILTER) {
					if (broker_address_is_v4(baddr)) {
						memcpy(&f->ip4addr.addr32,
						       broker_address_bytes(baddr),
						       sizeof(uint32_t));
						f->proto = IPVERSION;
					} else if (broker_address_is_v6(baddr)) {
						memcpy(f->ip6addr.addr32,
						       broker_address_bytes(baddr),
						       sizeof(uint32_t)*4);
						f->proto = IPV6_VERSION;
					}
				} else {
					if (broker_address_is_v4(baddr)) {
						if (f->conn.sip4addr.addr32 != 0)
							memcpy(&f->conn.dip4addr.addr32,
							       broker_address_bytes(baddr),
							       sizeof(uint32_t));
						else
							memcpy(&f->conn.sip4addr.addr32,
							       broker_address_bytes(baddr),
							       sizeof(uint32_t));
						f->proto = IPVERSION;
					} else if (broker_address_is_v6(baddr)) {
						if (f->conn.sip6addr.addr32[0] != 0)
							memcpy(f->conn.dip6addr.addr32,
							       broker_address_bytes(baddr),
							       sizeof(uint32_t)*4);
						else
							memcpy(f->conn.sip6addr.addr32,
							       broker_address_bytes(baddr),
							       sizeof(uint32_t)*4);
						f->proto = IPV6_VERSION;
					}
				}
				break;
			case broker_data_type_subnet:
				bsub = broker_data_as_subnet(inner_d);
				str = (char *)broker_string_data(broker_subnet_to_string(bsub));
				TRACE_DEBUG_LOG("Got a subnet: %s\n",
						str);
				needle = strstr(str, "/");
				if (needle == NULL) {
					strcpy(str_addr, str);
				} else {
					memcpy(str_addr, str, needle-str);
					str_addr[needle-str] = (char)0;
					mask = (uint8_t)atoi(needle + 1);
					TRACE_DEBUG_LOG("Mask is: %d\n", mask);
				}
				
				if (f->proto == IPVERSION) {
					if (inet_pton(AF_INET, str_addr, &addr) == 0) {
						TRACE_DEBUG_LOG("Passed an invalid address!\n");
						break;
					}
				} else /* f->proto == IPV6_VERSION */ {
					if (inet_pton(AF_INET6, str_addr, &addr6) == 0) {
						TRACE_DEBUG_LOG("Passed an invalid address!\n");
						break;
					}
				}
				
				if (f->filter_type_flag == BRICKS_IP_FILTER) {
					if (f->proto == IPVERSION)
						memcpy(&f->ip4addr.addr32,
						       &addr, sizeof(uint32_t));
					else /* IPV6_VERSION */
						memcpy(f->ip6addr.addr32,
						       &addr6, sizeof(uint32_t)*4);
				} else {
					if (f->proto == IPVERSION) {
						if (f->conn.sip4addr.addr32 != 0) {
							memcpy(&f->conn.dip4addr.addr32,
							       &addr,
							       sizeof(uint32_t));
							TRACE_DEBUG_LOG("Setting dest ip address: %u\n",
									f->conn.dip4addr.addr32);
							f->conn.dip4addr.mask = mask;
						} else {
							memcpy(&f->conn.sip4addr.addr32,
							       &addr,
							       sizeof(uint32_t));
							TRACE_DEBUG_LOG("Setting src ip address: %u\n",
									f->conn.sip4addr.addr32);
							f->conn.sip4addr.mask = mask;
						}
					} else /* IPV6_VERSION */ {
						if (f->conn.sip6addr.addr32[0] != 0) {
							memcpy(f->conn.dip6addr.addr32,
							       &addr6,
							       sizeof(uint32_t) * 4);
							TRACE_DEBUG_LOG("Setting dest ip address: %u %u %u %u\n",
									f->conn.dip6addr.addr32[0],
									f->conn.dip6addr.addr32[1],
									f->conn.dip6addr.addr32[2],
									f->conn.dip6addr.addr32[3]);
						} else {
							memcpy(f->conn.sip6addr.addr32,
							       &addr6,
							       sizeof(uint32_t) * 4);
							TRACE_DEBUG_LOG("Setting src ip address: %u %u %u %u\n",
									f->conn.sip6addr.addr32[0],
									f->conn.sip6addr.addr32[1],
									f->conn.sip6addr.addr32[2],
									f->conn.sip6addr.addr32[3]);
						}
					}
				}
				break;
			case broker_data_type_port:
				TRACE_DEBUG_LOG( "Got a port: %u\n",
						 broker_port_number(broker_data_as_port(inner_d)));
				break;
			case broker_data_type_time:
				TRACE_DEBUG_LOG( "Got time: %f\n",
						 broker_time_point_value(broker_data_as_time(inner_d)));
				break;
			case broker_data_type_duration:
				duration = broker_time_duration_value(broker_data_as_duration(inner_d));
				TRACE_DEBUG_LOG( "Got duration: %f\n",
						 duration);
				f->filt_time_period = (time_t)duration;
				break;
			case broker_data_type_enum_value:
				str = (char *)broker_enum_value_name(broker_data_as_enum(inner_d));
				TRACE_DEBUG_LOG( "Got enum: %s\n",
						 str);
				if (!strcmp(str, "NetControl::DROP"))
					f->tgt = DROP;
				else if (!strcmp(str, "NetControl::WHITELIST"))
					f->tgt = WHITELIST;
				else if (!strcmp(str, "NetControl::FLOW"))
					f->filter_type_flag = BRICKS_FLOW_FILTER;
				else if (!strcmp(str, "NetControl::CONNECTION"))
					f->filter_type_flag = BRICKS_CONNECTION_FILTER;
				else if (!strcmp(str, "NetControl::ADDRESS"))
					f->filter_type_flag = BRICKS_IP_FILTER;
				else if (!strcmp(str, "NetControl::MAC"))
					f->filter_type_flag = BRICKS_MAC_FILTER;
				break;
			case broker_data_type_set:
				TRACE_DEBUG_LOG( "Got a set\n");
				break;
			case broker_data_type_table:
				TRACE_DEBUG_LOG( "Got a table\n");
				break;
			case broker_data_type_vector:
				TRACE_DEBUG_LOG( "Got a vector\n");
				break;
			case broker_data_type_record:
				parse_record(inner_d, f);
				break;
			default:
				break;
			}
		}
		broker_record_iterator_next(inner_r, inner_it);
	}
	broker_record_iterator_delete(inner_it);
	TRACE_DEBUG_LOG( "End of record\n");
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
void
brokerize_request(engine *eng, broker_message_queue *q)
{
	TRACE_FILTER_FUNC_START();
	broker_deque_of_message *msgs = broker_message_queue_need_pop(q);
	int n = broker_deque_of_message_size(msgs);
	int i;
	Filter f;
	FilterContext *cn = NULL;

	memset(&f, 0, sizeof(f));

	/* reset time period to -1 */
	f.filt_time_period = (time_t)-1;

	/* reseting the protocol */
	f.proto = IPVERSION;
	
	/* check vector contents */
	for (i = 0; i < n; ++i) {
		broker_vector *m = broker_deque_of_message_at(msgs, i);
		broker_vector_iterator *it = broker_vector_iterator_create(m);
		int count = RULE_ACC;
		while (!broker_vector_iterator_at_last(m, it)) {
			broker_data *v = broker_vector_iterator_value(it);
			switch (count) {
			case RULE_REQUEST:
				parse_request(v, &f);
				break;
			case RULE_ACC:
				/* currently only supports rule addition */
			default:
				if (v != NULL) {
					if (broker_data_which(v) == broker_data_type_record)
						parse_record(v, &f);
					else
						TRACE_DEBUG_LOG("Got a message: %s!\n",
								broker_string_data(broker_data_to_string(v)));
				}
				break;
			}
			broker_vector_iterator_next(m, it);			
			count++;
		}
		broker_vector_iterator_delete(it);
	}
	
	broker_deque_of_message_delete(msgs);	

	if (f.node_name != NULL) {
		/* first locate the right commnode entry */
		TAILQ_FOREACH(cn, &eng->filter_list, entry) {
			if (!strcmp((char *)cn->name, (char *)f.node_name)) {
				/* apply the filter */
				apply_filter(cn, &f);
				break;
			} else {
				TRACE_LOG("ifname: %s does not match\n", cn->name);
			}
		}
	}	
#ifdef DEBUG
	printFilter(&f);
#endif
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
/*
 * Code self-explanatory...
 */
static void __unused
create_listening_socket_for_eng(engine *eng)
{
	TRACE_FILTER_FUNC_START();

	/* socket info about the listen sock */
	struct sockaddr_in serv; 
 
	/* zero the struct before filling the fields */
	memset(&serv, 0, sizeof(serv));
	/* set the type of connection to TCP/IP */           
	serv.sin_family = AF_INET;
	/* set the address to any interface */                
	serv.sin_addr.s_addr = htonl(INADDR_ANY); 
	/* set the server port number */    
	serv.sin_port = htons(PKTENGINE_LISTEN_PORT + eng->esrc[0]->dev_fd);
 
	eng->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (eng->listen_fd == -1) {
		TRACE_ERR("Failed to create listening socket for engine %s\n",
			  eng->name);
		TRACE_FILTER_FUNC_END();
	}
	
	/* bind serv information to mysocket */
	if (bind(eng->listen_fd, (struct sockaddr *)&serv, sizeof(struct sockaddr)) == -1) {
	  TRACE_ERR("Failed to bind listening socket to port %d of engine %s\n",
		    PKTENGINE_LISTEN_PORT + eng->esrc[0]->dev_fd, eng->name);
	  TRACE_FILTER_FUNC_END();
	}
	
	/* start listening, allowing a queue of up to 1 pending connection */
	if (listen(eng->listen_fd, LISTEN_BACKLOG) == -1) {
		TRACE_ERR("Failed to start listen on port %d (engine %s)\n",
			  PKTENGINE_LISTEN_PORT + eng->esrc[0]->dev_fd, eng->name);
		TRACE_FILTER_FUNC_END();
	}
	eng->listen_port = PKTENGINE_LISTEN_PORT + eng->esrc[0]->dev_fd;
	TRACE_LOG("Engine listening on port: %u\n", eng->listen_port);
	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
/**
 * Accepts connection for connecting to the backend...
 */
static int __unused
accept_request_backend(engine *eng, int sock)
{
	TRACE_FILTER_FUNC_START();
	int clientsock;
	struct sockaddr_in client;
	socklen_t c;

	if (sock == eng->listen_fd) {
		/* accept connection from an incoming client */
		clientsock = accept(eng->listen_fd,
				    (struct sockaddr *)&client, &c);
		if (clientsock < 0) {
			TRACE_LOG("accept failed: %s\n", strerror(errno));
			TRACE_FILTER_FUNC_END();
			return -1;
		}
		TRACE_FILTER_FUNC_END();
		return clientsock;
	}
	
	TRACE_FILTER_FUNC_END();
	return -1;
}
/*---------------------------------------------------------------------*/
/**
 * Services incoming request from userland applications and takes
 * necessary actions. The actions can be: (i) passing packets to 
 * userland apps etc.
 */
static int __unused
process_request_backend(int sock, engine *eng)
{
	TRACE_FILTER_FUNC_START();
	char buf[LINE_MAX];
	ssize_t read_size;
	req_block *rb;
	
	if ((read_size = read(sock, buf, sizeof(buf))) <
	    (ssize_t)(sizeof(req_block))) {
		TRACE_DEBUG_LOG("Request block malformed! (size: %d),"
			  " request block should be of size: %d\n",
			  (int)read_size, (int)sizeof(req_block));
		return -1;
	} else {
		rb = (req_block *)buf;
		TRACE_DEBUG_LOG("Total bytes read from client socket: %d\n", 
			  (int)read_size);
		
		install_filter(rb, eng);
		/* parse new rule */
		TRACE_DEBUG_LOG("Got a new rule\n");
		TRACE_FLUSH();
		
		return 0;
	}	
	TRACE_FILTER_FUNC_END();
	return -1;
}
/*---------------------------------------------------------------------*/
void
initialize_filt_comm(engine *eng)
{
	TRACE_FILTER_FUNC_START();
	broker_init(0);
	broker_endpoint *node = broker_endpoint_create(BROKER_NODE);
	broker_string *topic = broker_string_create(BROKER_TOPIC);
	broker_message_queue *pq = broker_message_queue_create(topic, node);

	/* only works for a single-threaded process */
	if (!broker_endpoint_listen(node, BROKER_PORT, BROKER_HOST, 1)) {
		TRACE_ERR("%s\n", broker_endpoint_last_error(node));
		TRACE_FILTER_FUNC_END();
		return;
	}

	/* 
	 * finally assigned the pq message ptr,
	 * topic ptr & node ptr respectively 
	 */
	eng->pq_ptr = (void *)pq;
	eng->topic_ptr = (void *)topic;
	eng->node_ptr = (void *)node;
	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
void
terminate_filt_comm(engine *eng)
{
	TRACE_FILTER_FUNC_START();
	broker_message_queue *pq = (broker_message_queue *)eng->pq_ptr;
	broker_string *topic = (broker_string *)eng->topic_ptr;
	broker_endpoint *node = (broker_endpoint *)eng->node_ptr;

	if (pq != NULL && topic != NULL && node != NULL) {
		broker_string_delete(topic);
		/* broker_message_queue_delete(pq); */
		broker_endpoint_delete(node);
		eng->pq_ptr = NULL;
		eng->topic_ptr = NULL;
		eng->node_ptr = NULL;
		broker_done();
	}
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
int
fetch_filter_comm_fd(engine *eng)
{
	TRACE_FILTER_FUNC_START();
	return broker_message_queue_fd((broker_message_queue *)eng->pq_ptr);
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
void
process_broker_request(engine *eng, struct pollfd *pfd, int i)
{
	TRACE_FILTER_FUNC_START();
	broker_message_queue *pq = (broker_message_queue *)eng->pq_ptr;
	if (pfd[i].fd == broker_message_queue_fd(pq) &&
	    (!(pfd[i].revents & (POLLERR |
				 POLLHUP |
				 POLLNVAL)) &&
	     (pfd[i].revents & POLLIN))) {
		brokerize_request(eng, pq);
	}	
	TRACE_FILTER_FUNC_END();
}
/*---------------------------------------------------------------------*/
#endif
