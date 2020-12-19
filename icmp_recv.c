#include <winsock.h>
#include <mstcpip.h>
#include <stdio.h>
 
//pragma comment(lib,"ws2_32.lib") //winsock 2.2 library

typedef uint8_t u_int8_t; 
typedef uint16_t u_int16_t; 
typedef uint32_t u_int32_t;

struct iphdr
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned int ihl:4;
  unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
  unsigned int version:4;
  unsigned int ihl:4;
#else
# error "Please fix <bits/endian.h>"
#endif
  u_int8_t tos;
  u_int16_t tot_len;
  u_int16_t id;
  u_int16_t frag_off;
  u_int8_t ttl;
  u_int8_t protocol;
  u_int16_t check;
  u_int32_t saddr;
  u_int32_t daddr;
  /*The options start here. */
};

struct icmphdr
{
  u_int8_t type;    /* message type */
  u_int8_t code;    /* type sub-code */
  u_int16_t checksum;
  union
  {
    struct
    {
      u_int16_t  id;
      u_int16_t  sequence;
    } echo;      /* echo datagram */
    u_int32_t  gateway;  /* gateway address */
    struct
    {
      u_int16_t  __unused;
      u_int16_t  mtu;
    } frag;      /* path mtu discovery */
  } un;
};

struct msghdr
{
  uint32_t  magic;
  uint8_t   type;
  uint8_t   code;
  uint16_t  id;
  uint32_t  sequence;
};

#define ICMP_MSG_NUM  0x00709394

int main(int argc, char *argv[])
{
  int buffer_size = 8000;
  int packet_size = sizeof (struct iphdr) + sizeof (struct icmphdr) + buffer_size;
  int ihl, read_size, payload_size, seq = 0;
  char *packet, *payload, *filename;
  FILE *file = NULL;
  SOCKET sockfd;

  if ( (packet = malloc(packet_size)) == NULL)
  {
    fprintf(stderr, "Could not allocate memory for packet\n");
    return -1;
  }
  struct iphdr *ip = (struct iphdr *) packet;
  struct icmphdr *icmp = (struct icmphdr *) (packet + sizeof (struct iphdr));
  struct msghdr *msg = (struct msghdr *) (packet + sizeof (struct iphdr) + sizeof (struct icmphdr));

  //Initialise Winsock
  WSADATA wsock;
  if (WSAStartup(MAKEWORD(2,2),&wsock) != 0)
  {
    fprintf(stderr, "WSAStartup() failed with error code %d\n", WSAGetLastError());
    return -1;
  }
  
  //Create Raw ICMP socket
  if ( (sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == INVALID_SOCKET )
  {
    fprintf(stderr, "socket() failed with error code %d\n", WSAGetLastError());
    return -1;
  }
  
  DWORD dwBytesRet;
  SOCKET_ADDRESS_LIST *slist=NULL;
  
  struct sockaddr_in if0;
  if0.sin_family = AF_INET;
  if0.sin_port = 0;
  
  if (argc >= 2) {
    if0.sin_addr.s_addr = inet_addr(argv[1]);
  } else {
    //get the ip of first interface
    if (WSAIoctl(sockfd, SIO_ADDRESS_LIST_QUERY, NULL, 0, packet, buffer_size, &dwBytesRet, NULL, NULL) == SOCKET_ERROR)
    {
        printf("WSAIoctl(SIO_ADDRESS_LIST_QUERY) failed with error code %d\n", WSAGetLastError());
        return -1;
    }
    slist = (SOCKET_ADDRESS_LIST *)packet;
    if0.sin_addr.s_addr = ((SOCKADDR_IN *)slist->Address[0].lpSockaddr)->sin_addr.s_addr;
  }
  printf("listening on ip %s\n",inet_ntoa((struct in_addr)if0.sin_addr.s_addr));
  
  //bind socket with ip
  if ( bind(sockfd, (SOCKADDR *) &if0, sizeof (if0)) == SOCKET_ERROR )
  {
    fprintf(stderr, "Failed to bind socket %d\n", WSAGetLastError());
    return -1;
  }
  
  DWORD optval = RCVALL_ON;
  //set socket to receive all packets
  if (WSAIoctl(sockfd, SIO_RCVALL, &optval, sizeof(optval), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR)
  {
    fprintf(stderr, "WSAIotcl(SIO_RCVALL) failed with error code %d\n", WSAGetLastError());
    return -1;
  }
  
  while(1) {
    if ((read_size = recvfrom(sockfd, packet, packet_size, 0, NULL, NULL)) == SOCKET_ERROR)
    {
      fprintf(stderr, "Failed to receive packets, %d\n", WSAGetLastError());
      return -1;
    }
    ihl = (int)ip->ihl << 2;
    icmp = (struct icmphdr *)(packet + ihl);
    msg = (struct msghdr *)(packet + ihl + sizeof (struct icmphdr));
    //printf("src: %s, id: %x, seq: %x, all: %d\n", inet_ntoa((struct in_addr)ip->saddr), ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.sequence), read_size);
    if ( msg->magic != ICMP_MSG_NUM ) continue;
    payload_size = read_size - ihl - sizeof (struct icmphdr) - sizeof (struct msghdr);
    if (payload_size < 0) continue;
    payload = (char *)(packet + ihl + sizeof (struct icmphdr) + sizeof (struct msghdr));
    //printf("msg type: %x, code: %x, id: %d, seq: %d, payload: %d\n", msg->type, msg->code, msg->id, msg->sequence, payload_size);

    switch (msg->type) {
    case 1:
      printf("message from %s: %s\n",inet_ntoa((struct in_addr)ip->saddr),payload);
      break;
    case 2:
      switch (msg->code) {
      case 0:
        if (msg->sequence <= seq) break;
        if (filename = strrchr(payload, '/')) filename++;
        else if (filename = strrchr(payload, '\\')) filename++;
        else filename = payload;
        printf("receive file '%s' from %s\n",filename,inet_ntoa((struct in_addr)ip->saddr));
        if (file) fclose(file);
        if ( (file = fopen(filename, "wb+")) == NULL) {
          printf("unable to open file for write");
          seq = 0;
          break;
        }
        seq = msg->sequence;
        break;
      case 1:
        if (!file) break;
        if (msg->sequence <= seq) break;
        if (msg->sequence != seq + 1) {
          printf("message is interrupted, please send file again\n");
          if (file) {
            fclose(file);
            file = NULL;
            seq = 0;
          }
          break;
        }
        //printf("write seq %d size %d\n", msg->sequence, payload_size);
        fwrite(payload, payload_size, 1, file);
        //fflush(file);
        seq = msg->sequence;
        break;
      case 2:
        if (!file) break;
        printf("receive done\n");
        fclose(file);
        file = NULL;
        seq = 0;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }
  
  return 0;
}
