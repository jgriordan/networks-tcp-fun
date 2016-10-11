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
//#include <sys/time.h>
#include <unistd.h> // for the unix commands
#include <dirent.h> // for directory information
#include <sys/stat.h> // for directory status

#define MAX_PENDING 5
#define MAX_LINE 256

void handle_input(char* msg, int s);
int list_dir(int s);
void remove_dir(int s);
void make_dir(int s);
void delete_file(int s);
void change_dir(int s);
int check_dir(char *dir); // checks that the directory exists
int check_file(char *file);
int receive_instruction(int s, char** buf);
void send_result(int s, short result);
void upload( int s );
int receive_file(int s, FILE* fp); 

int main( int argc, char* argv[] ){
	struct sockaddr_in sin, client_addr;
	char buf[MAX_LINE];
	int s, new_s, len, opt;
	uint16_t port;
	int response; // response to send back to 
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
	opt = 1;
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
			printf( "Waiting to receive command\n" );
			if( ( len = recv( new_s, buf, sizeof( buf ), 0 ) ) == -1 ){
				// changed from 1 to -1
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

void handle_input(char* msg, int s) {
	// check for all special 3 char messages
	if (!strncmp("DEL", msg, 3)) {
		delete_file(s);
	} else if (!strncmp("LIS", msg, 3)) {
		list_dir(s);

	} else if (!strncmp("MKD", msg, 3)) {
		make_dir(s);

	} else if (!strncmp("RMD", msg, 3)) {
		remove_dir(s);
	
	} else if (!strncmp("CHD", msg, 3)) {
		// start change_directory process and return its response
		change_dir(s);
	} else if (!strncmp("UPL", msg, 3)) {
		upload(s);
	}
}

void upload( int s ) {
	short result;
	char *buf;
	FILE *fp = NULL; // file pointer to write to
	int namelen; // file name length

	if ((namelen = receive_instruction(s, &buf)) <= 0) {
		fprintf( stderr, "myftpd: error receiving file name\n" );
		return;
	}
	// get file name
	
	// open the file to write to
	fp = fopen(buf, "wb");
	printf("opened file: %x\n", fp);

	if (fp == NULL) {
		fprintf( stderr, "myftdp: could not create file\n");
		result = -1; // not ready
		send_result(s, result);
		return;
	} else {
		result =  1; // ready 
		// tell client we are ready
		send_result(s, result);
	}	
	
	if (receive_file(s, fp) < 0) { // might return hash value instead of num bytes written
		fprintf( stderr, "myftpd: error writing new file\n" );
	}

	fclose(fp);
// TODO: calculate and compare hash	
}

int receive_file(int s, FILE* fp) { // MIGHT need pointer to pointer

	int i;
	uint32_t len, recvlen;
	char *buf;
	uint32_t thrput, netthrput;
	long int upload_time;
	struct timeval start, stop;

	printf( "Loading file...\n" );

	if( read( s, &len, sizeof(uint32_t) ) == -1 ){
		fprintf( stderr, "myftpd: size receive error\n" );
		return -1;
	}
	len = ntohl( len );
	printf( "File length: %i\n", len);
	buf = malloc( len );

	//TODO: calculate throughput
	
	gettimeofday(&start, NULL);
	
	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, buf+i, recvlen ) == -1 ){
			fprintf( stderr, "myftpd: error receiving instruction\n" );
			return -1;
		}
	}
	gettimeofday(&stop, NULL);


	upload_time = stop.tv_usec - start.tv_usec;
	
	if (upload_time != 0) {
	thrput = ((8*len)*100000/upload_time);
	} else {
		thrput = 0;
	}
	printf ("%u bits per second\n", thrput);

	if (fwrite(buf, 1, len, fp) != len) {
		fprintf( stderr, "myftpd: error writing to designated file\n"); // should return the number of bytes written, or an error
		return -1;
	}

	//TODO: receive MD5 hash, compute and compare (can send
	// send thrput
	// set thrput to -1 if there is an error (maybe 0 works better
	netthrput = htonl( thrput );
	if( write( s, &netthrput, sizeof(uint32_t) ) == -1 )
		fprintf( stderr, "myftpd: error sending result to client" );


	return 0;
}

void delete_file(int s) {
	char* file;
	char* confirm;
	uint16_t result, netresult;

	if (receive_instruction(s, &file) <= 0) {
		fprintf( stderr, "myftpd: instruction receive error\n" );
		return;
	}

	result = check_file( file );

	printf( "file: %s, result: %d\n", file, result );

	netresult = htons( result );
	// send response regarding file existence
	if (send(s, &netresult, sizeof(uint16_t), 0) == -1) {
		fprintf( stderr, "myftpd: error sending file status\n");
		return;
	}

	// file exists
	if( result == 1 ){
		// get confirmation from client
		if( receive_instruction( s, &confirm ) <= 0 ){
			fprintf( stderr, "myftpd: error receiving client confirmation\n" );
			return;
		}

		if (!strncmp(confirm, "Yes", 3)) {
			// success
			if( !remove( file ) )
				result = 1;
			// otherwise failure
			else
				result = -1;

		}
	}
	send_result(s, result);
}

int list_dir(int s) {
	DIR *dp; // pointer to current directory
	struct dirent *dep; // information about directory
	char *buf;
	uint16_t len, netlen; // length of message
	int alcnt = 1; // increment size (count allocs)
	uint32_t sendlen;

	// new method for continually allocating data
	
	dp = opendir("./"); // open working directory
	if (dp == NULL){
		fprintf( stderr, "myftpd: error opening directory\n" );
		return -1;
	}

	buf = malloc(sizeof(char)*MAX_LINE);
	buf[0] = '\0';

	while((dep = readdir(dp)) != NULL) {
		if (MAX_LINE*alcnt <= (strlen(buf) + strlen(dep->d_name) + 1)) {
			// size continually increments by the data added
			buf = realloc(buf, sizeof(char)*MAX_LINE*(++alcnt));
		}
		strcat(buf, dep->d_name);
		strcat(buf, "\n"); 
		// allocate more memory
	} // buffer keeps growing on the heap until we can send it
	
	if (closedir(dp) == -1) {
		fprintf( stderr, "myftpd: could not close directory\n" );
	}

	//send message length
	len = strlen(buf) + 1;
	netlen = htons( len );
	if( write( s, &netlen, sizeof(uint16_t) ) == -1 ){
		fprintf( stderr, "myftpd: error sending length\n" );
		free( buf );
		return -1;
	}

	// loop through for large strings
	int i;
	for (i = 0; i < len; i+= MAX_LINE) { 
		// check cases where we need to send more than one part
		sendlen = (len-i < MAX_LINE) ? len-i : MAX_LINE;
		if( write(s, buf + i, sendlen) == -1 ){
			fprintf( stderr, "myftpd: error sending directory listing\n" );
			free( buf );
			return -1;
		}
	}
	// client can concatenate all these strings and stop listening once the length equals the announced length

	free(buf);
}

void remove_dir(int s) {
	char* dir;
	char* confirm;
	uint16_t result, netresult;

	if (receive_instruction(s, &dir) <= 0) {
		fprintf( stderr, "myftpd: instruction receive error\n" );
		return;
	}

	result = check_dir(dir);

	netresult = htons( result );
	// send response regarding directory
	if (send(s, &netresult, sizeof(uint16_t), 0) == -1) {
		fprintf( stderr, "myftpd: error sending directory status\n");
		return;
	}

	// directory exists
	if( result == 1 ){
		// get confirmation from client
		if( receive_instruction( s, &confirm ) <= 0 ){
			fprintf( stderr, "myftpd: error receiving client confirmation\n" );
			return;
		}

		if (!strncmp(confirm, "Yes", 3)) {
			// success
			if( rmdir( dir ) == 0 )
				result = 1;
			// otherwise failure
			else
				result = -1;

			netresult = htons( result );
			if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
				fprintf( stderr, "myftpd: error sending result to client" );
		}
	}
}

// checks that file exists
int check_file(char *file) {
	// exists
	if( access( file, F_OK ) != -1 )
		return 1;
	// doesn't exist
	else
		return -1;
}

int check_dir(char *dir) { // checks that the directory exists

	DIR *dp; // pointer to current directory
	strtok(dir, "\n");
	dp = opendir(dir); // open working directory
	if (dp == NULL){
		//fprintf( stderr, "myftpd: error opening directory\n" );
		return -1;
	} else {
		closedir(dp);
		return 1; //directory found
	}
}

void make_dir(int s) {
	char* dir; 
	struct stat st = {0}; // holds directory status
	short result;

	if( receive_instruction( s, &dir ) <= 0 ){
		fprintf( stderr, "myftpd: instruction receive error\n" );
		return;
	}

	// directory doesn't exist
	if( check_dir( dir ) == -1 ){
		// successful mkdir
		if( !mkdir( dir, 0700 ) )
			result = 1;
		// otherwise failure to create dir
		else
			result = -1;
	// directory exists
	} else
		result = -2;
	
	send_result(s, result);
}

void change_dir(int s) {
	char* dir;
	short result;
	uint16_t netresult;

	if (receive_instruction(s, &dir) <= 0)
		result = -1;
	else if (check_dir(dir) == -1) // directory not found
		result = -2;
	else if( chdir( dir ) == 0 ) // success // TODO: remove new line, might replace with strtok(dir, "\n");
		result = 1;
	else
		result = -1;

	printf( "changed dir: %s, result: %d\n", dir, result );

	send_result(s, result);
/*
	netresult = htons( result );
	if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
		fprintf( stderr, "myftpd: error sending result to client" );

*/
	free( dir );
}

void send_result(int s, short result) {
	uint16_t netresult = htons( result );
	if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
		fprintf( stderr, "myftpd: error sending result to client" );

}

int receive_instruction(int s, char** buf) {
	int i;
	uint32_t len, recvlen;

	printf( "Waiting to receive instruction.\n" );

	if( read( s, &len, sizeof(uint32_t) ) == -1 ){
		fprintf( stderr, "myftpd: size receive error\n" );
		return -1;
	}
	len = ntohl( len );

	*buf = malloc( len );

	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, (*buf)+i, recvlen ) == -1 ){
			fprintf( stderr, "myftpd: error receiving instruction\n" );
			return -1;
		}
	}

	printf( "Received Instruction: %s", *buf );

	return len;	
}
