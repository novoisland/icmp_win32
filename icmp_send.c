#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>

//pragma comment(lib, "iphlpapi.lib")
//pragma comment(lib, "ws2_32.lib")

struct msghdr
{
  uint32_t  magic;
  uint8_t   type;
  uint8_t   code;
  uint16_t  id;
  uint32_t  sequence;
  uint32_t  v1;
};

#define ICMP_MSG_NUM  0x00709394

int main(int argc, char **argv)  {

  // Declare and initialize variables
  int readSize, sendBufferSize = 1470;  //1500 - 20 - 8 - 2
  int rBufferSize = 1024 * 512, rBufferPos = 0;
  HANDLE hIcmpFile;
  uint32_t ipaddr = INADDR_NONE;
  DWORD dwLastErr, dwRetVal = 0, ReplySize = 0;
  FILE *file = NULL;
  char *ReplyBuffer, *sendBuffer, *rBuffer;
  PICMP_ECHO_REPLY pEchoReply;
  
  sendBuffer = malloc(sendBufferSize);
  if (sendBuffer == NULL) {
      printf("\tUnable to allocate memory\n");
      return 1;
  }
  struct msghdr *msg = (struct msghdr *) sendBuffer;
  char *sendPayload = (char *)(sendBuffer + sizeof (struct msghdr));
    
  if ( (rBuffer = malloc(rBufferSize)) == NULL )
  {
    fprintf(stderr, "Could not allocate memory for rBuffer\n");
    return 1;
  }

  // Validate the parameters
  if (argc < 4) {
      printf("usage: %s <IP> <type> <content>\n", argv[0]);
      return 1;
  }

  ipaddr = inet_addr(argv[1]);
  if (ipaddr == INADDR_NONE) {
      printf("usage: %s <IP> <type> <content>\n", argv[0]);
      return 1;
  }
  
  hIcmpFile = IcmpCreateFile();
  if (hIcmpFile == INVALID_HANDLE_VALUE) {
      printf("IcmpCreatefile returned error: %ld\n", GetLastError() );
      return 1;
  }

  ReplySize = sizeof(ICMP_ECHO_REPLY) + sendBufferSize;
  ReplyBuffer = malloc(ReplySize);
  if (ReplyBuffer == NULL) {
      printf("\tUnable to allocate memory\n");
      return 1;
  }
  
  msg->magic=ICMP_MSG_NUM;
  msg->type=atoi(argv[2]);
  msg->code=0;
  msg->id=GetTickCount();
  msg->sequence=1;
  msg->v1=0;
  strncpy_s(sendPayload, sendBufferSize-sizeof (struct msghdr), argv[3], _TRUNCATE);

  switch (msg->type) {
  case 1:
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + strlen(argv[3])+1, NULL, ReplyBuffer, ReplySize, 1000);
    break;
  case 2:
    if ( (file = fopen(argv[3], "rb")) == NULL ) {
      printf("unable to open file '%s'", argv[3]);
      break;
    }
    if (argv[4]) {
      msg->code=3;
      msg->v1 = atoi(argv[4]);
      if (fseek(file, msg->v1, SEEK_SET) != 0) {
        printf("unable to seek file");
        break;
      }
    }
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + strlen(argv[3])+1, NULL, ReplyBuffer, ReplySize, 1000);
    msg->code=1;
    while((readSize = fread(sendPayload, 1, sendBufferSize - sizeof (struct msghdr), file)) > 0) {
      msg->sequence++;
      dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + readSize, NULL, ReplyBuffer, ReplySize, 20);
      if (dwRetVal > 0) {
        pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        if (pEchoReply->Status == 0)
          printf("sent package seq %d data size %d, return %d    \r", msg->sequence, readSize, pEchoReply->Status);
        else
          printf("sent package seq %d data size %d, return %d    \n", msg->sequence, readSize, pEchoReply->Status);
      } else
        printf("sent package seq %d data size %d, return %d    \n", msg->sequence, readSize, GetLastError());
    }
    msg->code=2;
    //sleep(1000);
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr), NULL, ReplyBuffer, ReplySize, 1000);
    printf("\ncompleted.\n");
    break;
  default:
    break;
  }
  
  return 0;
}
