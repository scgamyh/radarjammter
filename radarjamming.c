#include <stdio.h>
#include<ad9361.h>
#include<iio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#define KHZ(x) ((long long)(x*1000.0 + .5))
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))
#include<sys/select.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

#include <pthread.h>


//串口相关的头文件  
#include<stdio.h>      /*标准输入输出定义*/  
#include<stdlib.h>     /*标准函数库定义*/  
#include<unistd.h>     /*Unix 标准函数定义*/  
#include<sys/types.h>   
#include<sys/stat.h>     
#include<fcntl.h>      /*文件控制定义*/  
#include<termios.h>    /*PPSIX 终端控制定义*/  
#include<errno.h>      /*错误号定义*/  
#include<string.h>  

#include <ctype.h>

#include <pthread.h>

// powerctrl
#define GPIOpowerctrl_baseddr                         0x41200000
static volatile uint32_t *gpio;
#define MAXDATASIZE 65536
#define port_in 24576
#define port_out 24575
int currentswitchstate = 0;
#define IIO_ENSURE(expr) { \
	if (!(expr)) { \
		(void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
		(void) abort(); \
	} \
}

struct stream_cfg {
    long long bw_hz; // Analog banwidth in Hz
    long long fs_hz; // Baseband sample rate in Hz
    long long lo_hz; // Local oscillator frequency in Hz
    const char* rfport; // Port name
    double gain_db; // Hardware gain
	bool ispoweroff;
};
enum iodev { RX, TX };
struct stream_cfg txcfg;

struct iio_context *ctx = NULL;
struct iio_device *tx = NULL;
struct iio_device *phydev = NULL;
struct iio_channel *tx0_i = NULL;
struct iio_channel *tx0_q = NULL;
struct iio_buffer *tx_buffer = NULL;

struct iio_scan_context *scan_ctx;
struct iio_context_info **info;
FILE *fp = NULL;

bool stop;
const char *uri = NULL;
const char *ip = NULL;
void * func_closefan(void * arg);
/* cleanup and exit */
static void shuTdown()
{
	printf("* Destroying buffers\n");
	
	if (tx_buffer) { iio_buffer_destroy(tx_buffer); }

	printf("* Disabling streaming channels\n");
	if (tx0_i) { iio_channel_disable(tx0_i); }
	if (tx0_q) { iio_channel_disable(tx0_q); }

	printf("* Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(0);
}

/* applies streaming configuration through IIO */



//宏定义  
#define FALSE  -1  
#define TRUE   0  

/******************************************************************* 
* 名称：                UART0_Open
* 功能：                打开串口并返回串口设备文件描述 
* 入口参数: fd:文件描述符    port :串口号(ttyPS0,ttyPS1)
* 出口参数：             正确返回为1，错误返回为0
*******************************************************************/  
int UART0_Open(int fd,char* port)  
{  

    fd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);  //fd = -1

    if (FALSE == fd)
    {
        perror("Can't Open Serial Port");
        return(FALSE);
    }
    //恢复串口为阻塞状态
    if(fcntl(fd, F_SETFL, 0) < 0)
    {
        printf("fcntl failed!\n");
        return(FALSE);
    }
    else
    {
        printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));
    }
    //测试是否为终端设备
    if(0 == isatty(STDIN_FILENO))
    {
        printf("standard input is not a terminal device\n");
        return(FALSE);
    }
    else
    {
        printf("isatty success!\n");
    }
    printf("fd->open=%d\n",fd);
    return fd;
}  
/******************************************************************* 
* 名称：                UART0_Close 
* 功能：                关闭串口并返回串口设备文件描述 
* 入口参数： fd:文件描述符    port :串口号(ttyPS0,ttyPS1)
* 出口参数：             void
*******************************************************************/  

void UART0_Close(int fd)  
{  
    close(fd);
}  

/******************************************************************* 
* 名称：                UART0_Set 
* 功能：                设置串口数据位，停止位和效验位 
* 入口参数： fd          串口文件描述符
*          speed       串口速度
*          flow_ctrl   数据流控制
*          databits    数据位   取值为 7 或者8
*          stopbits    停止位   取值为 1 或者2
*          parity      效验类型 取值为N,E,O,,S
*出口参数：              正确返回为1，错误返回为0
*******************************************************************/  
int UART0_Set(int fd,int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  

    int   i;
    int   status;
    int   speed_arr[] = { B115200, B19200, B9600, B4800, B2400, B1200, B300};
    int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};

    struct termios options;

    /*tcgetattr(fd,&options)得到与fd指向对象的相关参数，并将它们保存于options,该函数还可以测试配置是否正确，
     *该串口是否可用等。若调用成功，函数返回值为0，若调用失败，函数返回值为1. */
    if  ( tcgetattr( fd,&options)  !=  0)  //erro: tcgetattr( fd,&options)=1
    {
        perror("SetupSerial 1");
        return(FALSE);
    }
    //设置串口输入波特率和输出波特率
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
    {
        if  (speed == name_arr[i])
        {
            cfsetispeed(&options, speed_arr[i]);
            cfsetospeed(&options, speed_arr[i]);
        }
    }
    //修改控制模式，保证程序不会占用串口
    options.c_cflag |= CLOCAL;
    //修改控制模式，使得能够从串口中读取输入数据
    options.c_cflag |= CREAD;
    //设置数据流控制
    switch(flow_ctrl)
    {
    case 0 ://不使用流控制
        options.c_cflag &= ~CRTSCTS;
        break;
    case 1 ://使用硬件流控制
        options.c_cflag |= CRTSCTS;
        break;
    case 2 ://使用软件流控制
        options.c_cflag |= IXON | IXOFF | IXANY;
        break;
    }
    //设置数据位  屏蔽其他标志位
    options.c_cflag &= ~CSIZE;
    switch (databits)
    {
    case 5    :
        options.c_cflag |= CS5;
        break;
    case 6    :
        options.c_cflag |= CS6;
        break;
    case 7    :
        options.c_cflag |= CS7;
        break;
    case 8:
        options.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr,"Unsupported data size\n");
        return (FALSE);
    }
    //设置校验位
    switch (parity)
    {
    case 'n':
    case 'N': //无奇偶校验位。
        options.c_cflag &= ~PARENB;
        options.c_iflag &= ~INPCK;
        break;
    case 'o':
    case 'O'://设置为奇校验
        options.c_cflag |= (PARODD | PARENB);
        options.c_iflag |= INPCK;
        break;
    case 'e':
    case 'E'://设置为偶校验
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_iflag |= INPCK;
        break;
    case 's':
    case 'S': //设置为空格
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        break;
    default:
        fprintf(stderr,"Unsupported parity\n");
        return (FALSE);
    }
    switch (stopbits)  // 设置停止位
    {
    case 1:
        options.c_cflag &= ~CSTOPB; break;
    case 2:
        options.c_cflag |= CSTOPB; break;
    default:
        fprintf(stderr,"Unsupported stop bits\n");
        return (FALSE);
    }
    //修改输出模式，原始数据输出
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);//我加的
    //options.c_lflag &= ~(ISIG | ICANON);

    //设置等待时间和最小接收字符
    options.c_cc[VTIME] = 1; /* 读取一个字符等待1*(1/10)s */
    options.c_cc[VMIN] = 1; /* 读取字符的最少个数为1 */

    //如果发生数据溢出，接收数据，但是不再读取 刷新收到的数据但是不读
    tcflush(fd,TCIFLUSH);

    //激活配置 (将修改后的termios数据设置到串口中）
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("com set error!\n");
        return (FALSE);
    }
    return (TRUE);
}  
/******************************************************************* 
* 名称：                UART0_Init() 
* 功能：                串口初始化 
* 入口参数：  fd:        文件描述符
*           speed:     串口速度
*           flow_ctrl  数据流控制
*           databits   数据位   取值为 7 或者8
*           stopbits   停止位   取值为 1 或者2
*           parity     效验类型 取值为N,E,O,,S
*                       
* 出口参数：  正确返回为1，错误返回为0
*******************************************************************/  
int UART0_Init(int fd, int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
    int err;

    if (UART0_Set(fd,9600,0,8,1,'N') == FALSE) //设置串口数据帧格式
        return FALSE;
    else
        return  TRUE;
}  

/******************************************************************* 
* 名称：                UART0_Recv
* 功能：                接收串口数据 
* 入口参数：   fd:       文件描述符
*            rcv_buf:  接收串口中数据存入rcv_buf缓冲区中
*            data_len: 一帧数据的长度
* 出口参数：   正确返回为1，错误返回为0
*******************************************************************/  
int UART0_Recv(int fd, char *rcv_buf,int data_len)  
{  
    int len,fs_sel;
    fd_set fs_read;

    struct timeval time;

    FD_ZERO(&fs_read);
    FD_SET(fd,&fs_read);

    time.tv_sec = 10;
    time.tv_usec = 0;

    //使用select实现串口的多路通信
    fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);
   if(fs_sel)
   {
        len = read(fd,rcv_buf,data_len);
        return len;
   }
   else
   {
       printf("Sorry,I am wrong!");
       return FALSE;
   }
}

/*******************************************************************
* 名称：                UART0_Recv
* 功能：                接收串口数据
* 入口参数：   fd:       文件描述符
*            send_buf: 存放串口发送数据
*            data_len: 一帧数据的长度
* 出口参数：   正确返回为1，错误返回为0
*******************************************************************/
int UART0_Send(int fd, char *send_buf,int data_len)  
{  
    int len = 0;

    len = write(fd,send_buf,data_len);
    if (len == data_len )
    {
        return len;
    }
    else
    {

        tcflush(fd,TCOFLUSH);
        return FALSE;
    }

} 













 int current_tx_length = 100000;
int main(int argc, char *argv[])
{
	printf("start init resource.\r\n");
//pthread ctrl fan
 
   int fd;
printf("start gpio mmap\r\n");
   if ((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
       printf("Unable to open /dev/mem: %s\n", strerror(errno));
       return -1;
   }
printf("success gpio mmap\r\n");
   gpio = (uint32_t *) mmap(0, getpagesize() * 250, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIOpowerctrl_baseddr);
   if ((void *) gpio == MAP_FAILED) {
       printf("mmap failed: %s\n", strerror(errno));
       exit(1);
   }

 unsigned int bandwidth_khz_st = 40*1000;
  unsigned int bandwidth_khz = bandwidth_khz_st*14/10;
  
    printf("init gpio success...\n");
  
char linebuf[100] = {0}; // 行数据缓存
char *currentmod  =" bpsk";
  int line_num = 0;
  /**** ofdm data
****/

  short *ptx_buffer = (short*)malloc(current_tx_length*2*2);
    memset(ptx_buffer,0,current_tx_length*2*2);    
    //  ad9361_set_bb_rate(phydev,1111); 
 
   printf("init buffer finish\r\n");

printf("init white noisy data\r\n");
  char *whitenoisyfilename = "test.iq";
 int fdi = open(whitenoisyfilename,  O_RDONLY);
     printf("read white noisy data finish:%d\r\n",fdi);
     int bsize = 4096;
    int16_t *whitenoisyiqbuf = (int16_t*)malloc(2*bsize*sizeof(int16_t));
    read(fdi, whitenoisyiqbuf, 2*bsize*sizeof(int16_t));
    close(fdi);
printf("init ofdm data\r\n");
   char *OFDMfilename = "OFDM.bin";
 int fdiOFDM = open(OFDMfilename,  O_RDONLY);
     printf("read ofdm data finish:%d\r\n",fdi);
    bsize = 4096;
    int16_t *OFDMiqbuf = (int16_t*)malloc(2*bsize*sizeof(int16_t));
    read(fdiOFDM, OFDMiqbuf, 2*bsize*sizeof(int16_t));
    close(fdiOFDM);

    printf("init BPSK data\r\n");
   char *BPSKfilename = "BPSK.bin";
 int fdiBPSK = open(BPSKfilename,  O_RDONLY);
     printf("read BPSK data finish:%d\r\n",fdiBPSK);
    bsize = 4096;
    int16_t *BPSKiqbuf = (int16_t*)malloc(2*bsize*sizeof(int16_t));
    read(fdiBPSK, BPSKiqbuf, 2*bsize*sizeof(int16_t));
    close(fdiBPSK);

printf("init QPSK data\r\n");
   char *QPSKfilename = "QPSK.bin";
 int fdiQPSK = open(QPSKfilename,  O_RDONLY);
     printf("read QPSK data finish:%d\r\n",fdiQPSK);
    bsize = 4096;
    int16_t *QPSKiqbuf = (int16_t*)malloc(2*bsize*sizeof(int16_t));
    read(fdiQPSK, QPSKiqbuf, 2*bsize*sizeof(int16_t));
    close(fdiQPSK);

  if(bandwidth_khz >56000){
        bandwidth_khz = 56000;
    }



   //  printf("read ofdmdataq finish:%d\r\n",length);
    // txcfg.bw_hz = KHZ(bandwidth_khz);
     txcfg.bw_hz = MHZ(56); // 3.0 MHz RF bandwidth
    txcfg.fs_hz = MHZ((61.44));   // 2.5 MS/s tx sample rate
    txcfg.lo_hz = GHZ(2.4); // 1.57542 GHz RF frequency
    txcfg.rfport = "A";
    txcfg.gain_db = -50.0;
    txcfg.ispoweroff = true;
   printf("* Acquiring IIO context\r\n");
    ctx = iio_create_default_context();
    scan_ctx = iio_create_scan_context(NULL, 0);
    if (scan_ctx) {
        int info_count = iio_scan_context_get_info_list(scan_ctx, &info);
        if(info_count > 0) {
          printf("* Found :%s\r\n",iio_context_info_get_description(info[0]));
            iio_context_info_list_free(info);
        }
        iio_scan_context_destroy(scan_ctx);
    }

    printf("* Acquiring devices\r\n");
    int device_count = iio_context_get_devices_count(ctx);
    if (!device_count) {
       printf("No supported SDR devices found.\n");
        return -1;
    }
  printf("* Context has %d device(s).\r\n",device_count);

   printf("* Acquiring TX device\n");
    tx = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    if (tx == NULL) {
       printf("Error opening  TX device: \n");
        return -1;
    }

    iio_device_set_kernel_buffers_count(tx, 8);

    phydev = iio_context_find_device(ctx, "ad9361-phy");
    struct iio_channel* phy_chn = iio_device_find_channel(phydev, "voltage0", true);
    iio_channel_attr_write(phy_chn, "rf_port_select", txcfg.rfport);
    iio_channel_attr_write_longlong(phy_chn, "rf_bandwidth", txcfg.bw_hz);
    iio_channel_attr_write_longlong(phy_chn, "sampling_frequency", txcfg.fs_hz);
    iio_channel_attr_write_double(phy_chn, "hardwaregain", txcfg.gain_db);
    iio_channel_attr_write_bool(
                iio_device_find_channel(phydev, "altvoltage0", true)
              , "powerdown", false); // Turn OFF RX LO
    iio_channel_attr_write_longlong(
                iio_device_find_channel(phydev, "altvoltage1", true)
                , "frequency", txcfg.lo_hz); // Set TX LO frequency

   printf ("* Initializing streaming channels\n");
    tx0_i = iio_device_find_channel(tx, "voltage0", true);
    if (!tx0_i)
        tx0_i = iio_device_find_channel(tx, "altvoltage0", true);

    tx0_q = iio_device_find_channel(tx, "voltage1", true);
    if (!tx0_q)
        tx0_q = iio_device_find_channel(tx, "altvoltage1", true);

    printf("* Enabling IIO streaming channels\n");

    printf("success create TX buffer\n");
    iio_channel_enable(tx0_i);
    iio_channel_enable(tx0_q);
   
    iio_channel_attr_write_longlong(
                    iio_device_find_channel(phydev, "altvoltage1", true)
                    , "frequency", txcfg.lo_hz); // Set TX LO frequency
//  tx_buffer = iio_device_create_buffer(tx, bsize, true);
//     ptx_buffer = (short *)iio_buffer_start(tx_buffer);
//     for(int i=0;i<bsize;i++){
//             ptx_buffer[i * 2] =*(iqbuf+2*i);//fill I data
//             ptx_buffer[i * 2+1]=*(iqbuf+2*i+1);//*(ofdmq_tmp+i) ;//fill q data
//         }
//         int   ntx = iio_buffer_push(tx_buffer);

       if(tx_buffer != NULL){
                iio_buffer_cancel(tx_buffer);
                iio_buffer_destroy(tx_buffer);
            }
             tx_buffer = iio_device_create_buffer(tx, bsize, true);
            ptx_buffer = (short *)iio_buffer_start(tx_buffer);
    // int32_t server_sockfd;
    // ssize_t len;
  
    // struct sockaddr_in my_addr;   //服务器网络地址信息结构体
    // struct sockaddr_in remote_addr; //客户端网络地址结构体
    // uint32_t sin_size;
    // uint8_t buf[MAXDATASIZE];  //数据传送的缓冲区

    // memset(&my_addr, 0,sizeof(my_addr)); //数据初始化--清零
    // my_addr.sin_family = AF_INET; //设置为IP通信
    // my_addr.sin_addr.s_addr = INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上
    // my_addr.sin_port = htons(port_in); //服务器端口号

    // /*创建服务器端套接字--IPv4协议，面向无连接通信，UDP协议*/
    // if((server_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    // {
    //     perror("socket error");
    //     return;
    // }

    // /*将套接字绑定到服务器的网络地址上*/
    // if (bind(server_sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0)
    // {
    //     perror("bind error");
    //     return;
    // }
    // sin_size = sizeof(struct sockaddr_in);
   iio_channel_attr_write_bool(
                 iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
                    , "powerdown", txcfg.ispoweroff); // Turn OFF TX LO
    printf(" waiting for a packet...\n");
   
    
    int powerctt = 0;
    
    int mainpowerctt = 0;
    
    *gpio  = 0x00;
    /*接收客户端的数据并将其发送给客户端--recvfrom是无连接的*/
   // while (((len = recvfrom(server_sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&remote_addr, &sin_size)) > 0))

	int err;                           //返回调用函数的状态 
	int len, serialfd = 0;
	 

	serialfd = UART0_Open(serialfd,"/dev/ttyPS1");
	do{  
		err = UART0_Init(serialfd,9600,0,8,1,'N');  
		printf("Set Port Exactly! fd=%d err=%d\n",serialfd,err);  
    	}while(FALSE == err || FALSE == serialfd);
    printf("init serial0 success !!! \r\n");
    while(1)
    {
        char buf[1024] ={0};
        len = UART0_Recv(serialfd, buf,50);
        //UART0_Send(serialfd,"device jammter is ok!!!\r\n",20);
         usleep(10000);      //100ms
        if(len > 10)
        {
        

       
     // printf(" received packet from:%s\n",inet_ntoa(remote_addr.sin_addr));
          // printf("%c\r\n",buf[0]);
            // sendto(server_sockfd,buf,strlen(buf),0,(struct sockaddr*)&remote_addr,sizeof(remote_addr));
      if(buf[0] =='$')
	{
  	   
               char header[10] ;
             
               double gaindb;
               int start;
               
               int bandwidth;
             
               
               sscanf(buf,"$,%d,%d,%d,%lf",&start,&bandwidth,&powerctt,&gaindb);
	          printf("gaindb:%lf,start:%d,powerctt:%d,bandwidth:%d\r\n",gaindb,start,powerctt,bandwidth);
	if(strstr(buf,"ON"))//signal source ON
	{

		if(powerctt)
		{
                *gpio = 0x00;
		mainpowerctt = powerctt;
		}
		// parse frequence
              	int frequence = start;
		// parse bandwidth
		if(txcfg.bw_hz!= MHZ(bandwidth))
		{
		iio_channel_attr_write_bool(
                iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
                 , "powerdown", true); // Turn OFF TX LO
		
		txcfg.bw_hz = MHZ(bandwidth);
		iio_channel_attr_write_longlong(phy_chn, "rf_bandwidth",txcfg.bw_hz);
		
		//iio_channel_attr_write_longlong(phy_chn, "sampling_frequency",txcfg.fs_hz);
		}
		
 		if(txcfg.gain_db != gaindb)
    		{
			iio_channel_attr_write_double(phy_chn, "hardwaregain", gaindb);
			usleep(100000);
			txcfg.gain_db = gaindb;
		}
		if(txcfg.lo_hz != MHZ(frequence))
  		{
		 iio_channel_attr_write_bool(
                 iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
                    , "powerdown", true); // Turn OFF TX LO
		usleep(100000);
		iio_channel_attr_write_longlong(
                iio_device_find_channel(phydev, "altvoltage1", true)
                , "frequency", MHZ(frequence)); // Set TX LO frequency	
		txcfg.lo_hz = MHZ(frequence);
		usleep(100000);
		} 
    //load  moddata
    	if(strstr(buf,"OFDM"))
      {
             for(int i=0;i<bsize;i++){
                ptx_buffer[i * 2] =  OFDMiqbuf[2*i];//fill I data
                ptx_buffer[i * 2+1]= OFDMiqbuf[2*i+1];//*(ofdmq_tmp+i) ;//fill q data
            }
            int   ntx = iio_buffer_push(tx_buffer);
            currentmod ="OFDM";
      }
      else if(strstr(buf,"BPSK"))
      {
            for(int i=0;i<bsize;i++){
                ptx_buffer[i * 2] =  BPSKiqbuf[2*i];//fill I data
                ptx_buffer[i * 2+1]= BPSKiqbuf[2*i+1];//*(ofdmq_tmp+i) ;//fill q data
            }
            int   ntx = iio_buffer_push(tx_buffer);
            currentmod ="BPSK";
      }
      else if(strstr(buf,"QPSK"))
      {
            for(int i=0;i<bsize;i++){
                ptx_buffer[i * 2] =  QPSKiqbuf[2*i];//fill I data
                ptx_buffer[i * 2+1]= QPSKiqbuf[2*i+1];//*(ofdmq_tmp+i) ;//fill q data
            }
            int   ntx = iio_buffer_push(tx_buffer);
            currentmod ="QPSK";
      }
      else if(strstr(buf,"Whitenoisy"))
      {

            for(int i=0;i<bsize;i++){
                ptx_buffer[i * 2] =*(whitenoisyiqbuf+2*i);//fill I data
                ptx_buffer[i * 2+1]=*(whitenoisyiqbuf+2*i+1);//*(ofdmq_tmp+i) ;//fill q data
            }
            int   ntx = iio_buffer_push(tx_buffer);
            currentmod ="Whitenoisy";
      }
                    iio_channel_attr_write_bool(
                 iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
                    , "powerdown", false); // Turn OFF TX LO 
		usleep(500000);
		if(powerctt)
		{
                *gpio = 0xFF;
		mainpowerctt = powerctt;
		}
		printf("open power signal source ...............\r\n");	

		}else if(strstr(buf,"OFF"))//signal source OFF
		{
			   printf("close device\r\n");
               		   currentswitchstate = 0;
		      
			      iio_channel_attr_write_bool(
                    iio_device_find_channel(iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
                    , "powerdown", true); // Turn OFF TX LO
			   usleep(100000);
 			   printf("close power\r\n");
		if(!powerctt)
		{
               		*gpio = 0x00;
			mainpowerctt = powerctt;
		}
			
			usleep(100000);
		
	}//CK
            else if(strstr(buf,"CHECK"))
            {
		printf("check device00\r\n");	
                char data[MAXDATASIZE] ;
                long long getbandwidth = 0;
 		long long getlo = 0;
		bool power;
		iio_channel_attr_read_longlong(phy_chn, "rf_bandwidth", &getbandwidth);
 		 iio_channel_attr_read_longlong(
                iio_device_find_channel(phydev, "altvoltage1", true)
                , "frequency", &getlo);
		double getgainvalue=-1.00;
		printf("check device1\r\n");	
		iio_channel_attr_read_double(phy_chn, "hardwaregain", &getgainvalue);
		printf("check device2\r\n");	
		iio_channel_attr_read_bool(
                	  iio_device_find_channel( iio_context_find_device(ctx, "ad9361-phy"), "altvoltage1", true)
               			 , "powerdown", &power); // Turn OFF RX LO
		printf("check device3\r\n");	
		int lo = getlo/1000000;
		int bandw = getbandwidth/1000000;
		if(!power)
		{
		sprintf(data,"#,%d,%d,%d,%lf,ON,%s,#END",lo,bandw,mainpowerctt,getgainvalue,currentmod );

               // sendto(server_sockfd,data,strlen(data),0,(struct sockaddr*)&remote_addr,sizeof(remote_addr));
                UART0_Send(serialfd,data,strlen(data));
		}else
		{
		sprintf(data,"#,%d,%d,%d,%lf,OFF,%s,#END",lo,bandw,mainpowerctt,getgainvalue,currentmod);

                 UART0_Send(serialfd,data,strlen(data));
		}
			printf("check device  success!\r\n");	
            }

    }

		
             
	      
	}   
    /*关闭套接字*/
    
    }
       munmap(gpio, getpagesize() * 250);
        close(fd);
        close(serialfd);
       sleep(1);
     

	return 0;
}

void * func_closefan(void * arg)
{
        printf("func_close fan run...\n");
	sleep(5*60);
	//*gpio = *gpio&0x00;	
        return NULL;
}


