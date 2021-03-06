#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
//#include <pthread.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define PORT 12345
int server_left = 0;
int client[FD_SETSIZE], listenfd;
/*pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *report_handler()
{
    struct timeval tv;
    char *txt = "server report";
    printf("start report\n");
    while (!server_left)
    {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (client[i] != -1)
            {
                write(client[i], txt, strlen(txt));
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}*/
void close_all_client()
{
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        if (client[i])
        {
            close(client[i]);
        }
    }
    close(listenfd);
}
void sig_handler(int sig_num)
{
    server_left = 1;
}

int main(int argc, char **argv)
{
    int connfd, sockfd, maxfd, cli_count = -1, i, nready, report_flag = 0;
    struct timeval tv;
    //pthread_t ptd = 0;
    fd_set rset, allset;
    char recv_buff[BUFFER_SIZE];
    struct sockaddr_in servaddr, cliaddr;
    char *txt = "server report";
    signal(SIGINT, sig_handler);
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("listen\t");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // bind
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind\t");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // listen
    if (listen(listenfd, 100) == -1)
    {
        perror("listen\t");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // set current maxfd for select
    maxfd = listenfd;

    // memset
    memset(client, -1, sizeof(client));
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    while (!server_left)
    {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rset = allset;
        // select arg server broadcast!!!!! dont use pthread
        if ((nready = select(maxfd + 1, &rset, NULL, NULL, &tv)) < 0) // select error
        {
            continue;
        }
        if (nready == 0) // select timeout
        {
            if (report_flag)
            {
                for (int i = 0; i < FD_SETSIZE; i++)
                {
                    if (client[i] != -1)
                    {
                        write(client[i], txt, strlen(txt));
                    }
                }
            }
        }
        else // select someting
        {
            // if any new client in
            if (FD_ISSET(listenfd, &rset))
            {
                socklen_t clilen = sizeof(cliaddr);
                if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) == -1)
                {
                    perror("accpet\t");
                    continue;
                }
                // check the max serving size

                printf("New client %d connected \n", connfd);
                for (i = 0; i < FD_SETSIZE; i++)
                {
                    if (client[i] < 0)
                    {
                        client[i] = connfd;
                        break;
                    }
                }
                if (i == FD_SETSIZE)
                {
                    printf("Reach Max... \n");
                    close(connfd);
                    continue;
                }
                FD_SET(connfd, &allset);
                if (connfd > maxfd)
                {
                    maxfd = connfd;
                }
                if (i > cli_count)
                {
                    cli_count = i;
                }
            }
            // for any serving client, dealing with the data passing
            for (i = 0; i <= cli_count; i++)
            {
                if ((sockfd = client[i]) > 0)
                {
                    if (FD_ISSET(sockfd, &rset))
                    {
                        memset(recv_buff, 0, BUFFER_SIZE);
                        ssize_t n = read(sockfd, recv_buff, BUFFER_SIZE);
                        if (n < 0)
                        {
                            perror("read\t");
                            close(sockfd);
                            FD_CLR(sockfd, &allset);
                            client[i] = -1;
                            continue;
                        }
                        if (n == 0)
                        {
                            printf("SockFD %d left!\n", sockfd);
                            close(sockfd);
                            FD_CLR(sockfd, &allset);
                            client[i] = -1;
                            continue;
                        }

                        printf("%s < %d\n", recv_buff, sockfd);
                        for (int j = 0; j < FD_SETSIZE; j++)
                        {
                            if (client[j] > 0 && i != j)
                            {
                                write(client[j], recv_buff, strlen(recv_buff));
                            }
                        }

                        if (strcmp(recv_buff, "exit") == 0)
                        {
                            close(sockfd);
                            FD_CLR(sockfd, &allset);
                            client[i] = -1;
                            continue;
                        }
                        if (strcmp(recv_buff, "report") == 0)
                        {
                            //pthread_create(&ptd, NULL, report_handler, NULL);
                            report_flag = 1;
                        }
                    }
                }
            }
        }
    }
    //pthread_join(ptd, NULL);
    // close all FD
    close_all_client();
    return 0;
}
