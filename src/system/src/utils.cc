extern "C" {
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

}

#include <cmath>
#include <cstdio>

#include "log.h"
#include "utils.h"

#define check_string(str) do \
    if (string == NULL) \
        error("load config file error"); \
while (0)

int
load_config(const char *file, struct Config *config)
{
    GKeyFile *key_file;
    unsigned long value;
    char *string;

    key_file = g_key_file_new();
    
    if (!g_key_file_load_from_file(key_file, file, G_KEY_FILE_NONE, NULL)) {
        error("Can't load config file %s\n", file);
        return -1;
    }

    string = g_key_file_get_string(key_file, "protocol", "local_addr", NULL);
    check_string(string);
    config->local_addr = string;

    string = g_key_file_get_string(key_file, "protocol", "bcast_addr", NULL);
    check_string(string);
    config->bcast_addr = string;

    value = g_key_file_get_integer(key_file, "protocol", "listen_port", NULL);
    config->listen_port = value;

    config->beacon_period = g_key_file_get_integer(key_file, "protocol", "beacon_period", NULL);

    config->entry_expire_period = g_key_file_get_integer(key_file, "protocol", "entry_expire_period", NULL);

    config->list_cleaner_period = g_key_file_get_integer(key_file, "protocol", "list_cleaner_period", NULL);

    config->enable_device = g_key_file_get_integer(key_file, "log", "enable_device", NULL);

    config->enable_track = g_key_file_get_integer(key_file, "log", "enable_track", NULL);

    string = g_key_file_get_string(key_file, "log", "track_file", NULL);
    check_string(string);
    config->track_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    string = g_key_file_get_string(key_file, "log", "bdcast_file", NULL);
    check_string(string);
    config->bdcast_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    string = g_key_file_get_string(key_file, "log", "recv_file", NULL);
    check_string(string);
    config->recv_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    string = g_key_file_get_string(key_file, "log", "node_file", NULL);
    check_string(string);
    config->node_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    string = g_key_file_get_string(key_file, "log", "neigh_file", NULL);
    check_string(string);
    config->neigh_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    string = g_key_file_get_string(key_file, "log", "forward_file", NULL);
    check_string(string);
    config->forward_file = g_strconcat(LOGDIR_PATH, "/", string, NULL);
    g_free(string);

    config->enable_record = g_key_file_get_integer(key_file, "log", "enable_record", NULL);

    string = g_key_file_get_string(key_file, "log", "record_file", NULL);
    check_string(string);
    config->record_file = string;

    return 0;
}

void
output_config(struct Config* config)
{
    message("\nGeosvr config list:\n"
            "\tlocal addr: %s\n"
            "\tbcast addr: %s\n"
            "\tlisten port: %d\n"
            "\tbeacon period: %d\n"
            "\tneigh entry expire period: %d\n"
            "\tneigh list cleaner period: %d\n"
            "\t%s\n\t%s (track file: %s record file: %s)\n"
            "\tbroadcast log: %s\n"
            "\tnode log: %s\n"
            "\tneighbour log: %s\n"
            "\trecv log: %s\n"
            "\tforward log: %s\n", config->local_addr,
            config->bcast_addr, config->listen_port, config->beacon_period,
            config->entry_expire_period, config->list_cleaner_period,
            (config->enable_device == 1 ? "enable gps device" : "disable gps device"),
            (config->enable_track == 1 ? "enable gps track" : "disable gps track"),
            config->track_file, config->record_file,
            config->bdcast_file, config->node_file, config->neigh_file,
            config->recv_file, config->forward_file);
}

void
now(char *buf, size_t n)
{
    time_t now;
    struct tm *tmp;

    if (buf == NULL)
        return;

    now = time(NULL);
    tmp = localtime(&now);
    if (strftime(buf, n, "%a, %d %b %Y %T", tmp) == 0) {
        buf[0] = '\0';
        return ;
    }
}
