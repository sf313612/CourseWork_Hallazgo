#include <iostream>
#include <string>

#include <httplib.h>
#include <crow.h>

int main(int argc, char* argv[]) {
    std::string query;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) query += " ";
            query += argv[i];
        }
    } else {
        std::cout << "Enter search query: ";
        std::getline(std::cin, query);
    }

    if (query.empty()) {
        std::cerr << "Error: query cannot be empty.\n";
        return 1;
    }

    httplib::Client client("127.0.0.1", 18080);
    client.set_connection_timeout(5);
    client.set_read_timeout(60);

    crow::json::wvalue request;
    request["query"] = query;
    request["mode"] = "keyword";

    std::cout << "\nSearching for: " << query << "\n";

    auto response = client.Post(
        "/api/search",
        request.dump(),
        "application/json"
    );

    if (!response) {
        std::cerr << "Error: could not connect to Hallazgo API.\n";
        return 1;
    }

    if (response->status != 200) {
        std::cerr << "Search failed (HTTP "
                  << response->status << "):\n"
                  << response->body << "\n";
        return 1;
    }

    auto result = crow::json::load(response->body);

    if (!result || !result.has("results")) {
        std::cerr << "Error: invalid API response.\n";
        return 1;
    }

    const auto& results = result["results"];

    std::cout << "\nFound " << results.size()
              << " results:\n\n";

    int number = 1;

    for (const auto& item : results) {
        std::string title = item["title"].s();
        std::string document_id = item["document_id"].s();

        std::cout << number++ << ". " << title << "\n";
        std::cout << "   Document ID: " << document_id << "\n";
        std::cout << "   Score: "
            << crow::json::wvalue(item["score"]).dump()
            << "\n\n";
    }

    if (result.has("query_expansion_used")) {
        std::cout << "Query expansion: "
                  << (result["query_expansion_used"].b()
                      ? "used" : "not used")
                  << "\n";
    }

    return 0;
}
