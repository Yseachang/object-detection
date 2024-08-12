#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <curl/curl.h>
#include <vector>

// curl写数据的回调函数
size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    std::vector<uchar> *buffer = (std::vector<uchar>*)stream;
    size_t total_size = size * nmemb;
    printf("Received data chunk size: %zu\n", total_size);
    buffer->insert(buffer->end(), (uchar*)ptr, (uchar*)ptr + total_size);
    return total_size;
}

int main(int argc, char* args[]) {
    // 初始化 curl
    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(!curl) {
        printf("Error: curl initialization failed.\n");
        return -1;
    }

    // 打开 framebuffer 设备
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }

    // 获取 framebuffer 的屏幕信息
    struct fb_var_screeninfo vinfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error: reading variable information");
        close(fbfd);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }

    // 输出 framebuffer 的分辨率和位深信息
    printf("Framebuffer resolution: %d x %d\n", vinfo.xres, vinfo.yres);
    printf("Framebuffer bits per pixel: %d\n", vinfo.bits_per_pixel);

    // 映射 framebuffer 到内存
    long screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
    char* fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        close(fbfd);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }

    // 清屏
    memset(fbp, 0, screensize);
    printf("Framebuffer mapped and cleared.\n");

    cv::Mat frame;
    std::vector<uchar> buffer;
    while (true) {
        // 每帧请求服务器
        buffer.clear();
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.104:5000/get_processed_frame"); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // 处理重定向
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 设置超时时间

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "Error: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            break;
        }

        printf("Received buffer size: %zu\n", buffer.size());

        if(buffer.size() > 0) {
            // 将收到的数据解码为图像
            frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
            if (frame.empty()) {
                printf("Error: Could not decode frame. Received buffer size: %zu\n", buffer.size());
                continue;
            }
            printf("Decoded frame size: %d x %d\n", frame.cols, frame.rows);

            // 将图像调整为 framebuffer 的大小
            cv::resize(frame, frame, cv::Size(vinfo.xres, vinfo.yres));
            printf("Resized frame size: %d x %d\n", frame.cols, frame.rows);

            // 将 BGR 图像转换为 BGRA 格式（添加透明度通道）
            cv::cvtColor(frame, frame, cv::COLOR_BGR2BGRA);

            // 计算每像素的字节数
            int expected_bytes_per_pixel = vinfo.bits_per_pixel / 8;
            int actual_bytes_per_pixel = frame.elemSize();
            printf("Expected bytes per pixel: %d, Actual bytes per pixel: %d\n", expected_bytes_per_pixel, actual_bytes_per_pixel);

            // 确保 framebuffer 和图像的尺寸匹配
            if (expected_bytes_per_pixel != actual_bytes_per_pixel) {
                printf("Error: Bytes per pixel mismatch. Expected: %d, Actual: %d\n", expected_bytes_per_pixel, actual_bytes_per_pixel);
            } else {
                long frame_size = frame.total() * frame.elemSize();
                printf("Resized frame data size: %ld\n", frame_size);
                
                if (frame_size == screensize) {
                    memcpy(fbp, frame.data, screensize);
                    printf("Frame successfully copied to framebuffer.\n");
                } else {
                    printf("Error: Resized frame data size (%ld) does not match framebuffer size (%ld).\n", frame_size, screensize);
                }
            }
        }

        // 控制帧率
        usleep(33000); // 大约30fps
    }

    // 释放资源
    munmap(fbp, screensize);
    close(fbfd);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return 0;
}