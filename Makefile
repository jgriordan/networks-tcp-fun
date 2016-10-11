all:	myftp myftpd

myftp:	myftp.c
	gcc myftp.c -o myftp -lmhash

myftpd:	myftpd.c
	gcc myftpd.c -o myftpd -lmhash

clean:
	rm myftp myftpd
