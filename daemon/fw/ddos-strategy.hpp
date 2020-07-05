#ifndef NFD_DAEMON_FW_BEST_ROUTE_STRATEGY_HPP
#define NFD_DAEMON_FW_BEST_ROUTE_STRATEGY_HPP

#include "strategy.hpp"
#include "nfd.hpp"
#include "ddos-record.hpp"
//#include "process-nack-traits.hpp"
namespace nfd
{
    namespace fw
    {
        Name getPrefixFromNack(const lp::Nack &nack)
        {
            return nack.getInterest().getName().getPrefix(nack.getHeader().getPrefix());
        }

        typedef std::map<FaceId, std::map<Name, RestrictionRecord>> DDoSRecords;

        class DDOSStrategyBase : public Strategy /* ,public ProcessNackTraits<DDOSStrategyBase>*/
        {
        public:
            DDoSRecords Records;

            void
            afterReceiveInterest(const FaceEndpoint &ingress, const Interest &interest,
                                 const shared_ptr<pit::Entry> &pitEntry) override;
            void
            afterReceiveNack(const FaceEndpoint &ingress, const lp::Nack &nack,
                             const shared_ptr<pit::Entry> &pitEntry);

        protected:
            DDOSStrategyBase(Forwarder &forwarder);
        };

        /** \brief Best Route strategy version 1
 *
 *  This strategy forwards a new Interest to the lowest-cost nexthop
 *  that is not same as the downstream, and does not violate scope.
 *  Subsequent similar Interests or consumer retransmissions are suppressed
 *  until after InterestLifetime expiry.
 *
 *  \note This strategy is superceded by Best Route strategy version 2,
 *        which allows consumer retransmissions. This version is kept for
 *        comparison purposes and is not recommended for general usage.
 *
 *  \note This strategy is not EndpointId-aware.
 */
        class DDOSStrategyEdge : public DDOSStrategyBase
        {
        public:
            explicit DDOSStrategyEdge(Forwarder &forwarder, const Name &name = getStrategyName());

            static const Name &
            getStrategyName();
            void afterReceiveInterest(const FaceEndpoint &ingress, const Interest &interest,
                                           const shared_ptr<pit::Entry> &pitEntry) override;
        };
        
        class DDOSStrategy : public DDOSStrategyBase
        {
        public:
            explicit DDOSStrategy(Forwarder &forwarder, const Name &name = getStrategyName());

            static const Name &
            getStrategyName();
            
        };
    } // namespace fw
} // namespace nfd
#endif // NFD_DAEMON_FW_BEST_ROUTE_STRATEGY_HPP