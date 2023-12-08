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

#include "htb-queue-disc.h"

#include "ns3/attribute.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/socket.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("HtbQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(HtbQueueDisc);

TypeId
HtbQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::HtbQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<HtbQueueDisc>();

    return tid;
}

HtbQueueDisc::HtbQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES)
{
    NS_LOG_FUNCTION(this);
}

HtbQueueDisc::~HtbQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void HtbQueueDisc::ActivateClass(htbClass* cl, uint32_t priority)
{
	htbClass* parent = cl->parent;

	if (!cl->activePriority[priority]) {
		cl->activePriority[priority] = true;
			
		levels[cl->level]->nextToDequeue[priority] = cl;
		levels[cl->level]->selfFeeds[priority].insert(cl);

		// NS_LOG_FUNCTION(this << " leaf " << cl << " priority " << priority);
	}
}

void HtbQueueDisc::DeactivateClass(htbClass* cl, uint32_t priority)
{
	if (cl->activePriority[priority]) {

		auto ptr = levels[cl->level]->selfFeeds[priority].find(cl);
		if (ptr != levels[cl->level]->selfFeeds[priority].end()) {
			// NS_LOG_LOGIC(this << " leaf found");

			levels[cl->level]->selfFeeds[priority].erase(ptr);
			levels[cl->level]->nextToDequeue[priority] = nullptr;

			cl->activePriority[priority] = false;

			// NS_LOG_LOGIC(this << " leaf " << cl);
		}
	}
}

Mode HtbQueueDisc::UpdateClassMode(htbClass* leaf, uint64_t* diff)
{
    NS_LOG_FUNCTION(this);

    int64_t pir_toks = 0;
    Time now = Simulator::Now();
    double delta = (now - m_timeCheckPoint).GetSeconds();

    pir_toks = leaf->pirTokens + round(delta * leaf->m_assignedRate.GetBitRate())/8;

    if (pir_toks >= leaf->m_ceilingBurstSize)
	pir_toks = leaf->m_ceilingBurstSize;

     pir_toks -= creditsToCharge;

     if (pir_toks < 0) {
	leaf->pirTokens = 0;
     } else
	leaf->pirTokens = pir_toks;

}

void HtbQueueDisc::HtbAccountTokens(htbClass* cl, int bytes, uint64_t diff)
{
	uint64_t toks = diff + cl->pir_tokens;
	
	if (toks > cl->buffer)
		toks = cl->buffer;


}

void HtbQueueDisc::ChargeClass(htbClass* leaf, uint32_t creditsToCharge)
{
    // NS_LOG_FUNCTION(this);

    if (leaf == nullptr) // rootNode, stop
	    return;


     NS_LOG_LOGIC("node " << leaf->level << " pir bucket " << leaf->pirTokens << " mode " << leaf->mode );

    leaf->m_timeCheckPoint = now;

    ChargeClass(leaf->parent, creditsToCharge);
}

bool
HtbQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    uint32_t queue_id = 0;
    uint32_t tagvalue = 0;

    SocketPriorityTag priorityTag;
    if (item->GetPacket()->PeekPacketTag(priorityTag))
    	tagvalue = priorityTag.GetPriority();

    queue_id = tagvalue;
    NS_LOG_LOGIC("queue_id " << queue_id);

    bool retval = GetInternalQueue(queue_id)->Enqueue(item);

    htbClass* currLeaf = leafNodes.at(queue_id);
    ActivateClass(currLeaf, currLeaf->leaf.priority);

    NS_LOG_LOGIC("Current queue size: " << GetNPackets() << " packets, " << GetNBytes()
                                        << " bytes");

    return true;
}

Ptr<QueueDiscItem>
HtbQueueDisc::DoDequeue()
{
    // NS_LOG_FUNCTION(this);
	
    uint32_t queue_id = 0;
    uint16_t level, priority;
    struct htbClass* leaf;
    int64_t pir_toks = 0;
    uint16_t cnt;

    for (level = 0; level < MAXHTBTREEDEPTH; level++) {
	for (priority = 0; priority < MAXHTBNUMPRIO; priority++) {

		leaf = levels[level]->nextToDequeue[priority];

		if (leaf)
			break;
	}

	if (leaf)
		break;
    }

    queue_id = leaf->leaf.queue_id; 
    NS_LOG_LOGIC("queue " << queue_id << " queue size " << GetInternalQueue(queue_id)->GetNPackets() << " priority " << priority);

    Ptr<const QueueDiscItem> itemPeek = GetInternalQueue(queue_id)->Peek();

    if (itemPeek) {
	uint32_t pktSize = itemPeek->GetSize();

	ChargeClass(leaf, pktSize);

	NS_LOG_LOGIC("Mode not sending " << leaf->mode);
	 if (leaf->mode != can_send) {
		return nullptr;
	 }

	// Actually dequeue
	Ptr<QueueDiscItem> item = GetInternalQueue(queue_id)->Dequeue();
	if (GetInternalQueue(queue_id)->GetNPackets() < 1) // queue size value online updated after dequeue
		DeactivateClass(leaf, priority);

        if (m_id.IsExpired()) {
    		Time requiredDelayTime;
    
    		requiredDelayTime = leaf->m_assignedRate.CalculateBytesTxTime(-pir_toks);
    		m_id = Simulator::Schedule(requiredDelayTime, &QueueDisc::Run, this);
    	}
	return item;
    }

    return nullptr;
}

bool
HtbQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);


    return true;
}

void
HtbQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);

    m_timeCheckPoint = Seconds(0);
    m_id = EventId();

    // rootClass->cirTokens = 0;
    struct htbClass* newClass = new htbClass();
    newClass->cirTokens = 10000;
    newClass->pirTokens = 10000;
    newClass->m_assignedBurstSize = 10000,
    newClass->m_ceilingBurstSize = 10000,
    newClass->level = 1;
    newClass->parent = nullptr;
    newClass->mode == can_send;

    rootClass = newClass;

    if (GetNInternalQueues() == 0) {

	    ObjectFactory factory;
	    factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");

	    for (uint32_t i = 0; i < 8; i++) {
	    	AddInternalQueue(factory.Create<InternalQueue>());
		struct htbLeaf node = { .priority = i, .queue_id = i };

		struct htbClass* leaf = new htbClass();
		leaf->level = 0;
		leaf->cirTokens = 10000;  // initialize later
		leaf->pirTokens = 10000;
		leaf->m_assignedBurstSize = 10000;
		leaf->m_ceilingBurstSize = 10000;
		leaf->m_assignedRate = DataRate("1bps");
		leaf->leaf = node;
		leaf->parent = rootClass;
    		leaf->mode == can_send;

		leafNodes.push_back(leaf);
		NS_LOG_LOGIC("Leaf" << leaf);
	    }
	}

    for (int level = 0; level < MAXHTBTREEDEPTH; level++) {
	htbLevel* newLevel = new htbLevel();

	levels.push_back(newLevel);
    }

}

} // namespace ns3
