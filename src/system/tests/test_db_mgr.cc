#include "database_manager.h"

#include <iostream>

int main(void)
{
    DatabaseManager mgr;
    int res = 0;

    res = mgr.connect("localhost", "root", "");
    mgr.get_map_data();
    std::cout << mgr.get_raw_data().size() << std::endl;
    std::cout << mgr.get_deduped_data().size() << std::endl;

    return 0;
}
