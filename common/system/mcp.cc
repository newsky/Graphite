#include "mcp.h"
#include "config.h"

#include "log.h"
#include "core.h"
#include "simulator.h"
#include <syscall.h>
#define LOG_DEFAULT_RANK m_network.getCore()->getId()
#define LOG_DEFAULT_MODULE MCP

#include <sched.h>
#include <iostream>
using namespace std;

MCP::MCP(Network & network)
      :
      m_finished(false),
      m_network(network),
      m_MCP_SERVER_MAX_BUFF(256*1024),
      m_scratch(new char[m_MCP_SERVER_MAX_BUFF]),
      m_syscall_server(m_network, m_send_buff, m_recv_buff, m_MCP_SERVER_MAX_BUFF, m_scratch),
      m_sync_server(m_network, m_recv_buff),
      m_network_model_analytical_server(m_network, m_recv_buff)
{
}

MCP::~MCP()
{
   delete[] m_scratch;
}

void MCP::processPacket()
{
   m_send_buff.clear();
   m_recv_buff.clear();

   NetPacket recv_pkt;

   NetMatch match;
   match.types.push_back(MCP_REQUEST_TYPE);
   match.types.push_back(MCP_SYSTEM_TYPE);
   recv_pkt = m_network.netRecv(match);

   m_recv_buff << make_pair(recv_pkt.data, recv_pkt.length);

   int msg_type;

   m_recv_buff >> msg_type;

   LOG_PRINT("MCP message type : %i", (SInt32)msg_type);

   switch (msg_type)
   {
   case MCP_MESSAGE_SYS_CALL:
      m_syscall_server.handleSyscall(recv_pkt.sender);
      break;
   case MCP_MESSAGE_QUIT:
      LOG_PRINT("Quit message received.");
      m_finished = true;
      break;
   case MCP_MESSAGE_MUTEX_INIT:
      m_sync_server.mutexInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_MUTEX_LOCK:
      m_sync_server.mutexLock(recv_pkt.sender);
      break;
   case MCP_MESSAGE_MUTEX_UNLOCK:
      m_sync_server.mutexUnlock(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_INIT:
      m_sync_server.condInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_WAIT:
      m_sync_server.condWait(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_SIGNAL:
      m_sync_server.condSignal(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_BROADCAST:
      m_sync_server.condBroadcast(recv_pkt.sender);
      break;
   case MCP_MESSAGE_BARRIER_INIT:
      m_sync_server.barrierInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_BARRIER_WAIT:
      m_sync_server.barrierWait(recv_pkt.sender);
      break;
   case MCP_MESSAGE_UTILIZATION_UPDATE:
      m_network_model_analytical_server.update(recv_pkt.sender);
      break;
   case MCP_MESSAGE_BROADCAST_COMM_MAP_UPDATE:
      recv_pkt.type = SIM_THREAD_UPDATE_COMM_MAP;
      broadcastPacketToProcesses(recv_pkt);
      break;
   default:
      LOG_ASSERT_ERROR(false, "Unhandled MCP message type: %i from %i", msg_type, recv_pkt.sender);
   }

   delete [](Byte*)recv_pkt.data;
}

void MCP::finish()
{
   LOG_PRINT("Send MCP quit message");

   int msg_type = MCP_MESSAGE_QUIT;
   m_network.netSend(Config::getSingleton()->getMCPCoreNum(), MCP_SYSTEM_TYPE, &msg_type, sizeof(msg_type));

   while (!finished())
   {
      sched_yield();
   }

   LOG_PRINT("MCP Finished.");
}

void MCP::broadcastPacket(NetPacket pkt)
{
   pkt.sender = Config::getSingleton()->getMCPCoreNum();

   for (UInt32 core_id = 0; core_id < Config::getSingleton()->getTotalCores(); core_id++)
   {
      pkt.receiver = core_id;
      m_network.netSend(pkt);
   }
}

void MCP::broadcastPacketToProcesses(NetPacket pkt)
{
   pkt.sender = Config::getSingleton()->getMCPCoreNum();

   for (UInt32 proc_id = 0; proc_id < Config::getSingleton()->getProcessCount(); proc_id++)
   {
      pkt.receiver = Config::getSingleton()->getCoreListForProcess(proc_id)[0];

      LOG_PRINT("CoreMap: Sending process broadcast to core: %d", pkt.receiver); 

      m_network.netSend(pkt);

      // Wait for a response
      NetPacket recv_pkt;
      NetMatch match;
      match.types.push_back(MCP_RESPONSE_TYPE);
      recv_pkt = m_network.netRecv(match);
      delete [] (Byte*)recv_pkt.data;
   }
}


void MCP::forwardPacket(NetPacket pkt)
{
   pkt.sender = Config::getSingleton()->getMCPCoreNum();

   assert(pkt.receiver != -1);

   m_network.netSend(pkt);
}

void MCP::run()
{
   int tid =  syscall(__NR_gettid);
   LOG_PRINT("Initializing the MCP (%i) with id: %i", (int)tid, Config::getSingleton()->getTotalCores()-1);

   int mcp_core_num = Config::getSingleton()->getMCPCoreNum();
   LOG_PRINT("%p", Sim());
   LOG_PRINT("%p", Sim()->getCoreManager());
   Sim()->getCoreManager()->initializeThread(mcp_core_num);
   Sim()->getCoreManager()->initializeCommId(mcp_core_num);

   while (!finished())
   {
      processPacket();
   }
}
