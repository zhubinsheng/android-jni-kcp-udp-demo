#include <cstdio>
#include <sys/types.h>
#include <fcntl.h>
#include <cstring>
#include <random>
#include <string>
#include <netinet/in.h>
#include <ctime>
#include<pthread.h>

#include <jni.h>
#include <android/log.h>

#include "kcpp-master/kcpp.h"


using kcpp::KcpSession;

#define SERVER_PORT 6666

#define SND_BUFF_LEN 1500
#define RCV_BUFF_LEN 1000

// if u modify this `PRACTICAL_CONDITION`,
// u have to update this var of the client side to have the same value.
#define PRACTICAL_CONDITION 1

#define TAG "native-kcp" // 这个是自定义的LOG的标识
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__) // 定义LOGD类型
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__) // 定义LOGI类型
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__) // 定义LOGW类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__) // 定义LOGE类型
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__) // 定义LOGF类型

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

float GetRandomFloatFromZeroToOne()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution< float > dis(0.f, 1.f);
    return dis(gen);
}

void udp_output(const void *buf, int len, int fd, struct sockaddr_in* dst)
{
    sendto(fd, (const char*)buf, len, 0, (struct sockaddr*)dst, sizeof(*(struct sockaddr*)dst));
}

bool isSimulatingPackageLoss = false;
float simulatePackageLossRate = 0; // simulate package loss rate 30%
kcpp::UserInputData udp_input(char* buf, int len, int fd, struct sockaddr_in* from)
{
    socklen_t fromAddrLen = sizeof(*from);
    int recvLen = ::recvfrom(fd, buf, len, 0,
                             (struct sockaddr*)from, &fromAddrLen);
    if (recvLen > 0)
    {
        LOGE("kcpSession do udp_input");

        isSimulatingPackageLoss =
                GetRandomFloatFromZeroToOne() < simulatePackageLossRate ? true : false;
        if (isSimulatingPackageLoss)
        {
            LOGE("server: simulate package loss!!\n");
            buf = nullptr;
            recvLen = 0;
        }
    }
    return kcpp::UserInputData(buf, recvLen);
}

void error_pause()
{
    LOGE("press any key to quit ...\n");
    char ch; scanf("%c", &ch);
}

void handle_udp_msg(int fd)
{
    char sndBuf[SND_BUFF_LEN];
    char rcvBuf[RCV_BUFF_LEN];

    // we can't use char array, cause we don't know how big the recv_data is
    kcpp::Buf kcppRcvBuf;

    struct sockaddr_in clientAddr;  //clent_addr用于记录发送方的地址信息
    static const uint32_t kInitIndex = 1;
    uint32_t nextRcvIndex = kInitIndex;
    uint32_t curRcvIndex = kInitIndex;
    int len = 0;
    uint32_t rcvedIndex = 0;
    IUINT32 startTs = 0;
    int64_t nextKcppUpdateTs = 0;
    int64_t nextSendTs = 0;

    KcpSession kcppServer(
            kcpp::RoleTypeE::kSrv,
            std::bind(udp_output, std::placeholders::_1, std::placeholders::_2, fd, &clientAddr),
            std::bind(udp_input, rcvBuf, RCV_BUFF_LEN, fd, &clientAddr),
            std::bind(iclock));


#if !PRACTICAL_CONDITION

    static const int64_t kSendInterval = 0;
	const uint32_t testPassIndex = 66666;
	kcppServer.SetConfig(666, 1024, 1024, 4096, 1, 1, 1, 1, 0, 5);

#else
    kcppServer.
            SetConfig(1500, 512, 512, 4096, 0, 10, 2, 1, 0, 20);
    static const int64_t kSendInterval = 1; // 20fps
//    const uint32_t testPassIndex = 500;

#endif // PRACTICAL_CONDITION


    while (1)
    {
        int64_t now = static_cast<int64_t>(iclock());
        if (now >= nextKcppUpdateTs)
            nextKcppUpdateTs = kcppServer.Update();

        while (kcppServer.Recv(&kcppRcvBuf, len))
        {
            LOGE("kcpSession do while");

            if (len < 0 && !isSimulatingPackageLoss)
            {
                LOGE("kcpSession Recv failed, Recv() = %d \n", len);
                error_pause();
                return;
            }
            else if (len > 0)
            {
                rcvedIndex = *(uint32_t*)(kcppRcvBuf.peek() + 0);
                kcppRcvBuf.retrieveAll();

//                if (rcvedIndex <= testPassIndex)
//                    LOGE("msg from client: %d\n", (int)rcvedIndex);

//                if (rcvedIndex == kInitIndex)
//                    startTs = iclock();

//                if (rcvedIndex == testPassIndex)
//                    LOGE("\n test passes, yay! \n simulate package loss rate %f %% \n"
//                           " avg rtt %d ms \n cost %f secs \n"
//                           " now u can close me ...\n",
//                           (simulatePackageLossRate * 100.f), kcppServer.GetKcpInstance()->rx_srtt,
//                           (1.0 * (iclock() - startTs) / 1000));

                if (kcppServer.IsConnected() && rcvedIndex != nextRcvIndex)
                {
                    // 如果收到的包不连续
                    LOGE("ERROR index != nextRcvIndex : %d != %d, kcpServer.IsKcpConnected() = %d\n",
                           (int)rcvedIndex, (int)nextRcvIndex, (kcppServer.IsConnected() ? 1 : 0));
                    error_pause();
//                    return;
                }
                ++nextRcvIndex;
            }
        }

        if (now >= nextSendTs)
        {
            nextSendTs = now + kSendInterval;
            while (curRcvIndex <= nextRcvIndex - 1)
            {
                memset(sndBuf, 0, SND_BUFF_LEN);
                ((uint32_t*)sndBuf)[0] = curRcvIndex++;
                //int result = kcppServer.Send(sndBuf, SND_BUFF_LEN, kcpp::TransmitModeE::kUnreliable);
                int result = kcppServer.Send(sndBuf, SND_BUFF_LEN);
                if (result < 0)
                {
                    LOGE("kcpSession Send failed\n");
                    error_pause();
                    return;
                }
            }
        }

    }
}


int test(){
    srand(static_cast<uint32_t>(time(nullptr)));

    int server_fd, ret;
    struct sockaddr_in ser_addr;
    server_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0)
    {
        return -1;
    }
    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(SERVER_PORT);
    ret = ::bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if (ret < 0)
    {
        LOGE("socket bind fail!");
        return -1;
    }
    LOGD("socket start success!");

    handle_udp_msg(server_fd);
//    clo(server_fd);

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
