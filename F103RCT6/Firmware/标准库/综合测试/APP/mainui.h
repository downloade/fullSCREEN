#include "dma.h"
#include "lcd.h"
#include "delay.h"
#include "malloc.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//主界面控制 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2014/2/22
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	   
  
#define MUI_BACK_COLOR		LIGHTBLUE		//MUI选择图标底色
#define MUI_FONT_COLOR 		BLUE 			//MUI字体颜色

#define MUI_EX_BACKCOLOR	0X0000			//窗体外部背景色
#define MUI_IN_BACKCOLOR	0X8C51			//窗体内部背景色

////////////////////////////////////////////////////////////////////////////////////////////
//各图标/图片路径
extern  uint8_t *const mui_icos_path_tbl[9]; //图标路径表
////////////////////////////////////////////////////////////////////////////////////////////
//每个图标的参数信息
__packed typedef struct _m_mui_icos
{										    
	uint16_t x;			//图标坐标及尺寸
	uint16_t y;
	uint8_t width;
	uint8_t height; 
	uint8_t * path;		//图标路径
	uint8_t * name;		//图标名字
}m_mui_icos;

//主界面 控制器
typedef struct _m_mui_dev
{					
	uint16_t tpx;			//触摸屏最近一次的X坐标
	uint16_t tpy;			//触摸屏最近一次的Y坐标
	uint8_t status;			//当前选中状态.
						//bit7:0,没有有效触摸;1,有有效触摸.
						//bit6~5:保留
						//bit4~0:0~8,被选中的图标编号;0XF,没有选中任何图标  
	m_mui_icos icos[9];	//1页,共9个图标
}m_mui_dev;
extern m_mui_dev muidev;

void mui_init(uint8_t mode);
void mui_load_icos(void);
void mui_language_set(uint8_t language);
void mui_set_sel(uint8_t sel); 
uint8_t mui_touch_chk(void);
























 




