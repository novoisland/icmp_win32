#include <winsock2.h>
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
  uint32_t  daddr;
  uint8_t   type;
  uint8_t   code;
  uint16_t  id;
  uint32_t  sequence;
};

#define ICMP_MSG_NUM  0x00709394

int main(int argc, char *argv[])
{
  int recvPayloadSize, recvBufferSize = 32768, wBufferSize = 1024 * 512, wBufferPos = 0, wFilePos = 0;
  char *recvPayload, *recvBuffer, *filename, *wBuffer;
  int ihl, readSize;
  uint32_t seq = 0, offset = 0;
  FILE *file = NULL;
  SOCKET sockfd;
  struct chain *bufferChain;
  fpos_t fPos;

  if ( (recvBuffer = malloc(recvBufferSize)) == NULL )
  {
    fprintf(stderr, "Could not allocate memory for recvBuffer\n");
    return 1;
  }
  struct iphdr *ip = (struct iphdr *) recvBuffer;
  struct icmphdr *icmp = (struct icmphdr *) (recvBuffer + sizeof (struct iphdr));
  struct msghdr *msg = (struct msghdr *) (recvBuffer + sizeof (struct iphdr) + sizeof (struct icmphdr));
  
  if ( (wBuffer = malloc(wBufferSize)) == NULL )
  {
    fprintf(stderr, "Could not allocate memory for wBuffer\n");
    return 1;
  }

  //Initialise Winsock
  WSADATA wsock;
  if (WSAStartup(MAKEWORD(2,2),&wsock) != 0)
  {
    fprintf(stderr, "WSAStartup() failed with error code %d\n", WSAGetLastError());
    return 1;
  }
  
  //Create Raw ICMP socket
  if ( (sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == INVALID_SOCKET )
  {
    fprintf(stderr, "socket() failed with error code %d\n", WSAGetLastError());
    return 1;
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
    if (WSAIoctl(sockfd, SIO_ADDRESS_LIST_QUERY, NULL, 0, recvBuffer, recvBufferSize, &dwBytesRet, NULL, NULL) == SOCKET_ERROR)
    {
        fprintf(stderr, "WSAIoctl(SIO_ADDRESS_LIST_QUERY) failed with error code %d\n", WSAGetLastError());
        return 1;
    }
    slist = (SOCKET_ADDRESS_LIST *)recvBuffer;
    if0 = *(struct sockaddr_in *)slist->Address[0].lpSockaddr;
  }
  printf("listening on ip %s\n",inet_ntoa((struct in_addr)if0.sin_addr.s_addr));
  
  //bind socket with ip
  if ( bind(sockfd, (SOCKADDR *) &if0, sizeof (if0)) == SOCKET_ERROR )
  {
    fprintf(stderr, "Failed to bind socket with error %d\n", WSAGetLastError());
    return 1;
  }
  
  DWORD optval = RCVALL_ON;
  //set socket to receive all packets
  if (WSAIoctl(sockfd, SIO_RCVALL, &optval, sizeof(optval), NULL, 0, &dwBytesRet, NULL, NULL) == SOCKET_ERROR)
  {
    fprintf(stderr, "WSAIotcl(SIO_RCVALL) failed with error code %d\n", WSAGetLastError());
    return 1;
  }
  
  while(1) {
    if ((readSize = recv(sockfd, recvBuffer, recvBufferSize, 0)) == SOCKET_ERROR)
    {
      fprintf(stderr, "Failed to receive packets with error %d\n", WSAGetLastError());
      return 1;
    }
    if (ip->saddr == if0.sin_addr.s_addr && ip->daddr != if0.sin_addr.s_addr) continue;
    ihl = (int)ip->ihl << 2;
    icmp = (struct icmphdr *)(recvBuffer + ihl);
    msg = (struct msghdr *)(recvBuffer + ihl + sizeof (struct icmphdr));
    //printf("src: %s, id: %x, seq: %x, all: %d\n", inet_ntoa((struct in_addr)ip->saddr), ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.sequence), readSize);
    if ( readSize - ihl - sizeof (struct icmphdr) <= 0 ) continue;
    if ( msg->magic != ICMP_MSG_NUM ) continue;
    if ( msg->daddr != if0.sin_addr.s_addr ) continue;
    recvPayloadSize = readSize - ihl - sizeof (struct icmphdr) - sizeof (struct msghdr);
    if (recvPayloadSize < 0) continue;
    recvPayload = (char *)(recvBuffer + ihl + sizeof (struct icmphdr) + sizeof (struct msghdr));
    //printf("msg type: %x, code: %x, id: %d, seq: %d, payload: %d\n", msg->type, msg->code, msg->id, msg->sequence, recvPayloadSize);

    switch (msg->type) {
    case 1:
      if (recvPayloadSize < 1) break;
      printf("message from %s: %s\n",inet_ntoa((struct in_addr)ip->saddr),recvPayload);
      break;
    case 2:
      switch (msg->code) {
      case 0:
        if (recvPayloadSize < 2) break;
        if (msg->sequence <= seq) break;
        if (filename = strrchr(recvPayload, '/')) filename++;
        else if (filename = strrchr(recvPayload, '\\')) filename++;
        else filename = recvPayload;
        printf("receive file '%s' from %s\n",filename,inet_ntoa((struct in_addr)ip->saddr));
        if (file) fclose(file);
        if ( (file = fopen(filename, "wb+")) == NULL ) {
          printf("unable to open file for write");
          seq = 0;
          break;
        }
        seq = msg->sequence;
        wBufferPos = 0;
        wFilePos = 0;
        break;
      case 1:
        //printf("got seq %d curr %d\n", msg->sequence, seq);
        if (file == NULL) break;
        if (msg->sequence <= seq) break;
        if (msg->sequence != seq + 1) {
          if (wBufferPos) fwrite(wBuffer, wBufferPos, 1, file);
          fgetpos(file, &fPos);
          printf("expect seq %d got %d, please send file again from %d\n", seq+1, msg->sequence, fPos);
          fclose(file);
          file = NULL;
          seq = 0;
          break;
        }
        wFilePos += recvPayloadSize;
        printf("get seq %d size %d, total %d      \r", msg->sequence, recvPayloadSize, wFilePos);
        if (wBufferPos + recvPayloadSize > wBufferSize) {
          fwrite(wBuffer, wBufferPos, 1, file);
          memcpy(wBuffer, recvPayload, recvPayloadSize);
          wBufferPos = recvPayloadSize;
        } else {
          memcpy(wBuffer + wBufferPos, recvPayload, recvPayloadSize);
          wBufferPos = wBufferPos + recvPayloadSize;
        }
        seq++;
        break;
      case 2:
        if (file == NULL) break;
        if (wBufferPos + recvPayloadSize > wBufferSize) {
          fwrite(wBuffer, wBufferPos, 1, file);
          fwrite(recvPayload, recvPayloadSize, 1, file);
        } else {
          memcpy(wBuffer + wBufferPos, recvPayload, recvPayloadSize);
          wBufferPos = wBufferPos + recvPayloadSize;
          fwrite(wBuffer, wBufferPos, 1, file);
        }
        fclose(file);
        file = NULL;
        seq = 0;
        printf("receive done                       \n");
        break;
      case 3:
        if (recvPayloadSize < sizeof(offset) + 2) break;
        if (msg->sequence <= seq) break;
        offset = *(uint32_t*)(recvPayload + strlen(recvPayload) + 1);
        if (filename = strrchr(recvPayload, '/')) filename++;
        else if (filename = strrchr(recvPayload, '\\')) filename++;
        else filename = recvPayload;
        printf("receive file '%s' from %s start at %d\n",filename,inet_ntoa((struct in_addr)ip->saddr),offset);
        if (file) fclose(file);
        if ( (file = fopen(filename, "rb+")) == NULL ) {
          printf("file not exists or unable to open file\n");
          seq = 0;
          break;
        }
        if (fseek(file, offset, SEEK_SET) != 0) {
          printf("unable to seek file\n");
          seq = 0;
          break;
        }
        seq = msg->sequence;
        wBufferPos = 0;
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
