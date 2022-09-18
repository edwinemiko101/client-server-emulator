#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    int i, j, pn, pid, myServer;
    char message[255], buff[255], myfifo[50];
    char *command, *replyCode;
    char *parameter = malloc(100 * sizeof(char));
    struct sockaddr_in servAdd; // server socket address

    if (argc != 3)
    {
        printf("Call model: %s <IP Address> <Port Number>\n", argv[0]);
        exit(0);
    }

    if ((myServer = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Cannot create socket\n");
        exit(1);
    }

    servAdd.sin_family = AF_INET;
    sscanf(argv[2], "%d", &pn);
    servAdd.sin_port = htons((uint16_t)pn);

    if (inet_pton(AF_INET, argv[1], &servAdd.sin_addr) < 0)
    {
        fprintf(stderr, " inet_pton() has failed\n");
        exit(2);
    }

    if (connect(myServer, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0) // "connect" will connect to myServer
    {
        fprintf(stderr, "connection has failed, exiting\n");
        exit(3);
    }

    read(myServer, message, 255);
    fprintf(stderr, "%s\n", message);

    pid = fork();

    if (pid)
        while (1) // reading server's messages
            if ((i = read(myServer, message, 255)))
            {
                message[i] = '\0';
                fprintf(stderr, "%s\n", message);
                replyCode = strtok(message, " ");
                if (!strcmp(replyCode, "221")) // whenever we get 221 replyCode it will exit
                {
                    close(myServer);
                    exit(0);
                }
            }

    if (!pid) // sending messages to server
        while (1)
        {
            if ((i = read(0, message, 255))) // reading from standard input and writing to server
            {
                message[i] = '\0';
                write(myServer, message, strlen(message) + 1);
                command = strtok(message, " \n\0");
                j = 0;
                parameter = strtok(NULL, " \n\0");
                j = 1;

                if (strcmp(command, "PORT") == 0)
                {

                    if (j != 1)
                    {
                        printf("Syntax error in parameters or arguments.");
                        continue;
                    }
                    strcpy(myfifo, "/tmp/");
                    strcat(myfifo, parameter);
                }
                if (strcmp(command, "RETR") == 0)
                {
                    int fd1, fd2;
                    char buffer[100];
                    long int i1;
                    char *transferStaus;
                    if (j != 1)
                    {
                        printf("Syntax error in parameters or arguments.");
                        continue;
                    }
                    printf("Named Pipr is %s done\n", myfifo);
                    if ((fd1 = open(myfifo, O_RDONLY)) == -1) // open in read only mode
                    {
                        perror("Client Error: Cannot open Named Pipe: ");
                        continue;
                    }
                    if ((fd2 = open(parameter, O_CREAT | O_WRONLY | O_TRUNC, 0700)) == -1)
                    {
                        perror("Client Error: Cannot generate File: ");
                        continue;
                    }
                    if (!fork())
                    {
                        printf("Client: Getting File ...\n");
                        while ((i1 = read(fd1, buffer, 100)) > 0) // read from fifo
                        {
                            if (write(fd2, buffer, i1) != i1) // write to file
                            {
                                perror("Client Error: Error writing to file: ");
                                close(fd1);
                                close(fd2);
                                exit(0);
                            }
                        }
                        if (i1 == -1)
                        {
                            perror("Client Error: Error Reading from Pipe");
                            close(fd1);
                            close(fd2);
                            exit(0);
                        }
                        close(fd1);
                        close(fd2);
                        printf("Client: File Saved.\n");
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                }
                if (strcmp(command, "STOR") == 0)
                {
                    int fd1, fd2;
                    char buffer[100];
                    long int i1;
                    if (j != 1)
                    {
                        printf("Client Error: Syntax error in parameters or arguments.\n");
                        continue;
                    }
                    if ((fd1 = open(myfifo, O_WRONLY)) == -1)
                    {
                        printf("Client Error: Cannot open connection.\n");
                        continue;
                    }
                    if ((fd2 = open(parameter, O_RDONLY)) == -1)
                    {
                        perror("Client Error: File unavailable.");
                        continue;
                    }
                    printf("Client: transfering.");
                    if (!fork())
                    {
                        while ((i1 = read(fd2, buffer, 100)) > 0) // read from file
                        {

                            if (write(fd1, buffer, i1) != i1) // write to fifo
                            {
                                perror("Client Erro: Cannot write");
                                exit(0);
                            }
                        }
                        if (i1 == -1)
                        {
                            perror("Client Error: Cannot read");
                            exit(0);
                        }
                        close(fd1);
                        close(fd2);
                        printf("Client: File Transfer completed.\n");
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                }

                if (strcmp(command, "APPE") == 0)
                {
                    int fd1, fd2;
                    char buffer[100];
                    long int i1;
                    if (j != 1)
                    {
                        printf("Client Error: Syntax error in parameters or arguments.\n");
                        continue;
                    }
                    if ((fd1 = open(myfifo, O_WRONLY)) == -1)
                    {
                        printf("Client Error: Can't open connection.\n");
                        continue;
                    }
                    if ((fd2 = open(parameter, O_RDONLY)) == -1)
                    {
                        perror("Client Error: File unavailable.");
                        continue;
                    }
                    printf("Client: transfering.");
                    if (!fork())
                    {
                        while ((i1 = read(fd2, buffer, 100)) > 0)
                        {

                            if (write(fd1, buffer, i1) != i1)
                            {
                                perror("Client Erro: Cannot write");
                                exit(0);
                            }
                        }
                        if (i1 == -1)
                        {
                            perror("Client Error: Cannot read");
                            exit(0);
                        }
                        close(fd1);
                        close(fd2);
                        printf("Client: File Transfer completed.\n");
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                }

                if (strcmp(command, "QUIT") == 0)
                {
                    close(myServer);
                    exit(0);
                }
            }
        }
}
