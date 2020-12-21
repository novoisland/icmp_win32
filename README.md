# icmp_win32
a simple message and file transfer program via ICMP for windows (POC)

## Feature
- It should work on Windows XP through Windows 10.
- It can send either text message or file.
- It is single direction (sender -> receiver), and no any feedback from receiver.
- The sender program does not require privilege, as it does not create raw socket, however the receiver program may still need privilege for windows 7 or earlier.
- Speed is not so optimized.
- The receiver can handle only one file transfer at a time, do not try to send multiple files at same time.

## Build
Download the win32 Tiny C Compiler (TCC) from [here](https://savannah.nongnu.org/projects/tinycc) and the full winapi package, extract them into same directory.

create library def file under the directory of source file

    tcc -impdef ws2_32.dll
    tcc -impdef iphlpapi.dll

build the binary

    tcc icmp_recv.c ws2_32.def
    tcc icmp_send.c ws2_32.def iphlpapi.def

## Usage

start the receiver, if get error, try with admin privilege

    icmp_recv

or if there are multiple IP addresses

    icmp_recv <IP address>

send message to receiver, 1 stands for message type

    icmp_send <IP address> 1 <message content>

send file to receiver, 2 stands for file transfer

    icmp_send <IP address> 2 <file path>
