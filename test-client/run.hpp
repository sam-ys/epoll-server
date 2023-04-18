/* run.hpp -- v1.0 -- main program run loop, handles control requests through http
   Author: Sam Y. 2023 */

#ifndef RUN_HPP
#define RUN_HPP

extern void run(const char* defaultIPAddr,
                int         defaultRemotePort,
                int         workerCount,
                const char* message,
                int         ctrlPanelPort);

#endif
