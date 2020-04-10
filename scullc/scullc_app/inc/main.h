#ifndef __MAIN_H__
#define __MAIN_H__
#define MAIN_PRINT(format, ...)	 	printf("%s %d:"format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define SCULLC_IOC_MAGIC							's'
#define SCULLC_IOC_GET_QSET							_IO(SCULLC_IOC_MAGIC, 0)
#define SCULLC_IOC_SET_QSET							_IO(SCULLC_IOC_MAGIC, 1)
#define SCULLC_IOC_GET_QSET_PTR						_IOR(SCULLC_IOC_MAGIC, 2, int)
#define SCULLC_IOC_SET_QSET_PTR						_IOW(SCULLC_IOC_MAGIC, 3, int)
#define SCULLC_IOC_EX_QSET							_IO(SCULLC_IOC_MAGIC, 4)
#define SCULLC_IOC_EX_QSET_PTR						_IOWR(SCULLC_IOC_MAGIC, 5, int)

#define SCULLC_IOC_GET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 6)
#define SCULLC_IOC_SET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 7)
#define SCULLC_IOC_GET_QUANTUM_PTR					_IOR(SCULLC_IOC_MAGIC, 8, int)
#define SCULLC_IOC_SET_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 9, int)
#define SCULLC_IOC_EX_QUANTUM						_IOW(SCULLC_IOC_MAGIC, 10, int)
#define SCULLC_IOC_EX_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 11, int)
#define SCULLC_IOC_MAX								12





#endif