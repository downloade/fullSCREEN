#include "nes_main.h" 
#include "nes_ppu.h"
#include "nes_mapper.h"
#include "nes_apu.h"
#include "usart3.h"	
#include "malloc.h" 
#include "key.h"
#include "lcd.h"    
#include "ff.h"
#include "string.h"
#include "usart.h" 
#include "timer.h" 
#include "joypad.h"    	
#include "vs10xx.h"    	
#include "spi.h"      	
#include "audioplay.h"     	
#include "spb.h"     

//////////////////////////////////////////////////////////////////////////////////	 
//本程序移植自网友ye781205的NES模拟器工程
//ALIENTEK STM32开发板
//NES主函数 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/7/1
//版本：V1.0  			  
////////////////////////////////////////////////////////////////////////////////// 	 
 

extern vu8 frame_cnt;	//帧计数器 
int MapperNo;			//map编号
int NES_scanline;		//nes扫描线
int VROM_1K_SIZE;
int VROM_8K_SIZE;
u32 NESrom_crc32;

uint8_t PADdata0;   			//手柄1键值 [7:0]右7 左6 下5 上4 Start3 Select2 B1 A0  
uint8_t PADdata1;   			//手柄2键值 [7:0]右7 左6 下5 上4 Start3 Select2 B1 A0  
uint8_t *NES_RAM;			//保持1024字节对齐
uint8_t *NES_SRAM;  
NES_header *RomHeader; 	//rom文件头
MAPPER *NES_Mapper;		 
MapperCommRes *MAPx;  


uint8_t* spr_ram;			//精灵RAM,256字节
ppu_data* ppu;			//ppu指针
uint8_t* VROM_banks;
uint8_t* VROM_tiles;

apu_t *apu; 			//apu指针
uint8_t *wave_buffers;


uint8_t *nesapusbuf[NES_APU_BUF_NUM];		//音频缓冲帧
uint8_t* romfile;							//nes文件指针,指向整个nes文件的起始地址.
//////////////////////////////////////////////////////////////////////////////////////

 
//加载ROM
//返回值:0,成功
//    1,内存错误
//    3,map错误
uint8_t nes_load_rom(void)
{  
    uint8_t* p;  
	uint8_t i;
	uint8_t res=0;
	p=(uint8_t*)romfile;	
	if(strncmp((char*)p,"NES",3)==0)
	{  
		RomHeader->ctrl_z=p[3];
		RomHeader->num_16k_rom_banks=p[4];
		RomHeader->num_8k_vrom_banks=p[5];
		RomHeader->flags_1=p[6];
		RomHeader->flags_2=p[7]; 
		if(RomHeader->flags_1&0x04)p+=512;		//有512字节的trainer:
		if(RomHeader->num_8k_vrom_banks>0)		//存在VROM,进行预解码
		{		
			VROM_banks=p+16+(RomHeader->num_16k_rom_banks*0x4000);
#if	NES_RAM_SPEED==1	//1:内存占用小 0:速度快	 
			VROM_tiles=VROM_banks;	 
#else  
			VROM_tiles=mymalloc(SRAMEX,RomHeader->num_8k_vrom_banks*8*1024);//这里可能申请多达1MB内存!!!
			if(VROM_tiles==0)VROM_tiles=VROM_banks;//内存不够用的情况下,尝试VROM_titles与VROM_banks共用内存			
			compile(RomHeader->num_8k_vrom_banks*8*1024/16,VROM_banks,VROM_tiles);  
#endif	
		}else 
		{
			VROM_banks=mymalloc(SRAMIN,8*1024);
			VROM_tiles=mymalloc(SRAMEX,8*1024);
			if(!VROM_banks||!VROM_tiles)res=1;
		}  	
		VROM_1K_SIZE = RomHeader->num_8k_vrom_banks * 8;
		VROM_8K_SIZE = RomHeader->num_8k_vrom_banks;  
		MapperNo=(RomHeader->flags_1>>4)|(RomHeader->flags_2&0xf0);
		if(RomHeader->flags_2 & 0x0E)MapperNo=RomHeader->flags_1>>4;//忽略高四位，如果头看起来很糟糕 
		printf("use map:%d\r\n",MapperNo);
		for(i=0;i<255;i++)  // 查找支持的Mapper号
		{		
			if (MapTab[i]==MapperNo)break;		
			if (MapTab[i]==-1)res=3; 
		} 
		if(res==0)
		{
			switch(MapperNo)
			{
				case 1:  
					MAP1=mymalloc(SRAMIN,sizeof(Mapper1Res)); 
					if(!MAP1)res=1;
					break;
				case 4:  
				case 6: 
				case 16:
				case 17:
				case 18:
				case 19:
				case 21: 
				case 23:
				case 24:
				case 25:
				case 64:
				case 65:
				case 67:
				case 69:
				case 85:
				case 189:
					MAPx=mymalloc(SRAMIN,sizeof(MapperCommRes)); 
					if(!MAPx)res=1;
					break;  
				default:
					break;
			}
		}
	} 
	return res;	//返回执行结果
} 
//释放内存 
void nes_sram_free(void)
{ 
	uint8_t i;
	myfree(SRAMIN,NES_RAM);		
	myfree(SRAMIN,NES_SRAM);	
	myfree(SRAMIN,RomHeader);	
	myfree(SRAMIN,NES_Mapper);
	myfree(SRAMIN,spr_ram);		
	myfree(SRAMIN,ppu);	
	myfree(SRAMIN,apu);	
	myfree(SRAMIN,wave_buffers);
	for(i=0;i<NES_APU_BUF_NUM;i++)myfree(SRAMEX,nesapusbuf[i]);//释放APU BUFs 
	myfree(SRAMEX,romfile);	  
	if((VROM_tiles!=VROM_banks)&&VROM_banks&&VROM_tiles)//如果分别为VROM_banks和VROM_tiles申请了内存,则释放
	{
		myfree(SRAMIN,VROM_banks);
		myfree(SRAMEX,VROM_tiles);		 
	}
	switch (MapperNo)//释放map内存
	{
		case 1: 			//释放内存
			myfree(SRAMIN,MAP1);
			break;	 	
		case 4: 
		case 6: 
		case 16:
		case 17:
		case 18:
		case 19:
		case 21:
		case 23:
		case 24:
		case 25:
		case 64:
		case 65:
		case 67:
		case 69:
		case 85:
		case 189:
			myfree(SRAMIN,MAPx);break;	 		//释放内存 
		default:break; 
	}
	NES_RAM=0;	
	NES_SRAM=0;
	RomHeader=0;
	NES_Mapper=0;
	spr_ram=0;
	ppu=0;
	apu=0;
	wave_buffers=0;
	for(i=0;i<NES_APU_BUF_NUM;i++)nesapusbuf[i]=0; 
	VROM_banks=0;
	VROM_tiles=0; 
	MAP1=0;
	MAPx=0;
} 
//为NES运行申请内存
//romsize:nes文件大小
//返回值:0,申请成功
//       1,申请失败
uint8_t nes_sram_malloc(u32 romsize)
{
	u16 i=0;
	for(i=0;i<64;i++)//为NES_RAM,查找1024对齐的内存
	{
		NES_SRAM=mymalloc(SRAMIN,i*32);
		NES_RAM=mymalloc(SRAMIN,0X800);	//申请2K字节,必须1024字节对齐
		if((u32)NES_RAM%1024)			//不是1024字节对齐
		{
			myfree(SRAMIN,NES_RAM);		//释放内存,然后重新尝试分配
			myfree(SRAMIN,NES_SRAM); 
		}else 
		{
			myfree(SRAMIN,NES_SRAM); 	//释放内存
			break;
		}
	}	 
 	NES_SRAM=mymalloc(SRAMIN,0X2000);
	RomHeader=mymalloc(SRAMIN,sizeof(NES_header));
	NES_Mapper=mymalloc(SRAMIN,sizeof(MAPPER));
	spr_ram=mymalloc(SRAMIN,0X100);		
	ppu=mymalloc(SRAMIN,sizeof(ppu_data));  
	apu=mymalloc(SRAMIN,sizeof(apu_t));		//sizeof(apu_t)=  12588
	wave_buffers=mymalloc(SRAMIN,APU_PCMBUF_SIZE);
	for(i=0;i<NES_APU_BUF_NUM;i++)
	{
		nesapusbuf[i]=mymalloc(SRAMEX,APU_PCMBUF_SIZE+10);//申请内存
	}
 	romfile=mymalloc(SRAMEX,romsize);			//申请游戏rom空间,等于nes文件大小 
	if(romfile==NULL)//内存不够?释放主界面占用内存,再重新申请
	{
		spb_delete();//释放SPB占用的内存
		romfile=mymalloc(SRAMEX,romsize);//重新申请
	}
	if(i==64||!NES_RAM||!NES_SRAM||!RomHeader||!NES_Mapper||!spr_ram||!ppu||!apu||!wave_buffers||!nesapusbuf[NES_APU_BUF_NUM-1]||!romfile)
	{
		nes_sram_free();
		return 1;
	}
	memset(NES_SRAM,0,0X2000);				//清零
	memset(RomHeader,0,sizeof(NES_header));	//清零
	memset(NES_Mapper,0,sizeof(MAPPER));	//清零
	memset(spr_ram,0,0X100);				//清零
	memset(ppu,0,sizeof(ppu_data));			//清零
	memset(apu,0,sizeof(apu_t));			//清零
	memset(wave_buffers,0,APU_PCMBUF_SIZE);	//清零
	for(i=0;i<NES_APU_BUF_NUM;i++)memset(nesapusbuf[i],0,APU_PCMBUF_SIZE+10);//清零 
	memset(romfile,0,romsize);				//清零 
	return 0;
} 

void uart_initx(u32 pclk2,u32 bound)
{  	 
	NVIC_InitTypeDef NVIC_InitStructure;
	float temp;
	u16 mantissa;
	u16 fraction;	   
	temp=(float)(pclk2*1000000)/(bound*16);//得到USARTDIV
	mantissa=temp;				 //得到整数部分
	fraction=(temp-mantissa)*16; //得到小数部分	 
    mantissa<<=4;
	mantissa+=fraction; 
	RCC->APB2ENR|=1<<2;   //使能PORTA口时钟  
	RCC->APB2ENR|=1<<14;  //使能串口时钟 
	GPIOA->CRH&=0XFFFFF00F;//IO状态设置
	GPIOA->CRH|=0X000008B0;//IO状态设置 
	RCC->APB2RSTR|=1<<14;   //复位串口1
	RCC->APB2RSTR&=~(1<<14);//停止复位	   	   
	//波特率设置
 	USART1->BRR=mantissa; // 波特率设置	 
	USART1->CR1|=0X200C;  //1位停止,无校验位.
#if EN_USART1_RX		  //如果使能了接收
	//使能接收中断 
	USART1->CR1|=1<<5;    //接收缓冲区非空中断使能	
	
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
#endif
}

//频率设置
//PLL,倍频数
void nes_clock_set(uint8_t PLL)
{
	u16 tPLL=PLL;
	uint8_t temp=0;	 
	RCC->CFGR&=0XFFFFFFFC;	//修改时钟频率为内部8M	   
	RCC->CR&=~0x01000000;  	//PLLOFF  
 	RCC->CFGR&=~(0XF<<18);	//清空原来的设置
 	PLL-=2;//抵消2个单位
	RCC->CFGR|=PLL<<18;   	//设置PLL值 2~16
	RCC->CFGR|=1<<16;	  	//PLLSRC ON 
	FLASH->ACR|=0x12;	  	//FLASH 2个延时周期
 	RCC->CR|=0x01000000;  	//PLLON
	while(!(RCC->CR>>25));	//等待PLL锁定
	RCC->CFGR|=0x02;		//PLL作为系统时钟	 
	while(temp!=0x02)    	//等待PLL作为系统时钟设置成功
	{
		temp=RCC->CFGR>>2;
		temp&=0x03;
	}
 	//顺便设置延时和串口
	delay_init(tPLL*8);			//延时初始化
	uart_initx(tPLL*8,115200); 	//串口1初始化  
	usart3_fast_init(tPLL*4,115200);//串口3 APB2时钟是串口1的2倍
}  
extern vu8 nes_spped_para;		//NES游戏进行时,将会对此值设置,默认为0.

//开始nes游戏
//pname:nes游戏路径
//返回值:
//0,正常退出
//1,内存错误
//2,文件错误
//3,不支持的map
uint8_t nes_load(uint8_t* pname)
{
	FIL *file; 
	UINT br;
	uint8_t res=0;   
	file=mymalloc(SRAMIN,sizeof(FIL));  
	if(file==0)return 1;						//内存申请失败.  
	res=f_open(file,(char*)pname,FA_READ);
	if(res!=FR_OK)	//打开文件失败
	{
		myfree(SRAMIN,file);
		return 2;
	}	 
	res=nes_sram_malloc(file->fsize);			//申请内存 
	if(res==0)
	{
		f_read(file,romfile,file->fsize,&br);	//读取nes文件
		NESrom_crc32=get_crc32(romfile+16, file->fsize-16);//获取CRC32的值	
		res=nes_load_rom();						//加载ROM
		if(res==0) 					
		{   
			nes_clock_set(16);					//128M
			TPAD_Init(16);
			nes_spped_para=1;					//SPI速度减半
			JOYPAD_Init();
			cpu6502_init();						//初始化6502,并复位	  	 
			Mapper_Init();						//map初始化
			PPU_reset();						//ppu复位
			apu_init(); 						//apu初始化 
			nes_sound_open(0,APU_SAMPLE_RATE);	//初始化播放设备
			nes_emulate_frame();				//进入NES模拟器主循环 
			nes_sound_close();					//关闭声音输出
			nes_clock_set(9);					//72M  
			TPAD_Init(6);
			nes_spped_para=0;					//SPI速度恢复
		}
	}
	f_close(file);
	myfree(SRAMIN,file);//释放内存
	nes_sram_free();	//释放内存
	return res;
} 
uint8_t nes_xoff=0;	//显示在x轴方向的偏移量(实际显示宽度=256-2*nes_xoff)
//设置游戏显示窗口
void nes_set_window(void)
{	
	u16 xoff=0,yoff=0; 
	u16 lcdwidth,lcdheight;
	if(lcddev.width==240)
	{
		lcdwidth=240;
		lcdheight=240;
		nes_xoff=(256-lcddev.width)/2;	//得到x轴方向的偏移量
 		xoff=0; 
	}else if(lcddev.width==320) 
	{
		lcdwidth=256;
		lcdheight=240; 
		nes_xoff=0;
		xoff=(lcddev.width-256)/2;
	}else if(lcddev.width==480)
	{
		lcdwidth=480;
		lcdheight=480; 
		nes_xoff=(256-(lcddev.width/2))/2;//得到x轴方向的偏移量
 		xoff=0;  
	}
	yoff=32 + 8;//标题高度(lcddev.height-lcdheight)/2;//屏幕高度 
	LCD_Set_Window(xoff,yoff,lcdwidth,lcdheight);//让NES始终在屏幕的正中央显示
	LCD_WriteRAM_Prepare();//写入LCD RAM的准备 	
}
extern void KEYBRD_FCPAD_Decode(uint8_t *fcbuf,uint8_t mode);
//读取游戏手柄数据
void nes_get_gamepadval(void)
{
	if(USART3_RX_STA & 0x08000)
	{
		//printf("usart3 data:%s\r\n",USART3_RX_BUF);
		if(USART3_RX_BUF[0] == 'A')
		{
			PADdata0 |= 0x10;
		}
		else
		{
			PADdata0 &= ~0x10;
		}
		if(USART3_RX_BUF[1] == 'B')
		{
			PADdata0 |= 0x20;
		}
		else
		{
			PADdata0 &= ~0x20;
		}
		if(USART3_RX_BUF[2] == 'C')
		{
			PADdata0 |= 0x40;
		}
		else
		{
			PADdata0 &= ~0x40;
		}
		if(USART3_RX_BUF[3] == 'D')
		{
			PADdata0 |= 0x80;
		}
		else
		{
			PADdata0 &= ~0x80;
		}
		
		if(USART3_RX_BUF[4] == 'E')
		{
			PADdata0 |= 0x01;
		}
		else
		{
			PADdata0 &= ~0x01;
		}
		if(USART3_RX_BUF[5] == 'F')
		{
			PADdata0 |= 0x02;
		}
		else
		{
			PADdata0 &= ~0x02;
		}
		if(USART3_RX_BUF[6] == 'G')
		{
			PADdata0 |= 0x04;
		}
		else
		{
			PADdata0 &= ~0x04;
		}
		if(USART3_RX_BUF[7] == 'H')
		{
			PADdata0 |= 0x08;
		}
		else
		{
			PADdata0 &= ~0x08;
		}
		USART3_RX_STA = 0;
	}
	else
	{
		PADdata0 = get_ctr_res();
	}
	//LCD_ShowNum(2,799-16,PADdata0,3,16);取消显示按键值
	//PADdata0=JOYPAD_Read();	//读取手柄1的值
	//PADdata1=0;				//没有手柄2,故不采用. 
}

_btn_obj * tbtn_game[8];		//总共8个按钮

static uint8_t game_btn_check(_btn_obj * btnx,void * in_key)
{
	_in_obj *key=(_in_obj*)in_key;
	uint8_t btnok=0;
	if((btnx->sta&0X03)==BTN_INACTIVE)return 0;//无效状态的按键,直接不检测
	switch(key->intype)
	{
		case IN_TYPE_TOUCH:	//触摸屏按下了
			if(btnx->top<key->y&&key->y<(btnx->top+btnx->height)&&btnx->left<key->x&&key->x<(btnx->left+btnx->width))//在按键内部
			{
				btnx->sta&=~(0X03);
				btnx->sta|=BTN_PRESS;//按下
				//btn_draw(btnx);//画按钮  屏蔽重绘按钮，严重影响帧数
				btnok=1;
			}else 
			{
				btnx->sta&=~(0X03);
				btnx->sta|=BTN_RELEASE;	//松开
				//btn_draw(btnx);			//画按钮
			}
			break;
		default:
			break;
	}
	return btnok;
}

void delete_ctr_ui(void)
{
	uint8_t i;
	for(i=0;i<8;i++)
	btn_delete(tbtn_game[i]);//删除按钮
}
uint8_t get_ctr_res(void)
{
	uint8_t res = 0,i,val = 0;
	tp_dev.scan(0);    
	in_obj.get_key(&tp_dev,IN_TYPE_TOUCH);	//得到按键键值
	for(i=0;i<8;i++)
	{
		res=game_btn_check(tbtn_game[i],&in_obj);//确认按钮检测
		if(res)
		{
			if((tbtn_game[i]->sta&0X80)==0)//有有效操作
			{
				switch(i)
				{
					case 0:
						val |= 1 << 2;
						break;
					case 1:
						val |= 1 << 3;
						break;
					case 2:
						val |= 1 << 6;
						break;
					case 3:
						val |= 1 << 7;
						break;
					case 4:
						val |= 1 << 4;
						break;
					case 5:
						val |= 1 << 5;
						break;
					case 6:
						val |= 1 << 0;
						break;
					case 7:
						val |= 1 << 1;
						break;
				}
			}
		}
	}
	return val;
}
void load_ctr_ui(void)
{
	u16 button_w,button_h,W,H;
	
	uint8_t rval=0;
	uint8_t i;
	
	if(lcddev.width == 480)
	{
		button_w = 60;
		button_h = 40;
	}
	else
	{
		button_w = 30;
		button_h = 18;
	}
	
	W = (lcddev.width - button_w * 6) / (6 + 1);//计算缝隙宽度
	H = (lcddev.height  - 552 - button_h * 3) / (3 + 1);//计算缝隙宽度
	
 	tbtn_game[0]=btn_creat(button_w * 1 + W * 2, 552 + button_h * 0 + H * 1,button_w,button_h,0,0x02);	//创建按钮
	tbtn_game[1]=btn_creat(button_w * 1 + W * 2, 552 + button_h * 2 + H * 3,button_w,button_h,1,0x02);	//创建按钮
	tbtn_game[2]=btn_creat(button_w * 0 + W * 1, 552 + button_h * 1 + H * 2,button_w,button_h,2,0x02);	//创建按钮
	tbtn_game[3]=btn_creat(button_w * 2 + W * 3, 552 + button_h * 1 + H * 2,button_w,button_h,3,0x02);	//创建按钮
	tbtn_game[4]=btn_creat(button_w * 4 + W * 5, 552 + button_h * 0 + H * 1,button_w,button_h,4,0x02);	//创建按钮
	tbtn_game[5]=btn_creat(button_w * 4 + W * 5, 552 + button_h * 2 + H * 3,button_w,button_h,5,0x02);	//创建按钮
	tbtn_game[6]=btn_creat(button_w * 3 + W * 4, 552 + button_h * 1 + H * 2,button_w,button_h,6,0x02);	//创建按钮
	tbtn_game[7]=btn_creat(button_w * 5 + W * 6, 552 + button_h * 1 + H * 2,button_w,button_h,7,0x02);	//创建按钮
	for(i=0;i<8;i++)
	{
		if(tbtn_game[i]==NULL)
		{
			rval=1;
			break;
		}
		
		tbtn_game[i]->bcfucolor=BLACK;//松开时为黑色
		tbtn_game[i]->bcfdcolor=WHITE;//按下时为白色			
		tbtn_game[i]->bkctbl[0]=0X453A;//边框颜色
		tbtn_game[i]->bkctbl[1]=0X5DDC;//第一行的颜色				
		tbtn_game[i]->bkctbl[2]=0X5DDC;//上半部分颜色
		tbtn_game[i]->bkctbl[3]=0X453A;//下半部分颜色

		if(i==0)tbtn_game[i]->caption="Select";
		if(i==1)tbtn_game[i]->caption="Start";	
		if(i==2)tbtn_game[i]->caption="Left";
		if(i==3)tbtn_game[i]->caption="Right";
		if(i==4)tbtn_game[i]->caption="Up";
		if(i==5)tbtn_game[i]->caption="Down";
		if(i==6)tbtn_game[i]->caption="A";
		if(i==7)tbtn_game[i]->caption="B";
	}
	if(rval==0)//无错误
	{
		for(i=0;i<8;i++)btn_draw(tbtn_game[i]);	//画按钮
 	}
}

//nes模拟器主循环
void nes_emulate_frame(void)
{
	uint8_t nes_frame;
	TIM3_Int_Init(10000-1,12800-1);//启动TIM3 ,1s中断一次
	load_ctr_ui();
	nes_set_window();//设置窗口
	system_task_return=0;
	while(1)
	{
		// LINES 0-239
		PPU_start_frame();
		for(NES_scanline = 0; NES_scanline< 240; NES_scanline++)
		{
			run6502(113*256);
			NES_Mapper->HSync(NES_scanline);
			//扫描一行		  
			if(nes_frame==0)scanline_draw(NES_scanline);
			else do_scanline_and_dont_draw(NES_scanline); 
		}  
		NES_scanline=240;
		run6502(113*256);//运行1线
		NES_Mapper->HSync(NES_scanline); 
		start_vblank(); 
		if(NMI_enabled()) 
		{
			cpunmi=1;
			run6502(7*256);//运行中断
		}
		NES_Mapper->VSync();
		// LINES 242-261    
		for(NES_scanline=241;NES_scanline<262;NES_scanline++)
		{
			run6502(113*256);	  
			NES_Mapper->HSync(NES_scanline);		  
		}	   
		end_vblank(); 
		//LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复屏幕窗口用于更新屏幕按键状态
		nes_get_gamepadval();	//每3帧查询一次USB
		//nes_set_window();//因为检测屏幕按键时候更新了按钮状态所以需要重新设置窗口
		apu_soundoutput();		//输出游戏声音	 
		frame_cnt++; 	
		nes_frame++;
		if(nes_frame>NES_SKIP_FRAME)
		{
			nes_frame=0;//跳帧  
			if(lcddev.id==0X1963)nes_set_window();//重设窗口
		}
		if(system_task_return)
		{
			system_task_return=0;
			break;
 		}
	}
	LCD_Set_Window(0,0,lcddev.width,lcddev.height);//恢复屏幕窗口
	delete_ctr_ui();
	TIM3->CR1&=~(1<<0);//关闭定时器3
}
//在6502.s里面被调用
void debug_6502(u16 reg0,uint8_t reg1)
{
	printf("6502 error:%x,%d\r\n",reg0,reg1);
}
////////////////////////////////////////////////////////////////////////////////// 	 
//nes,音频输出支持部分
vu16 nesbufpos=0;
vu8 nesplaybuf=0;		//即将播放的音频帧缓冲编号
vu8 nessavebuf=0;		//当前保存到的音频缓冲编号 

//音频播放回调函数
void nes_vs10xx_feeddata(void)
{  
	uint8_t n;
	uint8_t nbytes;
	uint8_t *p; 
	if(nesplaybuf==nessavebuf)return;//还没有收到新的音频数据
	if(VS_DQ!=0)//可以发送数据给VS10XX
	{		 
		p=nesapusbuf[nesplaybuf]+nesbufpos; 
		nesbufpos+=32; 
		if(nesbufpos>APU_PCMBUF_SIZE)
		{
			nesplaybuf++;
			if(nesplaybuf>(NES_APU_BUF_NUM-1))nesplaybuf=0; 	
			nbytes=APU_PCMBUF_SIZE+32-nesbufpos;
			nesbufpos=0; 
		}else nbytes=32;
		for(n=0;n<nbytes;n++)
		{
			if(p[n]!=0)break;	//判断是不是剩余所有的数据都为0? 
		}
		if(n==nbytes)return;	//都是0,则直接不写入VS1053了,以免引起哒哒声. 
		VS_XDCS=0;  
		for(n=0;n<nbytes;n++)
		{
			SPI1_ReadWriteByte(p[n]);	 			
		}
		VS_XDCS=1;     				   
	} 
}
//NES模拟器声音从VS1053输出,模拟WAV解码的wav头数据.
const uint8_t nes_wav_head[]=
{
0X52,0X49,0X46,0X46,0XFF,0XFF,0XFF,0XFF,0X57,0X41,0X56,0X45,0X66,0X6D,0X74,0X20,
0X10,0X00,0X00,0X00,0X01,0X00,0X01,0X00,0X11,0X2B,0X00,0X00,0X11,0X2B,0X00,0X00,
0X01,0X00,0X08,0X00,0X64,0X61,0X74,0X61,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
};
//NES打开音频输出
int nes_sound_open(int samples_per_sync,int sample_rate) 
{
	uint8_t *p;
	uint8_t i; 
	p=mymalloc(SRAMIN,100);	//申请100字节内存
	if(p==NULL)return 1;	//内存申请失败,直接退出
	printf("sound open:%d\r\n",sample_rate);
	for(i=0;i<sizeof(nes_wav_head);i++)//复制nes_wav_head内容
	{
		p[i]=nes_wav_head[i];
	}
	if(lcddev.width==480)	//是480*480屏幕
	{
		sample_rate=8000;	//设置8Khz,约原来速度的0.75倍
	}
	p[24]=sample_rate&0XFF;			//设置采样率
	p[25]=(sample_rate>>8)&0XFF;
	p[28]=sample_rate&0XFF;			//设置字节速率(8位模式,等于采样率)
	p[29]=(sample_rate>>8)&0XFF; 
	nesplaybuf=0;
	nessavebuf=0;	
	VS_HD_Reset();		   			//硬复位
	VS_Soft_Reset();  				//软复位 
	VS_Set_All();					//设置音量等参数 			 
	VS_Reset_DecodeTime();			//复位解码时间 	  	 
	while(VS_Send_MusicData(p));	//发送wav head
	while(VS_Send_MusicData(p+32));	//发送wav head
	TIM6_Int_Init(100-1,1280-1);	//1ms中断一次
	myfree(SRAMIN,p);				//释放内存
	return 1;
}
//NES关闭音频输出
void nes_sound_close(void) 
{ 
	TIM6->CR1&=~(1<<0);				//关闭定时器6
	VS_SPK_Set(0);					//关闭喇叭输出 
	VS_Set_Vol(0);					//设置音量为0 	
} 
//NES音频输出到VS1053缓存
void nes_apu_fill_buffer(int samples,uint8_t* wavebuf)
{	 
 	u16	i;	
	uint8_t tbuf;
	for(i=0;i<APU_PCMBUF_SIZE;i++)
	{
		nesapusbuf[nessavebuf][i]=wavebuf[i]; 
	}
	tbuf=nessavebuf;
	tbuf++;
	if(tbuf>(NES_APU_BUF_NUM-1))tbuf=0;
	while(tbuf==nesplaybuf)//输出数据赶上音频播放的位置了,等待.
	{ 
		delay_ms(5);
	}
	nessavebuf=tbuf; 
}	



















