#ifndef NFD_DAEMON_FW_DDOS_RECORD_HPP
#define NFD_DAEMON_FW_DDOS_RECORD_HPP

#include "nfd.hpp"
//#include "ns3/nstime.h"
#include <set>
#include <chrono>
//todo design choice?
/**
 * face ->  restriction records
 * restriction records: prefix -> last problem time
 * 
 * on receive nack: find all faces, add restriction records.
 * on receive interest: check input face's restriction records and interest's prefix, update timing info of restruction records if possible
 *
*/
namespace nfd
{
    namespace fw
    {
        using FaceId = uint64_t;
        struct RestrictionRecord
        {
            void refresh()
            {
                lastVoilationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();
            }
            bool expired() const
            {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
                return (now - lastVoilationTime > voilationTImeTolerance);
            }

        private:
            uint64_t lastVoilationTime;
            const static int voilationTImeTolerance = 200000;
        };

    } // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_DDOS_RECORD_HPP