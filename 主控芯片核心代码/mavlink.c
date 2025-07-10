/*******************************************************************************
* 文件名称：mavlink_main.c
*
* 摘    要：mavlink自定义文件
*
* 当前版本：
* 作    者：
* 日    期：2018/05/30
* 编译环境：keil5
*
* 历史信息：
*******************************************************************************/

#include "mavlink.h"
#include "mavlink_types.h"

#include "usart.h"

#include "ringbuffer.h"
#include "stdio.h"

extern float neg_z;
extern float x,y;
short z;
extern float yaw;
extern float q[4];
extern uint8_t yaw_received_flag;
extern uint8_t local_received_flag;

/*缓冲区管理器*/
//ringbuffer管理变量
RingBuffer  m_Mavlink_RX_RingBuffMgr;
/*MAVLINK接收数据缓冲区数组*/
uint8_t   m_Mavlink_RX_Buff[MAVLINK_READ_BUFFER_SIZE*10];
uint8_t mavlink_byte;

/*******************************函数声明****************************************
* 函数名称: void Mavlink_RB_Init(void)
* 输入参数:
* 返回参数:
* 功    能: 初始化一个循环队列，用来管理接收到的串口数据。其实就是一个数据缓冲区
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_RB_Init(void)
{
	//将m_Mavlink_RX_Buffm_Mavlink_RX_RingBuffMgr环队列进行关联管理。
	rbInit(&m_Mavlink_RX_RingBuffMgr, m_Mavlink_RX_Buff, sizeof(m_Mavlink_RX_Buff));
}

/*******************************函数声明****************************************
* 函数名称: void Mavlink_RB_Clear(void)
* 输入参数:
* 返回参数:
* 功    能: 归零ringbuffer里面的设置。
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_RB_Clear(void)
{
	rbClear(&m_Mavlink_RX_RingBuffMgr);
}

/*******************************函数声明****************************************
* 函数名称: uint8_t  Mavlink_RB_IsOverFlow(void)
* 输入参数:
* 返回参数:  溢出为1，反之为0
* 功    能: 判断缓冲器是否溢出
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
uint8_t  Mavlink_RB_IsOverFlow(void)
{
	return m_Mavlink_RX_RingBuffMgr.flagOverflow;
}

/*******************************函数声明****************************************
* 函数名称: void Mavlink_RB_Push(uint8_t data)
* 输入参数: data：待压入的数据
* 返回参数:
* 功    能: 将接收的数据压入缓冲区
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_RB_Push(uint8_t data)
{
	rbPush(&m_Mavlink_RX_RingBuffMgr, (uint8_t)(data & (uint8_t)0xFFU));
}

/*******************************函数声明****************************************
* 函数名称: uint8_t Mavlink_RB_Pop(void)
* 输入参数:
* 返回参数: uint8_t 读出的数据
* 功    能: 从缓冲器读出数据
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
uint8_t Mavlink_RB_Pop(void)
{
	return rbPop(&m_Mavlink_RX_RingBuffMgr);
}

/*******************************函数声明****************************************
* 函数名称: uint8_t Mavlink_RB_HasNew(void)
* 输入参数:
* 返回参数:
* 功    能: 判断是否有新的数据
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
uint8_t Mavlink_RB_HasNew(void)
{
	return !rbIsEmpty(&m_Mavlink_RX_RingBuffMgr);
}

/*******************************函数声明****************************************
* 函数名称: uint16_t Mavlink_RB_Count(void)
* 输入参数:
* 返回参数:
* 功    能: 判断有多少个新数据
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
uint16_t Mavlink_RB_Count(void)
{
	return rbGetCount(&m_Mavlink_RX_RingBuffMgr);
}

/*******************************函数声明****************************************
* 函数名称: void Mavlink_Rece_Enable(void)
* 输入参数:
* 返回参数:
* 功    能: 使能DMA接收
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_Rece_Enable(void)
{
	HAL_UART_Receive_DMA(mavlink, &mavlink_byte, 1);
}

/*******************************函数声明****************************************
* 函数名称: void Mavlink_Init(void)
* 输入参数:
* 返回参数:
* 功    能: 初始化MAVLINK：使能接收，ringbuffer关联
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_Init(void)
{
	Mavlink_Rece_Enable();
	Mavlink_RB_Init();
}

/*******************************函数声明****************************************
* 函数名称: void Mavlin_RX_InterruptHandler(void)
* 输入参数:
* 返回参数:
* 功    能: 串口中断的处理函数，主要是讲数据压入ringbuffer管理器
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/


/*在“mavlink_helpers.h中需要使用”*/
mavlink_system_t mavlink_system =
{
	1,
	1
}; // System ID, 1-255, Component/Subsystem ID, 1-255

/*******************************函数声明****************************************
* 函数名称: void mavlink_send_uart_bytes(mavlink_channel_t chan, const uint8_t *ch, int length)
* 输入参数:
* 返回参数:
* 功    能: 重新定义mavlink的发送函数，与底层硬件接口关联起来
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void mavlink_send_uart_bytes(mavlink_channel_t chan, const uint8_t *ch, int length)
{
	HAL_UART_Transmit(mavlink, (uint8_t *)ch, length, 2000);
}

/*******************************函数声明****************************************
* 函数名称: void Mavlink_Msg_Handle(void)
* 输入参数:
* 返回参数:
* 功    能: 处理从QGC上位机传过来的数据信息
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
void Mavlink_Msg_Handle(mavlink_message_t msg)
{
	switch(msg.msgid)
	{
		case MAVLINK_MSG_ID_LOCAL_POSITION_NED:
			neg_z=0.0-mavlink_msg_local_position_ned_get_z(&msg);
			x=mavlink_msg_local_position_ned_get_x(&msg);
			y=mavlink_msg_local_position_ned_get_y(&msg);
			local_received_flag=1;
			break;
		case MAVLINK_MSG_ID_ATTITUDE:
			yaw=mavlink_msg_attitude_get_yaw(&msg);
			yaw_received_flag=1;
			break;
		default:
			break;
	}
	//printf("%lf,%lf,%lf,%d,\r\n",neg_z,x,y,z);

}
/*******************************函数声明****************************************
* 函数名称: Loop_Mavlink_Parse(void)
* 输入参数:
* 返回参数:
* 功    能: 在main函数中不间断调用此函数
* 作    者: by Across
* 日    期: 2018/06/02
*******************************************************************************/
mavlink_message_t msg;
mavlink_status_t status;

void Loop_Mavlink_Parse(void)
{
	if(Mavlink_RB_IsOverFlow())
	{
		Mavlink_RB_Clear();
	}
	
	while(Mavlink_RB_HasNew())
	{
		uint8_t read = Mavlink_RB_Pop();
		
		if(mavlink_parse_char(MAVLINK_COMM_0, read, &msg, &status))
		{
			//信号处理函数
			Mavlink_Msg_Handle(msg);
			//printf("Received message with ID %d, sequence: %d from component %d of system %d\r\n", msg.msgid, msg.seq, msg.compid, msg.sysid);
		}
	}
}
