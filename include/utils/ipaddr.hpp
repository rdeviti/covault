// Code from:
// https://www.binarytides.com/c-program-to-get-ip-address-from-interface-name-on-linux/

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

std::string getipaddr(char* iface) {
    int fd;
    struct ifreq ifr;

    // replace with interface name
    // or ask user to input
    // char iface[] = "enp1s0";

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;

    // Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    // display result
    std::string ip = inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);

    return ip;
}