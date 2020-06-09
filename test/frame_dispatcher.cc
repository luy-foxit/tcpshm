#include "frame_dispatcher.h"
#include <unistd.h>
//#include <stdio.h>

bool gDemoStopped = false;
struct MonoImageArray g_image_arr;

MonoImageInfo::MonoImageInfo() {
    Clear();
}

MonoImageInfo::~MonoImageInfo() {
    Clear();
}

void MonoImageInfo::Clear() {
    image_buf = nullptr;
    size = 0;
    timestamp = 0.;
}

FrameDispatcher::FrameDispatcher() 
     : image_count_(0)
{
}

FrameDispatcher::~FrameDispatcher() {
    g_image_arr.image_list.clear();
    g_image_arr.start_idx = -1;
    g_image_arr.curr_idx = -1;
}

void FrameDispatcher::OnFrame() {
    std::lock_guard<std::mutex> lock(g_image_arr.producers_mutex);

    g_image_arr.curr_idx++;
    if(g_image_arr.curr_idx < 0) {
        return;
    }
    MonoImageInfo& info = g_image_arr.image_list[g_image_arr.curr_idx % g_image_arr.max_image_num];

    int size = image_width_ * image_height_;
    int llsize = size / 8;
    info.size = size;
    info.image_buf.reset(new uint8_t[size]);
    ((unsigned long long*)info.image_buf.get())[0] = image_count_;
    ((unsigned long long*)info.image_buf.get())[llsize - 2] = image_count_;
    info.timestamp = 0;
    image_count_++;
}

void CameraThread() {
    FrameDispatcher disp;
    while(!gDemoStopped) {
        usleep(100000); //sleep 100ms
        disp.OnFrame();
    }
}
