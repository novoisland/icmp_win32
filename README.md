# icmp_win32
a simple message and file transfer program via ICMP for windows

## Feature
- It can send either message or file.
- It is single direction (sender -> receiver), and no any feedback from receiver.
- The sender program do not require privilege, as it does not create raw socket, however the receiver program may still need privilege for windows 7 or earlier.

## Build
Download the win32 Tiny C Compiler (TCC) from [here](https://bellard.org/tcc/) and the full winapi package, extract them into same directory.

export library def file

    tcc -impdef ws2_32.dll
    tcc -impdef iphlpapi.dll

build the binary

    tcc icmp_recv.c ws2_32.def
    tcc icmp_send.c ws2_32.def iphlpapi.def

## Usage

start the receiver

    icmp_recv

or if there are multiple IP addresses

    icmp_recv <IP address>

send message to receiver

    icmp_send <IP address> 1 <message content>

send file to receiver

    icmp_send <IP address> 2 <file path>
