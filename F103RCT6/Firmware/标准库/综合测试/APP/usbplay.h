#ifndef __USBPLAY_H
#define __USBPLAY_H
#include "sys.h"
#include "includes.h" 	   	 
#include "common.h"  				    
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//APP-USB连接 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2014/2/16
//版本：V1.1
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//V1.1 20140216
//新增对各种分辨率LCD的支持.
////////////////////////////////////////////////////////////////////////////////// 	   

void usb_port_set(uint8_t enable);
uint8_t usb_play(void);
										   
#endif












