#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 8096
#define FORBIDDEN 403
#define NOTFOUND 404
#define NOTIMPLEMENTED 501

void err_msg(int type, int socket_fd) {
    switch (type)
    {
        case FORBIDDEN:
            write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
            break;
        case NOTFOUND:
            write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
            break;
        case NOTIMPLEMENTED:
            write(socket_fd, "HTTP/1.1 501 Not Implemented\nContent-Length: 151\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>501 Not Implemented</title>\n</head><body>\n<h1>Not Implemented</h1>\nThe requested method was not found on this server.\n</body></html>\n",239);
            break;
        default:
            break;
    }
}


/*
 Handles php requests
 */
// void php_cgi(char* script_path, int fd) {
//     send_new(fd, "HTTP/1.1 200 OK\n Server: Web Server in C\n Connection: close\n");
//     dup2(fd, STDOUT_FILENO);
//     char script[500];
//     strcpy(script, "SCRIPT_FILENAME=");
//     strcat(script, script_path);
//     putenv("GATEWAY_INTERFACE=CGI/1.1");
//     putenv(script);
//     putenv("QUERY_STRING=");
//     putenv("REQUEST_METHOD=GET");
//     putenv("REDIRECT_STATUS=true");
//     putenv("SERVER_PROTOCOL=HTTP/1.1");
//     putenv("REMOTE_HOST=127.0.0.1");
//     execl("/usr/bin/php-cgi", "php-cgi", NULL);
// }


int main(int argc, char const *argv[]) {
    int fd_server, fd_client, j, file_id;
    int on = 1;
    long valRead, i, len, ret;
    struct sockaddr_in server_add;
    int addrlen = sizeof(server_add);
    

    // Creating the socket to access the network
    fd_server = socket(AF_INET, SOCK_STREAM, 0);

    if (fd_server == 0) {
        perror("Cannot create Socket");
        exit(1);
    }

    //Option to reuse the port even in TIME_WAIT state
    setsockopt(fd_server,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(int));

    server_add.sin_family = AF_INET;
    server_add.sin_addr.s_addr = htonl(INADDR_ANY);
    server_add.sin_port = htons(PORT);

    memset(server_add.sin_zero, '\0', sizeof(server_add.sin_zero));

    //Assigning transport address to the socket
    if(bind(fd_server, (struct sockaddr *)&server_add, sizeof(server_add)) < 0) {
        perror("In Bind");
        exit(1);
    }

    //Waiting for an incoming connection
    if (listen(fd_server, 10) < 0) {
        perror("In listen");
        exit(1);
    }

    while(1) {
        printf("\nWAITING FOR NEW CLIENT CONNECTION\n\n");
        if ((fd_client = accept(fd_server, (struct sockaddr *)&server_add, (socklen_t*)&addrlen)) < 0) {
            perror("In Accept");
            exit(1);
        }

        printf("\nGot client connection...\n");

        char buffer[BUFFER_SIZE + 1] = {0};

        valRead = read(fd_client, buffer, 30000);
        
        if (valRead == 0 || valRead == -1) {
            err_msg(FORBIDDEN, fd_client);
        }
        if (valRead > 0 && valRead < BUFFER_SIZE) {
            buffer[valRead] = 0;
        } else {
            buffer[0] = 0;
        }

        // remove CR and LF characters
        for(i = 0; i < valRead; i++)
        {
            if (buffer[i] == '\r' || buffer[i] == '\n') {
                buffer[i] = '*';
            }
        }

        if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
            err_msg(NOTIMPLEMENTED, fd_client);
        }

        // Igonre extra stuff after second space`
        for(i = 4; i < BUFFER_SIZE; i++)
        {
            if (buffer[i] == ' ') {
                buffer[i] = 0;
                break;
            }
        }

        // Check for illegal parent directory use
        for(j = 0; j < i - 1; j++)
        {
            if (buffer[j] == '.' && buffer[j+1] == '.') {
                err_msg(FORBIDDEN, fd_client);
            }     
        }

        if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) {
            strcpy(buffer, "GET /index.html");
        }

        if ((file_id = open(&buffer[5], O_RDONLY)) == -1) {
            err_msg(NOTFOUND, fd_client);
        } 
        
        len = (long) lseek(file_id, (off_t) 0, SEEK_END);
        lseek(file_id, (off_t) 0, SEEK_SET);
        // Header + new line
        sprintf(buffer, "HTTP/1.1 200 OK\nServer: MyServer/1.0\nContent-Length: %ld\nConnection:close\nContent-Type: %s\n\n", len, "text/html");
        write(fd_client, buffer, strlen(buffer));

        // Send file in 8KB blocks
        while((ret = read(file_id, buffer, BUFFER_SIZE)) > 0){
            write(fd_client, buffer, ret);
        }
        
        printf("------------Response Sent----------------\n");
        close(fd_client);
    }   
    return 0;
}
