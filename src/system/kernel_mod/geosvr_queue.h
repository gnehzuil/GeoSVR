#ifndef GEOSVR_QUEUE_H
#define GEOSVR_QUEUE_H

#define GEOSVR_QUEUE_DROP 1
#define GEOSVR_QUEUE_SEND 2

struct sk_buff;
struct geosvr_nlmsg;

int geosvr_queue_find(__u32 daddr);
int geosvr_queue_enqueue_packet(struct sk_buff *skb,
							int (*okfn)(struct sk_buff *));
int geosvr_queue_send_pkt(struct geosvr_nlmsg *msg);
int geosvr_queue_drop_pkt(__u32 daddr);
void geosvr_queue_flush(void);
int geosvr_queue_init(void);
void geosvr_queue_fini(void);

#endif /* GEOSVR_QUEUE_H */
