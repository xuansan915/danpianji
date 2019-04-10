//#include "reg51.h"
#include "stc8.h"
#include "intrins.h"

#define FOSC            11059200UL
#define BRT             (65536 - FOSC / 115200 / 4)
#define BUF_SIZE_MAX	30
#define BUF_SIZE		17
#define TIME_NUM		1000
#define START_LEN  19


#define STATUS_PW_OFF    0
#define STATUS_PW_ON     1
#define STATUS_ERROR     2
#define STATUS_TIME_OUT  3


sbit    POWEREN			=	P3^7;   //电源控制IO

bit STATUS_flag = 0;
bit busy;
bit busy_1;
bit flag = 0;
bit time_out = 0;

char status = 0;

char wptr = 0;
char rptr = 0;
char buffer[BUF_SIZE_MAX];/*多定义长度防止有多余数据发出*/
int num = 0;
char common_signal[BUF_SIZE-3] = {0x7c,0x2,0x2,0,0x1,0,0,0,0,0,0,0,0,0};

char start_signal[START_LEN] = {0x7c,0x02,0x01,0x00,0x0c,0x41,0x31,0x31,0x38,0x30,0x31,0x30,0x31,0x30,0x30,0x30,0x31,0x77,0x7c};

unsigned char code DataStr[]=__DATE__; /*定义当前编译日期*/
unsigned char code TimeStr[]=__TIME__; /*定义当前编译时间*/
//unsigned char code TimeStr[]=__C51__; /*定义当前编译器版本*/

unsigned char code shuzhi_code[]="0123456789ABCDEF";
unsigned char code staus_PWON[]="staus_PWON\r\n";
unsigned char code staus_PWOFF[]="staus_PWOFF\r\n";
unsigned char code staus_error[]="staus_error\r\n";
unsigned char code staus_Time_out[]="staus_Time_out\r\n";


void time_init()
{
	//禁止计数
	TR0 = 0;
	
	TL0 = 0x0;  
    TH0 = 0x28;
	TR0 = 1; /*启动定时器*/
	ET0 = 1; /*使能定时器中断*/
	
}

void TM0_Isr() interrupt 1
{
	num++;

	if(num == TIME_NUM)
	{
		time_out = 1;
	}
}


void recieve_data_check()
{
	if(wptr >= START_LEN)
	{
		char bStart = 1;
		char i = 0;
		wptr = 0;
		for(; i < START_LEN; i++)
		{
			if(buffer[i] != start_signal[i])//接收到不正确的信号
			{
				bStart = 0;
				break;
			}
		}
		if(bStart == 1)
		{
			POWEREN = 0;
			status=STATUS_PW_ON;
			STATUS_flag=1;
			/*停止定时器*/
			TR0 = 0;
			ET0 = 0; /*使能定时器中断*/
		}
	}
	else if(wptr >= BUF_SIZE)
	{
		char i;	

		for(i = 0;i<BUF_SIZE-3;i++)
		{
			if(buffer[i]!=common_signal[i])//接收到不正确的信号
			{
				//status=STATUS_ERROR;
				//STATUS_flag=1;
				break;
			}
		}

		if(i != BUF_SIZE-3)//接收到不正确的信号, 不处理消息，不响应
		{
			//wptr = 0;
			//status=STATUS_ERROR;
			//STATUS_flag=1;
			return;
		}

		if(buffer[BUF_SIZE-3] == 0 && buffer[BUF_SIZE-2] == 1 && buffer[BUF_SIZE-1] == 0x7c) //开机
		{
			wptr = 0;
			POWEREN = 0;
			status=STATUS_PW_ON;
			STATUS_flag=1;
			/*停止定时器*/
			TR0 = 0;
			ET0 = 0; /*使能定时器中断*/	
		}
		else if(buffer[BUF_SIZE-3] == 0x1 && buffer[BUF_SIZE-2] == 0x11 && buffer[BUF_SIZE-1] == 0x7c) //关机
		{
			wptr = 0;
			POWEREN = 1;
			status=STATUS_PW_OFF;
			STATUS_flag=1;
			/*停止定时器*/
			TR0 = 0;
			ET0 = 0; /*使能定时器中断*/	
		}	

	}
	return;
}


void Uart2Isr() interrupt 8
{
    if (S2CON & 0x02)
    {
        S2CON &= ~0x02;
        busy = 0;
    }
    if (S2CON & 0x01)
    {
        S2CON &= ~0x01;
		if(wptr < BUF_SIZE_MAX)
		{
			buffer[wptr++] = S2BUF; 
		}
		num = 0;
		time_init();
    }
}


void Uart2Init()
{
    S2CON = 0x10;
    T2L = BRT;
    T2H = BRT >> 8;
    AUXR = 0x14;
    wptr = 0x00;
    //rptr = 0x00;
    busy = 0;
}

void UartInit()
{
   	/*
	SCON = 0x50;	//8位数据,可变波特率
	AUXR |= 0x40;	//定时器1时钟为Fosc,即1T
	AUXR &= 0xFE;	//串口1选择定时器1为波特率发生器
	TMOD &= 0x0F;	//设定定时器1为16位自动重装方式
	TL1 = 0xE8;		//设定定时初值
	TH1 = 0xFF;		//设定定时初值
	ET1 = 0;		//禁止定时器1中断
	TR1 = 1;		//启动定时器1
	*/

	SCON = 0x50;
    TMOD = 0x00;
    TL1 = BRT;
    TH1 = BRT >> 8;
    TR1 = 1;
    AUXR |= 0x40;
}


void UartIsr() interrupt 4
{
    if (TI)
    {
        TI = 0;
        busy_1 = 0;
    }
    if (RI)
    {
        RI = 0;
    }
}


void Uart1Send(char dat)
{
    while (busy_1);
    busy_1 = 1;
    SBUF = dat;
}

void Uart1SendStr(char *p)
{
    while (*p)
    {
        Uart1Send(*p++);
    }
}


#if 1
void Uart2Send(char dat)
{
    while (busy);
    busy = 1;
    S2BUF = dat;
}

void Uart2SendStr(char *p)
{
    while (*p)
    {
        Uart2Send(*p++);
    }
}
#endif

void main()
{
	/*PT0H, PT0 定时器0中断优先级设置为0*/
	PT0 = 0;
	IPH &= ((~PT0H) & 0xff);

	/*PS2H,PS2串口2终端优先级设置为3*/
	IP2 |= PS2;
	IP2H |= PS2H;
	
	Uart2Init();
	IE2 = 0x01;
	EA = 1;

	Uart2SendStr("Uart2 start\r\n");

	/*watchdog init*/
	WDT_CONTR=0x25;    /*使能看门狗，溢出时间约为1秒*/

	UartInit();
	ES=1;

	//TMOD = 0x0; /*自动重载模式,默认值是0，可以不赋值*/
	TR0 = 0;
	ET0 = 0;	
	AUXR |= 0x80;

	Uart1SendStr("uart1_start\r\n");
	Uart1SendStr("Ver:D_");
	Uart1SendStr(DataStr);
	Uart1SendStr(" T_");
	Uart1SendStr(TimeStr);
	Uart1SendStr("\r\n");
	
	while(1)
	{
		WDT_CONTR |= 0x10;  /*清看门狗*/ 

		if(STATUS_flag==1)
		{
			if(status==STATUS_PW_OFF)
			{
				Uart1SendStr("STS:");
				Uart1SendStr(staus_PWOFF);
				STATUS_flag=0;
			}
			
			if(status==STATUS_PW_ON)
			{
				Uart1SendStr("STS:");
				Uart1SendStr(staus_PWON);
				STATUS_flag=0;
			}
			
			if(status==STATUS_ERROR)
			{
				Uart1SendStr("STS:");	
				Uart1SendStr(staus_error);
				STATUS_flag=0;
			}

			if(status==STATUS_TIME_OUT)
			{
				Uart1SendStr("STS:");
				Uart1SendStr(staus_Time_out);
				STATUS_flag=0;
			}			
		}
		
		
		if (rptr != wptr)
		{
			//Uart1SendStr("Uart1_rcv:");
			//Uart1Send(rptr);
			//Uart1Send("uart2_rcv:");
			//Uart1Send(shuzhi_code[buffer[rptr++]]);
			//Uart1SendStr("uart2_rcv(HEX):");
			Uart1Send(buffer[rptr++]);
			//Uart1SendStr("END\r\n");
			if(rptr == BUF_SIZE || rptr == START_LEN)
			{
				rptr = 0;
			}
		}

		/*超时停止计时器*/
		if(time_out == 1)
		{
			wptr = 0;
			TR0 = 0;
			time_out = 0;
			ET0 = 0; /*关闭定时器中断*/

			status=STATUS_TIME_OUT;
			STATUS_flag=1;			
		}
		//Uart1Send(num);		
		recieve_data_check();
	}
}
