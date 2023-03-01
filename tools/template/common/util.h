#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "workflow/WFTaskFactory.h"

void print_peer_address(%s *server_task)
{
    char addrstr[128];
    struct sockaddr_storage addr;
    socklen_t l = sizeof addr;
    unsigned short port = 0;
    long long seq = server_task->get_task_seq();

    server_task->get_peer_addr((struct sockaddr *)&addr, &l);
    if (addr.ss_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
        inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
        port = ntohs(sin->sin_port);
    }
    else if (addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
        inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
        port = ntohs(sin6->sin6_port);
    }
    else
        strcpy(addrstr, "Unknown");

    fprintf(stderr, "peer address: %s:%d, seq: %lld.\n", addrstr, port, seq);
}

