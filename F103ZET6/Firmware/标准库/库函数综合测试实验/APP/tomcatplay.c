#include "tomcatplay.h"
#include "recorder.h"
#include "vs10xx.h"
#include "audioplay.h"
#include "spb.h"
#include "settings.h"
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//APP-TOM猫 代码
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


#define TOMCAT_TITLE_COLOR   	0XFFFF		//录音机标题颜色	
#define TOMCAT_TITLE_BKCOLOR	0X0000		//录音机标题背景色	


#define TOMCAT_TIME_COLOR    	0X07FF		//时间颜色
#define TOMCAT_MAIN_BKCOLOR		0X18E3  	//主背景色

//窗体内嵌字颜色
#define TOMCAT_INWIN_FONT_COLOR	0X736C		//0XAD53		


//通过设置以下2个参数,可以控制检测声音的灵敏度和效果.
//当声音第一次大于TOMCAT_START_LEVEL时,启动录音.当声音连续
//一段时间少于TOMCAT_STOP_LEVEL的时候,停止录音,并开始放音.
#define TOMCAT_STOP_LEVEL		3			//停止阀值
#define TOMCAT_START_LEVEL		5			//启动阀值


u8*const TOMCAT_DEMO_PIC="1:/SYSTEM/APP/TOMCAT/Demo.bmp";//demo图片路径

//標題
u8*const tomcat_caption_tbl[GUI_LANGUAGE_NUM]= {"会说话的猫","會說話的貓","Talking TOM",};
u8*const tomcat_set_tbl[GUI_LANGUAGE_NUM]= {"增益及语速设置","增益及語速設置","GAIN AND SPEED SET"};

//加载tom cat主界面UI
void tomcat_load_ui(void)
{
    gui_fill_rectangle(0,0,lcddev.width,gui_phy.tbheight,TOMCAT_TITLE_BKCOLOR);				//填充背景色
    gui_show_strmid(0,0,lcddev.width,gui_phy.tbheight,TOMCAT_TITLE_COLOR,gui_phy.tbfsize,(u8*)tomcat_caption_tbl[gui_phy.language]);//显示标题
    gui_fill_rectangle(0,gui_phy.tbheight,lcddev.width,lcddev.height-gui_phy.tbheight,TOMCAT_MAIN_BKCOLOR);//填充底色
    minibmp_decode((u8*)TOMCAT_DEMO_PIC,(lcddev.width-100)/2,100+(lcddev.height-320)/2,100,100,0,0);//解码100*100的图片DEMO
    recorder_vu_meter((lcddev.width-218)/2,(lcddev.height-100)/2+100,0);					//显示vu meter;
    app_gui_tcbar(0,lcddev.height-gui_phy.tbheight,lcddev.width,gui_phy.tbheight,0x01);		//上分界线
}
//显示AGC大小
//x,y:坐标
//spd:增益值 4000~16000;
void tomcat_show_spd(u16 x,u16 y,u16 spd)
{
    gui_phy.back_color=APP_WIN_BACK_COLOR;						//设置背景色为底色
    gui_fill_rectangle(x,y,30,12,APP_WIN_BACK_COLOR);			//填充背景色
    gui_show_num(x,y,5,TOMCAT_INWIN_FONT_COLOR,12,spd,0X80);	//显示数值
}
//AGC与播放速度设置界面.固定尺寸:180*158
//x,y:左上角坐标
//agc:自动增益指针	1~10
//speed:速度指针 	0~24->(8~32)*500K
//caption:窗口名字
//返回值:0,成功设置;
//    其他,不进行设置
u8 tomcat_agcspd_set(u16 x,u16 y,u8 *agc,u16 *speed,u8*caption)
{
    u8 rval=0,res;
    _window_obj* twin=0;			//窗体
    _btn_obj * rbtn=0;				//取消按钮
    _btn_obj * okbtn=0;				//确定按钮
    _progressbar_obj * agcprgb;		//AGC设置进度条
    _progressbar_obj * speedprgb;	//速度设置进度条
    u8 tempagc=0;
    u16 tempspd=*speed;

    if(*agc)tempagc=*agc-1;

    twin=window_creat(x,y,180,158,0,1|1<<5,16);			//创建窗口 　
    agcprgb=progressbar_creat(x+10,y+52,160,15,0X20);	//创建进度条
    if(agcprgb==NULL)rval=1;
    speedprgb=progressbar_creat(x+10,y+52+35,160,15,0X20);	//创建进度条
    if(agcprgb==NULL)rval=1;
    okbtn=btn_creat(x+20,y+118,60,30,0,0x02);		//创建按钮
    rbtn=btn_creat(x+20+60+20,y+118,60,30,0,0x02);	//创建按钮
    if(twin==NULL||rbtn==NULL||okbtn==NULL||rval)rval=1;
    else
    {
        //窗口的名字和背景色
        twin->caption=caption;
        twin->windowbkc=APP_WIN_BACK_COLOR;
        //返回按钮的颜色
        rbtn->bkctbl[0]=0X8452;		//边框颜色
        rbtn->bkctbl[1]=0XAD97;		//第一行的颜色
        rbtn->bkctbl[2]=0XAD97;		//上半部分颜色
        rbtn->bkctbl[3]=0X8452;		//下半部分颜色
        okbtn->bkctbl[0]=0X8452;	//边框颜色
        okbtn->bkctbl[1]=0XAD97;	//第一行的颜色
        okbtn->bkctbl[2]=0XAD97;	//上半部分颜色
        okbtn->bkctbl[3]=0X8452;	//下半部分颜色

        agcprgb->totallen=9;		//最大AGC为9(0~9,对应1~10的增益倍数)
        agcprgb->curpos=tempagc;	//当前尺寸

        speedprgb->totallen=24;		//最大SPEED为24(0~24,对应(8~32)*500K的采样率)
        speedprgb->curpos=tempspd;	//当前尺寸

        rbtn->caption=(u8*)GUI_CANCEL_CAPTION_TBL[gui_phy.language];	//标题为取消
        okbtn->caption=(u8*)GUI_OK_CAPTION_TBL[gui_phy.language];		//标题为确定
        window_draw(twin);				//画出窗体
        btn_draw(rbtn);					//画按钮
        btn_draw(okbtn);				//画按钮
        progressbar_draw_progressbar(agcprgb);
        progressbar_draw_progressbar(speedprgb);
        gui_show_string("AGC:",x+10,y+38,24,12,12,TOMCAT_INWIN_FONT_COLOR);//显示SIZE
        recorder_show_agc(x+10+24,y+38,tempagc+1);
        gui_show_string("SPEED:     Hz",x+10,y+38+35,84,12,12,TOMCAT_INWIN_FONT_COLOR);//显示SIZE
        tomcat_show_spd(x+10+36,y+38+35,(tempspd+8)*500);	//显示速度
        while(rval==0)
        {
            tp_dev.scan(0);
            in_obj.get_key(&tp_dev,IN_TYPE_TOUCH);	//得到按键键值
            delay_ms(1000/OS_TICKS_PER_SEC);		//延时一个时钟节拍
            if(system_task_return)break;			//TPAD返回
            res=btn_check(rbtn,&in_obj);			//取消按钮检测
            if(res&&((rbtn->sta&0X80)==0))			//有有效操作
            {
                rval=1;
                break;//退出
            }
            res=btn_check(okbtn,&in_obj); 			//确认按钮检测
            if(res&&((okbtn->sta&0X80)==0))			//有有效操作
            {
                rval=0XFF;
                break;//确认了
            }
            res=progressbar_check(agcprgb,&in_obj);
            if(res&&(tempagc!=agcprgb->curpos))	//进度条改动了
            {
                tempagc=agcprgb->curpos;		//保存最新的结果
                recorder_show_agc(x+10+24,y+38,tempagc+1);
                VS_WR_Cmd(SPI_AICTRL1,1024*(tempagc+1));//设置增益,0,自动增益.1024相当于1倍,512相当于0.5倍,最大值65535=64倍
            }
            res=progressbar_check(speedprgb,&in_obj);
            if(res&&(tempspd!=speedprgb->curpos))	//进度条改动了
            {
                tempspd=speedprgb->curpos;		//保存最新的结果
                tomcat_show_spd(x+10+36,y+38+35,(tempspd+8)*500);	//显示速度
            }
        }
    }
    window_delete(twin);			//删除窗口
    btn_delete(rbtn);				//删除按钮
    btn_delete(okbtn);				//删除按钮
    progressbar_delete(agcprgb);	//删除进度条
    progressbar_delete(speedprgb);	//删除进度条
    system_task_return=0;

    if(rval==0XFF)
    {
        *agc=tempagc+1;
        *speed=tempspd;
        return 0;
    }
    return rval;
}
//进入录音模式(无声)
void tomcat_rec_mode(u8 agc)
{
    recorder_enter_rec_mode();
    recoder_set_agc(agc);
    while(VS_RD_Reg(SPI_HDAT1)>>8);		//等到buf 较为空闲再开始
}
//播放wav文件
void tomcat_play_wav(u8 *buf,u32 len)
{
    u32 i=0;
    VS_HD_Reset();
    VS_Soft_Reset();  		//软复位VS1053
    VS_Set_Vol(vsset.mvol);	//设置音量
    VS_SPK_Set(1);			//控制板载喇叭开启
    do//主播放循环
    {
        if(VS_Send_MusicData(buf+i)==0)//给VS10XX发送音频数据
        {
            i+=32;
        } else delay_ms(5);				//允许调度
        if(system_task_return)break; 	//TPAD返回
    } while(i<len);//循环发送数据
}
//语音数据处理
//该函数实现类似fifo功能,将buf内size长度的数据整体左移dx字节.
//buf:数据缓存
//size:数据长度
//dx:左移的字节数
void tomcat_data_move(u8* buf,u16 size,u16 dx)
{
    u16 i=0;
    for(i=0; i<(size-dx); i++)
    {
        buf[i]=buf[i+dx];
    }
}
//tom猫游戏
u8 tomcat_play(void)
{
    u8 res;
    u8 rval=0;
    __WaveHeader *wavhead=0;
    _btn_obj * rbtn=0;				//取消按钮
    _btn_obj * mbtn=0;				//选项按钮
    u8 *mainbuf;					//主缓存地址=数据缓存区大小+sizeof(__WaveHeader);
    u8 *databuf;					//数据缓存首地址

    u16 w;
    u8 rec_sta=0;				 	//[0]:0,等待输入状态;1,正在录音;

    u8  vutimecnt=0;		    	//vu扫描时间计数器
    u16 timecnt=0;					//计时器

    u8 vulevel=0;
    u8 temp;
    short tempval;
    u16 maxval=0;

    u16 idx=0;
    u32 dcnt=0;		//数据计数器
    /////////////////////////////////////////
    u8 recagc=4;	//默认agc为4.不自动保存
    u16 wavspd=18;	//wav播放速度,0~24.对应8~32级. 默认速度:为(18+8)*500K

    mainbuf=gui_memex_malloc(176*1000+sizeof(__WaveHeader));	//申请176K+__WaveHeader头的字节数,保存最多11秒的数据
    if(mainbuf==NULL)//内存不够?释放主界面占用内存,再重新申请
    {
        spb_delete();//释放SPB占用的内存
        mainbuf=gui_memex_malloc(176*1000+sizeof(__WaveHeader));//申请176K+__WaveHeader头的字节数,保存最多11秒的数据
    }
    tomcat_load_ui();		//装载主界面
    rbtn=btn_creat(lcddev.width-2*gui_phy.tbfsize-8-1,lcddev.height-gui_phy.tbheight,2*gui_phy.tbfsize+8,gui_phy.tbheight-1,0,0x03);//创建文字按钮
    mbtn=btn_creat(0,lcddev.height-gui_phy.tbheight,2*gui_phy.tbfsize+8,gui_phy.tbheight-1,0,0x03);//创建文字按钮
    if(mainbuf&&rbtn&&mbtn)
    {
        rbtn->caption=(u8*)GUI_BACK_CAPTION_TBL[gui_phy.language];		//返回
        rbtn->font=gui_phy.tbfsize;//设置新的字体大小
        rbtn->bcfdcolor=WHITE;	//按下时的颜色
        rbtn->bcfucolor=WHITE;	//松开时的颜色

        mbtn->caption=(u8*)GUI_OPTION_CAPTION_TBL[gui_phy.language];	//选项
        mbtn->font=gui_phy.tbfsize;//设置新的字体大小
        mbtn->bcfdcolor=WHITE;	//按下时的颜色
        mbtn->bcfucolor=WHITE;	//松开时的颜色

        wavhead=(__WaveHeader*)mainbuf;				//将mainbuf的最开始部分转换为wav头数据
        databuf=mainbuf+sizeof(__WaveHeader);		//数据区紧接wav头之后

        btn_draw(rbtn);
        btn_draw(mbtn);
        if(audiodev.status&(1<<7))					//当前在放歌??必须停止
        {
            audio_stop_req(&audiodev);				//停止音频播放
            audio_task_delete();					//删除音乐播放任务.
        }
        recorder_wav_init(wavhead);					//初始化wav数据
        wavhead->fmt.SampleRate=(wavspd+8)*500;	 	//设置采样率,以获得不同的效果.
        tomcat_rec_mode(recagc);					//进入录音模式
        delay_ms(300);
        vutimecnt=240;//想让数据缓冲区数据满（8192字节）
        while(rval==0)
        {
            tp_dev.scan(0);
            in_obj.get_key(&tp_dev,IN_TYPE_TOUCH);	//得到按键键值
            delay_ms(1000/OS_TICKS_PER_SEC);		//延时一个时钟节拍
            if(system_task_return)break;			//TPAD返回
            res=btn_check(rbtn,&in_obj); 			//检查返回按钮
            if(res&&((rbtn->sta&(1<<7))==0)&&(rbtn->sta&(1<<6)))
            {
                if(rec_sta&0X80)//如果正在录音,则保存这个录音文件
                {
                    break;
                }
                break;//返回按钮
            }
            res=btn_check(mbtn,&in_obj); 			//检查设置按钮
            if(res&&((mbtn->sta&(1<<7))==0)&&(mbtn->sta&(1<<6)))//弹出设置界面
            {
                app_read_bkcolor((lcddev.width-180)/2,(lcddev.height-158)/2,180,158,(u16*)databuf);//读取背景色
                res=tomcat_agcspd_set((lcddev.width-180)/2,(lcddev.height-158)/2,&recagc,&wavspd,(u8*)tomcat_set_tbl[gui_phy.language]);//设置增益和语速
                if(res==0)
                {
                    VS_WR_Cmd(SPI_AICTRL1,1024*recagc);	//设置增益:1~10
                    wavhead->fmt.SampleRate=(wavspd+8)*500;	//设置采样率,以获得不同的效果.
                }
                app_recover_bkcolor((lcddev.width-180)/2,(lcddev.height-158)/2,180,158,(u16*)databuf);//恢复背景色
                dcnt=0;//执行了一次数据毁灭,清空数据计数器
            }
            w=VS_RD_Reg(SPI_HDAT1);
            if((w>=256)&&(w<896))
            {
                idx=0;
                while(idx<256) //一次读取256次,512字节
                {
                    w=VS_RD_Reg(SPI_HDAT0);
                    tempval=(short)w;
                    if(tempval<0)tempval=-tempval;
                    if(maxval<tempval)maxval=tempval;
                    if(rec_sta)//正在录音
                    {
                        if(dcnt<(176*1000-1))//开始录音,总记录长度不能超过BUF大小(11秒钟)
                        {
                            databuf[dcnt++]=w&0XFF;
                            databuf[dcnt++]=w>>8;
                        }
                    } else//没有开始录音的时候,总是记录最后的8192个音频数据
                    {
                        if(dcnt>=8192)//大于8192,左移512字节.
                        {
                            tomcat_data_move(databuf,8192,512);	//整体左移512字节
                            dcnt-=512;							//数组减少512字节
                        }
                        if(dcnt<8192)//不到8192,继续记录
                        {
                            databuf[dcnt++]=w&0XFF;
                            databuf[dcnt++]=w>>8;
                        }
                    }
                    idx++;
                }
                vutimecnt++;
                if(vutimecnt==2)//每读取1024字节进入一次
                {
                    vutimecnt=0;
                    temp=recorder_vu_get(maxval);
                    if(rec_sta)//正在录音
                    {
                        if(temp<TOMCAT_STOP_LEVEL)timecnt++;
                        else timecnt=0;
                    } else	//非录音状态
                    {
                        if(temp>TOMCAT_START_LEVEL)//收到有效电平
                        {
                            timecnt=0;//清空计时器
                            rec_sta=1;//标记要开始录音了.
                        }
                    }

                    if(temp>=vulevel)vulevel=temp;
                    else if(vulevel)vulevel--;
                    recorder_vu_meter((lcddev.width-218)/2,(lcddev.height-100)/2+100,vulevel);	//显示vu meter;
                    maxval=0;
                }
            }
            if(rec_sta)//正在录音
            {
                if(timecnt>=10)//连续0.64秒没有检测到有效音频
                {
                    wavhead->riff.ChunkSize=dcnt+36;	//整个文件的大小-8;
                    wavhead->data.ChunkSize=dcnt;		//数据大小
                    tomcat_play_wav(mainbuf,dcnt+sizeof(__WaveHeader));//播放录音
                    tomcat_rec_mode(recagc);			//进入录音模式
                    delay_ms(500);
                    dcnt=0;
                    timecnt=0;
                    rec_sta=0;
                }
            }
        }
        VS_HD_Reset();	//硬复位VS1003
    }
    gui_memex_free(mainbuf);
    btn_delete(rbtn);
    btn_delete(mbtn);
    return rval;
}



































