#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>

//pragma comment(lib, "iphlpapi.lib")
//pragma comment(lib, "ws2_32.lib")

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

int main(int argc, char **argv)  {

  // Declare and initialize variables
  int dataSize, readSize, sendBufferSize = 1470;  //1500 - 20 - 8 - 2
  int rBufferSize = 1024 * 512, rBufferPos = 0;
  HANDLE hIcmpFile;
  uint32_t time, ipaddr = INADDR_NONE;
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
  pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
  
  msg->magic=ICMP_MSG_NUM;
  msg->daddr=ipaddr;
  msg->type=atoi(argv[2]);
  msg->code=0;
  msg->id=GetTickCount();
  msg->sequence=1;
  strncpy_s(sendPayload, sendBufferSize-sizeof (struct msghdr), argv[3], _TRUNCATE);
  dataSize = strlen(argv[3]) + 1;

  switch (msg->type) {
  case 1:
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + dataSize, NULL, ReplyBuffer, ReplySize, 1000);
    //printf("reply size %d",pEchoReply->DataSize);
    break;
  case 2:
    if ( (file = fopen(argv[3], "rb")) == NULL ) {
      printf("unable to open file '%s'", argv[3]);
      break;
    }
    if (argv[4]) {
      if (fseek(file, atoi(argv[4]), SEEK_SET) != 0) {
        printf("unable to seek file\n");
        break;
      }
      msg->code = 3;
      *(uint32_t*)(sendPayload + dataSize) = atoi(argv[4]);
      dataSize += sizeof(uint32_t);
    }
    time = GetTickCount();
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + dataSize, NULL, ReplyBuffer, ReplySize, 1000);
    msg->code = 1;
    while((readSize = fread(rBuffer, 1, rBufferSize, file)) > 0) {
      rBufferPos = 0;
      while (rBufferPos < readSize) {
        if (readSize - rBufferPos >= sendBufferSize - sizeof (struct msghdr))
          dataSize = sendBufferSize - sizeof (struct msghdr);
        else
          dataSize = readSize - rBufferPos;
        memcpy(sendPayload, rBuffer+rBufferPos, dataSize);
        rBufferPos += dataSize;
        msg->sequence++;
        dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr) + dataSize, NULL, ReplyBuffer, ReplySize, 50);
        if (dwRetVal > 0) {
          if (pEchoReply->Status == 0)
            printf("sent package seq %d data size %d, return %d    \r", msg->sequence, dataSize, pEchoReply->Status);
          else
            printf("sent package seq %d data size %d, return %d    \n", msg->sequence, dataSize, pEchoReply->Status);
        } else {
          dwLastErr = GetLastError();
          printf("sent package seq %d data size %d, return %d    \n", msg->sequence, dataSize, dwLastErr);
          if (dwLastErr == WSA_QOS_ADMISSION_FAILURE) {
            //printf("pause 500ms\n");
            //sleep(500);
          }
        }
        //sleep(1);
      }
    }
    msg->code = 2;
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendBuffer, sizeof (struct msghdr), NULL, ReplyBuffer, ReplySize, 1000);
    printf("\ncompleted, time elapsed %dms\n", GetTickCount() - time);
    break;
  default:
    break;
  }
  
  return 0;
}
