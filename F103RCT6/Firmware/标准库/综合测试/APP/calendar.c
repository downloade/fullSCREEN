#include "calendar.h"
#include "stdio.h"
#include "settings.h"
#include "ds18b20.h"
#include "24cxx.h"
#include "math.h"
#include "rtc.h"
#include "led.h"
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//APP-日历 代码
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/7/20
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved
//*******************************************************************************
//修改信息
//无
//////////////////////////////////////////////////////////////////////////////////

_alarm_obj alarm;		//闹钟结构体
_calendar_obj calendar;	//日历结构体

static uint16_t TIME_TOPY;		//	120
static uint16_t OTHER_TOPY;		//	200

uint8_t*const calendar_week_table[GUI_LANGUAGE_NUM][7]=
{
    {"星期天","星期一","星期二","星期三","星期四","星期五","星期六"},
    {"星期天","星期一","星期二","星期三","星期四","星期五","星期六"},
    {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"},
};
//闹钟标题
uint8_t*const calendar_alarm_caption_table[GUI_LANGUAGE_NUM]=
{
    "闹钟","鬧鐘","ALARM",
};
//再响按钮
uint8_t*const calendar_alarm_realarm_table[GUI_LANGUAGE_NUM]=
{
    "再响","再響","REALARM",
};
uint8_t*const calendar_loading_str[GUI_LANGUAGE_NUM][3]=
{
    {
        "正在加载,请稍候...",
        "未检测到DS18B20!",
        "启用内部温度传感器...",
    },
    {
        "正在加载,请稍候...",
        "未检测到DS18B20!",
        "启用内部温度传感器...",
    },
    {
        "Loading...",
        "DS18B20 Check Failed!",
        "Start Internal Sensor...",
    },
};
//重新初始化闹钟
//alarmx:闹钟结构体
//calendarx:日历结构体
void calendar_alarm_init(_alarm_obj *alarmx,_calendar_obj *calendarx)
{
    uint8_t temp;
    RTC_Get();
    if(calendarx->week==7)temp=1<<0;
    else temp=1<<calendarx->week;
    if(alarmx->weekmask&temp)		//需要闹铃
    {
        printf("alarm:%d-%d-%d %d:%d\r\n",calendarx->w_year,calendarx->w_month,calendarx->w_date,alarmx->hour,alarmx->min);
        RTC_Alarm_Set(calendarx->w_year,calendarx->w_month,calendarx->w_date,alarmx->hour,alarmx->min,0);//设置闹铃时间
    }
}
//闹钟响闹铃
//type:闹铃类型
//0,滴.
//1,滴.滴.
//2,滴.滴.滴
//4,滴.滴.滴.滴
void calendar_alarm_ring(uint8_t type)
{
    uint8_t i;
    for(i=0; i<(type+1); i++)
    {
        LED1=0;
        delay_ms(100);
        LED1=1;
        delay_ms(100);
    }
}
//根据当前的日期,更新日历表.
void calendar_date_refresh(void)
{
    uint8_t weekn;   //周寄存
    uint16_t offx=(lcddev.width-240)/2;
    //显示阳历年月日
    POINT_COLOR=BRED;
    BACK_COLOR=BLACK;
    LCD_ShowxNum(offx+5,OTHER_TOPY+9,(calendar.w_year/100)%100,2,16,0x80);//显示年  20/19
    LCD_ShowxNum(offx+21,OTHER_TOPY+9,calendar.w_year%100,2,16,0x80);     //显示年
    LCD_ShowString(offx+37,OTHER_TOPY+9,lcddev.width,lcddev.height,16,"-"); //"-"
    LCD_ShowxNum(offx+45,OTHER_TOPY+9,calendar.w_month,2,16,0X80);     //显示月
    LCD_ShowString(offx+61,OTHER_TOPY+9,lcddev.width,lcddev.height,16,"-"); //"-"
    LCD_ShowxNum(offx+69,OTHER_TOPY+9,calendar.w_date,2,16,0X80);      //显示日
    //显示周几?
    POINT_COLOR=RED;
    weekn=calendar.week;
    Show_Str(5+offx,OTHER_TOPY+35,lcddev.width,lcddev.height,(uint8_t *)calendar_week_table[gui_phy.language][weekn],16,0); //显示周几?

}
//闹钟数据保存在:SYSTEM_PARA_SAVE_BASE+sizeof(_system_setings)+sizeof(_vs10xx_obj)
//读取日历闹钟信息
//alarm:闹钟信息
void calendar_read_para(_alarm_obj * alarm)
{
    AT24CXX_Read(SYSTEM_PARA_SAVE_BASE+sizeof(_system_setings),(uint8_t*)alarm,sizeof(_alarm_obj));
}
//写入日历闹钟信息
//alarm:闹钟信息
void calendar_save_para(_alarm_obj * alarm)
{
    OS_CPU_SR cpu_sr=0;
    alarm->ringsta&=0X7F;	//清空最高位
    OS_ENTER_CRITICAL();	//进入临界区(无法被中断打断)
    AT24CXX_Write(SYSTEM_PARA_SAVE_BASE+sizeof(_system_setings),(uint8_t*)alarm,sizeof(_alarm_obj));
    OS_EXIT_CRITICAL();		//退出临界区(可以被中断打断)
}

//闹铃处理(尺寸:44*20)
//x,y:坐标
//返回值,处理结果
uint8_t calendar_alarm_msg(uint16_t x,uint16_t y)
{
    uint8_t rval=0;
    uint16_t *lcdbuf=0;							//LCD显存
    lcdbuf=(uint16_t*)gui_memin_malloc(44*20*2);	//申请内存
    if(lcdbuf)								//申请成功
    {
        app_read_bkcolor(x,y,44,20,lcdbuf);	//读取背景色
        gui_fill_rectangle(x,y,44,20,LIGHTBLUE);
        gui_draw_rectangle(x,y,44,20,BROWN);
        gui_show_num(x+2,y+2,2,RED,16,alarm.hour,0X81);
        gui_show_ptchar(x+2+16,y+2,x+2+16+8,y+2+16,0,RED,16,':',1);
        gui_show_num(x+2+24,y+2,2,RED,16,alarm.min,0X81);
        OSTaskSuspend(6); //挂起主任务
        while(rval==0)
        {
            tp_dev.scan(0);
            if(tp_dev.sta&TP_PRES_DOWN)//触摸屏被按下
            {
                if(app_tp_is_in_area(&tp_dev,x,y,44,20))//判断某个时刻,触摸屏的值是不是在某个区域内
                {
                    rval=0XFF;//取消
                    break;
                }
            }
            delay_ms(5);
            if(system_task_return)break;			//需要返回
        }
        app_recover_bkcolor(x,y,44,20,lcdbuf);	//读取背景色
    } else rval=1;
    system_task_return=0;
    alarm.ringsta&=~(1<<7);	//取消闹铃
    calendar_alarm_init((_alarm_obj*)&alarm,&calendar);	//重新初始化闹钟
    gui_memin_free(lcdbuf);
    OSTaskResume(6); 		//恢复主任务
    system_task_return=0;
    return rval;
}
//画圆形指针表盘
//x,y:坐标中心点
//size:表盘大小(直径)
//d:表盘分割,秒钟的高度
void calendar_circle_clock_drawpanel(uint16_t x,uint16_t y,uint16_t size,uint16_t d)
{
    uint16_t r=size/2;//得到半径
    uint16_t sx=x-r;
    uint16_t sy=y-r;
    uint16_t px0,px1;
    uint16_t py0,py1;
    uint16_t i;
    gui_fill_circle(x,y,r,WHITE);		//画外圈
    gui_fill_circle(x,y,r-4,BLACK);		//画内圈
    for(i=0; i<60; i++) //画秒钟格
    {
        px0=sx+r+(r-4)*sin((app_pi/30)*i);
        py0=sy+r-(r-4)*cos((app_pi/30)*i);
        px1=sx+r+(r-d)*sin((app_pi/30)*i);
        py1=sy+r-(r-d)*cos((app_pi/30)*i);
        gui_draw_bline1(px0,py0,px1,py1,0,WHITE);
    }
    for(i=0; i<12; i++) //画小时格
    {
        px0=sx+r+(r-5)*sin((app_pi/6)*i);
        py0=sy+r-(r-5)*cos((app_pi/6)*i);
        px1=sx+r+(r-d)*sin((app_pi/6)*i);
        py1=sy+r-(r-d)*cos((app_pi/6)*i);
        gui_draw_bline1(px0,py0,px1,py1,2,YELLOW);
    }
    for(i=0; i<4; i++) //画3小时格
    {
        px0=sx+r+(r-5)*sin((app_pi/2)*i);
        py0=sy+r-(r-5)*cos((app_pi/2)*i);
        px1=sx+r+(r-d-3)*sin((app_pi/2)*i);
        py1=sy+r-(r-d-3)*cos((app_pi/2)*i);
        gui_draw_bline1(px0,py0,px1,py1,2,YELLOW);
    }
    gui_fill_circle(x,y,d/2,WHITE);		//画中心圈
}
//显示时间
//x,y:坐标中心点
//size:表盘大小(直径)
//d:表盘分割,秒钟的高度
//hour:时钟
//min:分钟
//sec:秒钟
void calendar_circle_clock_showtime(uint16_t x,uint16_t y,uint16_t size,uint16_t d,uint8_t hour,uint8_t min,uint8_t sec)
{
    static uint8_t oldhour=0;	//最近一次进入该函数的时分秒信息
    static uint8_t oldmin=0;
    static uint8_t oldsec=0;
    float temp;
    uint16_t r=size/2;//得到半径
    uint16_t sx=x-r;
    uint16_t sy=y-r;
    uint16_t px0,px1;
    uint16_t py0,py1;
    uint8_t r1;
    if(hour>11)hour-=12;
///////////////////////////////////////////////
    //清除小时
    r1=d/2+4;
    //清除上一次的数据
    temp=(float)oldmin/60;
    temp+=oldhour;
    px0=sx+r+(r-3*d-7)*sin((app_pi/6)*temp);
    py0=sy+r-(r-3*d-7)*cos((app_pi/6)*temp);
    px1=sx+r+r1*sin((app_pi/6)*temp);
    py1=sy+r-r1*cos((app_pi/6)*temp);
    gui_draw_bline1(px0,py0,px1,py1,2,BLACK);
    //清除分钟
    r1=d/2+3;
    temp=(float)oldsec/60;
    temp+=oldmin;
    //清除上一次的数据
    px0=sx+r+(r-2*d-7)*sin((app_pi/30)*temp);
    py0=sy+r-(r-2*d-7)*cos((app_pi/30)*temp);
    px1=sx+r+r1*sin((app_pi/30)*temp);
    py1=sy+r-r1*cos((app_pi/30)*temp);
    gui_draw_bline1(px0,py0,px1,py1,1,BLACK);
    //清除秒钟
    r1=d/2+3;
    //清除上一次的数据
    px0=sx+r+(r-d-7)*sin((app_pi/30)*oldsec);
    py0=sy+r-(r-d-7)*cos((app_pi/30)*oldsec);
    px1=sx+r+r1*sin((app_pi/30)*oldsec);
    py1=sy+r-r1*cos((app_pi/30)*oldsec);
    gui_draw_bline1(px0,py0,px1,py1,0,BLACK);
///////////////////////////////////////////////
    //显示小时
    r1=d/2+4;
    //显示新的时钟
    temp=(float)min/60;
    temp+=hour;
    px0=sx+r+(r-3*d-7)*sin((app_pi/6)*temp);
    py0=sy+r-(r-3*d-7)*cos((app_pi/6)*temp);
    px1=sx+r+r1*sin((app_pi/6)*temp);
    py1=sy+r-r1*cos((app_pi/6)*temp);
    gui_draw_bline1(px0,py0,px1,py1,2,YELLOW);
    //显示分钟
    r1=d/2+3;
    temp=(float)sec/60;
    temp+=min;
    //显示新的分钟
    px0=sx+r+(r-2*d-7)*sin((app_pi/30)*temp);
    py0=sy+r-(r-2*d-7)*cos((app_pi/30)*temp);
    px1=sx+r+r1*sin((app_pi/30)*temp);
    py1=sy+r-r1*cos((app_pi/30)*temp);
    gui_draw_bline1(px0,py0,px1,py1,1,GREEN);
    //显示秒钟
    r1=d/2+3;
    //显示新的秒钟
    px0=sx+r+(r-d-7)*sin((app_pi/30)*sec);
    py0=sy+r-(r-d-7)*cos((app_pi/30)*sec);
    px1=sx+r+r1*sin((app_pi/30)*sec);
    py1=sy+r-r1*cos((app_pi/30)*sec);
    gui_draw_bline1(px0,py0,px1,py1,0,RED);
    oldhour=hour;	//保存时
    oldmin=min;		//保存分
    oldsec=sec;		//保存秒
}
//时间显示模式
uint8_t calendar_play(void)
{
    uint8_t second=0;
    short temperate=0;	//温度值
    uint8_t t=0;
    uint8_t tempdate=0;
    uint8_t rval=0;			//返回值
    uint8_t res;
    uint16_t xoff=0;
    uint16_t yoff=0;	//表盘y偏移量
    uint16_t r=0;	//表盘半径
    uint8_t d=0;		//指针长度

    uint8_t TEMP_SEN_TYPE=0;	//默认使用DS18B20
    FIL* f_calendar=0;

    f_calendar=(FIL *)gui_memin_malloc(sizeof(FIL));//开辟FIL字节的内存区域
    if(f_calendar==NULL)rval=1;		//申请失败
    else
    {
        res=f_open(f_calendar,(const TCHAR*)APP_ASCII_S6030,FA_READ);//打开文件
        if(res==FR_OK)
        {
            asc2_s6030=(uint8_t*)gui_memex_malloc(f_calendar->fsize);	//为大字体开辟缓存地址
            if(asc2_s6030==0)rval=1;
            else
            {
                res=f_read(f_calendar,asc2_s6030,f_calendar->fsize,(UINT*)&br);	//一次读取整个文件
            }
            f_close(f_calendar);
        }
        if(res)rval=res;
    }
    if(rval==0)//无错误
    {
        LCD_Clear(BLACK);//清黑屏
        second=calendar.sec;//得到此刻的秒钟
        POINT_COLOR=GBLUE;
        Show_Str(48,60,lcddev.width,lcddev.height,(uint8_t*)calendar_loading_str[gui_phy.language][0],16,0x01); //显示进入信息
        if(DS18B20_Init())
        {
            Show_Str(48,76,lcddev.width,lcddev.height,(uint8_t*)calendar_loading_str[gui_phy.language][1],16,0x01);
            delay_ms(500);
            Show_Str(48,92,lcddev.width,lcddev.height,(uint8_t*)calendar_loading_str[gui_phy.language][2],16,0x01);
            TEMP_SEN_TYPE=1;
        }
        delay_ms(1100);//等待1.1s
        BACK_COLOR= BLACK;
        LCD_Clear(BLACK);//清黑屏
        if(lcddev.width==240)
        {
            r=80;
            d=7;
        } else if(lcddev.width==320)
        {
            r=120;
            d=9;
        } else if(lcddev.width==480)
        {
            r=160;
            d=12;
        }
        yoff=(lcddev.height-r*2-140)/2;
        TIME_TOPY=yoff+r*2+10;
        OTHER_TOPY=TIME_TOPY+60+10;
        xoff=(lcddev.width-240)/2;
        calendar_circle_clock_drawpanel(lcddev.width/2,yoff+r,r*2,d);//显示指针时钟表盘
        calendar_date_refresh();  //加载日历
        tempdate=calendar.w_date;//天数暂存器
        gui_phy.back_color=BLACK;
        gui_show_ptchar(xoff+70-4,TIME_TOPY,lcddev.width,lcddev.height,0,GBLUE,60,':',0);	//":"
        gui_show_ptchar(xoff+150-4,TIME_TOPY,lcddev.width,lcddev.height,0,GBLUE,60,':',0);	//":"
    }
    while(rval==0)
    {
        RTC_Get();					//更新时间
        if(system_task_return)break;//需要返回
        if(second!=calendar.sec)	//秒钟改变了
        {
            second=calendar.sec;
            calendar_circle_clock_showtime(lcddev.width/2,yoff+r,r*2,d,calendar.hour,calendar.min,calendar.sec);//指针时钟显示时间
            gui_phy.back_color=BLACK;
            gui_show_num(xoff+10,TIME_TOPY,2,GBLUE,60,calendar.hour,0X80);	//显示时
            gui_show_num(xoff+90,TIME_TOPY,2,GBLUE,60,calendar.min,0X80);	//显示分
            gui_show_num(xoff+170,TIME_TOPY,2,GBLUE,60,calendar.sec,0X80);	//显示秒
            if(t%2==0)//等待2秒钟
            {
                if(TEMP_SEN_TYPE)temperate=Get_Temp();//得到内部温度传感器的温度值,0.1℃
                else temperate=DS18B20_Get_Temp();//得到18b20温度
                if(temperate<0)//温度为负数的时候，红色显示
                {
                    POINT_COLOR=RED;
                    temperate=-temperate;	//改为正温度
                } else POINT_COLOR=BRRED;	//正常为棕红色字体显示
                gui_show_num(xoff+90,OTHER_TOPY,2,GBLUE,60,temperate/10,0X80);	//XX
                gui_show_ptchar(xoff+150,OTHER_TOPY,lcddev.width,lcddev.height,0,GBLUE,60,'.',0);	//"."
                gui_show_ptchar(xoff+180-15,OTHER_TOPY,lcddev.width,lcddev.height,0,GBLUE,60,temperate%10+'0',0);//显示小数
                gui_show_ptchar(xoff+210-10,OTHER_TOPY,lcddev.width,lcddev.height,0,GBLUE,60,95+' ',0);//显示℃
                if(t>0)t=0;
            }
            if(calendar.w_date!=tempdate)
            {
                calendar_date_refresh();	//天数变化了,更新日历.
                tempdate=calendar.w_date;	//修改tempdate，防止重复进入
            }
            t++;
        }
        delay_ms(20);
    };
    while(tp_dev.sta&TP_PRES_DOWN)tp_dev.scan(0);//等待TP松开.
    gui_memex_free(asc2_s6030);	//删除申请的内存
    asc2_s6030=0;				//清零
    gui_memin_free(f_calendar);	//删除申请的内存
    POINT_COLOR=BLUE;
    BACK_COLOR=WHITE ;
    return rval;
}
