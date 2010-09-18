#pragma once

#include "fixed_types.h"
#include "queue_model.h"
#include "queue_model_m_g_1.h"
#include "interval_tree.h"

class QueueModelHistoryTree : public QueueModel
{
public:
   QueueModelHistoryTree(UInt64 min_processing_time);
   ~QueueModelHistoryTree();

   UInt64 computeQueueDelay(UInt64 pkt_time, UInt64 processing_time, core_id_t requester = INVALID_CORE_ID);
   
   float getQueueUtilization();
   UInt64 getTotalRequestsUsingAnalyticalModel() { return _total_requests_using_analytical_model; }
   UInt64 getTotalRequests() { return _total_requests; }

private:
   void initializeQueueCounters();
   void updateQueueCounters(UInt64 processing_time);

   void allocateMemory();
   void releaseMemory();
   IntervalTree::Node* allocateNode(pair<UInt64,UInt64> interval);
   void releaseNode(IntervalTree::Node* node);

   // Private Fields
   QueueModelMG1* _queue_model_m_g_1;
   IntervalTree* _interval_tree;
   
   // Is analytical model used ?
   bool _analytical_model_enabled;
   
   UInt64 _min_processing_time;
   SInt32 _max_free_interval_size;
   UInt64 _MAX_CYCLE_COUNT;
   
   IntervalTree::Node* _memory_blocks;
   SInt32* _free_memory_block_list;
   SInt32 _free_memory_block_list_tail;

   // Queue Counters
   UInt64 _total_requests;
   UInt64 _total_requests_using_analytical_model;
   UInt64 _total_utilized_cycles;
};
