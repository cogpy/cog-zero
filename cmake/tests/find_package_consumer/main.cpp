// Minimal link check against the installed standalone SDK (Cog0::cog0lib).
#include "cog0/AtomStore.h"
#include "cog0/Logger.h"

#include <iostream>

int main() {
    cog0::Logger::instance().info("cog0 find_package consumer OK");
    cog0::AtomStore store;
    (void)store;
    std::cout << "ok\n";
    return 0;
}
