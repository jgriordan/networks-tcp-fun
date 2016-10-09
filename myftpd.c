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
#include <sys/stat.h> // for directory status

#define MAX_PENDING 5
#define MAX_LINE 256

int handle_input(char* msg, int s);
int list_dir(int s);
int remove_dir(int s);
int delete_file(int s);
int change_dir(int s);
int check_dir(char *dir); // checks that the directory exists
int check_file(char *file);
int receive_instruction(int s, char* buf);

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

			response = handle_input(buf, new_s);
			//	fprintf(stderr, "myftpd: failure to complete request\n");
			//}
			response = htons(response);
			if ( send( new_s, &response, sizeof(response), 0) == -1) {
				fprintf( stderr, "myftpd: response sending error\n"); 
				exit( 1 );
			}
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
		response = list_dir(s);

	} else if (!strncmp("MKD", msg, 3)) {

		response = make_dir(s);
	} else if (!strncmp("RMD", msg, 3)) {
		response = remove_dir(s);
	
	} else if (!strncmp("CHD", msg, 3)) {
		response = change_dir(s); // start change_directory process and return its response
	}
	return response;
}

int delete_file(int s) {

	char file[MAX_LINE]; 
	char buf[MAX_LINE];

	if (receive_instruction(s, file) <= 0) {
		return -3; // instruction error
	}

	int resp = check_file(file);

	resp = htons(resp);
	// send response regarding directory
	if (send(s, &resp, sizeof(resp), 0) == -1) {
		fprintf( stderr, "myftpd: error sending directory status\n");
	}

	int len;
	// get answer from client
	if( ( len = recv( s, buf, sizeof( buf ), 0 ) ) == -1 ){
		fprintf( stderr, "myftpd: instruction receive error\n" );
		exit( 1 );
	}

	if (!strncmp(buf, "Yes", 3)) {
		// string is Yes
		return remove(file); // try to remove directory and return status
		// 0 on success, otherwise -1
	} else if (!strncmp(buf, "No", 2)) {
		return 0;
	}
}

int list_dir(int s) {

	DIR *dp; // pointer to current directory
	struct dirent *dep; // information about directory
	char *buf;
	int len; // length of message
	int alcnt = 1; // increment size (count allocs)

	// new method for continually allocating data
	
	buf = malloc(sizeof(char)*MAX_LINE);
	dp = opendir("./"); // open working directory

	if (dp == NULL) return -1; // error opening directory

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

	// send directory
	// // TODO: send length first
	
	len = strlen(buf) + 1;
	int nlen = htons(len);
	//send message length
	if (send(s, &nlen, sizeof(nlen), 0) == -1) {	
		fprintf( stderr, "myftpd: error sending size\n");
	}

	// loop through for large strings
	int i;
	for (i = 0; i < len; i+= MAX_LINE) { // check cases where
	// we need to send more than one part  
		if (send(s, buf + i, len, 0) == -1) {	
			fprintf( stderr, "myftpd: error sending directory listing\n" );
		}
	}
	// client can concatenate all these strings and stop listening once the length equals the announced length

	free(buf);
}

int remove_dir(int s) {

	char dir[MAX_LINE]; 
	char buf[MAX_LINE];

	if (receive_instruction(s, dir) <= 0) {
		return -3; // instruction error
	}

	int resp = check_dir(dir);

	resp = htons(resp);
	// send response regarding directory
	if (send(s, &resp, sizeof(resp), 0) == -1) {
		fprintf( stderr, "myftpd: error sending directory status\n");
	}

	int len;
	// get answer from client
	if( ( len = recv( s, buf, sizeof( buf ), 0 ) ) == -1 ){
		fprintf( stderr, "myftpd: instruction receive error\n" );
		exit( 1 );
	}

	if (!strncmp(buf, "Yes", 3)) {
		// string is Yes
		return rmdir(dir); // try to remove directory and return status
		// 0 on success, otherwise -1
	} else if (!strncmp(buf, "No", 2)) {
		return 0;
	}
}
int check_file(char *file) { // checks that the directory exists

	if (access(file, F_OK) == -1) { // file not found
		return -1;
	} else {
		return 1; // positive confirmation
	}	
}

int check_dir(char *dir) { // checks that the directory exists

	struct stat st = {0}; // holds directory status

	if (stat(dir, &st) == -1) { // directory not found
		return -1;
	} else {
		return 1; // positive confirmation
	}	
}

int make_dir(int s) {
	char dir[MAX_LINE]; 
	struct stat st = {0}; // holds directory status

	if (receive_instruction(s, dir) <= 0) {
		return -3; // instruction error
	}

	if (stat(dir, &st) == -1) { // directory not found
		return mkdir(dir, 0700); // returns 0 on success, -1 on error
		
	} else {
		return -2; //
	}

}

int change_dir(int s) {
	char dir[MAX_LINE]; 
	if (receive_instruction(s, dir) <= 0) {
		return -3;
	}

	return chdir(dir); // returns 0 on success, -1 error
}

int receive_instruction(int s, char* buf) { // This should be okay as long as the length doesnt exceed max line, 
//just return error for now but client will need to loop through and free when reading large files (like list_dir)
//returns size sent and stores message in buf
	int size;
	int len_s; // length of size message
	int len_m; // length of message


	// todo: receive length first and loop, perhaps subtract len from expected total before loop?	
	if( ( len_s = recv( s, &size, sizeof( size ), 0 ) ) == -1 ){
		fprintf( stderr, "myftpd: size receive error\n" );
		exit( 1 );
	}

	size = ntohs(size);

	if (size > MAX_LINE) {
		fprintf( stderr, "file description longer than expected\n");
		return -1;
	}

	if( ( len_m = recv( s, buf, sizeof(buf), 0 ) ) == -1 ){
		fprintf( stderr, "myftpd: size receive error\n" );
		exit( 1 );
	}

	printf( "Received Instruction: %s", buf );			
	
	return size;	
}
