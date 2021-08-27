#include <cstdio>
#include <sys/types.h>
#include <fcntl.h>
#include <cstring>
#include <random>
#include <string>
#include <netinet/in.h>
#include <ctime>
#include<pthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <jni.h>
#include <android/log.h>

#include "kcpp-master/kcpp.h"

#define TAG "native-kcp" // 这个是自定义的LOG的标识
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__) // 定义LOGD类型
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__) // 定义LOGI类型
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__) // 定义LOGW类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__) // 定义LOGE类型
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__) // 定义LOGF类型

#define SERVER_PORT 6666

// if u modify this `PRACTICAL_CONDITION`,
// u have to update this var of the server side to have the same value.
#define PRACTICAL_CONDITION 1

#define SND_BUFF_LEN 1000
#define RCV_BUFF_LEN 1500

 //#define SERVER_IP "172.96.239.56"
//#define SERVER_IP "127.0.0.1"
#define SERVER_IP "192.168.12.102"

using kcpp::KcpSession;

IUINT32 iclock()
{
	long s, u;
	IUINT64 value;

	struct timeval time;
	gettimeofday(&time, NULL);
	s = time.tv_sec;
	u = time.tv_usec;

	value = ((IUINT64)s) * 1000 + (u / 1000);
	return (IUINT32)(value & 0xfffffffful);
}

void udp_output(const void *buf, int len, int fd, struct sockaddr* dst)
{
	::sendto(fd, (const char*)buf, len, 0, dst, sizeof(*dst));
}

kcpp::UserInputData udp_input(char *buf, int len, int fd, struct sockaddr_in from)
{
	socklen_t fromAddrLen = sizeof(from);
	int recvLen = ::recvfrom(fd, buf, len, 0,
		(struct sockaddr*)&from, &fromAddrLen);
	return kcpp::UserInputData(buf, recvLen);
}

void error_pause()
{
    LOGD("press any key to quit ...\n");
	char ch; scanf("%c", &ch);
}

FILE* rf;
IUINT32 startTs = 0;

void udp_msg_sender(int fd, struct sockaddr* dst)
{
	char sndBuf[SND_BUFF_LEN];
	char rcvBuf[RCV_BUFF_LEN];

	// we can't use char array, cause we don't know how big the recv_data is
	kcpp::Buf kcppRcvBuf;

	struct sockaddr_in from;
	int len = 0;
	uint32_t initIndex = 1;
	uint32_t nextSndIndex = initIndex;
	int64_t nextKcppUpdateTs = 0;
	int64_t nextSendTs = 0; 

	KcpSession kcppClient(
		kcpp::RoleTypeE::kCli,
		std::bind(udp_output, std::placeholders::_1, std::placeholders::_2, fd, dst),
		std::bind(udp_input, rcvBuf, RCV_BUFF_LEN, fd, std::ref(from)),
		std::bind(iclock));


#if !PRACTICAL_CONDITION

	static const int64_t kSendInterval = 0;
	const uint32_t testPassIndex = 66666;
	kcppClient.SetConfig(666, 1024, 1024, 4096, 1, 1, 1, 1, 0, 5);

	while (1)
	{

#else

//	kcppClient.SetConfig(1500, 32, 128, 128, 0, 100, 2, 0, 0, 100);

//nodelay:   0 不启用，1启用nodelay模式(即使用更小的 rx_minrto, 以更快检测到丢包)
//interval： 内部flush刷新时间
//resend:    0（默认）表示关闭。可以自己设置值，若设置为2（则2次ACK跨越将会直接重传）
//nc:        即 no congest 的缩写, 是否关闭拥塞控制，0（默认）代表不关闭，1代表关闭
// 非退让流控、流模式
	kcppClient.
			SetConfig(1500, 512, 512, 4096, 0, 10, 2, 1, 0, 20);
	static const int64_t kSendInterval = 0; // 30fps
	startTs = iclock();

//	const uint32_t testPassIndex = 500;
	while (1)
	{

#endif // PRACTICAL_CONDITION

		int64_t now = static_cast<int64_t>(iclock());
		if (now >= nextKcppUpdateTs)
			nextKcppUpdateTs = kcppClient.Update();

		if (kcppClient.CheckCanSend() && now >= nextSendTs)
		{
			nextSendTs = now + kSendInterval;
			memset(sndBuf, 0, SND_BUFF_LEN);

			char buff [SND_BUFF_LEN];
			int len = fread(buff, sizeof(char), SND_BUFF_LEN , rf);
//			LOGD("fread len%d ", len);

			if(len != 0){
				memcpy(sndBuf, buff, SND_BUFF_LEN);
			} else{
			LOGE("cost %f secs",
				   (1.0 * (iclock() - startTs) / 1000));
				return;
			}

			((uint32_t*)sndBuf)[0] = nextSndIndex++;

			len = kcppClient.Send(sndBuf, SND_BUFF_LEN);
			if (len < 0)
			{
                LOGD("kcpSession Send failed\n");
				error_pause();
				return;
			}
		}

		while (kcppClient.Recv(&kcppRcvBuf, len))
		{
			if (len < 0)
			{
                LOGD("kcpSession Recv failed, Recv() = %d \n", len);
				error_pause();
				return;
			}
			else if (len > 0)
			{
				uint32_t srvRcvMaxIndex = *(uint32_t*)(kcppRcvBuf.peek() + 0);
				kcppRcvBuf.retrieveAll();
                LOGD("msg from server: have recieved the max index = %d\n", (int)srvRcvMaxIndex);
//				if (srvRcvMaxIndex >= testPassIndex)
//				{
//                    LOGD("client test passes, yay! \n");
//					return;
//				}
			}
		}
	}
}

int test()
{
	int client_fd;
	struct sockaddr_in ser_addr;

	client_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_fd < 0)
	{
        LOGD("create socket fail!");
		return -1;
	}


	unsigned long flags = 1; /* 这里根据需要设置成0或1 */
	ioctl(client_fd, FIONBIO, &flags);

	memset(&ser_addr, 0, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	ser_addr.sin_port = htons(SERVER_PORT);


	auto* c_src = "/storage/emulated/0/out.h264";
	rf = fopen(c_src, "rb");

	LOGD("client create socket success!");
	udp_msg_sender(client_fd, (struct sockaddr*)&ser_addr);
	fclose(rf);


//	closesocket(client_fd);

	return 0;
}

pthread_t mThread;
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved){
    __android_log_print(ANDROID_LOG_INFO, "native", "Jni_OnLoad");
    JNIEnv* env = NULL;
    if(vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) //从JavaVM获取JNIEnv，一般使用1.4的版本
        return -1;
    int result = pthread_create(&mThread, NULL, reinterpret_cast<void *(*)(void *)>(test), NULL);
    __android_log_print(ANDROID_LOG_INFO, "Thread", "#阻塞#Thread end=%d",result);
    return JNI_VERSION_1_4;
}