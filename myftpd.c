// myftpd.c
// John Riordan
// jriorda2

// usage: myftpd port

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define MAX_PENDING 5
#define MAX_LINE 256

int main( int argc, char* argv[] ){
	struct sockaddr_in sin, client_addr;
	char buf[MAX_LINE];
	int s, new_s, len;
	uint16_t port;
	/*char timestamp[1024];
	struct timeval t_of_day;
	struct tm* local_t;
	time_t raw_t;*/

	// validate and put command line in variables
	if( argc == 2 ){
		port = atoi(argv[1]);
	} else {
		fprintf( stderr, "usage: myftpd port\n" );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&sin, sizeof( sin ) );
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;// use default IP address
	sin.sin_port = htons( port );

	// create the server-side socket
	if( ( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ){
		fprintf( stderr, "myftpd: socket creation error\n" );
		exit( 1 );
	}

	// set socket options
	if( ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int) ) ) < 0 ){
		fprintf( stderr, "myftpd: error setting socket options\n" );
		exit( 1 );
	}

	// bind the created socket to the address
	if( bind( s, (struct sockaddr*)&sin, sizeof( sin ) ) < 0 ){
		fprintf( stderr, "myftpd: socket binding error\n" );
		exit( 1 );
	}

	// open the passive socket by listening
	if( ( listen( s, MAX_PENDING ) ) < 0 ){
		fprintf( stderr, "myftpd: socket listening error\n" );
		exit( 1 );
	}

	// wait for connection, then receive and print text
	while( 1 ){
		if( ( new_s = accept( s, (struct sockaddr*)&sin, &len ) ) < 0 ){
			fprintf( stderr, "myftpd: accept error\n" );
			exit( 1 );
		}

		while( 1 ){
			if( ( len = recv( new_s, buf, sizeof( buf ), 0 ) ) == 1 ){
				fprintf( stderr, "myftpd: receive error\n" );
				exit( 1 );
			}

			if( len == 0 ) break;

			printf( "Received: %s", buf );			
		}

		close( new_s );
	}

	return 0;
}
