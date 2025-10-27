#include "Worker.h"
#include <iostream>

int main(int argc, char** argv) {
    std::string coord = "http://127.0.0.1:8080";
    std::string name = "worker-1";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--coordinator" && i + 1 < argc) coord = argv[++i];
        else if (a == "--name" && i + 1 < argc) name = argv[++i];
    }

    Worker w(coord, name);
    w.runLoop();
    return 0;
}
