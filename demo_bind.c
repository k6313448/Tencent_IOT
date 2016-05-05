/**
 * 这段代码的目的是用来演示：如何初始化SDK和设备登陆
 *
 * 主要调用了tx_init_device函数来初始化设备信息以及相关回调，这些信息用于服务器在设备登陆时做验证使用。
 * 在SDK初始化成功后，您应该使用我们最新版的手Q，确保手Q和设备连接上了同一个路由器，并且能够访问互联网，
 * 然后在手Q好友面板”我的设备“中点击”搜索新设备“，将扫描到待绑定的设备，绑定成功后将触发设备的登陆逻辑，
 * 这个登陆过程将在SDK内部完成后通过设置的回调函数向上层通知。
 *
 * 期望的结果：
 * 下面的两个函数 on_online_status，on_login_complete 回调的结果是：errcode为0，new 为 11
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/shm.h>

#include "TXSDKCommonDef.h"
#include "TXDeviceSDK.h"
#include "TXMsg.h"

#define UNIX_DOMAIN "/tmp/UNIX_QQBTSF.domain"  
#define UNIX_DOMAIN_WIFI "/tmp/UNIX_QQWIFISF.domain"  

int g_interval=1;
char g_rbuf[1024];
//char g_rbuf_wifi[1024];
/**
 * 登录完成的通知，errcode为0表示登录成功，其余请参考全局的错误码表
 */
void on_login_complete(int errcode) {
    printf("on_login_complete | code[%d]\n", errcode);
}

/**
 * 在线状态变化通知， 状态（status）取值为 11 表示 在线， 取值为 21 表示  离线
 * old是前一个状态，new是变化后的状态（当前）
 */
void on_online_status(int old, int new) {
	printf("online status: %s\n", 11 == new ? "true" : "false"); 
}

/**
 * 辅助函数: 从文件读取buffer
 * 这里用于读取 license 和 guid
 * 这样做的好处是不用频繁修改代码就可以更新license和guid
 */
bool readBufferFromFile(char *pPath, unsigned char *pBuffer, int nInSize, int *pSizeUsed) {
	if (!pPath || !pBuffer) {
		return false;
	}

	int uLen = 0;
	FILE * file = fopen(pPath, "rb");
	if (!file) {
	    return false;
	}

	fseek(file, 0L, SEEK_END);
	uLen = ftell(file);
	fseek(file, 0L, SEEK_SET);

	if (0 == uLen || nInSize < uLen) {
		printf("invalide file or buffer size is too small...\n");
        fclose(file);
		return false;
	}

	*pSizeUsed = fread(pBuffer, 1, uLen, file);
	// bugfix: 0x0a is a lineend char, no use.
	if (pBuffer[uLen-1] == 0x0a)
	{
		*pSizeUsed = uLen - 1;
		pBuffer[uLen-1] = '\0';
	}

	printf("len:%d, ulen:%d\n",uLen, *pSizeUsed);
	fclose(file);
	return true;
}

/**
 * 辅助函数：SDK的log输出回调
 * SDK内部调用改log输出函数，有助于开发者调试程序
 */
void log_func(int level, const char* module, int line, const char* message)
{
	printf("%s\n", message);
}

void send_dp_callback(unsigned int cookie, unsigned long long from_client, int err_code)
{
	printf(" cookie[%u] from_client[%llu] err_code[%d] \n",cookie,from_client, err_code);
}

/////////////////////////////////
void on_receive_data_point_internal(unsigned long long from_client, tx_data_point * data_points, int data_points_count)
{
	int i;
	printf("on_receive_data_point_internal: from_client=%lld, data_points_count=%d\n", from_client, data_points_count);

	if(data_points == NULL) return;
	for(i=0; i<data_points_count; ++i){
		if(&data_points[i] == NULL) continue;

		if(    data_points[i].id == 90001 
			|| data_points[i].id == 90002
			|| data_points[i].id == 90004
		)
		{
			printf("public datapoint\n");	    
		}
		else{
			printf("user datapoint:%d\n",data_points[i].id);	    

			char sendMsg[20]={0};
		
			//test
			if(data_points[i].id == 10000)
			{
				unsigned int cookie = 0;
				tx_data_point dp_to_send = {0};
	
				dp_to_send.id = data_points[i].id;
				dp_to_send.seq = data_points[i].seq;
				dp_to_send.ret_code = 0;

				dp_to_send.value = calloc(1,64);
				strcpy(dp_to_send.value,"{\"ret\":0,\"msg\":\"172.16.0.1\"}");

				tx_ack_data_points(from_client,&dp_to_send,1,&cookie, send_dp_callback);
				if(dp_to_send.value) free(dp_to_send.value);
			}
			//query ip
            else if(data_points[i].id == 100002868)
            {
				printf("IP Address Query.\n");
                unsigned int cookie = 0;
                tx_data_point dp_to_send = {0};
				char ip[20];
				if ( get_ip(ip) == 0 ){
					strcat(sendMsg,ip);
				}
				else{
					strcat(sendMsg,"error!");
				}

                dp_to_send.id = data_points[i].id;
                dp_to_send.seq = data_points[i].seq;
                dp_to_send.ret_code = 0;
                dp_to_send.value = calloc(1,64);
                strcpy(dp_to_send.value,"{\"ret\":0,\"msg\":\"");
				strcat(dp_to_send.value,sendMsg);
				strcat(dp_to_send.value,"\"}");

                tx_ack_data_points(from_client,&dp_to_send,1,&cookie, send_dp_callback);
                if(dp_to_send.value) free(dp_to_send.value);
            }
			//set interval
            else if(data_points[i].id == 100002876)
            {
				printf("Set interval.\n");
                unsigned int cookie = 0;
                tx_data_point dp_to_send = {0};
				int interval=0;

				interval=atoi(data_points[i].value);
				if(interval>=1&&interval<=20){
					g_interval=interval;
					strcpy(sendMsg,"Set interval success.");
				}
				else{
					strcpy(sendMsg,"Set interval failed.");
				}
			
				dp_to_send.id = data_points[i].id;
                dp_to_send.seq = data_points[i].seq;
                dp_to_send.ret_code = 0;

                dp_to_send.value = calloc(1,64);
                strcpy(dp_to_send.value,"{\"ret\":0,\"msg\":\"");
				strcat(dp_to_send.value,sendMsg);
				strcat(dp_to_send.value,"\"}");

                tx_ack_data_points(from_client,&dp_to_send,1,&cookie, send_dp_callback);
                if(dp_to_send.value) free(dp_to_send.value);
            }
			//set time
            else if(data_points[i].id == 100002877)
            {
				printf("Set Time.\n");
                unsigned int cookie = 0;
                tx_data_point dp_to_send = {0};
				char setTime[30]="0";
				char systemPara[50]="0";
				strcpy(setTime,data_points[i].value);
				strcpy(systemPara,"date -s ");
				strcat(systemPara,setTime);
				system(systemPara);
				printf("systemPara: %s\n",systemPara);
			
				strcpy(sendMsg,"Set time success.");
			
				dp_to_send.id = data_points[i].id;
                dp_to_send.seq = data_points[i].seq;
                dp_to_send.ret_code = 0;
                dp_to_send.value = calloc(1,64);
                strcpy(dp_to_send.value,"{\"ret\":0,\"msg\":\"");
				strcat(dp_to_send.value,sendMsg);
				strcat(dp_to_send.value,"\"}");
                tx_ack_data_points(from_client,&dp_to_send,1,&cookie, send_dp_callback);
                if(dp_to_send.value) free(dp_to_send.value);
            }   
			//query beacon
            else if(data_points[i].id == 100002883)
            {
				printf("Query Beacon\n");
				unsigned int cookie = 0;
				int value_int = 0;
                tx_data_point dp_to_send = {0};
				
				dp_to_send.id = data_points[i].id;
                dp_to_send.seq = data_points[i].seq;
                dp_to_send.ret_code = 0;
                dp_to_send.value = calloc(1,1024);
				value_int=atoi(data_points[i].value);
				if(value_int==1){
					printf("Beacon type: Bluetooth\n");
					beaconSocket();
					strcpy(dp_to_send.value,g_rbuf);
				}
				else if(value_int==2){
					printf("Beacon type: WiFi\n");
					beaconSocket_wifi();
					strcpy(dp_to_send.value,g_rbuf);
				}
				else{
					printf("Query Beacon Error.\n");
					strcpy(dp_to_send.value,"Query Beacon Error.");
				}
				printf("value:%s\n",dp_to_send.value);	
                tx_ack_data_points(from_client,&dp_to_send,1,&cookie, send_dp_callback);
				printf("Query Beacon Finished: cookie[%u]\n\n\n",cookie);
                if(dp_to_send.value) free(dp_to_send.value);
			}   
		}
	}
}
/////////////////////////////////

int tx_demo_init_data_point()
{
	tx_data_point_notify data_point_notify_internal = {0};
	data_point_notify_internal.on_receive_data_point = on_receive_data_point_internal;
	int ret = tx_init_data_point(&data_point_notify_internal);

	return ret;
}


/**
 * SDK初始化
 * 例如：
 * （1）填写设备基本信息
 * （2）打算监听哪些事件，事件监听的原理实际上就是设置各类消息的回调函数，
 * 	例如设置Datapoint消息通知回调：
 * 	开发者应该定义如下的 my_on_receive_data_point 函数，将其赋值tx_data_point_notify对象中对应的函数指针，并初始化：
 *
 * 			tx_data_point_notify msgOnRecv= {0};
 * 			msgOnRecv.on_receive_data_point = my_on_receive_data_point;
 * 			tx_init_data_point(&msgOnRecv);
 *
 * 	那么当SDK内部的一个线程收到对方发过来的DataPoint消息后（通过服务器转发），将同步调用 msgOnRecv.on_receive_data_point 
 */
bool initDevice() {
	// 读取 license
	unsigned char license[256] = {0};
	int nLicenseSize = 0;
	if (!readBufferFromFile("/etc/config/licence.txt", license, sizeof(license), &nLicenseSize)) {
		printf("[error]get license from file failed...\n");
		return false;
	}

	// 读取guid
	unsigned char guid[32] = {0};
	int nGUIDSize = 0;
	if(!readBufferFromFile("/etc/config/GUID_file.txt", guid, sizeof(guid), &nGUIDSize)) {
		printf("[error]get guid from file failed...\n");
		return false;
	}

    char svrPubkey[256] = {0};
    int nPubkeySize = 0;
    if (!readBufferFromFile("/etc/config/1700003134.pem", svrPubkey, sizeof(svrPubkey), &nPubkeySize))
    {
        printf("[error]get svrPubkey from file failed...\n");
        return NULL;
    }

    // 设备的基本信息
    tx_device_info info = {0};
    info.os_platform            = "Linux";

    info.device_name            = "demo1";
    info.device_serial_number   = guid;
    info.device_license         = license;
    info.product_version        = 1;
    info.network_type			= network_type_wifi;
    info.product_id             = 1700003134;
    info.server_pub_key         = svrPubkey;

	tx_demo_init_data_point();

	// 设备登录、在线状态、消息等相关的事件通知
	// 注意事项：
	// 如下的这些notify回调函数，都是来自硬件SDK内部的一个线程，所以在这些回调函数内部的代码一定要注意线程安全问题
	// 比如在on_login_complete操作某个全局变量时，一定要考虑是不是您自己的线程也有可能操作这个变量
    tx_device_notify notify      = {0};
    notify.on_login_complete     = on_login_complete;
    notify.on_online_status      = on_online_status;
    notify.on_binder_list_change = NULL;

    // SDK初始化目录，写入配置、Log输出等信息
    // 为了了解设备的运行状况，存在上传异常错误日志 到 服务器的必要
    // system_path：SDK会在该目录下写入保证正常运行必需的配置信息
    // system_path_capicity：是允许SDK在该目录下最多写入多少字节的数据（最小大小：10K，建议大小：100K）
    // app_path：用于保存运行中产生的log或者crash堆栈
    // app_path_capicity：同上，（最小大小：300K，建议大小：1M）
    // temp_path：可能会在该目录下写入临时文件
    // temp_path_capicity：这个参数实际没有用的，可以忽略
    tx_init_path init_path = {0};
    init_path.system_path = "./";
    init_path.system_path_capicity = 100 * 1024;
    init_path.app_path = "./";
    init_path.app_path_capicity = 1024 * 1024;
    init_path.temp_path = "./";
    init_path.temp_path_capicity = 10 * 1024;

    // 设置log输出函数，如果不想打印log，则无需设置。
    // 建议开发在开发调试阶段开启log，在产品发布的时候禁用log。
    tx_set_log_func(log_func,1,0);

    // 初始化SDK，若初始化成功，则内部会启动一个线程去执行相关逻辑，该线程会持续运行，直到收到 exit 调用
	int ret = tx_init_device(&info, &notify, &init_path);
	if (err_null == ret) {
		printf(" >>> tx_init_device success\n");
	}
	else {
		printf(" >>> tx_init_device failed [%d]\n", ret);
		return false;
	}

	return true;
}


void reset(unsigned int cookie, int err_code)
{
    printf("on_send_text_msg_ret, cookie[%u] ret[%d]\n", cookie, err_code);
}

void test_send_pic_alarm(char* input)
{
//	tx_send_text_msg(1,"Hello!",&reset,0,0,0);
	char ip[20];
	if ( get_ip(ip) == 0 )
	{
//		tx_send_text_msg(1,ip,&reset,0,0,0);
		char str_1[50];	
		strcpy(str_1,"{\"ret\":0,\"msg\":\"");
		strcat(str_1,ip);
		strcat(str_1,"\"}");
		tx_send_text_msg(1,str_1,&reset,0,0,0);
	}
	else
	{
		tx_send_text_msg(1,"error!",&reset,0,0,0);
	}

//	tx_send_text_msg(1,input,&reset,0,0,0);
}

void on_report_data_point_callback_internal(unsigned int cookie, int err_code)
{
	printf("on_report_data_point_callback_internal cookie[%u] err_code[%d]\n",cookie, err_code);
}

//Send Beacon to Server using datapoint
void sendBeaconReport()
{
	//printf("Sending Beacon Reports\n");
	tx_data_point bigDP;
	bigDP.id				= 100002878;
	bigDP.ret_code     = 0;
	bigDP.seq             = 10000;
				
	bigDP.value = calloc(1,1024);
	beaconSocket();
	strcpy(bigDP.value,g_rbuf);
	//printf("bigDP.value:%s\n",bigDP.value);	
	unsigned int cookie = 0;
	tx_report_data_points(&bigDP,1,&cookie,on_report_data_point_callback_internal);
	//printf("Report Finished: cookie[%u]\n\n\n",cookie);
	if(bigDP.value) free(bigDP.value);
}
void sendBeaconReport_wifi()
{
	//printf("Sending WiFi Beacon Reports\n");
	tx_data_point bigDP;
	bigDP.id				= 100002904;
	bigDP.ret_code     = 0;
	bigDP.seq             = 10000;
				
	bigDP.value = calloc(1,1024);
	beaconSocket_wifi();
	strcpy(bigDP.value,g_rbuf);
	//printf("bigDP.value:%s\n",bigDP.value);	
	unsigned int cookie = 0;
	tx_report_data_points(&bigDP,1,&cookie,on_report_data_point_callback_internal);
	//printf("Report Finished: cookie[%u]\n\n\n",cookie);
	if(bigDP.value) free(bigDP.value);
}
int beaconSocket(void)
{
    int connect_fd;
    int ret,len;
    int i;
    static struct sockaddr_un srv_addr;

        connect_fd=socket(PF_UNIX,SOCK_STREAM,0);
        if(connect_fd<0)
        {
            perror("cannot create communication socket");
            return 1;
        }
        srv_addr.sun_family=AF_UNIX;
        strcpy(srv_addr.sun_path,UNIX_DOMAIN);
        ret=connect(connect_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
        if(ret==-1)
        {
            perror("cannot connect to the server");
            close(connect_fd);
            return 1;
        }
        memset(g_rbuf,0,sizeof(g_rbuf));
        len = recv(connect_fd, g_rbuf, sizeof(g_rbuf),0);
        //if(len < 0)
        //    continue;
        //printf("%s\n", g_rbuf);
        close(connect_fd);
    return 0;
}
int beaconSocket_wifi(void)
{
    int connect_fd;
    int ret,len;
    int i;
    static struct sockaddr_un srv_addr;

        connect_fd=socket(PF_UNIX,SOCK_STREAM,0);
        if(connect_fd<0)
        {
            perror("cannot create communication socket");
            return 1;
        }
        srv_addr.sun_family=AF_UNIX;
        strcpy(srv_addr.sun_path,UNIX_DOMAIN_WIFI);
        ret=connect(connect_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
        if(ret==-1)
        {
            perror("cannot connect to the server");
            close(connect_fd);
            return 1;
        }
        memset(g_rbuf,0,sizeof(g_rbuf));
        len = recv(connect_fd, g_rbuf, sizeof(g_rbuf),0);
        //if(len < 0)
        //    continue;
        //printf("%s\n", g_rbuf);
        close(connect_fd);
    return 0;
}
void* thread_func_reportdata(void * arg)
{
	time_t now = time(NULL);
	time_t last_check_time_report_dp = now;

	while(1) {
		now = time(NULL);
		if((now - last_check_time_report_dp) > g_interval){             
			sendBeaconReport();
			sendBeaconReport_wifi();
			last_check_time_report_dp = now;
		}	
	}
}
/*
void* thread_func_reportdata_wifi(void * arg)
{
	time_t now = time(NULL);
	time_t last_check_time_report_dp = now;

	while(1) {
		now = time(NULL);
		if((now - last_check_time_report_dp) > g_interval){      
			sendBeaconReport_wifi();
			last_check_time_report_dp = now;
		}	
	}
}
*/
pthread_t ntid = 0;
//pthread_t ntid_wifi = 0;
bool create_report_thread()
{
	int err;
	err = pthread_create(&ntid, NULL, thread_func_reportdata, NULL);
	if(err == 0 && ntid != 0)
	{
        ntid = 0;
    }
    return true;
}
/*
bool create_report_thread_wifi()
{
	int err;
	err = pthread_create(&ntid_wifi, NULL, thread_func_reportdata_wifi, NULL);
	if(err == 0 && ntid_wifi != 0)
	{
        ntid_wifi = 0;
    }
    return true;
}*/

//get device ip
int get_ip(char* outip)
{
	int i=0;
	int sockfd;
	struct ifconf ifconf;
	char buf[512];
	struct ifreq *ifreq;
	char* ip;
	//初始化ifconf
	ifconf.ifc_len = 512;
	ifconf.ifc_buf = buf;
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0)
	{
		return -1;
	}
	ioctl(sockfd, SIOCGIFCONF, &ifconf);    //获取所有接口信息
	close(sockfd);
	//接下来一个一个的获取IP地址
	ifreq = (struct ifreq*)buf;
	for(i=(ifconf.ifc_len/sizeof(struct ifreq)); i>0; i--)
	{
		ip = inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr);
		if(strcmp(ip,"127.0.0.1")==0)  //排除127.0.0.1，继续下一个
		{
			ifreq++;
			continue;
		}
		strcpy(outip,ip);
		return 0;
	}
	return -1;
}



/****************************************************************
*  测试代码：
*
*  （1）while循环的作用仅仅是使 Demo进程不会退出，实际使用SDK时一般不需要
*
*  （2） 输入 "quit" 将会退出当前进程，这段逻辑存在的原因在于：
*     					在某些芯片上，直接用Ctrl+C 退出易产生僵尸进程
*
*  （3）while循环里面的sleep(1)在这里是必须的，因为如果Demo进程后台运行，scanf没有阻塞作用，会导致当前线程跑满CPU
*
*****************************************************************/
int main(int argc, char* argv[]) {
	if ( !initDevice() ) {
		return -1;
	}
	
	// 你可以在做其他相关的事情
	// ...
	create_report_thread();
	//create_report_thread_wifi();
	
	char input[100];
	//while (scanf("%s", input)) {
	while(1)
	{
/*		if(gets(input)){
			if ( !strcmp(input, "quit") ) {
				tx_exit_device();
				break;
		}
		test_send_pic_alarm(input);
	}
*/		
		sleep(1);
	}
	
    pthread_join(ntid,NULL);
    //pthread_join(ntid_wifi,NULL);

	return 0;
}
