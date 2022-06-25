#include <unistd.h>
#include <stdio.h>
#include <mtd/mtd-user.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define OF_NAME_LEN		32
#define OF_DATE_LEN		16
#define OF_TIME_LEN		16
#define OF_OFF_NUM		10		//最多只有10个分区

#define BLOCK_NAME_LEN	16
#define FILE_NAME_LEN	24
#define VERSION_LEN		16
#define OTA_STATUS_LEN	8

#define BUFSIZE (8 * 1024)

typedef struct of_header
{
	char of_name[OF_NAME_LEN];
	char of_date[OF_DATE_LEN];
	char of_time[OF_TIME_LEN];
	unsigned int file_num;
	unsigned int file_offset[OF_OFF_NUM];
    unsigned int file_len;
    unsigned short file_crc;
	char reseved[30];
}of_header_t;

typedef struct of_one_header
{
	char block_name[BLOCK_NAME_LEN];
	char file_name[FILE_NAME_LEN];	
	char version[VERSION_LEN];
	char ota_status[OTA_STATUS_LEN];
    unsigned int of_one_len;
	char reserved[28];			//预留空间，整个结构体是96B
}of_one_header_t;

int status_flag = 0;

void invert_uint8(unsigned char *des_buf, unsigned char *src_buf)
{
    int i;
    unsigned char temp = 0;
 
    for(i = 0; i < 8; i++)
    {
        if(src_buf[0] & (1 << i))
        {
            temp |= 1<<(7-i);
        }
    }

    des_buf[0] = temp;
}

void invert_uint16(unsigned short *des_buf, unsigned short *src_buf)
{
    int i;
    unsigned short temp = 0;
 
    for(i = 0; i < 16; i++)
    {
        if(src_buf[0] & (1 << i))
        {
            temp |= 1<<(15 - i);
        }
    }

    des_buf[0] = temp;
}
 
unsigned short make_crc(unsigned char *puch_msg, unsigned int us_data_len)
{
    unsigned short w_crc_in = 0x0000;
    unsigned short w_c_poly = 0x1021;
    unsigned char w_char 	= 0;

    while (us_data_len--)
    {
        w_char = *(puch_msg++);
        invert_uint8(&w_char, &w_char);
        w_crc_in ^= (w_char << 8);
 
        for(int i = 0; i < 8; i++)
        {
            if(w_crc_in & 0x8000)
            {
                w_crc_in = (w_crc_in << 1) ^ w_c_poly;
            }
            else
            {
                w_crc_in = w_crc_in << 1;
            }
        }
    }

    invert_uint16(&w_crc_in, &w_crc_in);

    return (w_crc_in);
}

int crc_check(void *buf, unsigned int len, unsigned short crc_ori)
{
    unsigned short crc_cal = 0;

    crc_cal = make_crc((unsigned char *)buf, len);
    if (crc_cal != crc_ori) {
        return -1;
    }else {
        return 0;
    }
}

int update_one_part(of_one_header_t *of_one_head, char *buf)
{
    char *buf_up   = NULL;
    char *buf_cmp  = NULL;
    int i       = 0;
    int fd_d    = 0;
    int ret     = 0;
    unsigned int done = 0;
    unsigned int rem  = 0;
    unsigned int erase_count = 0;
    unsigned int count       = 0;
    struct mtd_info_user mtd;
    struct erase_info_user e;

    fd_d = open(of_one_head->block_name, O_SYNC | O_RDWR);
    if (fd_d < 0) {
        printf("open %s fail.\n", of_one_head->block_name);
        return -1;
    }

    memset(&mtd, 0x00, sizeof(struct mtd_info_user));
    if (ioctl(fd_d, MEMGETINFO, &mtd) < 0) {
        printf("get mtd info error.\n");
        close(fd_d);
        return -1;
    }
    if ((of_one_head->of_one_len) > mtd.size) {
        printf("buffer too large.\n");
        close(fd_d);
        return -1;
    }

    erase_count = (unsigned int)(of_one_head->of_one_len + mtd.erasesize - 1) / mtd.erasesize;
    memset(&e, 0x00, sizeof(struct erase_info_user));
    e.length = mtd.erasesize;

    e.start = 0;
    for (i = 1; i <= erase_count; i++) {
        if (ioctl(fd_d, MEMERASE, &e) < 0) {
            printf("erase error.\n");
            close(fd_d);
            return -1;
        }
        e.start += mtd.erasesize;
    }

    buf_up = (char *)malloc(BUFSIZE);
    if (!buf_up) {
        printf("malloc buf0 error.\n");
        close(fd_d);
        return -1;
    }
    memset(buf_up, 0x00, BUFSIZE);
    buf_cmp = (char *)malloc(BUFSIZE);
    if (!buf_cmp) {
        printf("malloc buf2 error.\n");
        close(fd_d);
        free(buf_up);
        buf_up = NULL;
        return -1;
    }
    memset(buf_cmp, 0x00, BUFSIZE);

    for (i = 0; i <= 1; i++) {
        lseek(fd_d, 0, SEEK_SET);
        done    = 0;
        count   = BUFSIZE;

        while (1) {
            rem = of_one_head->of_one_len - done;
            if (0 == rem)
                break;
            if (rem < BUFSIZE)
                count = rem;

            if (0 == i) {
                if (count < BUFSIZE) {
                    memcpy(buf_up, buf + done, count);
                    memset(buf_up + count, 0x00, (BUFSIZE - count));
                } else {
                    memcpy(buf_up, buf + done, BUFSIZE);
                }
                ret = write(fd_d, buf_up, BUFSIZE);
                if (ret != BUFSIZE) {
                    printf("write error.\n");
                    close(fd_d);
                    free(buf_up);
                    buf_up = NULL;
                    free(buf_cmp);
                    buf_cmp = NULL;
                    return -1;
                }
            }else {
                read(fd_d, buf_cmp, BUFSIZE);
                if (0 != memcmp(buf_up, buf_cmp, BUFSIZE)) {
                    printf("write check error.\n");
                    close(fd_d);
                    free(buf_up);
                    buf_up = NULL;
                    free(buf_cmp);
                    buf_cmp = NULL;
                    return -1;
                }
            }

            done += count;
        }
    }

    close(fd_d);
    free(buf_up);
    buf_up = NULL;
    free(buf_cmp);
    buf_cmp = NULL;

    return 0;
}

void *print_speed(void *data)
{
    while (!status_flag) {
        printf(".\n");
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    int i       = 0;
    int fd      = 0;
    int of_offset       = 0;
    char *file_buf      = NULL;
    char *of_one_buf    = NULL;
    pthread_t thread;
    of_header_t of_head;
    of_one_header_t of_one_head;

    if (argc != 2) {
        printf("unpack file.ota.\n");
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if (-1 == fd) {
        printf("open %s error.\n", argv[1]);
        return -1;
    }
    memset(&of_head, 0x00, sizeof(of_header_t));
    memset(&of_one_head, 0x00, sizeof(of_header_t));

    read(fd, &of_head, sizeof(of_header_t));
    file_buf = (char *)malloc(of_head.file_len - sizeof(of_header_t));
    if (!file_buf) {
        printf("malloc error.\n");
        close(fd);
        return -1;
    }
    memset(file_buf, 0x00, (of_head.file_len - sizeof(of_header_t)));
    read(fd, file_buf, (of_head.file_len - sizeof(of_header_t)));
    if (crc_check(file_buf, (of_head.file_len - sizeof(of_header_t)), of_head.file_crc)) {
        printf("crc check header error.\n");
        return -1;
    } 
    free(file_buf);
    file_buf = NULL;

    pthread_create(&thread, NULL, print_speed, NULL);
    for (i = 0; i < of_head.file_num; i++) {
        lseek(fd, 0, SEEK_SET);
        lseek(fd, of_head.file_offset[i], SEEK_SET);
        read(fd, &of_one_head, sizeof(of_header_t));
        of_one_buf = (char *)malloc(of_one_head.of_one_len);
        if (!of_one_buf) {
            printf("malloc error.\n");
            close(fd);
            return -1;
        }
        memset(of_one_buf, 0x00, of_one_head.of_one_len);
        read(fd, of_one_buf, of_one_head.of_one_len);

        if (update_one_part(&of_one_head, of_one_buf)) {
            printf("update %s error.\n", of_one_head.block_name);
            free(of_one_buf);
            of_one_buf = NULL;
            close(fd);
            return -1;
        }

        free(of_one_buf);
        of_one_buf = NULL;
    }
    status_flag = 1;
    pthread_join(thread, NULL);

    close(fd);

    return 0;
}