// myftp.c
// John Riordan
// And Nolan
// jriorda2

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
		if( !strncmp( buf, "XIT", 3 ) ){
			printf( "Bye bye\n" );
			break;
		}

		len = strlen(buf) + 1;
		if( send( s, buf, len, 0 ) == -1 ){
			fprintf( stderr, "myftp: send error\n" );
			exit( 1 );
		} else {
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
}

void delete_file(int s){

	char* buf;
	short result;
	int alcnt = 1;
	uint32_t len = 0;
	char c;

	buf = malloc( sizeof(char)*MAX_LINE );

	printf( "Enter the file name to remove: " );
	fflush( stdin );
	while( (c = fgetc( stdin )) && c != '\n' && c != EOF ){
		buf[len++] = c;
		if( len == alcnt*MAX_LINE )
			buf = realloc( buf, ++alcnt*sizeof(char)*MAX_LINE );
	}

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
	
	printf("netlen: %u\n", netlen);
	len = ntohs( netlen );
	printf("len: %u\n", len);

	
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
	uint32_t len, sendlen;
	int i;

	len = strlen( message ) + 1;
	len = htonl( len );

	// send the length of the message
	if( write( s, &len, sizeof(uint32_t) ) == -1 ){
		fprintf( stderr, "myftp: error sending size\n" );
		return;
	}

	len = ntohl( len );

	printf( "length: %u\n", len );

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
