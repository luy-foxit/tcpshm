#include "../tcpshm_server.h"
//#include <bits/stdc++.h>
#include <unordered_map>
#include <signal.h>
#include "timestamp.h"
#include "common.h"
#include "cpupin.h"
#include "frame_dispatcher.h"
#include "logging.h"

using namespace std;
using namespace tcpshm;

static std::unordered_map<void*, int> g_id_map;

struct ServerConf : public CommonConf
{
  static const int64_t NanoInSecond = 1000000000LL;

  static const uint32_t MaxNewConnections = 5;
  static const uint32_t MaxShmConnsPerGrp = 4;
  static const uint32_t MaxShmGrps = 1;
  static const uint32_t MaxTcpConnsPerGrp = 4;
  static const uint32_t MaxTcpGrps = 1;

  // echo server's TcpQueueSize should be larger than that of client if client is in fast mode
  // otherwise server's send queue could be blocked and ack_seq can only be sent through HB which is slow

  static const uint32_t TcpQueueSize = 1024 * 1200; //3000;       // must be a multiple of 16
  static const uint32_t TcpRecvBufInitSize = 640*480 + 16; // must be a multiple of 16
  static const uint32_t TcpRecvBufMaxSize;// = 640*480 * 2;  // must be a multiple of 16
  //static const uint32_t TcpQueueSize = 1920;       // must be a multiple of 16
  //static const uint32_t TcpRecvBufInitSize = 960; // must be a multiple of 16
  //static const uint32_t TcpRecvBufMaxSize = 1920;  // must be a multiple of 16

  static const bool TcpNoDelay = true;

  static const int64_t NewConnectionTimeout = 3 * NanoInSecond;
  static const int64_t ConnectionTimeout = 10 * NanoInSecond;
  static const int64_t HeartBeatInverval = 3 * NanoInSecond;

  using ConnectionUserData = char;
};
const uint32_t ServerConf::TcpRecvBufMaxSize = 640*480 * 2;  // must be a multiple of 16

static void SignalHandler(int s) {
    gDemoStopped = true;
}

class CameraServer;
using TSServer = TcpShmServer<CameraServer, ServerConf>;

class CameraServer : public TSServer
{
public:
    CameraServer(const std::string& ptcp_dir, const std::string& name)
        : TSServer(name, ptcp_dir) {
        // capture SIGTERM to gracefully stop the server
        // we can also send other signals to crash the server and see how it recovers on restart

    }

    ~CameraServer() {
        //delete []image_buf;
    }

    void Run(const char* listen_ipv4, uint16_t listen_port) {
        if(!Start(listen_ipv4, listen_port)) return;
        vector<thread> threads;
        // create threads for polling tcp
        for(int i = 0; i < ServerConf::MaxTcpGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + i);
            while (!gDemoStopped) {
              usleep(30000);     //sleep 30 ms
              PollTcp(now(), i);
            }
          });
        }

        // create threads for polling shm
        for(int i = 0; i < ServerConf::MaxShmGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + ServerConf::MaxTcpGrps + i);
            while (!gDemoStopped) {
              usleep(30000);     //sleep 30 ms
              PollShm(i);
            }
          });
        }

        // polling control using this thread
        while(!gDemoStopped) {
          usleep(30000);     //sleep 30 ms
          PollCtl(now());
        }

        for(auto& thr : threads) {
            thr.join();
        }
        Stop();
        cout << "Server stopped" << endl;
    }

private:
    friend TSServer;

    // called with Start()
    // reporting errors on Starting the server
    void OnSystemError(const char* errno_msg, int sys_errno) {
        cout << "System Error: " << errno_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // if accept the connection, set user_data in login_rsp and return grpid(start from 0) with respect to tcp or shm
    // else set error_msg in login_rsp if possible, and return -1
    // Note that even if we accept it here, there could be other errors on handling the login,
    // so we have to wait OnClientLogon for confirmation
    int OnNewConnection(const struct sockaddr_in& addr, const LoginMsg* login, LoginRspMsg* login_rsp) {
        cout << "New Connection from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << login->client_name << ", use_shm: " << (bool)login->use_shm << endl;
        // here we simply hash client name to uniformly map to each group
        auto hh = hash<string>{}(string(login->client_name));
        if(login->use_shm) {
            if(ServerConf::MaxShmGrps > 0) {
                return hh % ServerConf::MaxShmGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Shm disabled");
                return -1;
            }
        }
        else {
            if(ServerConf::MaxTcpGrps > 0) {
                return hh % ServerConf::MaxTcpGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Tcp disabled");
                return -1;
            }
        }
    }

    // called by CTL thread
    // ptcp or shm files can't be open or are corrupt
    void OnClientFileError(Connection& conn, const char* reason, int sys_errno) {
        cout << "Client file errno, name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(Connection& conn,
                             uint32_t local_ack_seq,
                             uint32_t local_seq_start,
                             uint32_t local_seq_end,
                             uint32_t remote_ack_seq,
                             uint32_t remote_seq_start,
                             uint32_t remote_seq_end) {
        cout << "Client seq number mismatch, name: " << conn.GetRemoteName() << " ptcp file: " << conn.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by CTL thread
    // confirmation for client logon
    void OnClientLogon(const struct sockaddr_in& addr, Connection& conn) {
        cout << "Client Logon from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << conn.GetRemoteName() << endl;
    }

    // called by CTL thread
    // client is disconnected
    void OnClientDisconnected(Connection& conn, const char* reason, int sys_errno) {
        cout << "Client disconnected,.name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by APP thread
    void OnClientMsg(Connection& conn, MsgHeader* recv_header) {
        std::lock_guard<std::mutex> lock(g_image_arr.producers_mutex);

        if(g_image_arr.curr_idx < 0) {
            return;
        }

        auto it = g_id_map.find((void*)(&conn));
        if(it == g_id_map.end()) {
            g_id_map[(void*)(&conn)] = g_image_arr.curr_idx;
        } else {
            if(it->second >= g_image_arr.curr_idx) {
                return;
            }
            it->second = g_image_arr.curr_idx;
        }

        MonoImageInfo& info = g_image_arr.image_list[g_image_arr.curr_idx % g_image_arr.max_image_num];

        MsgHeader* send_header = conn.Alloc(info.size);
        if(!send_header) {
            LOG(ERROR) << "header alloc failed";
            return;
        }
        send_header->msg_type = 4;
        memcpy(send_header + 1, info.image_buf.get(), info.size);

        conn.Pop();     //only for tcp
        conn.Push();
    }

    // set do_cpupin to true to get more stable latency
#ifdef __ANDROID__
    bool do_cpupin = false;
#else
    bool do_cpupin = true;
#endif
};

int main() {

    signal(SIGTERM, SignalHandler);
    //signal(SIGINT, SignalHandler);

    std::thread camrea_thread = std::thread(CameraThread);

    {
        std::string ptcp_dir("server");
        std::string server_name("camera_server");
        CameraServer server(ptcp_dir, server_name);
        server.Run("0.0.0.0", 12345);
    }

    camrea_thread.join();

    return 0;
}
