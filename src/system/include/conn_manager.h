#ifndef CONN_MANAGER_H_
#define CONN_MANAGER_H_

extern "C" {
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <netinet/in.h>

#include <event.h>
}

#include <cstdio>

struct nlsock {
	int sock;
	int seq;
	struct sockaddr_nl local;
};

struct Config;

typedef void (*RecvCallback)(char* data, size_t len, void* arg);

class ConnManager {
    public:
        ConnManager(Config *config);
        ~ConnManager();

        int send_data(void* data, size_t len);
        void set_recv_callback(RecvCallback func, void* arg);
        void set_req_route_normal_callback(RecvCallback func, void* arg);

        struct in_addr get_local_addr() {
            return local_addr_.sin_addr;
        }

        // netlink communication
        int nl_send_msg(void* data, size_t len, u_int16_t type);
        void nl_recv_msg(int fd, short ev, void* arg);

    private:
        int nl_send(struct nlsock* nl, struct nlmsghdr* n);
        int socket_in_;
        int socket_out_;
        struct sockaddr_in local_addr_;

        struct nlsock geosvrnl_;
        struct sockaddr_nl peer_;

        struct event ev_recv_;
        struct event ev_nl_recv_;

        RecvCallback recv_cb_;
        void* recv_data_;

        RecvCallback req_route_normal_cb_;
        void* req_route_normal_data_;

        static void default_recv_cb(char* data, size_t len, void* arg);

        static void receive(int fd, short ev, void* arg);
        static void recv_nl_msg(int fd, short ev, void* arg);

        FILE* bdcast_file_;
};

#endif // CONN_MANAGER_H_
