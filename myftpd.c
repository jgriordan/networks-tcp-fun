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
#include <unistd.h> // for the unix commands
#include <dirent.h> // for directory information

#define MAX_PENDING 5
#define MAX_LINE 256

void handle_input(char* msg, int s);
int list_dir(int s);
int change_dir(int s);
char *receive_instruction(int s);

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

			// exit case.
			if (!strncmp("XIT", buf, 3)) { // client sent exit request
				break;
			}

			handle_input(buf, new_s);
		}

		close( new_s );
	}

	return 0;
}

int handle_input(char* msg, int s) {
	
	int response = 0;
	// check for all special 3 char messages
	if (!strncmp("DEL", msg, 3)) {
		
	} else if (!strncmp("LIS", msg, 3)) {
		return list_dir(s);

	} else if (!strncmp("MKD", msg, 3)) {


	} else if (!strncmp("RMD", msg, 3)) {

	
	} else if (!strncmp("CHD", msg, 3)) {
		return change_dir(s); // start change_directory process and return its response
	}
}

int list_dir(int s) {

	DIR *dp; // pointer to current directory
	struct dirent *dep; // information about directory
	char buf[MAX_LINE];
	int len; // length of message

	dp = opendir("./"); // open working directory

	if (dp == NULL) return -1; // error opening directory

	while(dep = readdir(dp)) {
		strcat(buf, ep->d_name);
		strcat(buf, "\n");
	}
	
	if (closedir(dp) == -1) {
		fprintf( stderr, "myftpd: could not close directory\n" );
	}

	// send directory
	len = strlen(buf) + 1;
	if (send(s, buf, len, 0) == -1) {	
		fprintf( stderr, "myftpd: error sending directory listing\n" );
	}
}

int change_dir(int s) {
	char* dir = receive_instruction(s);

	return chdir(dir); // returns 0 on success
}

char *receive_instruction(int s) {

	while(1) {
		if( ( len = recv( new_s, buf, sizeof( buf ), 0 ) ) == 1 ){
			fprintf( stderr, "myftpd: receive error\n" );
			exit( 1 );
		}

		if( len == 0 ) return ""; // perhaps send error flag?

		printf( "Received Instruction: %s", buf );			
		
		return buf;
	}
	
}
