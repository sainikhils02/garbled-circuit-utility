#include "common.h"
#include <iostream>

/**
 * Main entry point - determines whether to run as garbler or evaluator
 * based on compile-time definitions
 */

// Forward declarations
int garbler_main(int argc, char* argv[]);
int evaluator_main(int argc, char* argv[]);

int main(int argc, char* argv[]) {
#ifdef GARBLER_MODE
    return garbler_main(argc, argv);
#elif defined(EVALUATOR_MODE)
    return evaluator_main(argc, argv);
#else
    std::cerr << "Error: Neither GARBLER_MODE nor EVALUATOR_MODE is defined!" << std::endl;
    std::cerr << "Please build the specific targets: 'garbler' or 'evaluator'" << std::endl;
    return 1;
#endif
}

// These functions are implemented in garbler.cpp and evaluator.cpp
int garbler_main(int argc, char* argv[]) {
    std::cerr << "Garbler main not linked properly!" << std::endl;
    return 1;
}

int evaluator_main(int argc, char* argv[]) {
    std::cerr << "Evaluator main not linked properly!" << std::endl;
    return 1;
}
