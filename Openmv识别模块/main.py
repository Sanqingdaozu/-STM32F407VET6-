# Untitled - By: USER - Tue Jul 8 2025

import sensor
import time, image
from pyb import UART

uart = UART(3, 9600)  # 使用UART3(PB10=TX, PB11=RX)，波特率9600

NUM_SUBJECTS = 3        # 图像库中不同人数
NUM_SUBJECTS_IMGS = 20  # 每人的样本图片数量
THRESHOLD = 10000       # 识别阈值，低于此值认为匹配成功
LOOP_DELAY = 1000       # 每次检测之间的延迟(毫秒)

def man_detection():
    # 初始化摄像头
    sensor.reset()

    # 设置相机图像的对比度为1
    sensor.set_contrast(1)

    # 设置相机的增益上限为16
    sensor.set_gainceiling(16)

    # 设置采集到照片的大小
    sensor.set_framesize(sensor.HQVGA)

    # 设置采集到照片的格式：灰色图像
    sensor.set_pixformat(sensor.GRAYSCALE)

    # 加载Haar Cascade 模型
    # 默认使用25个步骤，减少步骤会加快速度但会影响识别成功率
    face_cascade = image.HaarCascade("frontalface", stage = 25)
    print(face_cascade)

    # 创建一个时钟来计算摄像头每秒采集的帧数FPS
    clock = time.clock()

    while(True):
        # 更新FPS时钟
        clock.tick()

        # 拍摄图片并返回img
        img = sensor.snapshot()

        # 寻找人脸对象
        # threshold和scale_factor两个参数控制着识别的速度和准确性
        objects = img.find_features(face_cascade, threshold=0.75, scale_factor=1.2)

        # 用矩形将人脸画出来
        for r in objects:
            img.draw_rectangle(r)
            uart.write("2")

        # 串口打印FPS参数
        print(clock.fps())

def color_detection():
    red_threshold = (29, 100, 28, 127, -21, 127)  # (R_min, R_max, G_min, G_max, B_min, B_max)
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time = 2000)  # 让摄像头稳定2秒
    sensor.set_auto_gain(False)  # 关闭自动增益
    sensor.set_auto_whitebal(False)  # 关闭自动白平衡
    clock = time.clock()
    while(True):
        clock.tick()
        img = sensor.snapshot()

        # 寻找黄色色块
        blobs = img.find_blobs([red_threshold], pixels_threshold=200, area_threshold=200)

        # 如果找到黄色色块
        if blobs:
            for blob in blobs:
                # 绘制矩形框
                img.draw_rectangle(blob.rect())
                # 绘制十字标记
                img.draw_cross(blob.cx(), blob.cy())
                uart.write("1\n")
                time.sleep_ms(100)
        else:
            # 未检测到色块
            uart.write("0\n")
            time.sleep_ms(100)

    # 拍摄当前人脸并提取LBP特征
def capture_face():
    img = sensor.snapshot()
    return img.find_lbp((0, 0, img.width(), img.height()))

    # 计算最匹配的人脸
def find_matching_face(descriptor):
    min_distance = float('inf')
    match_id = 0

    for subject_id in range(1, NUM_SUBJECTS + 1):
        subject_distance = 0
        valid_samples = 0

        for img_id in range(2, NUM_SUBJECTS_IMGS + 1):
            try:
                # 加载样本图像并提取LBP特征
                img = image.Image("singtown/s%d/%d.pgm" % (subject_id, img_id))
                sample_descriptor = img.find_lbp((0, 0, img.width(), img.height()))

                    # 计算特征差异度
                distance = image.match_descriptor(descriptor, sample_descriptor)
                subject_distance += distance
                valid_samples += 1
            except Exception as e:
                print("Error loading image s%d/%d: %s" % (subject_id, img_id, e))
                continue

        if valid_samples > 0:
            avg_distance = subject_distance / valid_samples
            print("Average dist for subject %d: %d" % (subject_id, avg_distance))

            if avg_distance < min_distance:
                    min_distance = avg_distance
                    match_id = subject_id

    return match_id, min_distance

    # 主循环
def face_detection():
    # 初始化摄像头
    sensor.reset()
    sensor.set_pixformat(sensor.GRAYSCALE)  # 设置为灰度图
    sensor.set_framesize(sensor.B128X128)   # 设置帧大小
    sensor.set_windowing((92, 112))         # 设置窗口大小为92x112
    sensor.skip_frames(10)                  # 让设置生效
    sensor.skip_frames(time = 2000)         # 等待2秒
    print("Starting continuous face recognition...")
    clock = time.clock()

    while(True):
        clock.tick()

        # 捕获当前人脸特征
        print("\nCapturing face...")
        face_descriptor = capture_face()

            # 查找匹配的人脸
        print("Searching for match...")
        match_id, distance = find_matching_face(face_descriptor)

        # 判断是否匹配成功并输出结果
        if distance <= THRESHOLD:
            print("Match found! Subject ID: %d, Distance: %d" % (match_id, distance))
            uart.write("%d\n" %(match_id+2))  # 发送匹配成功信号
        else:
            print("No match found. Closest distance: %d" % distance)
            uart.write("0\n")  # 发送匹配失败信号

        print("FPS: %.2f" % clock.fps())
        print("Waiting for next capture...")
        time.sleep_ms(LOOP_DELAY)  # 等待指定时间后再次检测

def main_loop():
# 等待并读取数据
    while(True):
    # 检查是否有数据可读
        if uart.any():
        # 读取一行数据（以换行符'\n'结尾）
            data = uart.readline()
            print("收到数据:", data)

        # 如果需要将数据转换为字符串（去除换行符）
            if data:
                string_data = data.decode('utf-8').strip()
                print("转换后的字符串:", string_data)

            # 可以在这里根据接收到的数据执行相应的操作
                if string_data == "1":
                    color_detection()
                elif string_data == "2":
                    man_detection()
                elif string_data == "3" or string_data== "4" or string_data== "5":
                    face_detection()
        else:
            print("没有数据可读，等待中...")

if __name__ == "__main__":
    main_loop()
