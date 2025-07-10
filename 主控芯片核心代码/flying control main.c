/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "usart.h"
#include "tim.h"
#include "gpio.h"
#include "arm.h"
#include "mavlink.h"
#include <string.h>
#include <stdbool.h>

#define TARGET_ALTITUDE -1.7f        // MAVLink高度向下为负
#define DETECTION_TIMEOUT_MS 10000   // 10秒识别超时
#define HOVER_TIMEOUT_MS 15000       // 15秒悬停超时
#define ALTITUDE_TOLERANCE 0.1f      // 高度容差
#define MAVLINK_CHANNEL MAVLINK_COMM_0
#define ARM_OPEN_ANGLE 45            // 舵机释放角度
#define ARM_CLOSE_ANGLE 70           // 舵机关闭角度

// 控制状态机
typedef enum {
    IDLE,                   // 空闲状态
    TARGET_RECEIVED,        // 收到目标指令
    WAITING_FOR_CONFIRM,    // 等待OpenMV确认
    HOVERING,               // 悬停状态
    DROPPING,               // 释放目标
    LANDING,                // 降落状态
    ERROR_STATE             // 错误状态
} SystemState;

SystemState currentState = IDLE;

uint8_t bluetooth_rx = 0;       // 来自APP的目标编号
uint8_t openmv_rx = 0;          // OpenMV返回的识别结果
uint32_t state_start_time = 0;   // 状态开始时间
uint8_t yaw_received_flag = 0, local_received_flag = 0;
float current_yaw = 0;           // 当前偏航角
float current_x, current_y, current_z; // 当前位置

// 环形缓冲区结构
typedef struct {
    uint8_t* buffer;
    int head;
    int tail;
    int size;
    int capacity;
} RingBuffer;

RingBuffer mav_rx_buf;
uint8_t mav_raw[256];
uint8_t dma_rx_buf[64];

// 环形缓冲区函数
void rbInit(RingBuffer* rb, uint8_t* buf, int capacity) {
    rb->buffer = buf;
    rb->capacity = capacity;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
}

void rbPush(RingBuffer* rb, uint8_t data) {
    if (rb->size < rb->capacity) {
        rb->buffer[rb->tail] = data;
        rb->tail = (rb->tail + 1) % rb->capacity;
        rb->size++;
    }
    // 缓冲区满时丢弃最旧数据
    else {
        rb->head = (rb->head + 1) % rb->capacity;
        rb->buffer[rb->tail] = data;
        rb->tail = (rb->tail + 1) % rb->capacity;
    }
}

uint8_t rbPop(RingBuffer* rb) {
    if (rb->size > 0) {
        uint8_t data = rb->buffer[rb->head];
        rb->head = (rb->head + 1) % rb->capacity;
        rb->size--;
        return data;
    }
    return 0;
}

int rbIsFull(RingBuffer* rb) {
    return rb->size == rb->capacity;
}

int rbIsEmpty(RingBuffer* rb) {
    return rb->size == 0;
}

// 舵机控制函数
void Servo_SetDropReady() {
    Servo_SetAngle(ARM_OPEN_ANGLE);  // 打开夹爪
}

void Servo_SetHold() {
    Servo_SetAngle(ARM_CLOSE_ANGLE); // 夹紧
}

// 安全降落指令
void mavlink_land() {
    mavlink_message_t msg;
    uint8_t buf[64];
    mavlink_msg_command_long_pack(1, 200, &msg,
        1, 0,
        MAV_CMD_NAV_LAND,
        0, 0, 0, 0, 0, 0, 0
    );
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    HAL_UART_Transmit(&huart2, buf, len, 50);
}

// 悬停控制指令
void mavlink_hover(float yaw_target) {
    mavlink_message_t msg;
    uint8_t buf[128];
    
    mavlink_msg_set_position_target_local_ned_pack(1, 200, &msg,
        HAL_GetTick(), 1, MAV_FRAME_LOCAL_NED,
        0b0000111111000111,  // 位置控制模式
        current_x, current_y, TARGET_ALTITUDE,
        0, 0, 0,            // 无速度控制
        0, 0, yaw_target    // 无加速度控制，仅偏航
    );

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    HAL_UART_Transmit(&huart2, buf, len, 50);
}

// MAVLink消息解析
void handle_mavlink_message(mavlink_message_t* msg) {
    switch (msg->msgid) {
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t att;
            mavlink_msg_attitude_decode(msg, &att);
            current_yaw = att.yaw;
            yaw_received_flag = 1;
            break;
        }
        case MAVLINK_MSG_ID_LOCAL_POSITION_NED: {
            mavlink_local_position_ned_t pos;
            mavlink_msg_local_position_ned_decode(msg, &pos);
            current_x = pos.x;
            current_y = pos.y;
            current_z = pos.z;  // 注意：z向下为正
            local_received_flag = 1;
            break;
        }
        // 添加心跳包处理
        case MAVLINK_MSG_ID_HEARTBEAT: {
            // 可在此添加飞控状态检查
            break;
        }
    }
}

// MAVLink数据解析循环
void Loop_Mavlink_Parse(void) {
    static mavlink_status_t status;
    mavlink_message_t msg;
    
    while (!rbIsEmpty(&mav_rx_buf)) {
        uint8_t c = rbPop(&mav_rx_buf);
        
        // 尝试解析消息
        if (mavlink_parse_char(MAVLINK_CHANNEL, c, &msg, &status)) {
            handle_mavlink_message(&msg);
        }
    }
}

// UART接收回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // 将DMA接收的数据推入环形缓冲区
        for (int i = 0; i < sizeof(dma_rx_buf); i++) {
            if (dma_rx_buf[i] != 0) {
                rbPush(&mav_rx_buf, dma_rx_buf[i]);
            }
        }
        // 重新启动DMA接收
        HAL_UART_Receive_DMA(&huart2, dma_rx_buf, sizeof(dma_rx_buf));
    }
}

// 主控状态机
void ControlLoop() {
    uint32_t current_time = HAL_GetTick();
    
    switch (currentState) {
        case IDLE:
            // 等待蓝牙指令
            if (HAL_UART_Receive(&huart3, &bluetooth_rx, 1, 100) == HAL_OK) {
                if (bluetooth_rx >= '1' && bluetooth_rx <= '5') {
                    // 发送目标给OpenMV
                    HAL_UART_Transmit(&huart1, &bluetooth_rx, 1, 100);
                    state_start_time = current_time;
                    currentState = TARGET_RECEIVED;
                }
            }
            break;

        case TARGET_RECEIVED:
            // 等待OpenMV确认
            if (HAL_UART_Receive(&huart1, &openmv_rx, 1, 100) == HAL_OK) {
                if (openmv_rx == bluetooth_rx) {
                    yaw_received_flag = 0;
                    local_received_flag = 0;
                    currentState = WAITING_FOR_CONFIRM;
                    state_start_time = current_time;
                }
            }
            // 超时保护
            if (current_time - state_start_time > DETECTION_TIMEOUT_MS) {
                currentState = LANDING;
            }
            break;

        case WAITING_FOR_CONFIRM:
            // 检查是否收到位置和姿态数据
            if (yaw_received_flag && local_received_flag) {
                currentState = HOVERING;
                state_start_time = current_time;
            }
            // 超时保护
            else if (current_time - state_start_time > DETECTION_TIMEOUT_MS) {
                currentState = LANDING;
            }
            break;

        case HOVERING:
            // 持续发送悬停指令
            mavlink_hover(current_yaw);
            
            // 检查是否达到目标高度
            if (fabsf(current_z - TARGET_ALTITUDE) < ALTITUDE_TOLERANCE) {
                Servo_SetDropReady(); // 打开夹爪
                currentState = DROPPING;
                state_start_time = current_time;
            }
            // 超时保护
            else if (current_time - state_start_time > HOVER_TIMEOUT_MS) {
                currentState = LANDING;
            }
            break;

        case DROPPING:
            // 保持释放状态2秒
            if (current_time - state_start_time > 2000) {
                Servo_SetHold(); // 收回夹爪
                currentState = LANDING;
            }
            break;

        case LANDING:
            mavlink_land();
            // 简单延时后回到空闲状态
            if (current_time - state_start_time > 5000) {
                currentState = IDLE;
            }
            break;

        case ERROR_STATE:
            // 系统错误时执行安全降落
            mavlink_land();
            // 尝试恢复系统
            if (current_time - state_start_time > 5000) {
                currentState = IDLE;
            }
            break;
    }
}

// 系统初始化
void System_Init() {
    // 初始化环形缓冲区
    rbInit(&mav_rx_buf, mav_raw, sizeof(mav_raw));
    
    // 启动DMA接收MAVLink数据
    HAL_UART_Receive_DMA(&huart2, dma_rx_buf, sizeof(dma_rx_buf));
    
    // 初始化舵机
    Servo_Init();
    Servo_SetHold();
}

// 主函数
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_TIM3_Init();

    System_Init();

    while (1) {
        Loop_Mavlink_Parse(); // 解析MAVLink数据
        ControlLoop();        // 执行状态机控制
        HAL_Delay(50);        // 适当延时
    }
}

// 错误处理函数
void Error_Handler(void) {
    // 进入错误状态
    currentState = ERROR_STATE;
    state_start_time = HAL_GetTick();
    
    // 可添加LED闪烁等指示
    while (1) {
        ControlLoop();  // 继续执行控制循环
        HAL_Delay(100);
    }
}