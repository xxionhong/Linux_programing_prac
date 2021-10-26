#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PORT 12345

int socketFD;

void *recv_handler()
{
    char recv_buff[BUFFER_SIZE];
    while (1)
    {
        int tmp = read(socketFD, recv_buff, sizeof(recv_buff));
        if (tmp > 0)
        {
            printf("%s\n", recv_buff);
            if (strcmp(recv_buff, "kicked"))
            {
                break;
            }
        }
        else if (tmp == 0)
        {
            break;
        }
        memset(recv_buff, 0, BUFFER_SIZE);
    }
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{

    char send_buff[BUFFER_SIZE], name[21];
    struct sockaddr_in conn_info;
    if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket\t");
        exit(EXIT_FAILURE);
    }
    conn_info.sin_family = AF_INET;
    conn_info.sin_port = htons(PORT);
    conn_info.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&(conn_info.sin_zero), 0, sizeof(conn_info.sin_zero));

    if (connect(socketFD, (struct sockaddr *)&conn_info, sizeof(struct sockaddr)) == -1)
    {
        perror("connect\t");
        exit(EXIT_FAILURE);
    }
    printf("Please input a Name (<20 Char)\n");
    scanf("%20s", name);
    send(socketFD, name, sizeof(name), 0);

    pthread_t recv_handler_t;
    if (pthread_create(&recv_handler_t, NULL, (void *)&recv_handler, NULL) != 0)
    {
        perror("recv_handler\t");
        close(socketFD);
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        printf("Please input data (exit)\n");
        scanf("%s", send_buff);
        send(socketFD, send_buff, BUFFER_SIZE, 0);
        if (!strcmp(send_buff, "exit"))
        {
            break;
        }
        memset(send_buff, 0, BUFFER_SIZE);
    }
    close(socketFD);
    return 0;
}
