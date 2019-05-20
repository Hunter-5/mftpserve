# mftpserve
CS 360 project. A simple file transfer system written in C.

Creator: William Purviance	william.purviance@wsu.edu	-4/28

Description: Project contains a server daemon and a client. The server allows clients to 
  connect via the TCP/IP sockets interface library to view and transfer files across systems.
  The server can recieve 5 commands:
  D - Establishes a data connection for transferring file content.
  C <pathname> - Changes directories to the specified path.
  L - Lists host directories on client output.
  G <pathname> - Gets a file and routes its contents to the client.
  P <pathname> - Tells the host to listen for an incoming file to store on the server.
  Q - Notifies the host that a client is exiting.
  Client commands:
  cd <pathname> - Changes client cwd to the specified path.
  rcd <pathname> - Changes the server cwd.
  ls - List client's cwd.
  rls - List server's cwd.
  get <pathname> - Tells the server to transfer the specified files content to the client.
  show <pathname> - Displays file contents from server on standard output.
  put <pathname> - Sends the contents of a file to the server.
  
Included files:
  mftpserve.c - Contains server code.
  mftp.c - Contains client code.
  mftp.h - Header file for both programs.
  Makefile

How to run:
  Run make in the directory containing the required files, this produces
  an executable for the client and server respectively.
