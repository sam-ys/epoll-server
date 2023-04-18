/* run.cpp -- v1.1 -- main program run loop, handles control requests through http
   Author: Sam Y. 2023 */

#include "http/httplib.hpp"

#include "echo.hpp"

/*! Main program run loop
 */
void run(int ctrlPanelPort, echo_worker& serverWorker, int serverPort)
{
    // Bind control panel server backends...
    httplib::Server http;

    http.Get("/", [&](const httplib::Request&, httplib::Response& res) {

        std::string response =
            "<html>"
            "<head><style>body { font-family:monospace; font-size: 12px; }</style></head>"
            "<body>"
            "<div>[<a href=\"/set/start\">START</a>]   [<a href=\"/set/stop\">STOP</a>]</div>"
            "<hr style='border: none; border-top: dashed black 1px' />";

        if (!serverWorker.is_running())
        {
            response += "<div>There is no running server instance</div>";
        }

        else
        {
            response += "<div>Server running (port: "
                + std::to_string(serverPort) + ")"
                + "</div>"
                "<hr style='border: none; border-top: dashed black 1px' />";

            response += "<div>Connected clients: "
                + std::to_string(serverWorker.get_active_client_count())
                + "</div>";
        }

        response += "</body></html>";
        res.set_content(response, "text/html");
    });

    http.Get("/set/start", [&](const httplib::Request&, httplib::Response& res) {

        std::string response = "<html><body>"
            "<head><style>body { font-family:monospace; font-size: 12px; }</style></head>";

        if (serverWorker.run())
        {
            response += "<div>Server started on port " + std::to_string(serverPort)
                + "</div>";
        }

        else
        {
            response += "<div>Server already started (port " + std::to_string(serverPort) + ")"
                + "</div>";
        }

        response += "</body></html>";
        res.set_content(response, "text/html");
    });

    http.Get("/set/stop", [&](const httplib::Request&, httplib::Response& res) {

        std::string response = "<html><body>"
            "<head><style>body { font-family:monospace; font-size: 12px; }</style></head>";

        if (serverWorker.stop())
        {
            response += "<div>Server stopped (former port: " + std::to_string(serverPort) + ")</div>";
        }

        else
        {
            response += "<div>Server already stopped</div>";
        }

        response += "</body></html>";
        res.set_content(response, "text/html");
    });

    // Print info
    ::fprintf(stdout, "> The control panel can be accessed through local port %d\n\n\n", ctrlPanelPort);

    // Enter run loop...
    if (!http.listen("0.0.0.0", ctrlPanelPort))
    {
        ::fprintf(stderr, "! Terminating program: Cannot initialize control panel, error binding to local port%d\n\n\n",
                  ctrlPanelPort);
    }
}
