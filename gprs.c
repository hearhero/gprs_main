#include <stdio.h>
#include <string.h>

#include "gprs.h"

//initialize serial port
bool termios_init(int fd, struct termios *tio, int baud)
{
	if (tcgetattr(fd, tio) == -1)
	{
		perror("Failed to tcgetattr");
		return false;
	}

	tio->c_iflag &= ~(INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_iflag |= (IGNBRK | IGNPAR);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_oflag &= ~OPOST;

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_cflag &= ~(CSIZE | CSTOPB | PARENB);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_cflag |= (CS8 | CREAD);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	cfsetispeed(tio, baud);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	cfsetospeed(tio, baud);

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_cc[VTIME] = 5;

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	tio->c_cc[VMIN] = 0;

	if (tcsetattr(fd, TCSANOW, tio) == -1)
	{
		perror("Failed to tcsetattr");
		return false;
	}

	return true;
}

//send AT commands to make initial configuration
bool gprs_init(int fd, const char *cmd[], int count)
{
	int i;
	char reply[256];

	for (i = 0; i < count; i++)
	{
		write(fd, cmd[i], strlen(cmd[i]));
		usleep(500000);
		memset(reply, 0, sizeof(reply));
		read(fd, reply, 256);

		if (NULL == strstr(reply, "OK")) 
		{
			return false;
		}
	}

	return true;
}

//send PDU message
bool message_send(int fd, char *cmd, int cmdlen, char *msg, int msglen)
{
	char reply[256];

	write(fd, cmd, cmdlen);
#ifdef _DEBUG_GPRS_
	printf("%s\n", cmd);
#endif
	usleep(500000);
	tcflush(fd, TCIFLUSH);

	write(fd, msg, msglen);
#ifdef _DEBUG_GPRS_
	printf("%s\n", msg);
#endif
	usleep(5000000);
	memset(reply, 0, sizeof(reply));
	read(fd, reply, 256);
#ifdef _DEBUG_GPRS_
	printf("%s\n", reply);
#endif

	if (NULL == strstr(reply, "OK")) 
	{
		return false;
	}

	return true;
}

//make byte order correct
void order_change(char *buf, int len)
{
	int i;

	for (i = 0; i < len; i += 2)
	{
		buf[i] = buf[i] ^ buf[i+1];
		buf[i+1] = buf[i] ^ buf[i+1];
		buf[i] = buf[i] ^ buf[i+1];
	}
}

//change HEX to ASCII
void hex_to_asc(char *dst, char *src, int len)
{
	int i;                                                                                                                                                                  
	int count = len * 2;

	for (i = 0; i < count; i += 2)
	{
		*(dst+i) = (*(src+i/2) & 0xf0) >> 4;
		*(dst+i+1) = *(src+i/2) & 0xf;
	}

	dst[count] = '\0';

	for (i = 0; i < count; i++)
	{
		if (dst[i] >= 0 && dst[i] <= 9)
		{
			dst[i] += 0x30;
		}
		else
		{
			dst[i] += 0x37;
		}
	}
}

//change UTF-8 to UCS2
bool utf8_to_unicode(char *inbuf, int inlen, char **outbuf, int *outlen)
{
	char *pos = *outbuf; //outbuf position pointer, point to the next spare byte in outbuf

	while (*inbuf)    
	{    
		if (*inbuf > 0x00 && *inbuf <= 0x7F) //deal with single byte UTF-8 characters(including letters and numbers)
		{    
			*pos = *inbuf;    
			*(++pos) = 0; //little-endian, fill in high addresses with zero
		}    
		else if (((*inbuf) & 0xE0) == 0xC0) //deal with dual bytes UTF-8 characters
		{    
			char low = *inbuf;    
			char high = *(++inbuf);    

			if ((high & 0xC0) != 0x80) //check if it is a legal UTF-8 character code
			{    
				return false;
			}    

			*pos = (low << 6) + (high & 0x3F);    
			*(++pos) = (low >> 2) & 0x07;    
		}    
		else if (((*inbuf) & 0xF0) == 0xE0) //deal with triple bytes UTF-8 characters
		{    
			char low = *inbuf;    
			char middle = *(++inbuf);    
			char high = *(++inbuf);    

			if (((middle & 0xC0) != 0x80) || ((high & 0xC0) != 0x80))    
			{    
				return false;    
			}    

			*pos = (middle << 6) + (high & 0x7F);    
			*(++pos) = (low << 4) + ((middle >> 2) & 0x0F);     
		}    
		else //leaving UTF-8 characters longer than three bytes not converted
		{    
			return false;    
		}    

		inbuf++;    
		pos++;    
		*outlen += 2;
	}    

	*pos = 0;    
	*(++pos) = 0;    

	return true;    
}
