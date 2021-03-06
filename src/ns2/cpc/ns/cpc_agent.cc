#include <common/packet.h>
#include <trace/cmu-trace.h>

#include "cpc_agent.h"
#include "cpc_queue_ns.h"
#include "../cpc_prot.h"
/*#include "../cpc_queue.h"*/
#include "../cpc_rt.h"

CpcAgent::CpcAgent(packet_t type)
			: Agent(type)
{
	cpc_init_ns(this);
	cpc_rt_init();
	cpc_queue_ns_init(this);
}

CpcAgent::~CpcAgent()
{
	cpc_des_ns();
	cpc_rt_des();
	cpc_queue_ns_des();
}

void CpcAgent::recv(Packet *p, Handler *h)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct in_addr src, dst;

	src.s_addr = ih->saddr();
	dst.s_addr = ih->daddr();

#if 0 
    if (ih->daddr() != -1)
        fprintf(stderr, "!!!!!! src %d dst %d\n", ih->saddr(), ih->daddr());
#endif

	if (src.s_addr == myAddr_.s_addr) {
		if (ch->num_forwards() > 0) {
			drop(p, DROP_RTR_ROUTE_LOOP);
			/*goto end;*/
			return;
		}
	}

	if (ch->ptype() == type_) {
		protHandler_((char*)access_cb(p), (int*)0, this);
        Packet::free(p);
    } else
		processPacket(p);

/*end:*/
	/*scheduleNextEvent();*/
}

void CpcAgent::processPacket(Packet *p)
{
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_cmn *ch = HDR_CMN(p);
	struct in_addr src, dst;
	struct in_addr *next;

	src.s_addr = ih->saddr();
	dst.s_addr = ih->daddr();

	/*if (ch->direction() == hdr_cmn::UP &&*/
		/*dst.s_addr == myAddr_.s_addr) {*/
	if (memcmp(&dst, &myAddr_, sizeof(struct in_addr)) == 0) {
		/*target_->recv(p, (Handler *)0);*/
		portDmux_->recv(p, (Handler *)0);
		return;
	}

	next = cpc_rt_find(dst);

	if (next != NULL) {
		ch->direction() = hdr_cmn::DOWN;
		ch->addr_type() = NS_AF_INET;
		ch->last_hop_ = myAddr_.s_addr;
		ch->next_hop_ = next->s_addr;
		send(p, 0);
	} else {
		cpc_queue_ns_add(p, dst);
        if (src.s_addr != myAddr_.s_addr)
            reqHandler_((char *)access_cb(p), src, dst, 0, this);
        else
            reqHandler_(NULL, src, dst, 0, this);
	}
}

void CpcAgent::dropPacket(Packet *p, const char *msg)
{
    drop(p, msg);
}

void CpcAgent::dropPacket(struct in_addr dst, const char *msg)
{
	Packet *p = cpc_queue_ns_get(dst);
    drop(p, msg);
}

void CpcAgent::sendData(Packet* p, struct in_addr dst)
{
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_cmn *ch = HDR_CMN(p);
	struct in_addr *next;

	next = cpc_rt_find(dst);

	if (next == NULL)
		return;

	ch->direction() = hdr_cmn::DOWN;
	ch->addr_type() = NS_AF_INET;
	ch->last_hop_ = myAddr_.s_addr;
	ch->next_hop_ = next->s_addr;
	send(p, 0);
}

void
CpcAgent::sendProtPacket(char *p, int n, struct in_addr dst)
{
	Packet *packet = allocpkt();
	struct hdr_ip *ih = HDR_IP(packet);
	struct hdr_cmn *ch = HDR_CMN(packet);

	ih->saddr() = myAddr_.s_addr;
	ih->daddr() = dst.s_addr;

	ch->direction() = hdr_cmn::DOWN;
	ch->addr_type() = NS_AF_INET;
	ch->last_hop_ = myAddr_.s_addr;
	ch->next_hop_ = dst.s_addr;

    memcpy(access_cb(packet), p, n);
	send(packet, 0);
}

#include <msvr/msvr_packet.h>

void
CpcAgent::sendProtPacketWithData(char *p, int n, struct in_addr src, struct in_addr dst, struct in_addr next)
{
	Packet* opkt, *npkt;

    /*npkt = allocpkt();*/
	opkt = cpc_queue_ns_get(dst);
    npkt = opkt->copy();

	struct hdr_ip *ih = HDR_IP(npkt);
	struct hdr_cmn *ch = HDR_CMN(npkt);
    struct hdr_msvr_routing *rh = HDR_MSVR_ROUTING(npkt);

	ih->saddr() = src.s_addr;
	ih->daddr() = dst.s_addr;

	ch->direction() = hdr_cmn::DOWN;
	ch->addr_type() = NS_AF_INET;
	ch->last_hop_ = myAddr_.s_addr;
	ch->next_hop_ = next.s_addr;

    struct hdr_ip *oih = HDR_IP(opkt);
    /*fprintf(stderr, "!!!! next %d opkt(src %d dst %d) npkt(src %d dst %d) para_dst %d\n", next, oih->saddr(), oih->daddr(), ih->saddr(), ih->daddr(), dst);*/

	memcpy(access_cb(npkt), p, n);
    Packet::free(opkt);
    cpc_queue_ns_remove(dst);
    /*fprintf(stderr, "!!!! next %d opkt(src %d dst %d) npkt(src %d dst %d) para_dst %d\n", next, oih->saddr(), oih->daddr(), ih->saddr(), ih->daddr(), dst);*/
	send(npkt, 0);
#if 0
    Packet *pkt;

    pkt = cpc_queue_ns_get(dst);

    struct hdr_ip *ih = HDR_IP(pkt);
    struct hdr_cmn *ch = HDR_CMN(pkt);

	ch->direction() = hdr_cmn::DOWN;
	ch->addr_type() = NS_AF_INET;
	ch->last_hop_ = myAddr_.s_addr;
	ch->next_hop_ = next.s_addr;
    fprintf(stderr, "!!!! next %d\n", next);

	memcpy(access_cb(pkt), p, n);
    cpc_queue_ns_remove(dst);
	send(pkt, 0);
#endif
}

int CpcAgent::command(int argc, const char * const *argv)
{
	TclObject *obj;

	if (argc == 3) {

		if (strcasecmp(argv[1], "addr") == 0) {
			myAddr_.s_addr = Address::instance().str2addr(argv[2]);
			return TCL_OK;
		}
	}

    /* Unknown commands are passed to the Agent base class */
    return Agent::command(argc, argv);
}
