/* main.cpp -- v1.0 -- an echo server application
   Author: Sam Y. 2021-22 

   main.cpp -- v1.1
   Modified: Added HTTP interface, 2023 */

#include <cstring>
#include <getopt.h>

#include "echo.hpp"

/*! Program run loop
 */
extern void run(int ctrlPanelPort, echo_worker& serverWorker, int serverPort);

namespace {
    /*! Helper
     *! Outputs usage statement to stdout
     */
    inline void print_usage(const char* app)
    {
        ::printf("Usage: %s [-nPph]\n"
                 "  [-h, --help]\n"
                 "  [-P, --ctrl=<local port to access the control panel / web interface>] (default: 8080)\n\n"
                 "  [-n, --client-count=<maximum number of clients>] (default: 100,000)\n"
                 "  [-p, --port=<server listen port>]\n"
                 , app);
    }
}

/*! Entry point
 */
int main(int argc, char** argv)
{
    int maxClients = 1e5; // 100,000 max. connections
    int workerCount = 10;
    int serverPort = 0;
    int ctrlPanelPort = 0;

    // CLI options
    const option longOptions[] = {
        { "help",          no_argument,       nullptr, 'h' },
        { "client-count=", required_argument, nullptr, 'n' },
        { "port=",         required_argument, nullptr, 'p' },
        { "ctrl=",         required_argument, nullptr, 'P' },
        { 0, 0, 0, 0 }
    };

    // Parse command line options...
    int opt, optindex;
    while ((opt = getopt_long(argc, argv, "n:P:p:h", longOptions, &optindex)) != -1)
    {
        switch (opt)
        {
            /* Print user message and return
             */
            case 'h':
            {
                print_usage(argv[0]);
                return 0;
            }

            /* Record # of workers/clients
             */
            case 'n':
            {
                char* value = optarg;
                for (::size_t i = 0; i != ::strlen(value); ++i)
                {
                    if (!::isdigit(value[i])) {
                        return ::fprintf(stderr, "Specified client count '%s' not correct format\n", value), 1;
                    }
                }

                if ((workerCount = ::atoi(value)) <= 0) {
                    return ::fprintf(stderr, "There needs to be at least 1 worker, %d specified\n", workerCount), 1;
                }

                break;
            }

            /* Record local listen port
             */
            case 'p':
            {
                char* value = optarg;
                for (::size_t i = 0; i != ::strlen(value); ++i)
                {
                    if (!::isdigit(value[i])) {
                        return ::fprintf(stderr, "Specified remote port '%s' not correct format\n", value), 1;
                    }
                }

                if ((serverPort = ::atoi(value)) <= 0) {
                    return ::fprintf(stderr, "Cannot use port %d\n", serverPort), 1;
                }

                break;
            }

            /* Record ctrl panel local port
             */
            case 'P':
            {
                char* value = optarg;
                for (::size_t i = 0; i != ::strlen(value); ++i)
                {
                    if (!::isdigit(value[i])) {
                        return ::fprintf(stderr, "Specified control panel port '%s' not correct format\n", value), 1;
                    }
                }

                ctrlPanelPort = ::atoi(value);
                break;
            }

            /* Bad input, print user message and return
             */
            default:
            {
                print_usage(argv[0]);
                return 0;
            }
        }
    }

    // Parse input & print additional info
    if (serverPort == 0)
    {
        serverPort = 8090;
        ::fprintf(stderr, "> Server listen port not specified; use --port=<port #> next time\n\n"
                  "* * * * * * * * * * * defaulting to %d\n\n\n", serverPort);
    }

    // Parse input & print additional info
    if (ctrlPanelPort == 0)
    {
        ctrlPanelPort = 8080;
        ::fprintf(stderr, "> Control panel port not specified; use --ctrl=<port #> next time\n\n"
                  "* * * * * * * * * * * defaulting to %d\n\n\n", ctrlPanelPort);
    }

    /* Initialize server
     */
    echo_worker serverWorker;
    if (!serverWorker.create(serverPort, workerCount, maxClients)) {
        return 1;
    }

    else
    {
        ::fprintf(stderr,
                  "> The server is listening on port %d; maximum # of persistent connections = %d\n\n\n",
                  serverPort,
                  maxClients);
    }

    /* Enter program loop
     */
    run(ctrlPanelPort, serverWorker, serverPort);
    return 0;
}
