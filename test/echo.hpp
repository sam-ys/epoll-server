/* echo.hpp -- v1.1 -- defines a worker class that encapsulates and echo server
   Author: Sam Y. 2023 */

#ifndef ECHO_HPP
#define ECHO_HPP

#include <memory>

#include "server/server.hpp"

// Fwd. decl.
class echo;

//! class echo_worker
/*! Encapsulates a running echo server instance
 */
class echo_worker {
public:

    /*! dtor.
     */
    ~echo_worker();

    /*! Init. server
     */
    bool create(int port, int nWorkers, int maxClients);

    /*! Starts server
     */
    bool run();

    /*! Stops server
     */
    bool stop();

    /*! @get
     */
    bool is_running() const;

    /*! @get
     */
    std::size_t get_active_client_count() const;

private:

    mutable std::mutex lock_;

    int svfd_;
    std::shared_ptr<comm::server<echo> > sv_;

    std::shared_ptr<std::thread> work_;
};


#endif
