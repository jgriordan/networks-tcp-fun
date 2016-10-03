all:	myftp myftpd

myftp:	myftp.c
	gcc myftp.c -o myftp

myftpd:	myftpd.c
	gcc myftpd.c -o myftpd

clean:
	rm myftp myftpd
