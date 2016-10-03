// myftp.c
// John Riordan
// And Nolan
// jriorda2

// usage: myftp Server_Name Port

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_LINE

int main( int argc, char* argv[] ){
	FILE* fp = NULL;
	struct hostent* hp;
	struct sockaddr_in sin;
	char* host;
	char buf[MYLEN], resp[BUF_LEN], key[BUF_LEN];
	int i, s, len, addr_len, key_len, resp_len;
	uint16_t port;
	struct timeval start, end;

	// validate and put command line in variables
	if( argc == 4 ){
		host = argv[1];
		port = atoi(argv[2]);
		input = argv[3];
	} else {
		fprintf( stderr, "usage: udpclient host port input\n" );
		exit( 1 );
	}

	// translate host name into peer's IP address
	hp = gethostbyname( host );
	if( !hp ){
		fprintf( stderr, "udpclient: unknown host: %s\n", host );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&sin, sizeof( sin ) );
	sin.sin_family = AF_INET;
	bcopy( hp->h_addr, (char*)&sin.sin_addr, hp->h_length );
	sin.sin_port = htons( port );

	// active open
	if( (s = socket( PF_INET, SOCK_DGRAM, 0 )) < 0 ){
		fprintf( stderr, "udpclient: socket error" );
		exit( 1 );
	}

		
	// prevent buffer overflow because hackers
	buf[BUF_LEN] = '\0';

	// try to open a file
	fp = fopen( input, "r" );

	// if open was successful, put file in the buffer
	if( fp ){
		fread( buf, sizeof( char ), BUF_LEN, fp );
		fclose( fp );
	// otherwise, treat input as simple text
	} else {
		strncpy( buf, input, BUF_LEN );
	}
	len = strlen(buf) + 1;

	// start roundtrip timer
	gettimeofday( &start, NULL );

	// send buffer to server
	if( sendto( s, buf, len, 0, (struct sockaddr*)&sin, sizeof( struct sockaddr ) ) == -1 ){
		fprintf( stderr, "udpclient: send error" );
		exit( 1 );
	}

	addr_len = sizeof( struct sockaddr );

	// get first response- encrypted fun
	if( ( resp_len = recvfrom( s, resp, BUF_LEN, 0, (struct sockaddr*)&sin, &addr_len ) ) == -1 ){
		fprintf( stderr, "udpclient: receive error" );
		exit( 1 );
	}

	// end roundtrip timer
	gettimeofday( &end, NULL );

	// get second response- key
	if( ( key_len = recvfrom( s, key, BUF_LEN, 0, (struct sockaddr*)&sin, &addr_len ) ) == -1 ){
		fprintf( stderr, "udpclient: receive error" );
		exit( 1 );
	}

	// decrypt back into buf and display
	for( i=0; i<resp_len; i++ ){
		buf[i] = resp[i] ^ key[i%key_len];
	}
	buf[resp_len] = '\0';
	printf( "\nDecrypted Message From Server:\n%s\n", buf );

	// compute RTT and display
	printf( "\nRTT: %ld microseconds\n\n", ( 1000000 * (end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec ) );

	return 0;
}
