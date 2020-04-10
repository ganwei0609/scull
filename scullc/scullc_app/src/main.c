#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include "main.h"

#define 	TEST_FILE 			"/dev/scullc0"
#define 	TEST_SIZE 			9000

static int test_read_and_write(char *file)
{
	FILE *fp;
	char *buff;
	int i;
	int retval = -1;
	fp = fopen("/dev/scullc0", "w+");
	if(NULL == fp){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE);	
		return -1;	
	}
	buff = malloc(TEST_SIZE);	
	if(NULL == buff){
		MAIN_PRINT("malloc fialed, TEST_SIZE = %d\n", TEST_SIZE);	
		fclose(fp);
		return -1;		
	}
	for(i = 0; i < TEST_SIZE; i++){
		buff[i] = i % 188;
	}
	for(i = 0; i < TEST_SIZE; i++){
		if(i % 16 == 0){
			printf("\n");
		}
		printf("%d\t", buff[i]);	

	}	
	retval = fwrite(buff, 1, TEST_SIZE, fp);	
	if(TEST_SIZE != retval){
		MAIN_PRINT("fwrite fialed, retval = %d\n", retval);
		free(buff);	
		buff = NULL;
		fclose(fp);
		return -1;		
	}
	retval = fseek(fp, 0, SEEK_SET);
	if(0 != retval){
		MAIN_PRINT("flseek fialed, retval = %d\n", retval);
		free(buff);	
		buff = NULL;
		fclose(fp);
		return -1;				
	}
	memset(buff, 0, TEST_SIZE);
	retval = fread(buff, 1, TEST_SIZE, fp);	
	if(TEST_SIZE != retval){
		MAIN_PRINT("fread fialed, retval = %d\n", retval);	
		free(buff);	
		buff = NULL;
		fclose(fp);
		return -1;		
	}
	for(i = 0; i < TEST_SIZE; i++){
		if(i % 16 == 0){
			printf("\n");
		}
		printf("%d\t", buff[i]);	

	}
	for(i = 0; i < TEST_SIZE; i++){
		if(buff[i] != (char)(i % 188)){
			MAIN_PRINT("verify fialed, buff[%d] = %d\n", i, buff[i]);	
			break;
		}
	}
	if(TEST_SIZE == i){
		retval = 0;	
	}
	free(buff);	
	buff = NULL;	
	fclose(fp);
	return retval;
}
static int test_ioctl(char *file)
{
	int fd;
	int retval = -1;
	int test_qset = 10;
	int test_quantum = 1024;
	int temp = 0;

	fd = open("/dev/scullc0", O_RDONLY);
	if(fd < 0){
		MAIN_PRINT("opne %s fialed\n", TEST_FILE);	
		return -1;	
	}
	MAIN_PRINT("new qset %d\n", test_qset);
	temp = test_qset;
	retval = ioctl(fd, SCULLC_IOC_EX_QSET_PTR, &temp);
	MAIN_PRINT("org qset %d\n", temp);
	retval = ioctl(fd, SCULLC_IOC_GET_QSET, 0);
	MAIN_PRINT("new qset %d\n", retval);
	if(retval != test_qset){
		MAIN_PRINT("test qset failed, retval = %d, test_qset = %d\n", retval, test_qset);
		close(fd);	
		return -1;	
	}
	MAIN_PRINT("new quantum %d\n", test_quantum);
	temp = test_quantum;
	retval = ioctl(fd, SCULLC_IOC_EX_QUANTUM_PTR, &temp);
	MAIN_PRINT("org quantum %d\n", temp);
	retval = ioctl(fd, SCULLC_IOC_GET_QUANTUM, 0);
	MAIN_PRINT("new quantum %d\n", retval);
	if(retval != test_quantum){
		MAIN_PRINT("test quantum failed, retval = %d, test_quantum = %d\n", retval, test_quantum);
		close(fd);	
		return -1;	
	}
	
	close(fd);	
	return 0;	
}
int main(int argc, char **argv)
{
	int retval;

	retval = test_read_and_write(TEST_FILE);
	if(0 == retval){
		MAIN_PRINT("test_read_and_write %s OK\n", TEST_FILE);
		
	}else {
		MAIN_PRINT("test_read_and_write %s FAILED\n", TEST_FILE);

	}
	retval = test_ioctl(TEST_FILE);
	if(0 == retval){
		MAIN_PRINT("test_ioctl %s OK\n", TEST_FILE);
		
	}else {
		MAIN_PRINT("test_read_and_write %s FAILED\n", TEST_FILE);

	}
	return retval;	
}