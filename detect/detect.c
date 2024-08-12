#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <curl/curl.h>

// 设置全局变量
#define MAX_EVENTS 10
#define FRAME_BUFFER_COUNT 4
#define SERVER_URL "http://192.168.0.104:5000/upload"

unsigned char *frame_buffers[FRAME_BUFFER_COUNT];//用于保存内存映射到用户态缓冲区buffer
struct v4l2_buffer buffer_info[FRAME_BUFFER_COUNT];//用于保存内存映射到用户态缓冲区buffer初始化信息以用于管理buffer
int fd;

pthread_mutex_t lock;
pthread_cond_t cond;
int frame_ready[FRAME_BUFFER_COUNT] = {0};//每当队列中一个buffer准备好进行处理时，frame_ready 数组中的相应元素就会被设置为1，表示该帧已经准备好进行上传。

// 上传视频帧
void upload_frame(const unsigned char *buffer, size_t length) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, length);
        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();
}

// 处理捕获线程
void *capture_thread(void *arg) {
    int i;
    while(1) {
        struct v4l2_buffer readbuffer;
        memset(&readbuffer, 0, sizeof(readbuffer));
        readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        readbuffer.memory = V4L2_MEMORY_MMAP;
        //获取空闲buffer：查找frame_ready中未被使用的buffer，将其索引赋值给readbuffer.index，帧数据优先存入该索引位置
        pthread_mutex_lock(&lock);
        for (i = 0; i < FRAME_BUFFER_COUNT; ++i) {
            if (!frame_ready[i]) {
                readbuffer.index = i;
                break;
            }
        }
        pthread_mutex_unlock(&lock);
        //检查是否所有缓冲区都在使用中
        if (i == FRAME_BUFFER_COUNT) {
            pthread_mutex_lock(&lock);
            pthread_cond_wait(&cond, &lock);
            pthread_mutex_unlock(&lock);
            continue;
        }
        //从缓冲区中队列中取出已经捕获的好的视频帧数据
        ioctl(fd, VIDIOC_DQBUF, &readbuffer);
        pthread_mutex_lock(&lock);
        frame_ready[readbuffer.index] = 1;//标记该缓冲区数据已经准备好被处理
        pthread_cond_signal(&cond);//通知解除阻塞
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// 处理上传线程
void *upload_thread(void *arg) {
    int i;
    while(1) {
        pthread_mutex_lock(&lock);
        //从完成链表中取出数据
        for (i = 0; i < FRAME_BUFFER_COUNT; ++i) {
            if (frame_ready[i]) {
                frame_ready[i] = 0;
                break;
            }
        }
        pthread_mutex_unlock(&lock);
        //检查是否所有缓冲区都在使用中：
        if (i == FRAME_BUFFER_COUNT) {
            pthread_mutex_lock(&lock);
            pthread_cond_wait(&cond, &lock);
            pthread_mutex_unlock(&lock);
            continue;
        }
        //上传帧数据
        upload_frame(frame_buffers[i], buffer_info[i].length);
        //将缓冲区重新放回空闲链表
        ioctl(fd, VIDIOC_QBUF, &buffer_info[i]);
        // 唤醒捕获线程
        pthread_cond_signal(&cond);
    }
    return NULL;
}

int main(void) {
    struct v4l2_format vfmt;//用于格式设置
    struct v4l2_requestbuffers reqbuffer;//内核态缓冲区申请
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 1. 打开设备
    fd = open("/dev/video0", O_RDWR);
    if(fd < 0) {
        perror("打开设备失败");
        return -1;
    }

    // 2. 设置采集格式
    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vfmt.fmt.pix.width = 640;
    vfmt.fmt.pix.height = 480;
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    ioctl(fd, VIDIOC_S_FMT, &vfmt);

    // 3. 申请内核空间
    memset(&reqbuffer, 0, sizeof(reqbuffer));
    reqbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuffer.count = FRAME_BUFFER_COUNT;
    reqbuffer.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &reqbuffer);

    // 4. 映射内核空间到用户空间
    for(int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        memset(&buffer_info[i], 0, sizeof(buffer_info[i]));
        buffer_info[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_info[i].memory = V4L2_MEMORY_MMAP;
        buffer_info[i].index = i;
        ioctl(fd, VIDIOC_QUERYBUF, &buffer_info[i]);
        frame_buffers[i] = (unsigned char *)mmap(NULL, buffer_info[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer_info[i].m.offset);
        ioctl(fd, VIDIOC_QBUF, &buffer_info[i]);
    }

    // 5. 开始视频采集
    ioctl(fd, VIDIOC_STREAMON, &type);//开始向缓冲区中写数据

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_t capture_tid, upload_tid;
    pthread_create(&capture_tid, NULL, capture_thread, NULL);
    pthread_create(&upload_tid, NULL, upload_thread, NULL);

    pthread_join(capture_tid, NULL);
    pthread_join(upload_tid, NULL);

    // 6. 停止采集并释放资源
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for(int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        munmap(frame_buffers[i], buffer_info[i].length);
    }
    close(fd);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    return 0;
}
