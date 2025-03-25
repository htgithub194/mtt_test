// C program to Implement Ping

// Compile as: gcc -o ping ping.c
// Run as: sudo ./ping <hostname>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

// Define the Packet Constants
#define PING_PKT_S 64           // ping packet size
#define PORT_NO 0               // automatic port number
#define PING_SLEEP_RATE 1000000 // ping sleep rate (in microseconds)

// Global variable
int ping_counter = 3;
int ping_timeout = 1;

// Ping packet structure
struct ping_pkt
{
    struct icmphdr hdr;
    char msg[PING_PKT_S - sizeof(struct icmphdr)];
};

// Function Declarations
unsigned short checksum(void *b, int len);
void intHandler(int dummy);
char *dns_lookup(const char *addr_host, struct sockaddr_in *addr_con);
void send_ping(int ping_sockfd, struct sockaddr_in *ping_addr, char *ping_ip, const char *rev_host);
void check_options(int argc, char *argv[]);

//
void printHelp()
{
   printf("Using this tool with root previleage\n");
   printf("sudo ./mtt_ping google.com\n");
   printf("-t\t Indicate the maximum time to wait a response\n");
   printf("-c\t Indicate the number of ping messages to send\n");
}

// parse cmd line options
void check_options(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hc:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printHelp();
            exit(0);
        case 'c':
            ping_counter = atoi(optarg);
            break;
        case 't':
            ping_timeout = atoi(optarg);
            break;
        case '?':
            printf("Unknown option: -%c\n", optopt);
            exit(1);
        }
    }
}

// Calculate the checksum (RFC 1071)
unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Interrupt handler
void intHandler(int dummy)
{
    ping_counter = 0;
}

// Perform a DNS lookup
char *dns_lookup(const char *addr_host, struct sockaddr_in *addr_con)
{
    struct hostent *host_entity;
    char *ip = (char *)malloc(NI_MAXHOST * sizeof(char));

    if ((host_entity = gethostbyname(addr_host)) == NULL)
    {
        // No IP found for hostname
        return NULL;
    }

    // Fill up address structure
    strcpy(ip, inet_ntoa(*(struct in_addr *)host_entity->h_addr));
    (*addr_con).sin_family = host_entity->h_addrtype;
    (*addr_con).sin_port = htons(PORT_NO);
    (*addr_con).sin_addr.s_addr = *(long *)host_entity->h_addr;

    return ip;
}

// Make a ping request
void send_ping(int ping_sockfd, struct sockaddr_in *ping_addr, char *ping_ip, const char *rev_host)
{
    int ttl_val = 64, msg_count = 0, i, addr_len, flag = 1, msg_received_count = 0;
    char rbuffer[128];
    struct ping_pkt pckt;
    struct sockaddr_in r_addr;
    struct timespec time_start, time_end, tfs, tfe;
    long double rtt_msec = 0, total_msec = 0;
    struct timeval tv_out;
    tv_out.tv_sec = ping_timeout;
    tv_out.tv_usec = 0;
    int iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    double timeElapsed;

    clock_gettime(CLOCK_MONOTONIC, &tfs);

    // Set socket options at IP to TTL and value to 64
    if (setsockopt(ping_sockfd, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val)) != 0)
    {
        printf("\nSetting socket options to TTL failed!\n");
        return;
    }

    // Setting timeout of receive setting
    setsockopt(ping_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof tv_out);

    // Send ICMP packet in an infinite loop
    while (ping_counter-- > 0)
    {
        // Flag to check if packet was sent or not
        flag = 1;

        // Fill the packet
        bzero(&pckt, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = getpid();

        for (i = 0; i < sizeof(pckt.msg) - 1; i++)
            pckt.msg[i] = i + '0';

        pckt.msg[i] = 0;
        pckt.hdr.un.echo.sequence = msg_count++;
        pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));

        usleep(PING_SLEEP_RATE);

        // Send packet
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        if (sendto(ping_sockfd, &pckt, sizeof(pckt), 0, (struct sockaddr *)ping_addr, sizeof(*ping_addr)) <= 0)
        {
            printf("\nPacket Sending Failed!\n");
            flag = 0;
        }

        // Receive packet
        addr_len = sizeof(r_addr);
        if (recvfrom(ping_sockfd, rbuffer, sizeof(rbuffer), 0, (struct sockaddr *)&r_addr, &addr_len) <= 0 && msg_count > 1)
        {
            // printf("\nPacket receive failed!\n");
            printf("From %s Time to live exceeded\n", ping_ip);
        }
        else
        {
            clock_gettime(CLOCK_MONOTONIC, &time_end);

            timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0;
            rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + timeElapsed;

            // If packet was not sent, don't receive
            if (flag)
            {
                ip = (struct ip *)rbuffer;
                iphdrlen = ip->ip_hl << 2;
                icmp = (struct icmp *)(rbuffer + iphdrlen);
                if (icmp->icmp_type == ICMP_ECHOREPLY)
                {
                    printf("%d bytes from %s: icmp_seq=%d ttl=%d rtt=%Lf ms\n", PING_PKT_S, ping_ip, msg_count, ttl_val, rtt_msec);
                    msg_received_count++;
                }
                else {
                    printf("From %s Destination Host Unreachable\n", ping_ip);
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &tfe);
    timeElapsed = ((double)(tfe.tv_nsec - tfs.tv_nsec)) / 1000000.0;
    total_msec = (tfe.tv_sec - tfs.tv_sec) * 1000.0 + timeElapsed;

    printf("\n=== %s ping statistics ===\n", ping_ip);
    printf("%d packets sent, %d packets received, %f%% packet loss. Total time: %Lf ms.\n\n", msg_count, msg_received_count, ((msg_count - msg_received_count) / (double)msg_count) * 100.0, total_msec);
}

// Driver Code
int main(int argc, char *argv[])
{
    int sockfd;
    char *ip_addr;
    struct sockaddr_in addr_con;
    int addrlen = sizeof(addr_con);
    char net_buf[NI_MAXHOST];
    const char *hostname = NULL;

    check_options(argc, argv);
    hostname = argv[optind];

    ip_addr = dns_lookup(hostname, &addr_con);
    if (ip_addr == NULL)
    {
        printf("\nDNS lookup failed! Could not resolve hostname!\n");
        return 0;
    }

    printf("PING %s (%s):\n", hostname, ip_addr);

    // Create a raw socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        printf("\nSocket file descriptor not received!\n");
        return 0;
    }

    signal(SIGINT, intHandler); // Catching interrupt

    // Send pings continuously
    send_ping(sockfd, &addr_con, ip_addr, hostname);

    return 0;
}
