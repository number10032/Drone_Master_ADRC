#include "protocol.h"
/**************文件说明**********************
地面站和飞控通过串口进行数据通信
********************************************/
//数据拆分宏定义，在发送大于1字节的数据类型时，比如int16、float等，需要把数据拆分成单独字节进行发送
#define BYTE0(dwTemp)       ( *( (char *)(&dwTemp)    ) )
#define BYTE1(dwTemp)       ( *( (char *)(&dwTemp) + 1) )
#define BYTE2(dwTemp)       ( *( (char *)(&dwTemp) + 2) )
#define BYTE3(dwTemp)       ( *( (char *)(&dwTemp) + 3) )

/*串口接收部分**********************************/
u8 RxData;  //从串口收到的一个字节
u8 RxTemp[12];  //临时保存串口接收到的待用数据
u8 FcnWord;  //功能字节(跨文件全局变量)
u8 LenWord;  //长度字节(跨文件全局变量)
u8 ErrCnt=0;  //未收到遥控器信号的次数(跨文件全局变量)
//以下为遥控器数据保存
short RCchannel[4];  //遥控器的4个控制通道(跨文件全局变量)
u8 RCmsg=0;  //其余有关的遥控器命令(跨文件全局变量)

/***********************
建立DMA接收通道,从地面站/遥控器接收数据
**********************/
void Protocol_Init(void)
{
	HAL_UART_Receive_DMA(&huart2,&RxData,1);
}
/***********************
接收字节处理
**********************/
u8 XDAA_Data_Receive_Precess(void)
{
	static u8 zRxState=0,sum=0,i=0;
	switch(zRxState)
	{
	case 0:  //帧头校验
		if(RxData=='<')
		{
			sum=RxData;
			zRxState=1;
		}
		break;
	case 1:  //功能字校验与保存
		sum+=RxData;
		FcnWord=RxData;
		zRxState=2;
		break;
	case 2:  //数据长度校验与保存
		if(RxData<=12)
		{
			sum+=RxData;
			LenWord=RxData;
			zRxState=3;
		}
		else
		{
			sum=0;
			zRxState=0;
		}
		break;
	case 3:  //临时保存待用数据
		sum+=RxData;
		RxTemp[i++]=RxData;
		if(i>=LenWord)
			zRxState=4;
		break;
	case 4:  //匹配校验和
		zRxState=0;
		i=0;
		if(sum==RxData)
			return 0;  //待用数据有效
		else
			return 1;
	default:
		zRxState=0;
		return 1;
	}
	return 1;  //待用数据无效
}
/***********************
串口通过DMA方式接收到一个字节
**********************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance!=USART2)
		return;
	if(XDAA_Data_Receive_Precess())
		return;
	switch(FcnWord)
	{
	case P_STAT:
		if(RxTemp[1]==WHO_AM_I)
			RCmsg=RxTemp[0];
		break;
	case P_CTRL:
		LenWord/=2;
		for(u8 i=0;i<LenWord;i++)
			RCchannel[i]=(RxTemp[2*i]<<8) | RxTemp[2*i+1];
		break;
	default:break;
	}
	ErrCnt=0;
	RCmsg|=RC_RECEIVE;
}
/*串口发送部分**********************************/
u8 DataToSend[16];  //待发送的数据
u8 SendBuff[SENDBUF_SIZE];  //发送缓冲区
u16 TotalLen=0;  //发送缓冲区待发送数据长度
/***********************
将待发送数据存入缓冲
**********************/
void DMA_Stuff(u8 *Data,u8 len)
{
	if(len==0)
	{
		TotalLen=0;
		return;
	}
	u8 i;
	for(i=0;i<len;i++)
	{
		if(TotalLen+i>=SENDBUF_SIZE)
			return;//若发送速率不够快导致缓冲区满则放弃新的数据
		SendBuff[TotalLen+i]=Data[i];
	}
	TotalLen+=len;
}
/***********************
将发送缓冲区的数据填入DMA发送并清空发送缓冲区
**********************/
void Total_Send(void)
{
	if(TotalLen==0)
		return;
	HAL_UART_Transmit_DMA(&huart2,SendBuff,TotalLen);
	DMA_Stuff(0,0);
}
/***********************
*@data:s16型数据
*@len:数据个数
*@fcn:功能字
**********************/
void XDAA_Send_S16_Data(s16 *data,u8 len,u8 fcn)
{
	u8 i,cnt=0,checksum=0;
	DataToSend[cnt++]='>';
	DataToSend[cnt++]=fcn;
	DataToSend[cnt++]=len*2;
	for(i=0;i<len;i++)
	{
		DataToSend[cnt++]=BYTE1(data[i]);
		DataToSend[cnt++]=BYTE0(data[i]);
	}
	for(i=0;i<cnt;i++)
		checksum+=DataToSend[i];
	DataToSend[cnt++]=checksum;
	DMA_Stuff(DataToSend,cnt);
}
/***********************
*@data:u8型数据
*@len:数据个数
*@fcn:功能字
**********************/
void XDAA_Send_U8_Data(u8 *data,u8 len,u8 fcn)
{
	u8 i,cnt=0,checksum=0;
	DataToSend[cnt++]='>';
	DataToSend[cnt++]=fcn;
	DataToSend[cnt++]=len;
	for(i=0;i<len;i++)
		DataToSend[cnt++]=data[i];
	for(i=0;i<cnt;i++)
		checksum+=DataToSend[i];
	DataToSend[cnt++]=checksum;
	DMA_Stuff(DataToSend,cnt);
}
