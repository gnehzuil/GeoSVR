#ifndef GEOSVR_PACKET_H_
#define GEOSVR_PACKET_H_

struct geosvr_node_packet {
    u_int32_t taddr;
    u_int32_t saddr;
    double    latitude;
    double    longitude;
    double    track;
    double    speed;
    u_int32_t direct : 1;
    u_int32_t seqno : 31;
};

#endif // GEOSVR_PACKET_H_
