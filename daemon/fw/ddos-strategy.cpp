#include "ddos-strategy.hpp"
#include "algorithm.hpp"
#include <iostream>
namespace nfd
{
    namespace fw
    {
        NFD_LOG_INIT(DDOSStrategyBase);
        NFD_REGISTER_STRATEGY(DDOSStrategy);
        NFD_REGISTER_STRATEGY(DDOSStrategyEdge);

        DDOSStrategyBase::DDOSStrategyBase(Forwarder &forwarder)
            : Strategy(forwarder) //,
                                  // ProcessNackTraits(this)
        {
        }
        void
        DDOSStrategyBase::afterReceiveInterest(const FaceEndpoint &ingress, const Interest &interest,
                                               const shared_ptr<pit::Entry> &pitEntry)
        {
            if (hasPendingOutRecords(*pitEntry))
            {
                // not a new Interest, don't forward
                return;
            }
            //check fib and forward
            const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
            for (const nfd::fib::NextHop &nexthop : fibEntry.getNextHops())
            {
                Face &outFace = nexthop.getFace();
                if (!wouldViolateScope(ingress.face, interest, outFace) &&
                    canForwardToLegacy(*pitEntry, outFace))
                {
                    this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
                    return;
                }
            }
            this->rejectPendingInterest(pitEntry);
        }

        void
        DDOSStrategyBase::afterReceiveNack(const FaceEndpoint &ingress, const lp::Nack &nack,
                                           const shared_ptr<pit::Entry> &pitEntry)
        {
            auto &hdr = nack.getHeader();
            if (hdr.getReason() != ndn::lp::NackReason::DDOS_FAKE_INTEREST)
            { //process other nacks
                //this->processNack(ingress.face, nack, pitEntry);
                return;
            }
            Name attackedPrefix = getPrefixFromNack(nack);
            NFD_LOG_INFO("prefix is ->" << attackedPrefix);
            std::map<FaceId, std::list<Name>> perFaceInvalidMap;
            std::map<FaceId, Interest> perFaceInterestMap;
            for (const auto &name : hdr.getNames())
            {
                auto pitEntryies = this->lookupPit(name);
                if (pitEntryies.size() == 0)
                {
                    NFD_LOG_INFO("did not see pending interest under name" << name);
                    continue;
                }
                else
                {
                    NFD_LOG_INFO("found pending interest under name" << name);
                }
                for (const auto &pitEntry : pitEntryies)
                {
                    this->rejectPendingInterest(pitEntry);
                    const auto &nackInterest = pitEntry->getInterest();
                    for (const auto &inRecord : pitEntry->getInRecords())
                    {
                        FaceId faceId = inRecord.getFace().getId();
                        //add to
                        NFD_LOG_INFO("Restricted face " << faceId << " with prefix" << attackedPrefix);
                        Records[faceId][attackedPrefix].refresh();
                        perFaceInvalidMap[faceId].push_back(name);
                        perFaceInterestMap[faceId] = nackInterest;
                    }
                }
            }
            for (const auto &faceRecord : perFaceInvalidMap)
            {
                auto nackPushback = make_shared<ndn::lp::Nack>(perFaceInterestMap[faceRecord.first]);
                //NFD_LOG_INFO("!! nack" << nackPushback->getInterest());
                auto &header = nackPushback->getHeader();
                header.setReason(ndn::lp::NackReason::DDOS_FAKE_INTEREST);
                header.setNames(faceRecord.second);
                header.setPrefix(nack.getHeader().getPrefix());
                getFace(faceRecord.first)->sendNack(*nackPushback);
            }
        }

        DDOSStrategyEdge::DDOSStrategyEdge(Forwarder &forwarder, const Name &name)
            : DDOSStrategyBase(forwarder)
        {
            ParsedInstanceName parsed = parseInstanceName(name);
            if (!parsed.parameters.empty())
            {
                NDN_THROW(std::invalid_argument("DDoSStrategy does not accept parameters"));
            }
            if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion())
            {
                NDN_THROW(std::invalid_argument(
                    "DDoSStrategy does not support version " + to_string(*parsed.version)));
            }
            this->setInstanceName(makeInstanceName(name, getStrategyName()));
        }

        const Name &
        DDOSStrategyEdge::getStrategyName()
        {
            static Name strategyName("/localhost/nfd/strategy/ddosedge/%FD%01");
            return strategyName;
        }

        void
        DDOSStrategyEdge::afterReceiveInterest(const FaceEndpoint &ingress, const Interest &interest,
                                               const shared_ptr<pit::Entry> &pitEntry)
        {
            if (hasPendingOutRecords(*pitEntry))
            {
                // not a new Interest, don't forward
                return;
            }
            //for each restriction on the face

            for (auto &record : Records[ingress.face.getId()])
            {
                const Name &prefix = record.first;
                nfd::fw::RestrictionRecord &restriction = record.second;
                //if prefix matched
                if (!prefix.isPrefixOf(interest.getName()))
                    continue;

                if (restriction.expired())
                { //on timeout, remove the record to stop restriction
                    Records[ingress.face.getId()].erase(record.first);
                    continue;
                }
                else
                {
                    restriction.refresh();
                    //on voilation,  do not forward this packet
                    //note that we already check no pending out record, so should be no collateral damage
                    NFD_LOG_INFO("FITT voilation detected " << interest.getName());
                    this->rejectPendingInterest(pitEntry);
                }
            }
            //check fib and forward
            const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
            for (const nfd::fib::NextHop &nexthop : fibEntry.getNextHops())
            {
                Face &outFace = nexthop.getFace();
                if (!wouldViolateScope(ingress.face, interest, outFace) &&
                    canForwardToLegacy(*pitEntry, outFace))
                {
                    this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
                    return;
                }
            }

            this->rejectPendingInterest(pitEntry);
        }

        DDOSStrategy::DDOSStrategy(Forwarder &forwarder, const Name &name)
            : DDOSStrategyBase(forwarder)
        {
            ParsedInstanceName parsed = parseInstanceName(name);
            if (!parsed.parameters.empty())
            {
                NDN_THROW(std::invalid_argument("DDoSStrategy does not accept parameters"));
            }
            if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion())
            {
                NDN_THROW(std::invalid_argument(
                    "DDoSStrategy does not support version " + to_string(*parsed.version)));
            }
            this->setInstanceName(makeInstanceName(name, getStrategyName()));
        }

        const Name &
        DDOSStrategy::getStrategyName()
        {
            static Name strategyName("/localhost/nfd/strategy/ddos/%FD%01");
            return strategyName;
        }

    } // namespace fw
} // namespace nfd