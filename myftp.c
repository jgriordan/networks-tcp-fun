// myftp.c
// John Riordan
// And Nolan
// jriorda2

// usage: myftp server_name port

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
	char buf[MAX_LINE];
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

	// main (fucking awesome) loop
	while( fgets( buf, sizeof(buf), stdin ) ){
		buf[MAX_LINE-1] = '\0';
		if( !strncmp( buf, "Exit", 4 ) ){
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
	}

	close( s );

	return 0;
}

int handle_action(char* msg, int s) {
	
	int response = 0;
	// check for all special 3 char messages
	if (!strncmp("DEL", msg, 3)) {
		
	} else if (!strncmp("LIS", msg, 3)) {
		response = list_dir(s);

	} else if (!strncmp("MKD", msg, 3)) {

		response = make_dir(s);
	} else if (!strncmp("RMD", msg, 3)) {
		response = remove_dir(s);
	
	} else if (!strncmp("CHD", msg, 3)) {
		response = change_dir(s); // start change_directory process and return its response
	}
	return response;
	// might print response in each print statement, that way we have the appropriate statement for each case. 
}

int send_instruction(int s, char* message) { // This should be okay as long as the length doesnt exceed max line, 
//just return error for now but client will need to loop through and free when reading large files (like list_dir)
//returns size sent and stores message in buf
	int size = strlen(message);
	int len_s; // length of size message
	int len_m; // length of message
	
	if (size > MAX_LINE) {
		fprintf( stderr, "file description longer than expected\n");
		return -1;
	}

	size = htons(size);
	if( send( s, &size, sizeof(size), 0 ) == -1 ){
		fprintf( stderr, "myftp: send error\n" );
		exit( 1 );
	}

	if( send( s, message, sizeof(message), 0 ) == -1 ){
		fprintf( stderr, "myftp: size receive error\n" );
		exit( 1 );
	}
	
	return size;	
}
