
all : mftp mftpserve

mftp : mftp.o
	gcc mftp.o -o mftp

mftp.o : mftp.c
	gcc -c mftp.c

mftpserve : mftpserve.o
	gcc mftpserve.o -o mftpserve

mftpserve.o : mftpserve.c
	gcc -c mftpserve.c

clean :
	rm mftp mftp.o mftpserve mftpserve.o
