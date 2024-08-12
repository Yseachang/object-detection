# object-detection
https://github.com/user-attachments/assets/ed060edf-0afc-47e8-a13b-c565f2b141bc

# 简介

基于全志T113开发的物体检测程序，客户端为开发板，通过v4l2框架编写usb摄像头应用程序采集视频流数据，通过http协议实时上传帧数据至服务器端，服务器端采用Flask框架，调用本地yolov5s模型进行检测。客户端有另一进程请求处理结果，并采用framebuffer将结果帧展示在开发板屏幕上。
<a name="XcmnV"></a>

# 编译

交叉编译时采用动态链接，需将对应动态库粘贴到开发板上。<br />需要交叉编译的的动态库：-lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs  -lcurl -lssl -lcrypto 

