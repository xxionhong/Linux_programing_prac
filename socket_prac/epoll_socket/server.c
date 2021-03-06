#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include "singlylinkedlist.c"

#define CONNECT_SIZE 256
#define PORT 12345
#define BUFFER_SIZE 2048
int listenfd, flag = 0, pipe_fd[2];
pid_t pid;
Node *head = NULL;

// using fcntl to set FD nonblocking
void setNonblocking(int fd)
{
    int opts;
    opts = fcntl(fd, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(sock,GETFL)");
        return;
    }

    opts = opts | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opts) < 0)
    {
        perror("fcntl(sock,SETFL,opts)");
        return;
    }
}
void close_linkedlist()
{
    Node *temp = head;
    while (temp != NULL)
    {
        close(temp->data);
        temp = temp->next;
    }
    freeallNodes(head);
}

void sig_handler(int num)
{
    // pipe to transmit flags!!
    char *text = "exit";
    write(pipe_fd[1], text, sizeof(text));
    // flag = 1;
}

int main(int argc, char **argv)
{
    int i, connfd, sockfd, nfds;
    ssize_t n;
    char recv_buff[BUFFER_SIZE], pipe_buff[5];
    struct sockaddr_in servaddr, cliaddr;
    struct epoll_event ev, events[CONNECT_SIZE];

    if (pipe(pipe_fd)) // o for read, 1 for write
    {
        perror("pipe\t");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sig_handler);
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("listen\t");
        exit(EXIT_FAILURE);
    }
    setNonblocking(listenfd);
    int epfd = epoll_create(CONNECT_SIZE);
    ev.data.fd = listenfd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    ev.data.fd = pipe_fd[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd[0], &ev);

    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind\t");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    if (listen(listenfd, CONNECT_SIZE) == -1)
    {
        perror("listen\t");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    while (!flag)
    { // !!
        nfds = epoll_wait(epfd, events, CONNECT_SIZE, 0);
        if (nfds <= 0)
        {
            continue;
        }
        for (i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == pipe_fd[0])
            {
                read(pipe_fd[0], pipe_buff, 5);
                if (!strcmp(pipe_buff, "exit"))
                {
                    flag = 1;
                    break;
                }
            }
            else if (events[i].data.fd == listenfd)
            {
                socklen_t clilen = sizeof(cliaddr);
                if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) < 0)
                {
                    perror("accept\t");
                    close(connfd);
                    break;
                }
                appendNode(&head, connfd);
                printf("accpet a new client: %d\n", connfd);
                setNonblocking(connfd);
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            // read from client
            else if (events[i].events & EPOLLIN)
            {
                if ((sockfd = events[i].data.fd) < 0)
                    continue;
                memset(recv_buff, 0, BUFFER_SIZE);
                if ((n = read(sockfd, recv_buff, BUFFER_SIZE)) <= 0)
                {
                    printf("sockfd %d left\n", sockfd);
                    close(sockfd);
                    events[i].data.fd = -1;
                    deleteNode(&head, sockfd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, &ev);
                }
                else
                {
                    recv_buff[n] = '\0';
                    printf("%s < %d\n", recv_buff, sockfd);
                    Node *temp = head;
                    while (temp != NULL)
                    {
                        if (temp->data != sockfd)
                        {
                            write(temp->data, recv_buff, n);
                        }
                        temp = temp->next;
                    }
                    if (!strcmp(recv_buff, "exit"))
                    {
                        deleteNode(&head, sockfd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, &ev);
                        close(sockfd);
                        continue;
                    }
                }
            }
        }
    }
    close_linkedlist();
    close(listenfd);
    close(epfd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}