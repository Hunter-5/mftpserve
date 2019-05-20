// Final project - ftp - header file.

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MY_PORT_NUMBER 49999 // Port to listen on.

int responseReader(int socketfd); // Reads response from server after client command sent.
int createDataConnection(int socketDescriptor, char *hostname); // Creates client data connection.
