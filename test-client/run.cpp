/* run.cpp -- v1.0 -- main program run loop, handles control requests through http
   Author: Sam Y. 2023 */

#include <pthread.h>

#include "http/httplib.hpp"
#include "run.hpp"
#include "work.hpp"

namespace {
    /*! Helper
     *! Generates worker data
     */
    inline std::vector<std::shared_ptr<work> >
    load_workers(const char* ipAddr,
                 int         port,
                 int         workerCount,
                 const char* message)
    {
        int ipAddrLen = ::strlen(ipAddr);
        int messageLen = ::strlen(message);

        // Data container
        std::vector<std::shared_ptr<work> > data;

        // Start all clients...
        for (int i = 0; i != workerCount; ++i)
        {
            std::shared_ptr<work> w(new work(ipAddr,
                                             ipAddrLen,
                                             port,
                                             message,
                                             messageLen,
                                             i));
            /* Start
             */
            pthread_t pthreadID;
            if (pthread_create(&pthreadID, 0, worker, w.get()) != 0)
                break; // Uh oh
            w->pthreadID = pthreadID;
            start(*w);
            data.push_back(w);
        }

        /* Return list of successfully-started threads
         */
        return data;
    }
}

/*! Main program run loop
 */
void run(const char* defaultIPAddr,
         int         defaultRemotePort,
         int         workerCount,
         const char* message,
         int         ctrlPanelPort)
{
    char ipAddr[256] = {};
    if (defaultIPAddr != nullptr) {
        ::memcpy(ipAddr, defaultIPAddr, sizeof(ipAddr));
    }

    int remotePort = defaultRemotePort;

    /* Contains all active workers/clients
     */
    std::vector<std::shared_ptr<work> > workers;

    // Bind control panel server backends...
    httplib::Server http;

    http.Get("/", [&](const httplib::Request&, httplib::Response& res) {

        if (workers.empty())
        {
            std::string response =
                "<html>"
                "<head><style>body { font-family:monospace; font-size: 12px; }</style></head>"
                "<body>"
                "<div>[<a href=\"/set/start\">START</a>]   [<a href=\"/set/stop\">STOP</a>]</div>"
                "<hr style='border: none; border-top: dashed black 1px' />"
                "<div>There are no active workers</div>"
                "</body>"
                "</html>";
            res.set_content(response, "text/html");
        }

        else
        {
            std::string response =
                "<html>"
                "<body>"
                "<head><style>body { font-family:monospace; } td { font-size: 12px; }</style></head>"
                "<div>[<a href=\"/set/start\">START</a>]   [<a href=\"/set/stop\">STOP</a>]</div>"
                "<hr style='border: none; border-top: dashed black 1px' />";

            response +=
                "<div>"
                "<table width='50%'>"

                "<tr>"
                "<td>Target ip address</td>"
                "<td>" + std::string(ipAddr) + " on port " + std::to_string(remotePort) + "</td>"
                "<tr/>"

                "<tr>"
                "<td colspan=2><hr style='border: none; border-top: dashed black 1px' /></td>"
                "</tr>";

            response +=
                "<tr>"
                "<td>Packet data</td>"
                "<td>" + std::string(message) + " (" + std::to_string(::strlen(message)) + " bytes)</td>"
                "</tr>"

                "</table>"
                "</div>"
                "<br />"
                "<br />"
                "<br />";

            response +=
                "<div>"
                "<table width='100%' style='text-align: center;'>"
                "<tr style='border-bottom: dashed black 1px'>"
                "<th>#</th><th>Sent/Received</th><th>Sent (bytes)</th><th>Received (bytes)</th><th>Received/Bad (bytes)</th>"
                "</tr>"

                "<tr>"
                "<td colspan=5><hr style='border: none; border-top: dashed black 1px' /></td>"
                "</tr>";

            for (auto itr = workers.begin(); itr != workers.end(); ++itr)
            {
                work& ref = **itr;
                work::log workLog = get_work_log(ref);

                response +=
                    "<tr>"
                    "<td>" + std::to_string(workLog.index + 1)
                    + "</td>"
                    "<td class='packets-log'>" + std::to_string(workLog.sent) + "/" + std::to_string(workLog.recvGood)
                    + "</td>"
                    "<td class='sent-bytes'>" + std::to_string(workLog.sentBytes)
                    + "</td>"
                    "<td class='received-bytes'>" + std::to_string(workLog.recvGoodBytes)
                    + "</td>"
                    "<td class='received-bad-bytes'>" + std::to_string(workLog.recvBadBytes)
                    + "</td>"
                    "</tr>";
            }

            response +=
                "</table>"
                "</body>"
                "</html>";
            res.set_content(response, "text/html");
        }
    });

    http.Get("/set/start", [&](const httplib::Request&, httplib::Response& res) {

        if (!workers.empty())
        {
            /* Bad command
             */
            std::string response =
                "<html><body><head><style>body { font-family:monospace; } td { font-size: 12px; }</style></head>"
                + std::to_string(workers.size()) + " worker(s) already started</body></html>";
            res.set_content(response, "text/html");
        }

        else
        {
            /* Load worker data & sart
             */
            workers = load_workers(ipAddr, remotePort, workerCount, message);
            /* Success
             */
            std::string response =
                "<html><body><head><style>body { font-family:monospace; } td { font-size: 12px; }</style></head>"
                "Started " + std::to_string(workers.size()) + " worker(s)</body></html>";
            res.set_content(response, "text/html");
        }
    });

    http.Get("/set/stop", [&](const httplib::Request&, httplib::Response& res) {

        if (workers.empty())
        {
            /* Bad command
             */
            std::string response =
                "<html><body><head><style>body { font-family:monospace; } td { font-size: 12px; }</style></head>"
                "There are no active workers</body></html>";
            res.set_content(response, "text/html");
        }

        else
        {
            /* Stop all workers
             */
            for (auto itr = workers.begin(); itr != workers.end(); ++itr)
            {
                work& ref = **itr;
                stop(ref);
            }

            /* Wait...
             */
            for (auto itr = workers.begin(); itr != workers.end(); ++itr)
            {
                work& ref = **itr;
                pthread_join(ref.pthreadID, nullptr);
            }

            /* Success
             */
            std::string response =
                "<html><body><head><style>body { font-family:monospace; } td { font-size: 12px; }</style></head>"
                "Stopped " + std::to_string(workers.size()) + " worker(s)</body></html>";
            res.set_content(response, "text/html");
            workers.clear();
        }
    });

    // Print info
    ::fprintf(stdout, "> The control panel can be access through local port %d\n\n\n", ctrlPanelPort);

    // Start listening
    if (!http.listen("0.0.0.0", ctrlPanelPort))
    {
        ::fprintf(stderr, "! Terminating program: Cannot initialize control panel, error binding to local port%d\n\n\n",
                  ctrlPanelPort);
    }
}
