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
};

#define ICMP_MSG_NUM  0x00709394

int main(int argc, char **argv)  {

    // Declare and initialize variables
    int icmp_payload_size = 1470;  //1500 - 20 - 8 - 2
    HANDLE hIcmpFile;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    int readSize;
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;
    FILE *file = NULL;
    char *SendData;
    SendData = malloc(icmp_payload_size);
    if (SendData == NULL) {
        printf("\tUnable to allocate memory\n");
        return 1;
    }
    struct msghdr *msg = (struct msghdr *) SendData;
    char *payload = (char *)(SendData + sizeof (struct msghdr));
    
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
        printf("\tUnable to open handle.\n");
        printf("IcmpCreatefile returned error: %ld\n", GetLastError() );
        return 1;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + icmp_payload_size;
    ReplyBuffer = (VOID*) malloc(ReplySize);
    if (ReplyBuffer == NULL) {
        printf("\tUnable to allocate memory\n");
        return 1;
    }
    
    msg->magic=ICMP_MSG_NUM;
    msg->type=atoi(argv[2]);
    msg->code=0;
    msg->id=time(NULL);
    msg->sequence=1;
    strcpy(payload, argv[3]);

    switch (msg->type) {
    case 1:
      dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (struct msghdr) + strlen(argv[3])+1, NULL, ReplyBuffer, ReplySize, 1000);
      break;
    case 2:
      if ( (file = fopen(argv[3], "rb")) == NULL ) {
        printf("unable to open file '%s'", argv[3]);
        break;
      }
      dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (struct msghdr) + strlen(argv[3])+1, NULL, ReplyBuffer, ReplySize, 1000);
      msg->code=1;
      while((readSize = fread(payload, 1, icmp_payload_size - sizeof (struct msghdr), file)) > 0) {
        msg->sequence++;
        dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (struct msghdr) + readSize, NULL, ReplyBuffer, ReplySize, 10);
        if (dwRetVal > 0)
          printf("sent package seq %d data size %d, return %d    \r", msg->sequence, readSize, dwRetVal);
        else
          printf("sent package seq %d data size %d, return %d    \n", msg->sequence, readSize, dwRetVal);
      }
      msg->code=2;
      dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (struct msghdr), NULL, ReplyBuffer, ReplySize, 1000);
      break;
    default:
      break;
    }
    
    return 0;
}
