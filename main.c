#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gprs.h"

#define COMMAND_NUM 4
#define ALARM_NUM 3

#define NORMAL 0
#define FIRE 1
#define THIEF 2

const char *gprs_init_commands[COMMAND_NUM] = {
	"ate0\r",
	"at\r",
	"at+cmgf=0\r",
	"at+csca=8613800100500,145\r",
};

const char *gprs_alarm[ALARM_NUM] = {
	"家里一切正常！",
	"家里着火了！",
	"家里来小偷了！",
};

char gprs_src[256];
char gprs_dst[256];
char gprs_msg[256];
char gprs_cmd[256];
char prefix[] = "0011000D9168";
char phone[] = "8157910894F9";
char postfix[] = "000801";

void init_daemon(void);

int main(int argc, char *argv[])
{
	int keyno;
	int fd;
	pid_t pid;

	init_daemon();

	if ((pid = fork()) == -1)
	{
		perror("Failed to fork child in main()");
		exit(-1);
	}
	else if (pid == 0) //alarm process
	{
		while (1)
		{
			if ((fd = open("/dev/KEYSCAN", O_RDONLY)) == -1)
			{
				perror("Failed to open KEYSCAN");
				exit(-1);
			}

			read(fd, &keyno, sizeof(keyno));

			if (keyno != 0)
			{
				char *buff = NULL;
				int msglen = 0;
				int fd_serial, fd_phone, fd_ada;
				struct termios uart;

				if ((fd_serial = open("/dev/s3c2410_serial1", O_RDWR|O_NOCTTY|O_NONBLOCK)) == -1)
				{
					perror("Failed to open serial port");
					exit(-1);
				}

				tcflush(fd_serial, TCIFLUSH);

				if (!termios_init(fd_serial, &uart, B115200))
				{
					printf("Failed to init serial port\n");
					exit(-1);
				}

				if ((fd_phone = open("/gprs.phone", O_RDONLY)) == -1)
				{
					perror("Failed to open gprs.phone");
					exit(-1);
				}

				if ((fd_ada = open("/gprs.ada", O_RDONLY)) == -1)
				{
					perror("Failed to open gprs.ada");
					exit(-1);
				}

				if (gprs_init(fd_serial, gprs_init_commands, COMMAND_NUM))
				{
					printf("Init succeed\n");
				}
				else
				{
					printf("Init failed\n");
					exit(-1);
				}

				memset(phone, 0, sizeof(phone));
				read(fd_phone, phone, 256);
				phone[11] = 'F';
				order_change(phone, strlen(phone));

				memset(gprs_src, 0, sizeof(gprs_src));
				memset(gprs_dst, 0, sizeof(gprs_dst));
				read(fd_ada, gprs_src, 256);
				buff = (char *)gprs_dst;
				utf8_to_unicode(gprs_src, strlen(gprs_src), &buff, &msglen);
				order_change(buff, msglen-2);
				hex_to_asc(gprs_msg, buff, msglen-2);

				sprintf(gprs_cmd, "at+cmgs=%d\r", strlen(gprs_msg)/2+15);
				sprintf(buff, "%s%s%s%02X%s\x1a", prefix, phone, postfix,
						strlen(gprs_msg)/2, gprs_msg);

				if (message_send(fd_serial, gprs_cmd, strlen(gprs_cmd), 
							buff, strlen(buff)))
				{
					printf("Send succeed\n");
				}
				else
				{
					printf("Send failed\n");
					exit(-1);
				}

				exit(0);
			}
			else
			{
				usleep(10000);
			}
		}
	}
	else //listen process
	{
		//if ((fd = open("/dev/KEYSCAN", O_RDONLY)) == -1)
		//{
		//	perror("Failed to open KEYSCAN");
		//	exit(-1);
		//}

		//while (1)
		//{
		//	read(fd, &keyno, 4);
		//	usleep(10000);
		//}
	}

	return 0;
}

void init_daemon(void)
{
	pid_t pid;
	//int i;

	if ((pid = fork()) == -1)
	{
		perror("Failed to fork first child in init_daemon()");
		exit(-1);
	}
	else if (pid > 0)
	{
		exit(0);
	}

	setsid();

	if ((pid = fork()) == -1)
	{
		perror("Failed to fork second child in init_daemon()");
		exit(-1);
	}
	else if (pid > 0)
	{
		exit(0);
	}

#if 0
	for (i = 0; i < getdtablesize(); i++)
	{
		close(i);
	}
#endif

	chdir("/tmp");
	umask(0);
}
