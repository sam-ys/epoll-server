/* work.hpp -- v1.0 -- worker run loop / initiates and uses client connection to server
   Author: Sam Y. 2023 */

#ifndef WORKER_HPP
#define WORKER_HPP

#include <memory>
#include <mutex>

/* Fwd. decl.
 */
extern void* worker(void* pvoid);

//! struct work
/*! Worker paramaters
 */
struct work {

    //! struct work_log
    /*! Network status log
     */
    struct log {
        // Worker index
        int index;
        // Sent packets
        int sent;
        // Sent packets
        int sentBytes;
        // Received packets
        int recvGood;
        // Received packets
        int recvGoodBytes;
        // Received packets (badly formed)
        int recvBad;
        // Received packets (badly formed)
        int recvBadBytes;
    };

    static const ::size_t MAXBUFLEN = 128;

    // Expected echo'd message
    char message[MAXBUFLEN + 1];

    // Run status
    bool run;

    // Remote host ip
    char ip[16];
    // Remote host port
    int port;

    // Worker log
    log workLog;
    // General Access lock
    mutable std::mutex lock;

    // Thread id
    pthread_t pthreadID;

    //! ctor.
    work(const char* ipArg,
         int         ipArgLen,
         int         port,
         const char* messageArg,
         int         messageArgLen,
         int         index);
};

/*! @get
 */
extern bool is_running(work& w);

/*! @get
 */
extern work::log get_work_log(const work& w);

/*! @set
 */
extern void start(work& w);

/*! @set
 */
extern void stop(work& w);

/*! @set
 */
extern void log_message_sent(work& w, ::size_t len);

/*! @set
 */
extern void log_message_received(work& w, const char* receivedMessage, ::size_t len);

#endif
