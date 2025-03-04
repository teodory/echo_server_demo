#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
char stopWork[20] = "\n";

int main(int argc, char *argv[])
{
    int sockfd, portno, n, input;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("CLIENT ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"CLIENT ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        error("CLIENT ERROR connecting");
    } 
    
    // bzero(buffer, 256);
    // n = read(sockfd, buffer, 255);
    // printf("%s\n", buffer);

    while (true)
    {
        bzero(buffer, 256);
        n = read(sockfd, buffer, 255);
        // if (n < 0) 
        //     error("CLIENT ERROR reading from socket");
        printf("%s", buffer);

        printf("Please enter the message: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        if (strlen(buffer) > 0)
        {   
            n = write(sockfd, buffer, strlen(buffer));
            if (n < 0)
            {
                printf("n < 0");
                // error("CLIENT ERROR writing to socket");
                // close(sockfd);
            }            
            if (strcmp(buffer, stopWork) == 0)
            {
                printf("bye!\n");
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}
