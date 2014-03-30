#ifndef GEOSVR_MOD_H
#define GEOSVR_MOD_H

#include <linux/inetdevice.h>

struct if_info {
    struct in_addr if_addr;
    struct in_addr bc_addr;
    struct net_device *dev;
};

extern struct if_info *ifip;

static inline int
if_info_add(struct net_device *dev)
{
    struct if_info *ifi;
    struct in_device *indev;

    ifi = (struct if_info *)kmalloc(sizeof(struct if_info), GFP_ATOMIC);
    if (ifi == NULL)
        return -1;

    ifi->dev = dev;
    dev_hold(dev);
    indev = in_dev_get(dev);
    if (indev) {
        struct in_ifaddr **ifap;
        struct in_ifaddr *ifa;

        for (ifap = &indev->ifa_list; (ifa = *ifap) != NULL;
             ifap = &ifa->ifa_next)
            if (strcmp(dev->name, ifa->ifa_label) == 0)
                break;

        if (ifa) {
            ifi->if_addr.s_addr = ifa->ifa_address;
            ifi->bc_addr.s_addr = ifa->ifa_broadcast;
        }

        in_dev_put(indev);
    }

    ifip = ifi;

    return 0;
}

static inline void
if_info_purge(void)
{
    dev_put(ifip->dev);
    kfree(ifip);
}

#endif /* GEOSVR_MOD_H */
