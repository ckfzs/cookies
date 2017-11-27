#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_systm.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#define BUFSIZE 1500
char sendbuf[BUFSIZE];
pid_t pid;
int datalen = 56;
char *host;
int sockfd;
int nsent = 0;
int verbose = 1;
socklen_t salen;
struct sockaddr *sasend, *sarecv;

/* 用于解析名字到地址或者服务到端口
 */
struct addrinfo *host_serv(const char *host, const char *serv, int family, int socktype)
{
    int n;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = family;
    hints.ai_socktype = socktype;

    if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
        return NULL;

    return res;
}

/* 用于求取两个时间值的间隔
 */
void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}

/* 用于求取ICMP首部和数据的校验和
 */
uint16_t in_cksum(uint16_t *addr, int len)
{
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

/* 发送PING包
 */
void send_msg(void)
{
    int len;
    struct icmp *icmp;
    icmp = (struct icmp *)sendbuf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = pid;
    icmp->icmp_seq = nsent++;
    memset(icmp->icmp_data, 0xa5, datalen);
    gettimeofday((struct timeval *)icmp->icmp_data, NULL);
    len = 8 + datalen;
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = in_cksum((uint16_t *)icmp, len); 
    sendto(sockfd, sendbuf, len, 0, sasend, salen); 
}

/* SIGALRM信号处理函数
 * 从下面可以看出, 每隔1秒钟发送一个PING包
 */
void sig_alrm(int signo)
{
    send_msg();
    alarm(1);
    return;
}

/* 将二进制的IP地址转换为点分十进制字符串形式
 */
char *sock_ntop(const struct sockaddr *sa, socklen_t salen)
{
    static char str[INET_ADDRSTRLEN];
    struct sockaddr_in *sin;
    if (sa->sa_family != AF_INET)
        return NULL;
    sin = (struct sockaddr_in *)sa;
    return inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str));
}

/* 对接收的ICMP数据包进行解析处理
 */
void process_msg(char *ptr, ssize_t len, struct msghdr *msg, struct timeval *tvrecv)
{
    int hlen, icmplen;
    double rtt;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;

    ip = (struct ip *)ptr;
    hlen = ip->ip_hl << 2;
    if (ip->ip_p != IPPROTO_ICMP)
        return;

    icmp = (struct icmp *)(ptr + hlen);
    if ((icmplen = len - hlen) < 8)
        return;

    if (icmp->icmp_type == ICMP_ECHOREPLY)
    {
        if (icmp->icmp_id != pid)
            return;
        if (icmplen < 16)
            return;
        
        tvsend = (struct timeval *)icmp->icmp_data;
        tv_sub(tvrecv, tvsend);
        rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
        printf("%d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n", icmplen, sock_ntop(sarecv, salen), icmp->icmp_seq, ip->ip_ttl, rtt);
    } else if (verbose)
    {
        printf("-->>%d bytes from %s: type = %d, code = %d\n", icmplen, sock_ntop(sarecv, salen), icmp->icmp_type, icmp->icmp_code);
    }
}

/* 接收数据包主循环
 */
void readloop(void)
{
    int size;
    char recvbuf[BUFSIZE];
    char controlbuf[BUFSIZE];
    struct msghdr msg;
    struct iovec iov;
    ssize_t n;
    struct timeval tval;

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    setuid(getuid());
    
    size = 60 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    sig_alrm(SIGALRM);
    iov.iov_base = recvbuf;
    iov.iov_len = sizeof(recvbuf);
    msg.msg_name = sarecv;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = controlbuf;
    for (; ;)
    {
        msg.msg_namelen = salen;
        msg.msg_controllen = sizeof(controlbuf);
        n = recvmsg(sockfd, &msg, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("recvmsg error");
                exit(1);
            }
        }
        gettimeofday(&tval, NULL);
        process_msg(recvbuf, n, &msg, &tval); 
    }
}

int main(int argc, char *argv[])
{
    int c;
    struct addrinfo *ai;
    char *h;
    struct sockaddr_in *sa;
    host = argv[1];
    pid = getpid() & 0xffff;
    signal(SIGALRM, sig_alrm);
    ai = host_serv(host, NULL, 0, 0);
    if (ai->ai_family != AF_INET)
    {
        printf("Current version just support IPv4!\n");
        exit(1);
    }
    sa = (struct sockaddr_in *)ai->ai_addr;
    h = sock_ntop(sa, ai->ai_addrlen);
    printf("PING %s (%s): %d data bytes\n", ai->ai_canonname? ai->ai_canonname: h, h, datalen);
    
    sasend = ai->ai_addr;
    sarecv = calloc(1, ai->ai_addrlen);
    salen = ai->ai_addrlen;
    readloop();
}
