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
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __ANDROID__
#include <linux/ashmem.h>
#endif
#include <iostream>
#include <fstream>

namespace tcpshm {

template<class T>
  class MyMMap {
  public:
    MyMMap()
      : fd_(-1)
      , fd_setted(false)
    {
    }

    int open_fd(const char* filename, bool use_shm, const char** error_msg) {
        if(use_shm) {
#ifdef __ANDROID__
            fd_ = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
#else
            fd_ = shm_open(filename, O_CREAT | O_RDWR, 0666);
#endif
        }
        else {
            fd_ = open(filename, O_CREAT | O_RDWR, 0644);
        }
        if(fd_ < 0) {
            *error_msg = "open";
        }
        return fd_;
    }

    T* my_mmap(bool use_shm, const char** error_msg) {
        if(fd_ == -1) {
            *error_msg = "mmap not open";
            return nullptr;
        }
#ifdef __ANDROID__
        if(use_shm) {
            if(fd_setted) {
                int ret = ioctl(fd_, ASHMEM_GET_SIZE, NULL);
                if(ret != sizeof(T)) {
                    *error_msg = "ioctl";
                    return nullptr;
                }
            } else {
                int ret = ioctl(fd_, ASHMEM_SET_SIZE, sizeof(T));
                if(ret) {
                    *error_msg = "ioctl";
                    return nullptr;
                }
            }
        } else
#endif
        {
            int ret = ftruncate(fd_, sizeof(T));
            if(ret) {
                *error_msg = "ftruncate";
                return nullptr;
            }
        }
        T* map_ptr = (T*)mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if(map_ptr == MAP_FAILED) {
            *error_msg = "mmap";
            return nullptr;
        }
        return map_ptr;
    }

    void my_munmap(void* addr) {
        munmap(addr, sizeof(T));
    }

    void close_fd() {
        if(fd_ != -1 && !fd_setted) {
            close(fd_);
            fd_ = -1;
        }
    }
    int fd() { return fd_; }

    void set_fd(int fd) {
        fd_ = fd;
        fd_setted = true;
    }

  private:
    int fd_;
    bool fd_setted;
};

template<class T>
T* tcp_mmap(const char* filename, const char** error_msg) {
    MyMMap<T> tcp_map;
    int fd = tcp_map.open_fd(filename, false, error_msg);
    if(fd < 0) {
        return nullptr;
    }

    T* map_ptr = tcp_map.my_mmap(false, error_msg);
    tcp_map.close_fd();
    return map_ptr;
}

template<class T>
void tcp_munmap(void* addr) {
    MyMMap<T> tcp_map;
    tcp_map.my_munmap(addr);
}

} // namespace tcpshm
