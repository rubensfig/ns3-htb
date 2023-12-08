/*
 * Copyright (c) 2017 Kungliga Tekniska Högskolan
 *               2017 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * TBF, The Token Bucket Filter Queueing discipline
 *
 * This implementation is based on linux kernel code by
 * Authors:     Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *              Dmitry Torokhov <dtor@mail.ru> - allow attaching inner qdiscs -
 *                                               original idea by Martin Devera
 *
 * Implemented in ns-3 by: Surya Seetharaman <suryaseetharaman.9@gmail.com>
 *                         Stefano Avallone <stavallo@unina.it>
 */
#ifndef HTB_QUEUE_DISC_H
#define HTB_QUEUE_DISC_H

#include "queue-disc.h"

#include "ns3/object-factory.h"

#include <map>
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/traced-value.h"

namespace ns3
{

#define MAXHTBNUMPRIO 8
#define MAXHTBTREEDEPTH 2

struct htbClass; //forward declaration


enum Mode {
    can_send,
    may_borrow,
    cant_send	
};
// This should be in an union
struct htbLeaf {
	int priority;
	int queue_id;
};

struct htbInner {
	std::set<htbClass*> innerFeeds[8];
	htbClass* nextToDequeue[8];
};

/**
 * \ingroup traffic-control
 *
 * \brief A HTB packet queue disc
 */
class HtbQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief HtbQueueDisc Constructor
     *
     * Create a TBF queue disc
     */
    HtbQueueDisc();

    /**
     * \brief Destructor
     *
     * Destructor
     */
    ~HtbQueueDisc() override;


  protected:


    struct htbClass {

    	uint32_t level = 0;
	uint32_t cirTokens;
	uint32_t pirTokens;

	uint32_t m_assignedBurstSize;
	uint32_t m_ceilingBurstSize;

	DataRate m_assignedRate; // cir
	DataRate m_ceilingRate; // pir

    	bool activePriority[MAXHTBNUMPRIO] = { false };

    	Time m_timeCheckPoint; // time of last token update

	Mode mode;
	
	htbInner inner;
	htbLeaf leaf;

	htbClass* parent = nullptr;
    };

    struct htbLevel {

	uint16_t levelId; // Level number

	// Which priorities the class are active
	std::set<htbClass*> selfFeeds[MAXHTBNUMPRIO];
	htbClass* nextToDequeue[MAXHTBNUMPRIO];
	// std::multiset<htbClass*, waitComp> waitingClasses;
    };

    std::vector<struct htbLevel*> levels;

    void ActivateClass(htbClass* leaf, uint32_t priority);
    void DeactivateClass(htbClass* leaf, uint32_t priority);
    Mode UpdateClassMode(htbClass* leaf, uint64_t* diff);
    void ChargeClass(htbClass* leaf, uint32_t creditsToCharge);
  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    Time m_timeCheckPoint;
    

    EventId m_id;

    // queue_id 
    htbClass* rootClass = nullptr;
    std::vector<struct htbClass*> leafNodes;
    std::vector<struct htbClass*> innerNodes;
};

} // namespace ns3

#endif /* HTB_QUEUE_DISC_H */
