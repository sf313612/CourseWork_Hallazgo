
#include "crow.h"
#include "routes.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

crow::response serveFile(
    const std::string& path,
    const std::string& content_type
) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        crow::response response;
        response.code = 404;
        response.body = "File not found: " + path;
        return response;
    }

    std::ostringstream content;
    content << file.rdbuf();

    crow::response response;
    response.code = 200;
    response.set_header("Content-Type", content_type);
    response.body = content.str();

    return response;
}

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

    // Web UI
    CROW_ROUTE(app, "/")([] {
        return serveFile(
            "clients/web/index.html",
            "text/html; charset=utf-8"
        );
    });

    CROW_ROUTE(app, "/style.css")([] {
        return serveFile(
            "clients/web/style.css",
            "text/css; charset=utf-8"
        );
    });

    CROW_ROUTE(app, "/app.js")([] {
        return serveFile(
            "clients/web/app.js",
            "application/javascript; charset=utf-8"
        );
    });

    CROW_ROUTE(app, "/assets/hero-library.jpg")([] {
        return serveFile(
            "clients/web/assets/hero-library.jpg",
            "image/jpeg"
        );
    });

    // REST API and WebSocket routes
    registerRoutes(app, state);

    app.port(18080).multithreaded().run();
}
