/*
MIT License

Copyright (c) 2018 Meng Rao <raomeng1@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <string>
#include <array>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "tcpshm_conn.h"

namespace tcpshm {

template<class Derived, class Conf>
class TcpShmClient
{
public:
    using Connection = TcpShmConnection<Conf>;
    using LoginMsg = LoginMsgTpl<Conf>;
    using LoginRspMsg = LoginRspMsgTpl<Conf>;

protected:
    TcpShmClient(const std::string& client_name, const std::string& ptcp_dir)
        : ptcp_dir_(ptcp_dir) {
        strncpy(client_name_, client_name.c_str(), sizeof(client_name_) - 1);
        mkdir(ptcp_dir_.c_str(), 0755);
        client_name_[sizeof(client_name_) - 1] = 0;
        conn_.init(ptcp_dir.c_str(), client_name_);
    }

    ~TcpShmClient() {
        Stop();
    }

    int SocketUnixfd(std::string& sock_addr) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if(fd < 0) {
            static_cast<Derived*>(this)->OnSystemError("socket unix", errno);
            return -1;
        }

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) ) {
            static_cast<Derived*>(this)->OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return -1;
        }

        if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return -1;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_addr.c_str(), sock_addr.size());

        if(connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("connect", errno);
            close(fd);
            return -1;
        }

        return fd;
    }

    int SocketInetfd(const char* server_ipv4, uint16_t server_port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd < 0) {
            static_cast<Derived*>(this)->OnSystemError("socket", errno);
            return -1;
        }
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return -1;
        }

        if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return -1;
        }
        int yes = 1;
        if(Conf::TcpNoDelay && setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("setsockopt TCP_NODELAY", errno);
            close(fd);
            return -1;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, server_ipv4, &(server_addr.sin_addr));
        server_addr.sin_port = htons(server_port);
        bzero(&(server_addr.sin_zero), 8);

        if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            static_cast<Derived*>(this)->OnSystemError("connect", errno);
            close(fd);
            return -1;
        }

        return fd;
    }

    int SendLoginMsg(int fd, MsgHeader* sendbuf, int sendsize, int fd_arr[2]) {
        struct msghdr msg;
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;

        struct iovec iov;
        iov.iov_base = sendbuf;
        iov.iov_len = sendsize;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        std::unique_ptr<char> cmsg_buf;
#ifdef __ANDROID__
        LoginMsg* login = (LoginMsg*)(sendbuf + 1);
        if (login->use_shm) {
            uint32_t fd_mem_size = 2 * sizeof(int);     //send fd and recv fd.
            cmsg_buf.reset(new char[CMSG_SPACE(fd_mem_size)]);
            msg.msg_control = (caddr_t)cmsg_buf.get();
            msg.msg_controllen = CMSG_LEN(fd_mem_size);
            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_len = CMSG_LEN(fd_mem_size);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            memcpy(CMSG_DATA(cmsg), fd_arr, fd_mem_size);
        } else
#endif
        {
            msg.msg_control = nullptr;
            msg.msg_controllen = 0;
        }
        msg.msg_flags = 0;

        return sendmsg(fd, &msg, MSG_NOSIGNAL);
    }

    // connect and login to server, may block for a short time
    // return true if success
    bool Connect(bool use_shm,
                 const char* server_ipv4,
                 uint16_t server_port,
                 const typename Conf::LoginUserData& login_user_data) {
        if(!conn_.IsClosed()) {
            static_cast<Derived*>(this)->OnSystemError("already connected", 0);
            return false;
        }
        conn_.TryCloseFd();
        const char* error_msg;
        if(!server_name_) {
            std::string last_server_name_file = std::string(ptcp_dir_) + "/" + client_name_ + ".lastserver";
            server_name_ = (char*)tcp_mmap<ServerName>(last_server_name_file.c_str(), &error_msg);
            if(!server_name_) {
                static_cast<Derived*>(this)->OnSystemError(error_msg, errno);
                return false;
            }
            strncpy(conn_.GetRemoteName(), server_name_, sizeof(ServerName));
        }
        MsgHeader sendbuf[1 + BlockCount(sizeof(LoginMsg))];
        sendbuf[0].size = sizeof(MsgHeader) + sizeof(LoginMsg);
        sendbuf[0].msg_type = LoginMsg::msg_type;
        sendbuf[0].ack_seq = 0;
        LoginMsg* login = (LoginMsg*)(sendbuf + 1);
        strncpy(login->client_name, client_name_, sizeof(login->client_name));
        strncpy(login->last_server_name, server_name_, sizeof(login->last_server_name));
        login->use_shm = use_shm;
        login->client_seq_start = login->client_seq_end = 0;
        login->user_data = login_user_data;
        login->client_shm_send_fd = -1;
        login->client_shm_recv_fd = -1;
#ifdef __ANDROID__
        bool use_ashm = use_shm;
#else
        bool use_ashm = false;
#endif
        if((server_name_[0] || use_ashm) &&
           (!conn_.OpenFile(use_shm, &error_msg) ||
            !conn_.GetSeq(&sendbuf[0].ack_seq, &login->client_seq_start, &login->client_seq_end, &error_msg))) {
            static_cast<Derived*>(this)->OnSystemError(error_msg, errno);
            return false;
        }

        int fd;
        if(use_shm) {
            std::string sock_addr("/data/tmp/tcpshm_server");
            fd = SocketUnixfd(sock_addr);
        } else {
            fd = SocketInetfd(server_ipv4, server_port);
        }
        if(fd < 0) {
            return false;
        }

        int sendsize = (1 + BlockCount(sizeof(LoginMsg))) * BLK_SIZE;
        int fd_arr[2] = { conn_.shm_send_fd(), conn_.shm_recv_fd() };
        int ret = SendLoginMsg(fd, &sendbuf[0], sendsize, fd_arr);
        //int ret = sendmsg(fd, &msg, MSG_NOSIGNAL);
        if(ret <= 0) {
            static_cast<Derived*>(this)->OnSystemError("send", ret < 0 ? errno : 0);
            close(fd);
            return false;
        }

        MsgHeader recvbuf[1 + BlockCount(sizeof(LoginRspMsg))];
        ret = recv(fd, recvbuf, sizeof(recvbuf), 0);
        if(ret != sizeof(recvbuf)) {
            static_cast<Derived*>(this)->OnSystemError("recv", ret < 0 ? errno : 0);
            close(fd);
            return false;
        }

        LoginRspMsg* login_rsp = (LoginRspMsg*)(recvbuf + 1);
        recvbuf[0].template ConvertByteOrder<Conf::ToLittleEndian>();
        login_rsp->ConvertByteOrder();
        if(recvbuf[0].size != sizeof(MsgHeader) + sizeof(LoginRspMsg) || recvbuf[0].msg_type != LoginRspMsg::msg_type ||
           login_rsp->server_name[0] == 0) {
            static_cast<Derived*>(this)->OnSystemError("Invalid LoginRsp", 0);
            close(fd);
            return false;
        }
        if(login_rsp->status != 0) {
            if(login_rsp->status == 1) { // seq number mismatch
                sendbuf[0].template ConvertByteOrder<Conf::ToLittleEndian>();
                login->ConvertByteOrder();
                static_cast<Derived*>(this)->OnSeqNumberMismatch(sendbuf[0].ack_seq,
                                                                 login->client_seq_start,
                                                                 login->client_seq_end,
                                                                 recvbuf[0].ack_seq,
                                                                 login_rsp->server_seq_start,
                                                                 login_rsp->server_seq_end);
            }
            else {
                static_cast<Derived*>(this)->OnLoginReject(login_rsp);
            }
            close(fd);
            return false;
        }
        login_rsp->server_name[sizeof(login_rsp->server_name) - 1] = 0;
        // check if server name has changed
        if(strncmp(server_name_, login_rsp->server_name, sizeof(ServerName)) != 0 && !use_ashm) {
            conn_.Release();
            strncpy(server_name_, login_rsp->server_name, sizeof(ServerName));
            strncpy(conn_.GetRemoteName(), server_name_, sizeof(ServerName));
            if(!conn_.OpenFile(use_shm, &error_msg)) {
                static_cast<Derived*>(this)->OnSystemError(error_msg, errno);
                close(fd);
                return false;
            }
            conn_.Reset();
        }
        fcntl(fd, F_SETFL, O_NONBLOCK);
        int64_t now = static_cast<Derived*>(this)->OnLoginSuccess(login_rsp);

        conn_.Open(fd, recvbuf[0].ack_seq, now);
        return true;
    }

    // we need to PollTcp even if using shm
    void PollTcp(int64_t now) {
        if(!conn_.IsClosed()) {
            MsgHeader* head = conn_.TcpFront(now);
            if(head) static_cast<Derived*>(this)->OnServerMsg(head);
            //static_cast<Derived*>(this)->OnServerMsg(head);
        }
        if(conn_.TryCloseFd()) {
            int sys_errno;
            const char* reason = conn_.GetCloseReason(&sys_errno);
            static_cast<Derived*>(this)->OnDisconnected(reason, sys_errno);
        }
    }

    // only for using shm
    void PollShm() {
        MsgHeader* head = conn_.ShmFront();
        if(head) static_cast<Derived*>(this)->OnServerMsg(head);
    }

    // stop the connection and close files
    void Stop() {
        if(server_name_) {
            tcp_munmap<ServerName>(server_name_);
            server_name_ = nullptr;
        }
        conn_.Release();
    }

    // get the connection reference which can be kept by user as long as TcpShmClient is not destructed
    Connection& GetConnection() {
        return conn_;
    }

private:
    char client_name_[Conf::NameSize];
    using ServerName = std::array<char, Conf::NameSize>;
    char* server_name_ = nullptr;
    std::string ptcp_dir_;
    Connection conn_;
};
} // namespace tcpshm
