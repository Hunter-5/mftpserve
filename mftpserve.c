/* Creator: William Purviance	william.purviance@wsu.edu	-4/28
 *
 * Description: Server daemon that allows clients to connect via the TCP/IP sockets interface library
 * to view and transfer files across systems. The server can recieve 5 commands: 
 * D - Establishes a data connection for transferring file content.
 * C <pathname> - Changes directories to the specified path.
 * L - Lists host directories on client output.
 * G <pathname> - Gets a file and routes its contents to the client.
 * P <pathname> - Tells the host to listen for an incoming file to store on the server.
 * Q - Notifies the host that a client is exiting. */

#include <sys/types.h>
#include <time.h>
#include "mftp.h"

#define COMM_MAX 300 // Max size for command (max command line arg.).

int main(){
	// Server socket to listen on, and client socket, respectively.
	struct sockaddr_in servAddr, clientAddr;
	/* listenfd listens for a connection, childPID is used
	 * to keep track of the client processes, connectfd is 
	 * the connection to route data on. */
	int listenfd, childPID, connectfd,
	    length, status, pidEr; // pidEr checks for error when waiting for child processes.
	
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Error: ");
		exit(1);
	}

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(MY_PORT_NUMBER);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ( bind( listenfd,
		(struct sockaddr *) &servAddr, sizeof(servAddr)) < 0 ){
			perror("bind");
			exit(1);
	}
	listen(listenfd, 4);
	length = sizeof(struct sockaddr_in);
	
	while (1){ // Server loop.
	
		connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);
		
		if ((childPID = fork()) < 0){
			fprintf(stderr, "Error: fork() failed - error#%d -- %s", errno, strerror(errno));
			fflush(stderr);
		}
		
		if (childPID == 0){ // Client process.
	
			char command;
			struct hostent* hostEntry;
			char* hostName;
			int listendatafd; // Listens for the data connection.

			if ((hostEntry = gethostbyaddr(&(clientAddr.sin_addr),
					sizeof(struct in_addr), AF_INET)) == NULL){
				herror("Error: ");
				exit(1);
			}	

			hostName = hostEntry->h_name;

			printf("Child %d: Client hostname -> %s\n", getpid(), hostName);
			printf("Child %d: Client IP address -> %s\n", getpid(), inet_ntoa(clientAddr.sin_addr));
			printf("Child %d: Connection accepted from host %s\n", getpid(), inet_ntoa(servAddr.sin_addr));
			fflush(stdout);

			int portNum = 0; //port number to listen on.
			
			while (1){
				
				int childPID2, datafd, bread = 0, // bread - bytes read (for read()).
					listendatafd, fd;
				// Reads user input byte-by-byte into these buffers, comm stores input.
				char testcomm, readBuff, comm[COMM_MAX];
				// Read user input into buffer.
				while (bread += read(connectfd, &testcomm, 1) == 1){
					if (testcomm == '\n'){
						break;
					}
					comm[bread-1] = testcomm;
				}
				comm[bread-1] = '\0';
				// Change dir.
				if (comm[0] == 'C'){

					if (chdir(comm + 1) == -1){
						// Send error message to client.
						fprintf(stderr, "Child %d: cd to %s failed with error -- %s\n", getpid(), comm+1, strerror(errno));
						fflush(stderr);
						write(connectfd, "E", 1);
						write(connectfd, strerror(errno), strlen(strerror(errno)));
						write(connectfd, "\n", 1);
					} else {
						// Send acknowledgment to client.
						printf("Child %d: Changed current directory to %s\n", getpid(), comm+1);
						fflush(stdout);
						write(connectfd, "A\n", 2);
					}
				// List dir.
				} else if (comm[0] == 'L'){
					
					datafd = accept(listendatafd, (struct sockaddr *) &clientAddr, &length);
					
					if ((childPID2 = fork()) < 0){
						fprintf(stderr, "Error: fork() failed - error#%d -- %s", errno, strerror(errno));
						fflush(stderr);
						exit(1);
					}
					if (childPID2 == 0){
						dup2(datafd, 1);
						close(datafd);
						close(connectfd);
						close(listenfd);
						execlp("/bin/ls", "ls", "-l", (char *) 0);
					} else{
						close(datafd);
						wait(NULL);
					}

					write(connectfd, "A\n", 2);
				// Get file.
				} else if (comm[0] == 'G'){
					
					struct stat area, *s = &area; // For checking if file is directory (stat()).
					
					datafd = accept(listendatafd, (struct sockaddr *) &clientAddr, &length);
					printf("Child %d: Reading file %s\n", getpid(), (comm+1));
					fflush(stdout);
					
					if ((fd = open(comm+1, O_RDONLY )) < 0){
						write(connectfd, "E", 1);
						write(connectfd, strerror(errno), strlen(strerror(errno))*sizeof(char));
						write(connectfd, "\n", 1);
					} else if (stat((comm+1), s) == 0){ // Check if file is a directory.
						if (S_ISDIR (s->st_mode)){
							write(connectfd, "E", 1);
							write(connectfd, "File is a directory", 19);
							write(connectfd, "\n", 1);
						} else { // Clear to read file.
							write(connectfd, "A\n", 2);
							int readEr, writeErr; // tests for fd errors.
							printf("Child %d: transmitting file %s to client\n", getpid(), (comm+1));
							fflush(stdout);
							while ((readEr = read(fd, &readBuff, 1)) != 0){
								if (readEr < 0){
										fprintf(stderr, "Error: reading file %s -- %s\n", comm+1, strerror(errno));
										fflush(stderr);
										break;
								}
								if ((writeErr = write(datafd, &readBuff, 1)) < 0){
									break;
								}
							}
						}
					}

					close(fd);
					close(datafd);
				// Puts file into client dir.
				} else if (comm[0] == 'P'){
					// Establish data connection.
					datafd = accept(listendatafd, (struct sockaddr *) &clientAddr, &length);
					
					char fileBuff;
					char *filename, *prev;

					filename = strtok(comm+1, "/");
					do {
						prev = filename;
					} while ((filename = strtok(NULL, "/")) != NULL);

					printf("Child %d: Writing file %s\n", getpid(), prev);
					fflush(stdout);

					if ((fd = open(prev, O_CREAT | O_APPEND | O_WRONLY | O_EXCL, 0666)) < 0){
						write(connectfd, "E", 1);
						write(connectfd, strerror(errno), strlen(strerror(errno))*sizeof(char));
						write(connectfd, "\n", 1);
					} else {
						write(connectfd, "A\n", 2);
						printf("Child %d: receiving file %s from client\n", getpid(), prev);
						fflush(stdout);
						while (read(datafd, &fileBuff, 1) > 0){
							write(fd, &fileBuff, 1);
						}
					}

					close(fd);
					close(datafd);
				// Establish data connection with client.
				} else if (comm[0] == 'D'){
					
					struct sockaddr_in dataServAddr;
					struct sockaddr_in emptyServAddr;
					int namelength; // Size of sockaddr for connection.
        			char buff[8]; // Buff for sending server response to client.

					if ((listendatafd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                			perror("Error: ");
                			exit(1);
        			}
					
					memset(&dataServAddr, 0, sizeof(dataServAddr));
					dataServAddr.sin_family = AF_INET;
					dataServAddr.sin_port = htons(0);
					dataServAddr.sin_addr.s_addr = htonl(INADDR_ANY);

					if ( bind( listendatafd,
						(struct sockaddr *) &dataServAddr, sizeof(dataServAddr)) < 0){
						perror("bind");
						exit(1);
					}

					listen(listendatafd, 4);

					memset(&emptyServAddr, 0, sizeof(emptyServAddr));
					namelength = sizeof(struct sockaddr_in);
					getsockname(listendatafd, (struct sockaddr *) &emptyServAddr, &namelength);
					portNum = ntohs(emptyServAddr.sin_port);
					
					sprintf(buff, "A%d\n", portNum);
					write(connectfd, buff, 7); 
				} else if (comm[0] == 'Q'){ // Client is quitting.
					
					printf("Child %d: Quitting\n", getpid());
					write(connectfd, "A\n", 2);
					break;
				}
			}

			close(connectfd);
			close(listenfd);
			return 0;
		} else{
			
			while ((pidEr = waitpid(childPID, &status, WNOHANG)) > 0){ // We don't like zombies.
				if (pidEr == -1){
					fprintf(stderr, "Error: waitpid() - %s", strerror(errno));
				}
			}
			close(connectfd);
		}
	}

	return 0;
}
