#include "../tcpshm_client.h"
#include <bits/stdc++.h>
#include "timestamp.h"
#include "common.h"
#include "cpupin.h"

using namespace std;
using namespace tcpshm;

struct ClientConf : public CommonConf
{
  static const int64_t NanoInSecond = 1000000000LL;

  static const uint32_t TcpQueueSize = 2000;       // must be a multiple of 8
  static const uint32_t TcpRecvBufInitSize = 1000; // must be a multiple of 8
  static const uint32_t TcpRecvBufMaxSize = 2000;  // must be a multiple of 8
  static const bool TcpNoDelay = true;

  static const int64_t ConnectionTimeout = 10 * NanoInSecond;
  static const int64_t HeartBeatInverval = 3 * NanoInSecond;

  using ConnectionUserData = char;
};

class EchoClient;
using TSClient = TcpShmClient<EchoClient, ClientConf>;

class EchoClient : public TSClient
{
public:
    EchoClient(const std::string& ptcp_dir, const std::string& name)
        : TSClient(ptcp_dir, name)
        , conn(GetConnection()) {
        srand(time(NULL));
        signal(SIGINT, EchoClient::SignalHandler);
    }

    static void SignalHandler(int s) {
        std::cout << "Catch signal" << std::endl;
        stopped = true;
    }

    void Run(bool use_shm, const char* server_ipv4, uint16_t server_port) {
        if(!Connect(use_shm, server_ipv4, server_port, 0)) return;
        // we mmap the send and recv number to file in case of program crash
        if(use_shm) {
            thread shm_thr([this]() {
                if(do_cpupin) cpupin(7);
                start_time = now();
                while(!conn.IsClosed() && !stopped) {
                    PollShm();
                }
                stop_time = now();
            });

            // we still need to poll tcp for heartbeats even if using shm
            while(!conn.IsClosed() && !stopped) {
              PollTcp(now());
            }
            shm_thr.join();
        }
        else {
            if(do_cpupin) cpupin(7);
            start_time = now();
            while(!conn.IsClosed() && !stopped) {
                PollTcp(now());
            }
            stop_time = now();
        }
        uint64_t latency = stop_time - start_time;
        Stop();
    }

private:

private:
    friend TSClient;
    // called within Connect()
    // reporting errors on connecting to the server
    void OnSystemError(const char* error_msg, int sys_errno) {
        cout << "System Error: " << error_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called within Connect()
    // Login rejected by server
    void OnLoginReject(const LoginRspMsg* login_rsp) {
        cout << "Login Rejected: " << login_rsp->error_msg << endl;
    }

    // called within Connect()
    // confirmation for login success
    int64_t OnLoginSuccess(const LoginRspMsg* login_rsp) {
        cout << "Login Success" << endl;
        return now();
    }

    // called within Connect()
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(uint32_t local_ack_seq,
                             uint32_t local_seq_start,
                             uint32_t local_seq_end,
                             uint32_t remote_ack_seq,
                             uint32_t remote_seq_start,
                             uint32_t remote_seq_end) {
        cout << "Seq number mismatch, name: " << conn.GetRemoteName() << " ptcp file: " << conn.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by APP thread
    void OnServerMsg(MsgHeader* header) {
        auto size = header->size - sizeof(MsgHeader);
        std::cout << "OnServerMsg. recv size:" << (int)size << std::endl;
        unsigned char* data = (unsigned char*)(header + 1);
        std::cout << "OnServerMsg. recv data:" << ((unsigned long long*)data)[0] << std::endl;
        conn.Pop();
    }

    // called by tcp thread
    void OnDisconnected(const char* reason, int sys_errno) {
        cout << "Client disconnected reason: " << reason << " syserrno: " << strerror(sys_errno) << endl;
    }

private:
    static const int MaxNum = 10000000;
    Connection& conn;
    int msg_sent = 0;
    uint64_t start_time = 0;
    uint64_t stop_time = 0;
    // set slow to false to send msgs as fast as it can
    bool slow = true;
    // set do_cpupin to true to get more stable latency
    bool do_cpupin = true;
    int* send_num;
    int* recv_num;

public:
    static volatile bool stopped;
};

volatile bool EchoClient::stopped = false;

int main(int argc, const char** argv) {
    if(argc != 4) {
        cout << "usage: echo_client NAME SERVER_IP USE_SHM[0|1]" << endl;
        exit(1);
    }
    const char* name = argv[1];
    const char* server_ip = argv[2];
    bool use_shm = argv[3][0] != '0';

    EchoClient client(name, name);
    client.Run(use_shm, server_ip, 12345);

    return 0;
}

