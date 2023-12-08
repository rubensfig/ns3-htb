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
 * Authors: Surya Seetharaman <suryaseetharaman.9@gmail.com>
 *          Stefano Avallone <stavallo@unina.it>
 */

#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/htb-queue-disc.h"
#include "ns3/queue-disc.h"
#include "ns3/test.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/uinteger.h"

using namespace ns3;

/**
 * \ingroup traffic-control-test
 *
 * \brief Htb Queue Disc Test Item
 */
class HtbQueueDiscTestItem : public QueueDiscItem
{
  public:
    /**
     * Constructor
     *
     * \param p the packet
     * \param addr the address
     */
    HtbQueueDiscTestItem(Ptr<Packet> p, const Address& addr);
    ~HtbQueueDiscTestItem() override;

    // Delete default constructor, copy constructor and assignment operator to avoid misuse
    HtbQueueDiscTestItem() = delete;
    HtbQueueDiscTestItem(const HtbQueueDiscTestItem&) = delete;
    HtbQueueDiscTestItem& operator=(const HtbQueueDiscTestItem&) = delete;

    void AddHeader() override;
    bool Mark() override;
};

HtbQueueDiscTestItem::HtbQueueDiscTestItem(Ptr<Packet> p, const Address& addr)
    : QueueDiscItem(p, addr, 0)
{
}

HtbQueueDiscTestItem::~HtbQueueDiscTestItem()
{
}

void
HtbQueueDiscTestItem::AddHeader()
{
}

bool
HtbQueueDiscTestItem::Mark()
{
    return false;
}

/**
 * \ingroup traffic-control-test
 *
 * \brief Htb Queue Disc Test Case
 */
class HtbQueueDiscTestCase : public TestCase
{
  public:
    HtbQueueDiscTestCase();
    void DoRun() override;

  private:
    /**
     * Enqueue function
     * \param queue the queue disc into which enqueue needs to be done
     * \param dest the destination address
     * \param size the size of the packet in bytes to be enqueued
     */
    void Enqueue(Ptr<HtbQueueDisc> queue, Address dest, uint32_t size);
    /**
     * DequeueAndCheck function to check if a packet is blocked or not after dequeuing and verify
     * against expected result
     * \param queue the queue disc on which DequeueAndCheck needs to be done
     * \param flag the boolean value against which the return value of dequeue ()
     * has to be compared with
     * \param printStatement the string to be printed in the NS_TEST_EXPECT_MSG_EQ
     */
    void DequeueAndCheck(Ptr<HtbQueueDisc> queue, bool flag, std::string printStatement);
    /**
     * Run Htb test function
     * \param mode the mode
     */
    void RunHtbTest(QueueSizeUnit mode);
};

HtbQueueDiscTestCase::HtbQueueDiscTestCase()
    : TestCase("Sanity check on the Htb queue implementation")
{
}

void
HtbQueueDiscTestCase::RunHtbTest(QueueSizeUnit mode)
{
    uint32_t pktSize = 1500;
    // 1 for packets; pktSize for bytes
    uint32_t modeSize = 1;
    uint32_t qSize = 4;
    uint32_t burst = 6000;
    uint32_t mtu = 0;
    DataRate rate = DataRate("6KB/s");
    DataRate peakRate = DataRate("0KB/s");

    Ptr<HtbQueueDisc> queue = CreateObject<HtbQueueDisc>();

    // test 1: Simple Enqueue/Dequeue with verification of attribute setting
    /* 1. There is no second bucket since "peakRate" is set to 0.
       2. A simple enqueue of five packets, each containing 1500B is followed by
          the dequeue those five packets.
       3. The subtraction of tokens from the first bucket to send out each of the
          five packets is monitored and verified.
       Note : The number of tokens in the first bucket is full at the beginning.
              With the dequeuing of each packet, the number of tokens keeps decreasing.
              So packets are dequeued as long as there are enough tokens in the bucket. */

    Address dest;

    /*
    Ptr<Packet> p1;
    p1 = Create<Packet>(pktSize);
    Ptr<Packet> p2;
    p2 = Create<Packet>(pktSize);
    Ptr<Packet> p3;
    p3 = Create<Packet>(pktSize);
    Ptr<Packet> p4;
    p4 = Create<Packet>(pktSize);
*/
    queue->Initialize();
    NS_TEST_ASSERT_MSG_EQ(queue->GetCurrentSize().GetValue(),
                          0,
                          "There should be no packets in there");
    /*
    queue->Enqueue(Create<HtbQueueDiscTestItem>(p1, dest));
    NS_TEST_ASSERT_MSG_EQ(queue->GetCurrentSize().GetValue(),
                          1,
                          "There should be one packet in there");
    queue->Enqueue(Create<HtbQueueDiscTestItem>(p2, dest));
    NS_TEST_ASSERT_MSG_EQ(queue->GetCurrentSize().GetValue(),
                          2,
                          "There should be one packet in there");
    queue->Enqueue(Create<HtbQueueDiscTestItem>(p3, dest));
    queue->Enqueue(Create<HtbQueueDiscTestItem>(p4, dest));
    */

    uint32_t num_packets = 10;
    Ptr<Packet> p;

    SocketPriorityTag priorityTag;

    for (uint32_t i = 0; i < num_packets; i++) {

    	p = Create<Packet>(pktSize);
    	priorityTag.SetPriority(i%8);
    	p->ReplacePacketTag(priorityTag);
	queue->Enqueue(Create<HtbQueueDiscTestItem>(p, dest));
    }
    NS_TEST_ASSERT_MSG_EQ(queue->GetCurrentSize().GetValue(),
			  num_packets,
			  "There should be num_packets packet in there");

    Ptr<QueueDiscItem> item;

    for (uint32_t i = num_packets; i != 0; i--) 
	{
		item = queue->Dequeue();
		
	}
    NS_TEST_ASSERT_MSG_EQ(queue->GetCurrentSize().GetValue(),
		  0,
		  "There should be one packet in there");

}

void
HtbQueueDiscTestCase::Enqueue(Ptr<HtbQueueDisc> queue, Address dest, uint32_t size)
{
    queue->Enqueue(Create<HtbQueueDiscTestItem>(Create<Packet>(size), dest));
}

void
HtbQueueDiscTestCase::DequeueAndCheck(Ptr<HtbQueueDisc> queue,
                                      bool flag,
                                      std::string printStatement)
{
    Ptr<QueueDiscItem> item = queue->Dequeue();
    NS_TEST_EXPECT_MSG_EQ((item != nullptr), flag, printStatement);
}

void
HtbQueueDiscTestCase::DoRun()
{
    RunHtbTest(QueueSizeUnit::PACKETS);
    Simulator::Destroy();
}

/**
 * \ingroup traffic-control-test
 *
 * \brief Htb Queue Disc Test Suite
 */
static class HtbQueueDiscTestSuite : public TestSuite
{
  public:
    HtbQueueDiscTestSuite()
        : TestSuite("htb-queue-disc", UNIT)
    {
        AddTestCase(new HtbQueueDiscTestCase(), TestCase::QUICK);
    }
} g_HtbQueueTestSuite; ///< the test suite
