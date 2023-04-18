/* work.cpp -- v1.0 -- worker run loop / initiates and uses client connection to server
   Author: Sam Y. 2023 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "work.hpp"

/*! ctor.
 */
work::work(const char* ipArg,
           int         ipArgLen,
           int         port,
           const char* messageArg,
           int         messageArgLen,
           int         index) : port(port)
{
    ::memset(&workLog, 0, sizeof(log));
    workLog.index = index;

    ::memset(ip, 0, sizeof(ip));
    ::memset(message, 0, sizeof(message));

    ::memcpy(ip, ipArg, ipArgLen);
    ::memcpy(message, messageArg, messageArgLen);
}

/*! @get
 */
bool is_running(work& w)
{
    std::lock_guard<std::mutex> lock(w.lock);
    return w.run;
}

/*! @get
 */
work::log get_work_log(const work& w)
{
    std::lock_guard<std::mutex> lock(w.lock);
    return w.workLog;
}

/*! @set
 */
void start(work& w)
{
    std::lock_guard<std::mutex> lock(w.lock);
    w.run = true;
}

/*! @set
 */
void stop(work& w)
{
    std::lock_guard<std::mutex> lock(w.lock);
    w.run = false;
}

/*! @set
 */
void log_message_sent(work& w, ::size_t len)
{
    if (len == 0)
        return;
    std::lock_guard<std::mutex> lock(w.lock);
    ++(w.workLog).sent;
    (w.workLog).sentBytes = (w.workLog).sent * len;
}

/*! @set
 */
void log_message_received(work& w, const char* receivedMessage, ::size_t len)
{
    bool ret = (::memcmp(receivedMessage, w.message, len) == 0);

    std::lock_guard<std::mutex> lock(w.lock);
    if (!ret)
    {
        ++(w.workLog).recvBad;
        (w.workLog).recvBadBytes = (w.workLog).recvBad * len;
    }

    else
    {
        ++(w.workLog).recvGood;
        (w.workLog).recvGoodBytes = (w.workLog).recvGood * len;
    }
}

/*! Run loop
 */
void* worker(void* pvoid)
{
    work* ptrWork = reinterpret_cast<work*>(pvoid);
    assert(ptrWork);

    /* Create socket
     */
    int sfd = 0;
    if ((sfd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        ::perror("socket");
        return nullptr;
    }

    /* Set to nonblocking
     */
    int flags = ::fcntl(sfd, F_GETFL, 0);
    if (::fcntl(sfd, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        ::close(sfd);
        ::perror("fcntl");
        return nullptr;
    }

    /* Connect to remote host
     */
    sockaddr_in destination = {};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = ::inet_addr(ptrWork->ip);
    destination.sin_port = ::htons(ptrWork->port);

    if (connect(sfd, (struct sockaddr*)&destination, sizeof(destination)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            ::close(sfd);
            ::perror("connect");
            return nullptr;
        }
    }

    /* Send & receive
     */
    while (true)
    {
        if (!is_running(*ptrWork))
        {
            ::close(sfd);
            return nullptr;
        }

        /* Send message...
         */
        for (int remaining = work::MAXBUFLEN; remaining > 0; )
        {
            if (!is_running(*ptrWork))
            {
                ::close(sfd);
                return nullptr;
            }

            char* ptr = ptrWork->message + work::MAXBUFLEN - remaining;

            int n = ::send(sfd, ptr, remaining, 0);
            if (n != -1)
                remaining -= n;
            else
            {
                if (errno != EAGAIN) {
                    break;
                }
            }

            // Maybe log message
            if (remaining == 0) {
                log_message_sent(*ptrWork, work::MAXBUFLEN);
            }
        }

        /* Receive message...
         */
        char message[work::MAXBUFLEN + 1] = {};

        for (int remaining = work::MAXBUFLEN; remaining > 0; )
        {
            if (!is_running(*ptrWork))
            {
                ::close(sfd);
                return nullptr;
            }

            char* ptr = message + work::MAXBUFLEN - remaining;

            int n = ::recv(sfd, ptr, remaining, 0);
            if (n != -1)
                remaining -= n;
            else
            {
                if (errno != EAGAIN) {
                    break;
                }
            }

            if (remaining == 0) {
                log_message_received(*ptrWork, message, work::MAXBUFLEN);
            }
        }

        ::usleep(1e3);
    }
}
