#include "filter.h"

/**
** This are the rules to filter our requst. the first rule that match the packet will be activated. Keep the in mind
** also, when adding aditional rules keep in mind that anything that come after "default" wont take place.
** rule struct - { name, direction, src_ip, dst_ip, protocol, action } !
************************************
** important - DONT add any rule name "default". if you do, then all the rules after won't be checked :( 
************************************
**/
struct rule rules[] ={
	{"default", VALUE_ANY, VALUE_ANY, VALUE_ANY, VALUE_ANY, VALUE_ANY, ACTION_DENAY}
};


//**************************************************************************
//			function implementions 
//**************************************************************************

static char *get_protocol_name(int num)
{
	switch (num){
	case ICMP :
		return "ICMP";
	case IGMP :
		return "IGMP";
	case TCP  :
		return "TCP";

	case UDP  :
		return "UDP";

	case ENCAP:
		return "ENCAP";

	case OSPF :
		return "OSPP";

	case SCTP :
		return "SCTP";
	}
	return "";

}
/*
in ip_cmp we can decide how much sepcific we want to be with the ip. 
*/
static int ip_cmp(int rule_num, uint32_t head_ip,uint32_t rule_ip)
{
	switch (rules[rule_num].range){
	case RANGE_ALL :
		return 1;
	case RANGE_ONE :
		if ( ip4_addr1(head_ip) == ip4_addr1(rule_ip) )
			return 1;
		break;
	case RANGE_TWO :
		if ( ip4_addr1(head_ip) == ip4_addr1(rule_ip) && ip4_addr2(head_ip) == ip4_addr2(rule_ip) )
			return 1;
		break;
	case RANGE_THREE :
		if ( ip4_addr1(head_ip) == ip4_addr1(rule_ip) && ip4_addr2(head_ip) == ip4_addr2(rule_ip) && ip4_addr3(head_ip) == ip4_addr3(rule_ip) )
			return 1;
		break;
	case RANGE_FOUR :
		if ( ip4_addr1(head_ip) == ip4_addr1(rule_ip) && ip4_addr2(head_ip) == ip4_addr2(rule_ip) &&
								 ip4_addr3(head_ip) == ip4_addr3(rule_ip) && ip4_addr4(head_ip) == ip4_addr4(rule_ip) )
			return 1;
		break;	
	}
	return 0;
}

/*
* take a rule, and check if it fits the given ipheader. if so- return the action
*/
int check_rule(int rule_num, uint32_t src_ip, uint32_t dst_ip, int protocol)
{
	if (!ip_cmp(rules[rule_num].range, src_ip, rules[rule_num].src_ip))
		return -1;
	if ( rules[rule_num].protocol != protocol)
		return -1;
	if (!ip_cmp(rules[rule_num].range, dst_ip, rules[rule_num].dst_ip))
		return -1;
	return rules[rule_num].action;
	
}

int check_packet(uint32_t src_ip, uint32_t dst_ip, int protocol)
{
	int i = 0;
	int res;
	while (strcmp(rules[i].name,"default")){
		if ( (res = check_rule(uint32_t src_ip, uint32_t dst_ip, int protocol)) >= 0 ){ // rule match !
			if (res == ACTION_DEN_WARN){
				cprintf("FILTER: WARNING, unapproved packet src ip - %d.%d.%d.%d dst ip - %d.%d.%d.%d request for protocol %s !\n",
					ip4_addr1(src_ip),ip4_addr2(src_ip),ip4_addr3(src_ip),ip4_addr4(src_ip),
					ip4_addr1(dst_ip),ip4_addr2(dst_ip),ip4_addr3(dst_ip),ip4_addr4(dst_ip),
					!strcmp(get_protocol_name(protocol),"") ? "UNKNOWN PROTOCOL" : get_protocol_name(protocol));
				return 0;
			}
			return res;
		}
		i++;
	}
	res = rules[i].action;
	if (res == ACTION_DEN_WARN){
		cprintf("FILTER: WARNING, unapproved packet src ip - %d.%d.%d.%d dst ip - %d.%d.%d.%d request for protocol %s !\n",
			ip4_addr1(src_ip),ip4_addr2(src_ip),ip4_addr3(src_ip),ip4_addr4(src_ip),
			ip4_addr1(dst_ip),ip4_addr2(dst_ip),ip4_addr3(dst_ip),ip4_addr4(dst_ip),
			!strcmp(get_protocol_name(protocol),"") ? "UNKNOWN PROTOCOL" : get_protocol_name(protocol));
		return 0;
	}
	return res;
}


