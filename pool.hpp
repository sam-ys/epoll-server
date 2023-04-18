/* pool.hpp -- v1.0 -- client and server event handlers
   Author: Sam Y. 2021-22 

   pool.hpp -- v1.1
   Modified: Class now uses stack for memory management, 2023 */

#ifndef _COMM_POOL_HPP
#define _COMM_POOL_HPP

#include <mutex>
#include <thread>
#include <vector>

#include <sys/ioctl.h>

#include "atomic_stack.hpp"
#include "epoll.hpp"

namespace comm {

    class client_pool_base {};
    class server_pool_base {};

    //! @class client_pool
    /*! encapsulates event handling for multiple clients
     */
    template <typename Tderiv>
    class client_pool : public client_pool_base,
                        public epoll<client_pool<Tderiv> > {
    public:

        //! dtor.
        //
        ~client_pool() {

            std::lock_guard<std::mutex> lock(lock_);
            freeMem_.destroy();
        }

        //! ctor.
        //! @param workerCount     client handler thread count
        //! @param clientCap    maximum number of clients
        client_pool(const std::size_t workerCount, std::size_t clientCap) : workerCount_(workerCount)
                                                                          , clientCap_(clientCap)
                                                                          , clientCount_(0) {
            if (!freeMem_.create(&clientCap_)) {
                throw std::bad_alloc();
            }
        }

        //! @get
        //! @return number of active clients
        std::size_t get_active_count() const {

            return clientCount_;
        }

        //! Adds a new client
        //! @param sfd    file descriptor
        bool add_client(const int sfd) {

            client* cl;
            if ((cl = use(sfd)) == nullptr)
                return false;
            return epoll<client_pool>::add(cl) == 0;
        }

        //! Starts instance
        //!
        void run() {

            std::lock_guard<std::mutex> lock(lock_);

            if (threads_.empty())
            {
                threadCount_.store(workerCount_);
                for (std::size_t i = 0; i != workerCount_; ++i)
                {
                    threads_.emplace_back([this] {
                        epoll<client_pool<Tderiv> >::wait(threadCount_);
                    });
                }
            }
        }

        //! Stops running instance
        //!
        void stop() {

            std::lock_guard<std::mutex> lock(lock_);

            if (!threads_.empty())
            {
                // Master thread initiates the shutdown daisy-chain
                epoll<client_pool<Tderiv> >::close();
                for (std::size_t i = 0; i != threads_.size(); ++i) {
                    threads_[i].join();
                }

                threads_.clear();

                // Maybe reset clients...
                atomic_node<client>* data = freeMem_.data();
                for (std::size_t i = 0; i != clientCap_; ++i)
                {
                    client& ref = static_cast<client&>(data[i]);
                    // Maybe reset client
                    if (ref.sfd) {
                        unuse(&ref);
                    }
                }
            }
        }

        //! Override this to handle out-of-band events
        //! @param sfd        triggered file descriptor
        //! @param oobdata    oob byte
        inline void on_oob(int sfd, char oobdata) {
            (void)sfd;
            (void)oobdata;
        }

        //! Override to handle input events
        //! @param sfd        triggered file descriptor
        //! @param data       received data
        //! @param datalen    received data length
        inline void on_input(int sfd, char* data, int datalen) {
            (void)sfd;
            (void)data;
            (void)datalen;
        }

        //! Override this to handle output-ready events
        //! @param sfd    triggered file descriptor
        inline void on_write_ready(int sfd) {
            (void)sfd;
        }

    private:

        friend epoll<client_pool<Tderiv> >;

        // Applied to critical section when starting and stopping the running instance
        mutable std::mutex lock_;

        // Threads
        std::size_t workerCount_; // Total # of worker threads
        std::size_t clientCap_; // Total # of allowed clients

        std::atomic<std::size_t> clientCount_; // Current number of allocated clients

        atomic_stack<client> freeMem_; // Stack of allocated inactive clients

        std::vector<std::thread> threads_; // Workers
        std::atomic<std::size_t> threadCount_; // Current number of running threads

        /*! Called on epoll event, casts epoll data value to correct type before passing it to process()
         */
        client* cast(epoll_data data) {
            return static_cast<client*>(data.ptr);
        }

        /*! Called on epoll event to processes triggered file descriptor
         */
        inline void process(client* const cl, const int flags);

        /*! Closes socket and stores client to unused queue
         */
        void unuse(client* const cl) {

            epoll<client_pool>::remove(cl->sfd);
            endpoint_close(cl->sfd);
            cl->sfd = 0;
            freeMem_.push(cl);
            --clientCount_;
        }

        /*! Allocates new client
         */
        client* use(const int sfd) {

            client* mem;
            if ((mem = freeMem_.pop()) == nullptr) {
                return nullptr;
            }

            else
            {
                ++clientCount_;
                return new (mem) client(sfd);
            }
        }

        /*! EPOLLOUT
         */
        inline void handle_epollout(client* const);
        /*! EPOLLIN
         */
        inline void handle_epollin(client* const);
        /*! EPOLLPRI
         */
        inline void handle_epollpri(client* const);
    };


    /*! Processes epoll events
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::process(client* const client, int flags)
    {
        switch (flags)
        {
            case EPOLLHUP:
            case EPOLLHUP | EPOLLOUT:

            case EPOLLRDHUP:
            case EPOLLRDHUP | EPOLLOUT: // ignore EPOLLOUT
            {
                unuse(client);
                break;
            }

            case EPOLLIN:
            case EPOLLIN | EPOLLHUP:
            case EPOLLIN | EPOLLHUP | EPOLLOUT: // ignore EPOLLOUT

            case EPOLLIN | EPOLLRDHUP:
            case EPOLLIN | EPOLLRDHUP | EPOLLOUT: // ignore EPOLLOUT
            {
                handle_epollin(client); // Also processes hangup (on 0-byte read)
                break;
            }

            case EPOLLPRI:
            case EPOLLPRI | EPOLLHUP:
            case EPOLLPRI | EPOLLHUP | EPOLLOUT: // ignore EPOLLOUT

            case EPOLLPRI | EPOLLRDHUP:
            case EPOLLPRI | EPOLLRDHUP | EPOLLOUT: // ignore EPOLLOUT
            {
                handle_epollpri(client); // Also process hangup (on 0-byte read)
                break;
            }

            case EPOLLIN | EPOLLPRI:
            case EPOLLIN | EPOLLPRI | EPOLLHUP:
            case EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLOUT: // ignore EPOLLOUT

            case EPOLLIN | EPOLLPRI | EPOLLRDHUP:
            case EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLOUT: // ignore EPOLLOUT
            {
                handle_epollpri(client); // Also processes hangup (on 0-byte read)
                break;
            }

            case EPOLLOUT:
            {
                handle_epollout(client);
                break;
            }

            case EPOLLIN | EPOLLOUT:
            {
                handle_epollin(client);
                handle_epollout(client);
                break;
            }

            case EPOLLPRI | EPOLLOUT:
            {
                handle_epollpri(client);
                handle_epollout(client);
                break;
            }

            case EPOLLIN | EPOLLPRI | EPOLLOUT:
            {
                handle_epollpri(client);
                handle_epollout(client);
                break;
            }

            default:
            {
                // Have error, close the socket
                if ((flags & EPOLLERR) == EPOLLERR) {
                    unuse(client);
                }
            }
        }
    }

    /*! EPOLLOUT
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollout(client* const cl)
    {
        static_cast<Tderiv*>(this)->on_write_ready(cl->sfd);
    }

    /*! EPOLLIN
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollin(client* const cl)
    {
        while (true)
        {
            int nbytes;
            switch (nbytes = endpoint_read(cl->sfd, cl->buff, static_cast<int>(cl->size)))
            {
                case -1:
                {
                    if (errno != EAGAIN)
                        unuse(cl); // Have actual error - done with client
                    else
                        epoll<client_pool>::rearm(cl);
                    return;
                }

                case 0:
                {
                    unuse(cl); // Disconnection - done with client
                    return;
                }

                // Have data to process...
                default:
                {
                    static_cast<Tderiv*>(this)->on_input(cl->sfd, cl->buff, nbytes);
                    break;
                }
            }
        }
    }

    /*! EPOLLPRI
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollpri(client* const cl)
    {
        while (true)
        {
            int mark;
            if (::ioctl(cl->sfd, SIOCATMARK, &mark) == -1)
            {
                unuse(cl); // Have actual error - done with client
                break;
            }

            else
            {
                if (mark)
                {
                    char oobdata;
                    if (endpoint_read_oob(cl->sfd, &oobdata) != -1)
                        on_oob(cl->sfd, oobdata);

                    else
                    {
                        unuse(cl); // Have actual error - done with client
                        break;
                    }
                }
            }

            int nbytes;
            switch ((nbytes = endpoint_read(cl->sfd, cl->buff, static_cast<int>(cl->size))))
            {
                case -1:
                {
                    if (errno != EAGAIN)
                        unuse(cl); // Have actual error - done with client
                    else
                        epoll<client_pool>::rearm(cl);
                    return;
                }

                case 0:
                {
                    unuse(cl); // Disconnection - done with client
                    return;
                }

                // Have data to process...
                default:
                {
                    static_cast<Tderiv*>(this)->on_input(cl->sfd, cl->buff, nbytes);
                    break;
                }
            }
        }
    }

    //! @class server_pool
    /*! encapsulates event handling for multiple server sockets and their clients
     */
    template <typename T>
    class server_pool : public server_pool_base, public epoll<server_pool<T> > {
    public:

        //! ctor.
        //! @param workerCount     number of client handler thread
        //! @param clientCap    maximum number of clients
        server_pool(const std::size_t workerCount,
                    const std::size_t clientCap) : clientPool_(workerCount, clientCap) {}

        ::size_t get_active_count() const {
            return clientPool_.get_active_count();
        }

        //! Starts listening on all server sockets
        //!
        void run() {

            std::lock_guard<std::mutex> lock(lock_);

            // Maybe start the server
            if (threadCount_.load() == 0)
            {
                clientPool_.run();
                // Start server
                // Server instance listens on only one thread
                threadCount_ = 1;
                epoll<server_pool<T> >::wait(threadCount_);
            }
        }

        //! Stops listening on all server sockets
        //!
        void stop() {

            int threadCount = threadCount_.load();
            // Maybe stop the server
            // Server instance listens on only one thread
            if (threadCount == 1)
            {
                epoll<server_pool<T> >::close();

                std::lock_guard<std::mutex> lock(lock_);
                clientPool_.stop();
            }
        }

        //! Binds a listener socket to port
        //! @param port        port number
        //! @param queuelen    backlog queue length for accept()
        bool bind(const int port, const int queuelen) {

            int sfd;
            if ((sfd = comm::endpoint_tcp_server(port, queuelen)) == -1
                || comm::endpoint_unblock(sfd) == -1)
                return false;

            const int ret = epoll<server_pool<T> >::add(sfd);
            return ret == 0;
        }

        //! Adds a listener socket
        //! @param sfd    file descriptor
        bool add(const int sfd) {

            const int ret = epoll<server_pool<T> >::add(sfd);
            return ret == 0;
        }

    private:

        friend epoll<server_pool<T> >;

        /*! Called on epoll event, casts epoll data value to correct type before passing it to process()
         */
        int cast(epoll_data data) {
            return static_cast<int>(data.u32);
        }

        /*! Called on epoll event to handle connection requests
         */
        inline void process(const int sfd, const int flags);

        T                        clientPool_;
        std::atomic<std::size_t> threadCount_; // Current number of running threads
        mutable std::mutex       lock_;
    };

    /*! Called on epoll event to handle connection requests
     */
    template <typename T>
    void server_pool<T>::process(const int sfd, const int flags)
    {
        switch (flags)
        {
            case EPOLLERR:
            {
                endpoint_close(sfd);
                break;
            }

            default:
            {
                int cfd;
                while ((cfd = endpoint_accept(sfd)) != -1)
                {
                    if (endpoint_unblock(cfd) != 0
                        || !clientPool_.add_client(cfd)) {
                        endpoint_close(cfd);
                    }
                }
            }
        }
    }
}

#endif
