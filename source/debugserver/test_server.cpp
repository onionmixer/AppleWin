/*
 * Simple test program for HttpServer
 * Compile and run to verify the HTTP server implementation works correctly.
 *
 * Build (standalone):
 *   g++ -std=c++17 -o test_server test_server.cpp HttpRequest.cpp HttpResponse.cpp HttpServer.cpp -lpthread
 *
 * Run:
 *   ./test_server
 *
 * Test:
 *   curl http://localhost:8080/
 *   curl http://localhost:8080/test?param=value
 *   curl http://localhost:8080/json
 */

#include "HttpServer.h"
#include <iostream>
#include <csignal>

static debugserver::HttpServer* g_server = nullptr;

void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nShutting down server..." << std::endl;
        g_server->Stop();
    }
}

int main() {
    // Create server on port 8080
    debugserver::HttpServer server(8080, "127.0.0.1");
    g_server = &server;

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Set request handler
    server.SetHandler([](const debugserver::HttpRequest& request,
                          debugserver::HttpResponse& response) {
        std::string path = request.GetPath();

        std::cout << "Request: " << request.GetMethod() << " " << path << std::endl;

        if (path == "/" || path == "/index.html") {
            // Serve HTML page
            std::string html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>AppleWin Debug Server Test</title>
    <style>
        body { font-family: monospace; background: #1e1e2e; color: #cdd6f4; padding: 20px; }
        h1 { color: #89b4fa; }
        a { color: #a6e3a1; }
        .info { background: #313244; padding: 10px; margin: 10px 0; border-radius: 5px; }
    </style>
</head>
<body>
    <h1>AppleWin Debug Server - Test Page</h1>
    <div class="info">
        <p>Server is running successfully!</p>
        <p>Test endpoints:</p>
        <ul>
            <li><a href="/">/</a> - This page</li>
            <li><a href="/json">/json</a> - JSON response</li>
            <li><a href="/test?param=value">/test?param=value</a> - Query parameter test</li>
        </ul>
    </div>
</body>
</html>
)HTML";
            response.SendHTML(html);

        } else if (path == "/json") {
            // Serve JSON response
            std::string json = R"({
    "status": "ok",
    "server": "AppleWin Debug Server",
    "version": "1.0",
    "message": "JSON endpoint working"
})";
            response.SendJSON(json);

        } else if (path == "/test") {
            // Test query parameters
            std::string param = request.GetQueryParam("param", "(not set)");
            std::string html = "<html><body><h1>Query Parameter Test</h1>";
            html += "<p>param = " + param + "</p>";
            html += "<p>All query params:</p><ul>";
            for (const auto& kv : request.GetQueryParams()) {
                html += "<li>" + kv.first + " = " + kv.second + "</li>";
            }
            html += "</ul></body></html>";
            response.SendHTML(html);

        } else {
            // 404 Not Found
            response.SendError(404, "The requested resource was not found.");
        }
    });

    // Start server
    if (!server.Start()) {
        std::cerr << "Failed to start server: " << server.GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Server started on http://" << server.GetBindAddress()
              << ":" << server.GetPort() << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    // Wait for server to stop
    while (server.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Server stopped." << std::endl;
    return 0;
}
