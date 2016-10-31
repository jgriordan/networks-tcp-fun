// myftp.c
// John Riordan
// And Nolan
// jriorda2
// jnolan5

// usage: myftp server_name port

#include "myftp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_LINE 256

int main( int argc, char* argv[] ){
	FILE* fp = NULL;
	struct hostent* hp;
	struct sockaddr_in sin;
	char* host;
	char buf[MAX_LINE+1];
	int i, s, len;
	uint16_t port;
	/*struct timeval start, end;*/

	// validate and put command line in variables
	if( argc == 3 ){
		host = argv[1];
		port = atoi(argv[2]);
	} else {
		fprintf( stderr, "usage: myftp server_name port\n" );
		exit( 1 );
	}

	// translate host name into peer's IP address
	hp = gethostbyname( host );
	if( !hp ){
		fprintf( stderr, "myftp: unknown host: %s\n", host );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&sin, sizeof( sin ) );
	sin.sin_family = AF_INET;
	bcopy( hp->h_addr, (char*)&sin.sin_addr, hp->h_length );
	sin.sin_port = htons( port );

	// create the socket
	if( (s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ){
		fprintf( stderr, "myftp: socket creation error\n" );
		exit( 1 );
	}

	// connect the created socket to the remote server
	if( connect( s, (struct sockaddr*)&sin, sizeof(sin) ) < 0 ){
		fprintf( stderr, "myftp: connect error\n" );
		close( s );
		exit( 1 );
	}

	printf( "Enter your operation (XIT to quit): " );
	// main loop
	while( fgets( buf, MAX_LINE, stdin ) ){

		buf[MAX_LINE] = '\0';
		

		if( send( s, buf, 3, 0 ) == -1 ){
			fprintf( stderr, "myftp: send error\n" );
			exit( 1 );
		} else {
		if( !strncmp( buf, "XIT", 3 ) ){
			printf( "Bye bye\n" );
			break;
		}
		handle_action(buf, s);
		}
		printf( "Enter your operation (XIT to quit): " );
	}

	close( s );

	return 0;
}

void handle_action(char* msg, int s) {
	// check for all special 3 char messages
	if (!strncmp("DEL", msg, 3))
		delete_file( s );
	else if (!strncmp("LIS", msg, 3))
		list_dir( s );
	else if (!strncmp("MKD", msg, 3))
		make_dir( s );
	else if (!strncmp("RMD", msg, 3))
		remove_dir( s );
	else if (!strncmp("CHD", msg, 3))
		change_dir( s );
	else if (!strncmp("UPL", msg, 3))
		upload( s );
	else if (!strncmp("REQ", msg, 3))
		request( s );
}

void request( int s ){
	char c;
	char fileName[MAX_LINE];
	char hash[16], hashCmp[16];
	double thrput, upload_time;
	long fileLen;
	FILE* fp;
	char* fileText;
	long recvlen, i;
	struct timeval start, stop;
	MHASH compute;

	printf( "Enter the file name to request: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// send the file name over
	send_instruction( s, fileName );

	// check if file existed on server
	fileLen = receive_result32( s );

	// both are -1
	if( fileLen == -1 || fileLen == 4294967295 ){
		printf( "File does not exist\n" );
		return;
	}

	// empty file case
	if( fileLen == 0 ){
		fp = fopen( fileName, "w+" );
		fclose( fp );
		return;
	}

	// get hash from server
	if( read( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myftp: error receiving md5 hash\n" );
		return;
	}

	// get the file's text from the server
	fileText = malloc( fileLen );
	gettimeofday(&start, NULL);
	for( i = 0; i < fileLen; i += MAX_LINE ){
		recvlen = (fileLen - i < MAX_LINE ) ? fileLen-i : MAX_LINE;
		if( read( s, fileText+i, recvlen ) == -1 ){
			fprintf( stderr, "myftp: error receiving file data\n" );
			free( fileText );
			return;
		}
	}

	gettimeofday(&stop, NULL);

	upload_time = stop.tv_sec - start.tv_sec + ((double)stop.tv_usec)/1000000 - ((double)start.tv_usec)/1000000;

	if (upload_time != 0) {
		thrput = ((double)fileLen)/upload_time;
	} else {
		thrput = 0;
	}
	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myftpd: hash failed\n" );
		free( fileText );
		return;
	}
	
	// create and write the file
	fp = fopen( fileName, "w+" );
	for( i = 0; i < fileLen; i++ ){
		c = fileText[i];
		fputc( c, fp );
		mhash( compute, &c, 1 );
	}
	fclose( fp );
	free( fileText );

	mhash_deinit( compute, hashCmp );
	if( !strncmp( hash, hashCmp, 16 ) ){
		printf( "Transfer was successful\n" );
		printf("%ld bytes transferred in %.2f seconds: %.3f Megabytes per second\n", fileLen, ((float)fileLen)/((float)thrput),(float)thrput/1000000.0);
		printf( "File MD5sum: " );
		for (i = 0; i < 16; i++) {
			printf("%0x", hash[i]);
		}
		printf( "\n" );
		int j;
	} else
		printf( "Transfer was unsuccessful\n" );
}

void upload( int s ) {
	char fileName[MAX_LINE];
	short result, net_result;
	FILE* fp; // file pointer to read data
	struct stat fileStats;
	long fileLen, net_size, sendlen;
	char* fileText;
	MHASH compute;
	char hash[16];
	char c;
	double thrput;
	long i;

	printf( "Enter the file name to upload: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// confirm file exists
	if( access( fileName, F_OK ) != -1 )
		result = 1;
	else{
		printf( "The file does not exist\n" );
		result = 0;
	}
	net_result = htons( result );

	if( write( s, &net_result, sizeof(short) ) == -1 ){
		fprintf( stderr, "myftp: error sending confirmation file exists\n" );
		return;
	}

	if( !result )
		return;

	send_instruction( s, fileName );
	
	stat(fileName, &fileStats);
	fileLen = fileStats.st_size;
	result = receive_result( s );
	if( result != 1 ){ // not ready to receive for some reason
		printf( "The server is not ready to receive that file.\n" );
		return;
	}

	// send the length of the message
	net_size = htonl( fileLen );

	if( write( s, &net_size, sizeof(long) ) == -1 ){
		fprintf( stderr, "myftp: error sending size\n" );
		return;
	}

	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myftpd: hash failed\n" );
		return;
	}

	fileText = malloc( fileLen );
	fp = fopen(fileName, "r");
	for ( i = 0; i < fileLen; i++) {
		c = fgetc( fp );
		fileText[i] = c;
		mhash( compute, &c, 1);
	}
	fclose( fp );

	for (i = 0; i < fileLen; i += MAX_LINE) {
		sendlen = (fileLen-i < MAX_LINE) ? fileLen-i : MAX_LINE;
		if( write( s, fileText+i, sendlen ) == -1 ){
			fprintf( stderr, "myftp: error sending file data\n" );
			free( fileText );
			return;
		}
		// some timing issue resolved by waiting a little
		usleep( 7000 );
	}
	free( fileText );

	// end hash computation and send to server 
	mhash_deinit( compute, hash );
	if( write( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myftpd: error sending hash\n" );
		return;
	}

	printf( "hash: " );
	for( i=0; i<16; i++ )
		printf( "%.2x", hash[i] );
	printf( "\n" );

	// receive response
	long upload_time;

	upload_time = receive_result32( s );
	// signed and unsigned -1
	if( upload_time == -1 || upload_time == 4294967295 ){
		printf( "myftp: Transfer was unsuccessful\n" );
	} else {
		printf( "Transfer was successful\n" );
		thrput = ((double)fileLen)/upload_time;
		printf("%ld bytes transferred in %.2f seconds: %.3f Megabytes per second\n", fileLen, ((double)upload_time)/1000000, thrput);
		printf("File MD5sum: %x\n", hash);
	}

}

void delete_file(int s){
	char buf[MAX_LINE];
	short result;

	// buf = malloc( sizeof(char)*MAX_LINE );

	printf( "Enter the file name to remove: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );

	// trim the \n off the end of buf
	buf[strlen(buf)-1] = 0;

	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 ){
		buf[0] = '\0';

		while( strncmp( buf, "Yes", 3 ) && strncmp( buf, "No", 2 ) ){
			printf( "Confirm you want to delete the file: \"Yes\" to delete, \"No\" to ignore: " );
			fflush( stdin );
			fgets( buf, MAX_LINE, stdin );
		}

		send_instruction( s, buf );
		
		if( !strncmp( buf, "No", 2 ) )
			printf( "Delete abandoned by the user!\n" );
		else{
			result = receive_result( s );
			if( result == 1 )
				printf( "File deleted\n" );
			else if( result == -1 )
				printf( "Failed to delete file\n" );
		}
	} else if( result == -1 )
		printf( "The file does not exist on the server\n" );
}

void list_dir(int s){
	char buf[MAX_LINE+1];
	uint16_t len, netlen = 0;
	int i;

	buf[MAX_LINE] = '\0';
	while (netlen == 0) { // sends an empty message, so skip those cases
		if( read( s, &netlen, sizeof(uint16_t) ) == -1 ){
			fprintf( stderr, "myftp: error receiving listing size\n" );
			return;
		}
	}
	
	len = ntohs( netlen );
	
	for( i = 0; i < len; i += MAX_LINE ){
		if( read( s, buf, MAX_LINE ) == -1 ){
			fprintf( stderr, "myftp: error receiving listing\n" );
			return;
		}
		printf( "%s", buf );
	}
}

void make_dir(int s) {
	char buf[MAX_LINE];
	short result;

	printf( "Enter the directory name to create: ");
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );
	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 )
		printf( "The directory was successfully made\n" );
	else if( result == -1 )
		printf( "Error in making directory\n" );
	else if( result == -2 )
		printf( "The directory already exists on server\n" );
}

void remove_dir(int s) {
	char buf[MAX_LINE];
	short result;

	printf( "Enter the directory name to remove: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );
	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 ){
		buf[0] = '\0';

		while( strncmp( buf, "Yes", 3 ) && strncmp( buf, "No", 2 ) ){
			printf( "Confirm you want to delete the directory: \"Yes\" to delete, \"No\" to ignore: " );
			fflush( stdin );
			fgets( buf, MAX_LINE, stdin );
		}

		send_instruction( s, buf );
		
		if( !strncmp( buf, "No", 2 ) )
			printf( "Delete abandoned by the user!\n" );
		else{
			result = receive_result( s );
			if( result == 1 )
				printf( "Directory deleted\n" );
			else if( result == -1 )
				printf( "Failed to delete directory\n" );
		}
	} else if( result == -1 )
		printf( "The directory does not exist on the server\n" );
	else
		printf( "WTF: %d\n", result );
}

void change_dir(int s) {
	char buf[MAX_LINE];
	short result;
	printf( "Enter the directory name: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );

	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 )
		printf( "Changed current directory\n" );
	else if( result == -1 )
		printf( "Error in changing directory\n" );
	else if( result == -2 )
		printf( "The directory does not exist on server\n" );
}

void send_instruction(int s, char* message) {
	long len, sendlen;
	int i;

	len = strlen( message ) + 1;
	len = htonl( len );

	// send the length of the message
	if( write( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myftp: error sending size\n" );
		return;
	}

	len = ntohl( len );

	// send the actual message
	for (i = 0; i < len; i += MAX_LINE) {
		sendlen = (len-i < MAX_LINE) ? len-i : MAX_LINE;
		if( write( s, &message[i], sendlen ) == -1 ){
			fprintf( stderr, "myftp: error sending message\n" );
			exit( 1 );
		}
	}
}

uint16_t receive_result(int s){
	uint16_t result;

	if( read( s, &result, sizeof(uint16_t) ) == -1 ){
		fprintf( stderr, "myftp: error receiving result\n" );
		return 0;
	}

	return ntohs( result );
}

long receive_result32(int s){
	long result;

	if( read( s, &result, sizeof(long) ) == -1 ){
		fprintf( stderr, "myftp: error receiving result\n" );
		return 0;
	}

	return ntohl( result );
}
