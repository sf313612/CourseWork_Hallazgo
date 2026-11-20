#include "crow.h"
#include "routes.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const std::string dataset_directory =
        argc > 1 ? argv[1] : "../dataset/pg19";

    const std::string metadata_path =
        argc > 2 ? argv[2] : dataset_directory + "/metadata.csv";

    ApiState state(dataset_directory);

    if (!state.metadata.load(metadata_path)) {
        std::cerr << "Warning: metadata could not be loaded\n";
    }

    crow::SimpleApp app;
    registerRoutes(app, state);

    app.port(18080).multithreaded().run();
}