#ifndef _FRAME_DISPATCHER_H_
#define _FRAME_DISPATCHER_H_

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>

extern bool gDemoStopped;

class MonoImageInfo {
public:
    MonoImageInfo();
    ~MonoImageInfo();

    void Clear();
    bool Empty() { (image_buf == nullptr); }

public:
    std::shared_ptr<uint8_t> image_buf;
    int size;
    double timestamp;
};

struct MonoImageArray {
    MonoImageArray()
       : start_idx(-1)
       , curr_idx(-1)
    {
        image_list.resize(max_image_num);
    }
    ~MonoImageArray() {
        image_list.clear();
        start_idx = -1;
        curr_idx = -1;
    }

    static const int max_image_num = 30;
    std::vector<MonoImageInfo> image_list;
    int start_idx;
    int curr_idx;

    std::mutex producers_mutex;
}; 
extern struct MonoImageArray g_image_arr;

class FrameDispatcher {
public:
    FrameDispatcher();
    ~FrameDispatcher();
    void OnFrame();

private:
    static const int image_width_ = 640;
    static const int image_height_ = 480; //480;
    unsigned long long image_count_;
};

extern void CameraThread();

#endif //_FRAME_DISPATCHER_H_
