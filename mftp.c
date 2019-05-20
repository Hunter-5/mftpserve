/* Client process for mftp. Establishes a data connection with a server (as specified
 * in the command line). The user can view and transfer files accross the server file
 * system using the following commands:
 *
 * cd <pathname> - Changes client cwd to the specified path.
 * rcd <pathname> - Changes the server cwd.
 * ls - List client's cwd.
 * rls - List server's cwd.
 * get <pathname> - Tells the server to transfer the specified files contents
 * 	to the client.
 * show <pathname> - Displays file contents from server on standard output.
 * put <pathname> - Sends the contents of a file to the server. */

#include "mftp.h"

int main(int argc, char *argv[]){
	
	if (argc != 2){
		fprintf(stderr, "Usage: ./mftp [-d] <hostname | IP address>\n");
		exit(1);
	}

	int socketfd, bytesRead; // bytesRead when reading commands from server.
	// Socket structure to establish connection with server.
	struct sockaddr_in servAddr;
	struct hostent* hostEntry;
	struct in_addr **pptr;
	char *buff;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(MY_PORT_NUMBER);
	
	if ((hostEntry = gethostbyname(argv[1])) == NULL){
		herror("Error: ");
		exit(1);
	}

	pptr = (struct in_addr **) hostEntry->h_addr_list;
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

	connect(socketfd, (struct sockaddr *) &servAddr,
				sizeof(servAddr));

	printf("Connected to server %s\n", argv[1]);

	buff = (char *) malloc(sizeof(char) * 200);

	while (1){
		// control[]: Server control commands translated from client.
		char control[500];
		int childPID; // child-Pid for command processes. 
		char *token[2]; // Parts of user command (1 = command; 2 = argument).
		
		printf("MFTP> "); // Prompt.
		fgets(buff, 300, stdin);
	
		if ((token[0] = strtok(buff, " \n\t\v\f\r")) == 0){ // Server command.
			continue;
		}
		token[1] = strtok(NULL, " \n\t\v\f\r"); // Command argument.
		
		if (strcmp(token[0], "rcd") == 0){ // Change server dir.
			if (token[1] == NULL){ // Ensure there are is 1 argument.i
				printf("Command error: expecting a parameter.\n");
				continue;
			}
			sprintf(control, "C%s\n", token[1]);
			if (write(socketfd, control, strlen(token[1]) + 2) == -1){
				fprintf(stderr, "Error: writing to server - error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}
			responseReader(socketfd);
		} else if (strcmp(token[0], "rls") == 0){ // Display server dir.

			int dconnectionfd = createDataConnection(socketfd, argv[1]); // Establish data connection.

			if (write(socketfd, "L\n", 2) == -1){
				fprintf(stderr, "Error: writing to server - error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}

			if (responseReader(socketfd) == -1){ // Check for response, if error, stop command operation.
				continue;
			}
			if ((childPID = fork()) < 0){
				fprintf(stderr, "Error: error using fork() -- error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}

			if (childPID == 0){
				dup2(dconnectionfd, 0);
				close(dconnectionfd);
				close(socketfd);
				execlp("/bin/more", "more", "-20", (char *) 0);
			}

			close(dconnectionfd);
			wait(NULL);
		} else if (strcmp(token[0], "ls") == 0){ // List client dir.

			if ((childPID = fork()) < 0){
				fprintf(stderr, "Error: error using fork() -- error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}

			if (childPID == 0){
				int fd[2];
				int child2PID;
				if (pipe(fd) < 0){
					fprintf(stderr, "Error: error using pipe() - error# %d -- %s\n", errno, strerror(errno));
					exit(1);
				}
				close(socketfd);
				if ((child2PID = fork()) < 0){
					fprintf(stderr, "Error: error using fork() -- error# %d -- %s\n", errno, strerror(errno));
					exit(1);
				}
				if (child2PID == 0){
					close(fd[0]);
					dup2(fd[1], 1);
					close(fd[1]);
					execlp("/bin/ls", "ls", "-l", (char *) 0);
				} else{
					close(fd[1]);
					dup2(fd[0], 0);
					close(fd[0]);
					execlp("/bin/more", "more", "-20", (char *) 0);
				}
			}

			wait(NULL);
		} else if (strcmp(token[0], "cd") == 0){ // Change client dir.
			
				if (token[1] == NULL){
				printf("Command error: expecting a parameter.\n");
				continue;
			}
			if (token[1] == NULL){
				printf("Command error: expecting a parameter.\n");
			} else if (chdir(token[1]) == -1){
				fprintf(stderr, "Error: Changing directories - error# %d -- %s\n", errno, strerror(errno));
			}

			continue;
		} else if (strcmp(token[0], "get") == 0){ // Get file from server.
	
			int fd; // File descriptor for file to write to.
			char fileBuff;
			char *filename, *prev;
			if (token[1] == NULL){
				printf("Command error: expecting a parameter.\n");
				continue;
			}

			filename = strtok(token[1], "/");
			
			do { // Get filename from path.
				prev = filename;
			} while ((filename = strtok(NULL, "/")) != NULL);	
			
			// Check if file exists, if not create it.
			if (open(prev, O_WRONLY, 0666) > 0){
				printf("Open/creating local file: File exists\n");
				continue; // End operation if file cannot be opened.
			}
	
			int dconnectionfd = createDataConnection(socketfd, argv[1]);
			sprintf(control, "G%s\n", token[1]);	
			
			if (write(socketfd, control, strlen(token[1])+2) == -1){
				fprintf(stderr, "Error: writing to server - error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}
			// If an error is returned, ignore command.
			if (responseReader(socketfd) == -1){
				continue;
			}
			// Create specified file for writing.
			if ((fd = open(prev, O_CREAT | O_APPEND | O_WRONLY | O_EXCL, 0666)) < 0){
				fprintf(stderr, "Open/creating local file: %s\n", strerror(errno));
				close(fd);
				continue; // End operation if file cannot be opened.
			}
			while (read(dconnectionfd, &fileBuff, 1) > 0){
				write(fd, &fileBuff, 1);
			}

			close(fd);
			close(dconnectionfd);
		} else if (strcmp(token[0], "put") == 0){ // Puts file onto server.
			
			if (token[1] == NULL){
				printf("Command error: expecting a parameter.\n");
				continue;
			}

			struct stat area, *s = &area;
			char readBuff;
			int fd, readEr; // File descriptor for reading, and readEr for tracking read() errors.
			// If file can't be opened, ignore command.
			if ((fd = open(token[1], O_RDONLY)) < 0){
				fprintf(stderr, "Opening file for reading: %s\n", strerror(errno));
			} else if (stat((token[1]), s) == 0){ // Check if file is dir.
				if (S_ISDIR (s->st_mode)){
					printf("Local file is a directory, command ignored.\n");
					close(fd);	
					continue;
				} else {
					int dconnectionfd = createDataConnection(socketfd, argv[1]);
					sprintf(control, "P%s\n", token[1]);
					if (write(socketfd, control, strlen(token[1])+2) == -1){
						fprintf(stderr, "Error: writing to server - error# %d -- %s\n", errno, strerror(errno));
						exit(1);
					}
					// Check for server response, then begin writing the file.
					if (responseReader(socketfd) == -1){
						close(dconnectionfd);
						continue;
					}	

					while ((readEr = read(fd, &readBuff, 1)) != 0){
						if (readEr < 0){
							fprintf(stderr, "Error: reading file -- %s\n", strerror(errno));
							break;
						}
						if (write(dconnectionfd, &readBuff, 1) < 0){
							fprintf(stderr, "Error: writing file -- %s\n", strerror(errno));
							break;
						}
					}
					close(fd);
					close(dconnectionfd);
				}
			}
		} else if (strcmp(token[0], "show") == 0){ // Show contents of server file on standard output.

			if (token[1] == NULL){
				printf("Command error: expecting a parameter.\n");
				continue;
			}

			int dconnectionfd = createDataConnection(socketfd, argv[1]);
			
			sprintf(control, "G%s\n", token[1]);
			write(socketfd, control, strlen(token[1])+2);
			
			if (responseReader(socketfd) == -1){
				close(dconnectionfd);
				continue;
			}	
			if ((childPID = fork()) < 0){
				fprintf(stderr, "Error: error using fork() -- error# %d -- %s\n", errno, strerror(errno));
				exit(1);
			}
			if (childPID == 0){
				dup2(dconnectionfd, 0);
				close(dconnectionfd);
				execlp("/bin/more", "more", "-20", (char *) 0);
			}

			close(dconnectionfd);
			wait(NULL);
		} else if (strcmp(token[0], "exit") == 0){
			// Exit process.
			write(socketfd, "Q\n", 2);
			break;
		} else {
			printf("Command \'%s\' is unknown - ignored\n", token[0]);
			continue;
		}
	}

	close(socketfd);
	return 0;
}
// Handles server response (A or E).
int responseReader(int socketfd){	
	// responseBuff: Stores server response, response: For reading server response,
	char responseBuff[100], response; 
	int responseCount = 0;  // Response counter.

	while (responseCount += read(socketfd, &response, 1) == 1){
		if (response == '\n'){
			break;
		}
		responseBuff[responseCount - 1] = response;
	}
	responseBuff[responseCount - 1] = '\0';
	if (responseBuff[0] == 'E'){
		fprintf(stderr, "Error response from server: %s\n", responseBuff + 1);
		fflush(stdout);
		return -1;
	} else if (responseBuff[0] != 'A'){
		// Incase server sends weird response.
		fprintf(stderr, "Server Error: Unknown server response. \n");
		fflush(stdout);
		return -1;
	}
	return 0;
}

int createDataConnection(int socketDescriptor, char *hostname){
	//add error return value.
	char *connectionCom = "D\n";
	char portNumList[20];
	char charRead;
	int port;
	write(socketDescriptor, connectionCom, 2);
	int reading = 0;
	while (reading += read(socketDescriptor, &charRead, 1) == 1){
		if (charRead == '\n'){
			break;
		}
		portNumList[reading-1] = charRead;
	}
	portNumList[reading-1] = '\0';
	port = atoi(portNumList + 1);

	int socketfd, bytesRead;
	struct sockaddr_in servAddr;
	struct hostent* hostEntry;
	struct in_addr **pptr;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(port);
	if ((hostEntry = gethostbyname(hostname)) == NULL){
		herror("Error: ");
		exit(1);
	}
	// TEST ERROR hERROR()
	pptr = (struct in_addr **) hostEntry->h_addr_list;
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
	
	//printf("%s", inet_ntoa(servAddr.sin_addr));


	connect(socketfd, (struct sockaddr *) &servAddr,
				sizeof(servAddr));
	return socketfd;
}
