#include <iostream>

#include "Runloop.h"

int main(int argc, char* argv[])
{
    try {
        ::Runloop runloop{argc, argv};
        runloop.Run();
    } catch (const std::exception &error) {
        std::cout << error.what() << std::endl;
    }

    return 0;
}
