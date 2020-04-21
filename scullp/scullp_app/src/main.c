#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include "main.h"

#define 	TEST_FILE0 			"/dev/scullp0"
#define 	TEST_FILE1 			"/dev/scullp1"
#define 	TEST_FILE2 			"/dev/scullp2"
#define 	TEST_FILE3 			"/dev/scullp3"
#define 	TEST_FILE_NUMBER	4
#define 	TEST_SIZE 			4096

int scullp_test_poll(void)
{
	int retval;
	int fd0;
	int fd1;
	int fd2;
	int fd3;
	char *buff;
	int buff_length = TEST_SIZE;
	struct pollfd scullp_poll_fds[4];	
	buff = malloc(buff_length);
	if(buff == NULL){
		MAIN_PRINT("malloc fialed\n");	
		return -1;			
	}

	fd0 = open(TEST_FILE0, O_RDONLY);
	if(fd0 < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE0);	
		free(buff);
		buff = NULL;
		return -1;	
	}
	fd1 = open(TEST_FILE1, O_RDONLY);
	if(fd1 < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE1);	
		close(fd0);
		free(buff);
		buff = NULL;
		return -1;	
	}	
	fd2 = open(TEST_FILE2, O_RDONLY);
	if(fd1 < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE2);	
		close(fd0);
		close(fd1);
		free(buff);
		buff = NULL;
		return -1;	
	}
	fd3 = open(TEST_FILE3, O_RDONLY);
	if(fd1 < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE3);	
		close(fd0);
		close(fd1);
		close(fd2);
		free(buff);
		buff = NULL;
		return -1;	
	}
	MAIN_PRINT("fd0 = %d\n", fd0);	
	MAIN_PRINT("fd1 = %d\n", fd1);	
	MAIN_PRINT("fd2 = %d\n", fd2);	
	MAIN_PRINT("fd3 = %d\n", fd3);	

	scullp_poll_fds[0].fd = fd0;
    scullp_poll_fds[0].events = POLLIN | POLLOUT;
	scullp_poll_fds[1].fd = fd1;
    scullp_poll_fds[1].events = POLLIN | POLLOUT;
	scullp_poll_fds[2].fd = fd2;
    scullp_poll_fds[2].events = POLLIN | POLLOUT;
	scullp_poll_fds[3].fd = fd3;
    scullp_poll_fds[3].events = POLLIN | POLLOUT;	
	while(1){
		retval = poll(&scullp_poll_fds, TEST_FILE_NUMBER, -1);
			MAIN_PRINT("poll, retval = %d\n", retval);
			MAIN_PRINT("scullp_poll_fds[0].revents = %d\n", scullp_poll_fds[0].revents);
			MAIN_PRINT("scullp_poll_fds[1].revents = %d\n", scullp_poll_fds[1].revents);
			MAIN_PRINT("scullp_poll_fds[2].revents = %d\n", scullp_poll_fds[2].revents);
			MAIN_PRINT("scullp_poll_fds[3].revents = %d\n", scullp_poll_fds[3].revents);
		if(retval > 0){
			if(scullp_poll_fds[0].revents & POLLIN){
				retval = read(scullp_poll_fds[0].fd, buff, buff_length);
				MAIN_PRINT("read %d :%s\n", retval, buff);
	
				
			}
			if(scullp_poll_fds[1].revents & POLLIN){
				retval = read(scullp_poll_fds[1].fd, buff, buff_length);
				MAIN_PRINT("read %d :%s\n", retval, buff);
				
				
			}
			if(scullp_poll_fds[2].revents & POLLIN){
				retval = read(scullp_poll_fds[2].fd, buff, buff_length);
				MAIN_PRINT("read %d :%s\n", retval, buff);
				
				
			}
			if(scullp_poll_fds[3].revents & POLLIN){
				retval = read(scullp_poll_fds[3].fd, buff, buff_length);
				MAIN_PRINT("read %d :%s\n", retval, buff);
				
			}
			
		}else{
			continue;
			
		}
	}
	close(fd0);
	close(fd1);
	close(fd2);
	close(fd3);
	free(buff);
	buff = NULL;	
	return -1;	
}
char *buff_async;
int buff_length_async = TEST_SIZE;
int fd0_async;

void input_func(int signal)
{
	int retval;
	if(signal & SIGIO){
		retval = read(fd0_async, buff_async, buff_length_async);
		MAIN_PRINT("read %d :%s\n", retval, buff_async);			
	}
}
int scullp_test_fasync(void)
{
	int retval;
	int flag;
	buff_async = malloc(buff_length_async);
	if(buff_async == NULL){
		MAIN_PRINT("malloc fialed\n");	
		return -1;			
	}

	fd0_async = open(TEST_FILE0, O_RDONLY);
	if(fd0_async < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE0);	
		free(buff_async);
		buff_async = NULL;
		return -1;	
	}

	MAIN_PRINT("fd0_async = %d\n", fd0_async);	
	signal(SIGIO, input_func);
	retval = fcntl(fd0_async, F_SETOWN, getpid());
	if(retval < 0){
		MAIN_PRINT("F_SETOWN fialed, retval = %d\n", retval);	
		close(fd0_async);
		free(buff_async);
		buff_async = NULL;	
		return -1;			
	}	
	flag = fcntl(fd0_async, F_GETFL);	
	retval = fcntl(fd0_async, F_SETFL, flag | O_ASYNC);
	if(retval < 0){
		MAIN_PRINT("F_SETFL fialed, retval = %d\n", retval);	
		close(fd0_async);
		free(buff_async);
		buff_async = NULL;	
		return -1;			
	}
	while(1){
		sleep(1);
	}	
	close(fd0_async);
	free(buff_async);
	buff_async = NULL;	
	return -1;	
}
int scullp_test_noblock(void)
{
	int retval;
	int fd0;
	int flag;
	char *buff;
	int buff_length = TEST_SIZE;
	buff = malloc(buff_length);
	if(buff == NULL){
		MAIN_PRINT("malloc fialed\n");	
		return -1;			
	}

	fd0 = open(TEST_FILE0, O_RDONLY);
	if(fd0 < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE0);	
		free(buff);
		buff = NULL;
		return -1;	
	}

	MAIN_PRINT("fd0 = %d\n", fd0);	
    flag = fcntl(fd0,F_GETFL);
	flag = flag | O_NONBLOCK;
	retval = fcntl(fd0, F_SETFL, flag);
	if(retval < 0){
		MAIN_PRINT("F_SETFL  fialed, retval = %d\n", retval);	
		close(fd0);
		free(buff);
		buff = NULL;		
	}
	retval = read(fd0, buff, buff_length);
	MAIN_PRINT("read %d :%s\n", retval, buff);
	
	close(fd0);
	free(buff);
	buff = NULL;	
	return -1;		
	
	
}
int main(int argc, char **argv)
{
  
	scullp_test_noblock();
	return 0;
}