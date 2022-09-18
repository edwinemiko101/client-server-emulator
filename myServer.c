#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>

int users = 0, ftpConnectionOpen = 0;
void input(int, char *);
void child(int);

int main(int argc, char *argv[])
{
    int fd, pn, myClient, pStatus;
    struct sockaddr_in servAdd; // client socket address

    // now we are creating a socket and waiting for client to connect
    if (argc != 2)
    {
        printf("Call model: %s <Port Number>\n", argv[0]);
        exit(0);
    }
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Unable to generate socket\n");
        exit(1);
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    sscanf(argv[1], "%d", &pn);
    servAdd.sin_port = htons((uint16_t)pn);

    bind(fd, (struct sockaddr *)&servAdd, sizeof(servAdd));
    listen(fd, 5);

    while (1)
    {
        printf("Ready to accept client connection!!! \n");
        myClient = accept(fd, NULL, NULL);
        printf("Client connected.\n");

        // creating a child process
        if (!fork())
            child(myClient);

        close(myClient);

        waitpid(0, &pStatus, WNOHANG);
    }
}

void input(int fd, char *message)
{
    write(fd, message, strlen(message));
}

// child process. the child process will talk to the client
void child(int fd)
{
    char message[255];
    int i, j, pid, rnfrFile = 0;
    char *command, *tempmyfifo;
    char *parameter = malloc(100 * sizeof(char));
    char myfifo[50], rnfr[PATH_MAX];

    // if connection successful print success and enter a loop
    input(fd, "Connected! Please proceed.");
    while (1)
        if ((i = read(fd, message, 255))) // read from socket
        {

            message[i] = '\0';
            printf("Request received From Client: %s\n", message);
            command = strtok(message, " \n\0"); // accepting all parameters
            j = 0;
            parameter = strtok(NULL, " \n\0"); // "
            j = 1;

            if (strcmp(command, "USER") == 0)
            {
                users = 1; // user is logged in
                input(fd, "Logged in, continue.");
            }

            else if (strcmp(command, "CWD") == 0) // cwd will change current working directory
            {
                if (j != 1)
                {
                    input(fd, "Syntax error in parameters.");
                    continue;
                }
                if (chdir(parameter) == 0)
                    input(fd, "200 Working directory changed");
                else
                    input(fd, "550 No action done.");
            }

            else if (strcmp(command, "CDUP") == 0) // CDUP will move to the current working directory's parent directory
            {
                if (chdir("..") == 0)
                    input(fd, "200 Working directory changed");
                else
                    input(fd, "550 No action done.");
            }

            else if (strcmp(command, "REIN") == 0) // REIN restarts FTP connection
            {
                unlink(myfifo); // delete namedpipe
                users = 0;      // log out user
                ftpConnectionOpen = 0;
            }

            else if (strcmp(command, "QUIT") == 0) // close FTP connection
            {
                input(fd, "221 Service closing connection. Logged out.");
                close(fd);
                exit(0);
            }

            else if (strcmp(command, "PORT") == 0) // PORT technically specify host and port for data transfer. PORT will also create a namedpipe
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                strcpy(myfifo, "/tmp/");

                strcat(myfifo, parameter); // parameter has file name

                unlink(myfifo);
                if (mkfifo(myfifo, 0777) != 0)
                {
                    input(fd, "425 Cannot open connection.");
                    continue;
                }
                chmod("myfifo", 0777);
                ftpConnectionOpen = 1;
                input(fd, "200 Command done.");
            }

            else if (strcmp(command, "RETR") == 0) // RETR retrieves/downloads files
            {
                int fd1, fd2;
                char buffer[100];
                long int i1;
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if ((fd1 = open(myfifo, O_WRONLY)) == -1) // open in write only mode
                {
                    input(fd, "425 Can't open connection.");
                    continue;
                }
                if ((fd2 = open(parameter, O_RDONLY)) == -1) // open in read only mode
                {
                    input(fd, "550 Unable to execute requested action. File unavailable (file not found / no access).");
                    continue;
                }

                input(fd, "125 connection already open; transfering...");

                // now while reading from file and writing to fifo...
                if (!fork())
                {
                    while ((i1 = read(fd2, buffer, 100)) > 0) // read from file (parameter)
                    {

                        if (write(fd1, buffer, i1) != i1) // write to fifo
                        {
                            input(fd, "552 Action aborted. Cannot write");
                            exit(0);
                        }
                    }
                    if (i1 == -1)
                    {
                        input(fd, "552 Action aborted. cannot read");
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                    input(fd, "250 Action valid, completed.");
                    exit(0);
                }
                close(fd1);
                close(fd2);
            }

            else if (strcmp(command, "STOR") == 0) // STOR will store or replace files on server
            {
                int fd1, fd2;
                char buffer[100];
                long int i1;
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if ((fd1 = open(myfifo, O_RDONLY)) == -1) // open in read only
                {
                    input(fd, "425 Cannot open connection.");
                    continue;
                }
                if ((fd2 = open(parameter, O_CREAT | O_WRONLY | O_TRUNC, 0700)) == -1) // difference betwween STOR & APPE = TRUNC & STOR will replace a file, APPE will append a file.
                {
                    input(fd, "550 Unable to execute requested action. Cannot create file.");
                    continue;
                }
                if (!fork())
                {
                    input(fd, "125 connection already open; transfering...");
                    while ((i1 = read(fd1, buffer, 100)) > 0) // read from fifo
                    {
                        if (write(fd2, buffer, i1) != i1) // write to file
                        {
                            input(fd, "552 Action aborted. Cannot write to file.");
                            close(fd1);
                            close(fd2);
                            exit(0);
                        }
                    }
                    if (i1 == -1)
                    {
                        input(fd, "552 Action aborted. Cannott read from Pipe.");
                        close(fd1);
                        close(fd2);
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                    input(fd, "250 Action valid, completed.");
                    exit(0);
                }
                close(fd1);
                close(fd2);
            }

            else if (strcmp(command, "APPE") == 0) // append file to an existing file on the server
            {
                int fd1, fd2;
                char buffer[100];
                long int i1;
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if ((fd1 = open(myfifo, O_RDONLY)) == -1) // open in read only
                {
                    input(fd, "425 Cannot open connection.");
                    continue;
                }
                if ((fd2 = open(parameter, O_CREAT | O_WRONLY | O_APPEND, 0700)) == -1) // open in write only
                {
                    input(fd, "550 No action done. Cannot create file.");
                    continue;
                }
                if (!fork())
                {
                    input(fd, "125 connection already open; transfering...");
                    while ((i1 = read(fd1, buffer, 100)) > 0) // read from fifo
                    {
                        if (write(fd2, buffer, i1) != i1) // write to file
                        {
                            input(fd, "552 Action aborted. Cannot write to file.");
                            close(fd1);
                            close(fd2);
                            exit(0);
                        }
                    }
                    if (i1 == -1)
                    {
                        input(fd, "552 Action aborted. Cannot read from Pipe.");
                        close(fd1);
                        close(fd2);
                        exit(0);
                    }
                    close(fd1);
                    close(fd2);
                    input(fd, "250 Action valid, completed.");
                    exit(0);
                }
                close(fd1);
                close(fd2);
            }

            else if (strcmp(command, "REST") == 0) // REST will restart file transfer at a specified marker
            {
            }

            else if (strcmp(command, "RNFR") == 0) // RNFR (rename file from) renames files on the server
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                strcpy(rnfr, parameter); // rnfr will send name of file
                rnfrFile = 1;            // and save to variable rnfrFile
                input(fd, "200 Command okay.");
            }

            else if (!strcmp(command, "RNTO")) // RNTO (rename file to)
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if (!rnfrFile)
                {
                    input(fd, "503 wrong commands.");
                    continue;
                }
                if (rename(rnfr, parameter) == 0)
                    input(fd, "250 Action valid, completed.");
                else
                    input(fd, "553 No Action.");
            }

            else if (strcmp(command, "ABOR") == 0) // abort file transfer
            {
            }

            else if (strcmp(command, "DELE") == 0) // delete file from server
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if (remove(parameter) == 0) // delete file
                    input(fd, "250 Action valid, completed.");
                else
                    input(fd, "550 No Action.");
            }

            else if (strcmp(command, "RMD") == 0) // remove a directory on the server
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if (rmdir(parameter) == 0) // remove directory/file
                    input(fd, "250 Action valid, completed.");
                else
                    input(fd, "550 No Action.");
            }

            else if (strcmp(command, "MKD") == 0) // make a new directory on the server
            {
                if (j != 1)
                {
                    input(fd, "501 Syntax error in parameters or arguments.");
                    continue;
                }
                if (mkdir(parameter, 0700) == 0) // make directory
                    input(fd, "250 Action valid, completed.");
                else
                    input(fd, "550 No Action.");
            }

            else if (strcmp(command, "PWD") == 0) // print working directory
            {
                char *buff = malloc(PATH_MAX);
                input(fd, getcwd(buff, PATH_MAX)); // getcwd gets current working directory
            }

            else if (strcmp(command, "LIST") == 0) // list all files in working directory
            {
                if (!users) // no users print this
                {
                    input(fd, "530 You are not logged in.");
                    continue;
                }
                if (!ftpConnectionOpen) // no connection print this
                {
                    input(fd, "425 Can't open connection.");
                    continue;
                }
                input(fd, "150 File status okay; opening connection.\n");
                input(fd, "125 connection already open; transfering...\n");
                DIR *file;
                struct dirent *dir;
                file = opendir(".");
                if (file)
                {
                    while ((dir = readdir(file)) != NULL) // navigating through all the folders and printing the name
                    {
                        input(fd, dir->d_name);
                        input(fd, "\n");
                    }
                    closedir(file);
                }
                input(fd, "250 Action valid, completed.");
            }

            else if (strcmp(command, "STAT") == 0) // status of file
            {
            }

            else if (strcmp(command, "NOOP") == 0) // this command does nothing
            {
                input(fd, "200 Command done");
            }

            else
            {
                input(fd, "502 invalid command.");
            }
        }
}