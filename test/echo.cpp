/* echo.cpp -- v1.1 -- defines a worker class that encapsulates and echo server
   Author: Sam Y. 2023 */

#include "echo.hpp"

/*! @class echo
 *! Client packet handler
 */
class echo : public comm::client_callback_handler<echo> {
public:

    inline echo(const std::size_t nworkers,
                const std::size_t size) : comm::client_callback_handler<echo>(nworkers, size) {}

    inline void on_input(int sfd, char* data, int datalen) {
        // Just echo the message back
        comm::endpoint_write(sfd, data, datalen);
    }
};

namespace {

    /*! Prints socket creation error to console
     */
    inline void print_server_socket_error(const int port)
    {
        static const char* errstr = "Server socket creation error on port";

        int temp = port;
        int charCount = 0;

        do {
            ++charCount;
        } while(temp /= 10);

        char* buff = static_cast<char*>(std::calloc(::strlen(errstr) + charCount + 2, sizeof(char)));
        std::sprintf(buff, "%s %d", errstr, port);

        ::perror(buff);
        ::free(buff);
    }
}

namespace {

    /*! Helper: Create server socket and client pool
     */
    inline comm::server<echo>* init_server(int port, int nWorkers, int maxClients, int* svfd)
    {

        // Create listen socket
        if ((*svfd = comm::endpoint_tcp_server(port, 100000)) == -1
            || comm::endpoint_unblock(*svfd) == -1) {
            return print_server_socket_error(port), nullptr;
        }

        // Initialize server
        comm::server<echo>* sv;

        try
        {
            // Initialise pool
            sv = new comm::server<echo>(nWorkers, maxClients);

            if (!sv->add(*svfd)) {
                return perror(""), nullptr;
            }
        }

        catch (std::runtime_error& e) {
            return perror(e.what()), nullptr;
        }

        return sv;
    }
}

/*! dtor.
 */
echo_worker::~echo_worker()
{
    if (sv_.get()) {
        comm::endpoint_close(svfd_);
    }
}

/*! Init. server
 */
bool echo_worker::create(int port, int nWorkers, int maxClients)
{
    std::lock_guard<std::mutex> lock(lock_);

    if (sv_.get()) {
        return false;
    }

    int svfd;
    comm::server<echo>* sv;
    if ((sv = init_server(port, nWorkers, maxClients, &svfd)) == nullptr) {
        return false;
    }

    return sv_.reset(sv), true;
}

/*! Starts server
 */
bool echo_worker::run()
{
    std::lock_guard<std::mutex> lock(lock_);

    if (sv_.get())
    {
        if (!work_.get())
        {
            work_.reset(new std::thread(&comm::server<echo>::run, sv_.get()));
            return true;
        }
    }

    return false;
}

/*! Stops server
 */
bool echo_worker::stop()
{
    std::lock_guard<std::mutex> lock(lock_);

    if (sv_.get())
    {
        if (work_.get())
        {
            sv_->stop();
            work_->join();
            work_.reset();
            return true;
        }
    }

    return false;
}

/*! @get
 */
bool echo_worker::is_running() const
{
    std::lock_guard<std::mutex> lock(lock_);
    return work_.get() != nullptr;
}

/*! @get
 */
std::size_t echo_worker::get_active_client_count() const
{

    std::lock_guard<std::mutex> lock(lock_);

    if (!sv_.get()) {
        return 0;
    }

    return sv_->get_active_count();
}
