/* main.cpp -- v1.0 -- an echo client application
   Author: Sam Y. 2023 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>

#include "run.hpp"

namespace {
    /*! Helper
     *! Outputs usage statement to stdout
     */
    inline void print_usage(const char* app)
    {
        ::printf("Usage: %s [-nPpimh]\n"
                 "  [-h, --help]\n"
                 "  [-P, --ctrl=<local port to access the control panel / web interface>] (default: 8081)\n\n"
                 "  [-n, --client-count=<number of clients>] (default: 8081)\n"
                 "  [-p, --port=<remote server port>]\n"
                 "  [-i, --ip=<remote server address>]\n"
                 "  [-m, --message=<message to send to server>] (default: 'Hello World')\n"
                 , app);
    }
}

/*! Entry point
 */
int main(int argc, char** argv)
{
    /* Records message to send to server
     */
    const char* message = "Hello World";
    /* Records # of workers/clients
     * Default: 1
     */
    int workerCount = 1;
    /* Records remote host port
     */
    int remotePort = 0;
    /* Records remote host IP address
     */
    char ipAddr[256] = {};
    /* Records control panel port (local port)
     * Default: 8081
     */
    int ctrlPanelPort = 8081;

    // CLI options
    const option longOptions[] = {
        { "help",          no_argument,       nullptr, 'h' },
        { "message=",      required_argument, nullptr, 'm' },
        { "client-count=", required_argument, nullptr, 'n' },
        { "ip=",           required_argument, nullptr, 'i' },
        { "port=",         required_argument, nullptr, 'p' },
        { "ctrl=",         required_argument, nullptr, 'P' },
        { 0, 0, 0, 0 }
    };

    // Parse command line options...
    int opt, optindex;
    while ((opt = getopt_long(argc, argv, "n:i:p:h", longOptions, &optindex)) != -1)
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

            /* Record message to send to server
             */
            case 'm':
            {
                message = optarg;
                break;
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

            /* Record remote host IP address
             */
            case 'i':
            {
                ::size_t len = ::strlen(optarg);
                if (len > sizeof(ipAddr))
                    return ::fprintf(stderr, "Invalid IP address specified: %s\n", optarg), 1;
                ::memcpy(ipAddr, optarg, len);
                break;
            }

            /* Record remote host port
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

                if ((remotePort = ::atoi(value)) <= 0) {
                    return ::fprintf(stderr, "Cannot use port %d\n", remotePort), 1;
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

    // Parse input
    if (remotePort == 0)
    {
        remotePort = 8090;
        // Print info
        ::fprintf(stdout, "> Remote host port not specified; use --port=<port #> next time\n\n"
                  "* * * * * * * * * * * defaulting to %d\n\n\n", remotePort);
    }

    // Parse input
    if (::strlen(ipAddr) == 0)
    {
        static const char* ip = "127.0.0.1";
        ::memcpy(ipAddr, ip, ::strlen(ip));
        // Print info
        ::fprintf(stdout, "> Remote host iP address not specified; use --ip=<ip address> next time\n\n"
                  "* * * * * * * * * * * defaulting to %s\n\n\n", ipAddr);
    }

    // Print info
    if (workerCount == 1)
        ::fprintf(stdout, "> %d client is ready to launch\n\n\n", workerCount);
    else
        ::fprintf(stdout, "> %d clients are ready to launch\n\n\n", workerCount);

    /* Enter run loop
     */
    run(ipAddr, remotePort, workerCount, message, ctrlPanelPort);
    return 0;
}
