#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/stat.h>

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t terminate = 0;

void signal_handler(int signum)
{
    if (SIGINT == signum || SIGTERM == signum)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        terminate = 1;
    }
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    int opt;
    
    while (-1 != (opt = getopt(argc, argv, "d"))) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if (-1 == sigaction(SIGINT, &sa, NULL) || -1 == sigaction(SIGTERM, &sa, NULL))
    {
        syslog(LOG_ERR, "sigaction failed: %s", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
    {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(sockfd);
        closelog();
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(sockfd);
        closelog();
        exit(EXIT_FAILURE);
    }

    if (-1 == listen(sockfd, BACKLOG))
    {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(sockfd);
        closelog();
        exit(EXIT_FAILURE);
    }
    
    if (daemon_mode) {
        pid_t pid = fork();
        if (-1 == pid) {
            syslog(LOG_ERR, "fork failed: %s", strerror(errno));
            close(sockfd);
            closelog();
            exit(EXIT_FAILURE);
        }
        if (0 < pid) {
            exit(EXIT_SUCCESS);
        }
        if (-1 == setsid()) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        }
        if (-1 == chdir("/")) {
            syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        }
        int dev_null = open("/dev/null", O_RDWR);
        if (-1 != dev_null) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
        }
    }
    
    struct sockaddr_in client_addr;
    char client_ip[INET_ADDRSTRLEN], recv_buffer[BUFFER_SIZE], file_buffer[BUFFER_SIZE];
    while (!terminate)
    {
        socklen_t addr_size = sizeof(client_addr);
        int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (-1 == clientfd)
        {
            if (terminate)
            {
                break;
            }
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }
        if (NULL == inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)))
        {
            strncpy(client_ip, "unknown", sizeof(client_ip));
        }
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char *acc = NULL;
        size_t acc_size = 0;
        ssize_t recv_bytes;
        while (0 < (recv_bytes = recv(clientfd, recv_buffer, BUFFER_SIZE, 0)))
        {
            char *temp = realloc(acc, acc_size + recv_bytes);
            if (NULL == temp)
            {
                syslog(LOG_ERR, "malloc/realloc failed: %s", strerror(errno));
                close(clientfd);
                free(acc);
                break;
            }
            acc = temp;
            memcpy(acc + acc_size, recv_buffer, recv_bytes);
            acc_size += recv_bytes;

            char *newline_ptr;
            while (NULL != (newline_ptr = memchr(acc, '\n', acc_size)))
            {
                size_t line_length = newline_ptr - acc + 1;
                int fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (-1 == fd)
                {
                    syslog(LOG_ERR, "open file failed: %s", strerror(errno));
                    break;
                }
                if (-1 == write(fd, acc, line_length))
                {
                    syslog(LOG_ERR, "write to file failed: %s", strerror(errno));
                    close(fd);
                    break;
                }
                close(fd);

                int fd_read = open("/var/tmp/aesdsocketdata", O_RDONLY);
                if (-1 == fd_read)
                {
                    syslog(LOG_ERR, "open file for reading failed: %s", strerror(errno));
                    break;
                }
                ssize_t read_bytes;
                while (0 < (read_bytes = read(fd_read, file_buffer, BUFFER_SIZE)))
                {
                    ssize_t sent_total = 0;
                    while (sent_total < read_bytes)
                    {
                        ssize_t sent = send(clientfd, file_buffer + sent_total,
                                            read_bytes - sent_total, 0);
                        if (-1 == sent)
                        {
                            syslog(LOG_ERR, "send failed: %s", strerror(errno));
                            break;
                        }
                        sent_total += sent;
                    }
                    if (sent_total < read_bytes)
                    {
                        break;
                    }
                }
                close(fd_read);

                size_t remaining = acc_size - line_length;
                memmove(acc, acc + line_length, remaining);
                acc_size = remaining;

                if (acc_size == 0) {
                    free(acc);
                    acc = NULL;
                } else {
                    char *new_acc = realloc(acc, acc_size);
                    if (new_acc == NULL) {
                        syslog(LOG_ERR, "realloc failed: %s", strerror(errno));
                        free(acc);
                        acc = NULL;
                        /* Optionally handle the error (e.g., break out of the loop) */
                    } else {
                        acc = new_acc;
                    }
                }

            }
        }
        close(clientfd);
        free(acc);
        syslog(LOG_INFO, "closed connection from %s", client_ip);
    }
    close(sockfd);
    if (-1 == remove("/var/tmp/aesdsocketdata"))
    {
        syslog(LOG_ERR, "failed to remove /var/tmp/aesdsocketdata: %s", strerror(errno));
    }
    closelog();
    return EXIT_SUCCESS;
}
