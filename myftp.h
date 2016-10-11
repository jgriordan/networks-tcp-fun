// myftp.h

#ifndef MYFTP_H
#define MYFTP_H

#include <stdint.h>
#include <stdio.h>

void handle_action( char*, int );
void delete_file( int );
void list_dir( int );
void make_dir( int );
void remove_dir( int );
void change_dir( int );
void send_instruction( int, char* );
uint16_t receive_result( int );
void upload( int);
void send_file( int, FILE*);

#endif
