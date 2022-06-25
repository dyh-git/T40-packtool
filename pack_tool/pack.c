/*
  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  cJson库 路径 https://sourceforge.net/projects/cjson/
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "cJSON.h"

#define BLOCK_NAME_LEN	16
#define FILE_NAME_LEN	24
#define VERSION_LEN		16
#define OTA_STATUS_LEN	8
#define PART_NAME_LEN	16

#define FW_NAME_LEN		16
#define FLASH_TYPE_LEN	16

#define OF_NAME_LEN		32
#define OF_DATE_LEN		16
#define OF_TIME_LEN		16
#define OF_OFF_NUM		10		//最多只有10个分区

typedef struct partitions{
	unsigned int block_offset;
	unsigned int block_size;
	char block_name[BLOCK_NAME_LEN];
	char file_name[FILE_NAME_LEN];
	char version[VERSION_LEN];
	char ota_status[OTA_STATUS_LEN];
	char part_name[PART_NAME_LEN];
}partitions_t;

typedef struct pack{
	char fw_name[FW_NAME_LEN];
	char flash_type[FLASH_TYPE_LEN];
	unsigned int flash_size;
	partitions_t *part;
}pack_t;

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

pack_t *g_pack_info 		= NULL;

int get_pack_info(int part_num, cJSON *format)
{
	int i 					= 0;	
	cJSON *json_part_info 	= NULL;
	char part_name[10] 		= {0x00};

	g_pack_info = (pack_t *)malloc(sizeof(pack_t));
	if (!g_pack_info) {
		printf("malloc pack error.\n");

		return -1;
	}
	memset(g_pack_info, 0x00, sizeof(pack_t));

	g_pack_info->part = (partitions_t *)malloc(sizeof(partitions_t) * part_num);
	if (!(g_pack_info->part)) {
		printf("malloc partitions error.\n");
		free(g_pack_info);

		return -1;
	}
	memset(g_pack_info->part, 0x00, (sizeof(partitions_t) * part_num));

	for (i = 0; i < part_num; i++) {
		memset(part_name, 0x00, sizeof(part_name)/sizeof(part_name[0]));
		(void)snprintf(part_name, sizeof(part_name)/sizeof(part_name[0]), "part%d", (i + 1));
		json_part_info = cJSON_GetObjectItem(format, part_name);
		sscanf(cJSON_GetObjectItem(json_part_info, "block_offset")->valuestring, "%x", &(g_pack_info->part[i].block_offset));
		sscanf(cJSON_GetObjectItem(json_part_info, "block_size")->valuestring, "%x", &(g_pack_info->part[i].block_size));
		strncpy(g_pack_info->part[i].block_name, 	cJSON_GetObjectItem(json_part_info, "block_name")->valuestring, BLOCK_NAME_LEN);
		strncpy(g_pack_info->part[i].file_name, 	cJSON_GetObjectItem(json_part_info, "file_name")->valuestring, 	FILE_NAME_LEN);
		strncpy(g_pack_info->part[i].version, 		cJSON_GetObjectItem(json_part_info, "version")->valuestring, 	VERSION_LEN);
		strncpy(g_pack_info->part[i].ota_status, 	cJSON_GetObjectItem(json_part_info, "need_ota")->valuestring, 	OTA_STATUS_LEN);
		strncpy(g_pack_info->part[i].part_name, 	cJSON_GetObjectItem(json_part_info, "part_name")->valuestring, 	PART_NAME_LEN);
	}

	return 0;
}

int ana_cjson_file(void)
{
	cJSON *json_root 		= NULL;
	cJSON *json_part 		= NULL;		 
	FILE *fp 				= NULL;
	char *content 			= NULL;
	unsigned int len 		= 0;
	unsigned int part_num	= 0;

	fp = fopen("partitions.json", "rb");
	if (!fp) {
		printf("open partitions.json error.\n");

		return -1;
	}	
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	content = (char *)malloc(len * sizeof(char));
	if (!content) {
		printf("malloc error.\n");
		fclose(fp);

		return -1;
	}
	fread(content, 1, len, fp);
	fclose(fp);

	json_root = cJSON_Parse(content);
	if (!json_root)
	{
		printf("error before: [%s]\n", cJSON_GetErrorPtr());
		free(content);

		return -1;
	}

	json_part = cJSON_GetObjectItem(json_root, "partitions");
	part_num  = cJSON_GetArraySize(json_part);
	if (get_pack_info(part_num, json_part)) {
		printf("get pack info error.\n");

		return -1;
	}

	strncpy(g_pack_info->fw_name, 		cJSON_GetObjectItem(json_root, "fw_name")->valuestring, FW_NAME_LEN);
	strncpy(g_pack_info->flash_type, 	cJSON_GetObjectItem(json_root, "flash_type")->valuestring, FLASH_TYPE_LEN);
	sscanf(cJSON_GetObjectItem(json_root, "total_size")->valuestring, "%x", &(g_pack_info->flash_size));

	cJSON_Delete(json_root);

	return part_num;
}

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

int make_ota_file(int part_num, char *of_name)
{
	int fd			= 0;
	int fd_tmp		= 0;
	int i 			= 0;
	int tmp_cnt		= 0;
	int ret			= 0;
	char *file_buf 	= NULL;
	struct stat	statbuf;
	unsigned int offset_tmp		= 0;
	unsigned int *offset_buf 	= NULL;
	of_one_header_t	*one_header_tmp	= NULL;
	of_header_t	of_header_tmp;

	fd = open(of_name, O_CREAT | O_RDWR, 0644);
	if (-1 == fd) {
		printf("open %s fail.\n", of_name);

		return -1;
	}

	memset(&of_header_tmp, 0x00, sizeof(of_header_t));
	write(fd, &of_header_tmp, sizeof(of_header_t));
	offset_tmp = sizeof(of_header_t);

	for (i = 0; i < part_num; i++) {
		if (!(strcmp("no", g_pack_info->part[i].ota_status))) {
			continue;
		}

		fd_tmp = open(g_pack_info->part[i].file_name, O_RDONLY);
		if (-1 == fd_tmp) {
			printf("open %s fail.\n", g_pack_info->part[i].file_name);

			close(fd);
			return -1;
		}
		memset(&statbuf, 0x00, sizeof(struct stat));
		fstat(fd_tmp, &statbuf);
		if (statbuf.st_size > g_pack_info->part[i].block_size) {
			printf("file %s is too big.\n", g_pack_info->part[i].file_name);

			ret = -1;
			goto f_end;
		}

		file_buf = (char *)malloc((sizeof(char) * statbuf.st_size) + sizeof(of_one_header_t));
		if (!file_buf) {
			printf("malloc error.\n");

			ret = -1;
			goto f_end;
		}

		memset(file_buf, 0x00, ((sizeof(char) * statbuf.st_size) + sizeof(of_one_header_t)));
		read(fd_tmp, (file_buf + sizeof(of_one_header_t)), statbuf.st_size);
		close(fd_tmp);
		//填充子文件头信息
		one_header_tmp = (of_one_header_t *)file_buf;
		one_header_tmp->of_one_len = statbuf.st_size;
		strncpy(one_header_tmp->block_name, 	g_pack_info->part[i].block_name, BLOCK_NAME_LEN);
		strncpy(one_header_tmp->file_name, 		g_pack_info->part[i].file_name, FILE_NAME_LEN);
		strncpy(one_header_tmp->version, 		g_pack_info->part[i].version, VERSION_LEN);
		strncpy(one_header_tmp->ota_status, 	g_pack_info->part[i].ota_status, OTA_STATUS_LEN);

		write(fd, file_buf, (sizeof(of_one_header_t) + statbuf.st_size));

		free(file_buf);
		file_buf = NULL;

		//填充总文件头信息，各个模块的偏移位置，模块的数量
		of_header_tmp.file_offset[tmp_cnt++] 	 = offset_tmp;
		of_header_tmp.file_num++;
		offset_tmp 								+= (sizeof(char) * statbuf.st_size) + sizeof(of_one_header_t);				
	}

	//填充总文件头信息
	of_header_tmp.file_len = offset_tmp;
	strncpy(of_header_tmp.of_name, of_name, OF_NAME_LEN);
	strncpy(of_header_tmp.of_date, __DATE__, OF_DATE_LEN);
	strncpy(of_header_tmp.of_time, __TIME__, OF_TIME_LEN);

	//填充总文件头信息，制作CRC值
	lseek(fd, sizeof(of_header_t), SEEK_SET);
	file_buf = (char *)malloc((of_header_tmp.file_len - sizeof(of_header_t)));
	if (!file_buf) {
		printf("malloc file_buf error.\n");
		close(fd);
		return -1;
	}
	read(fd, file_buf, (of_header_tmp.file_len - sizeof(of_header_t)));
	of_header_tmp.file_crc = make_crc((unsigned char *)file_buf, (of_header_tmp.file_len - sizeof(of_header_t)));
	lseek(fd, 0, SEEK_SET);
	write(fd, &of_header_tmp, sizeof(of_header_t));

	close(fd);

	return 0;
f_end:
	close(fd_tmp);
	close(fd);

	return ret;
}

int make_whole_flash_file(int part_num, char *wf_name)
{
	int fd			= 0;
	int fd_tmp		= 0;
	int i 			= 0;
	int ret			= 0;
	char *ff_buf 	= NULL;
	char *file_buf 	= NULL;
	struct stat	statbuf;

	fd = open(wf_name, O_CREAT | O_RDWR, 0644);
	if (-1 == fd) {
		printf("open %s fail.\n", wf_name);

		return -1;
	}

	for (i = 0; i < part_num; i++) {
		if ((!strcmp("data.bin", g_pack_info->part[i].file_name)) || ((!strcmp("backup.bin", g_pack_info->part[i].file_name)))
			|| (!strcmp("cfg.bin", g_pack_info->part[i].file_name))) {
			ff_buf = (char *)malloc(sizeof(char) * (g_pack_info->part[i].block_size));
			if (!ff_buf) {
				printf("malloc zero_buf error.\n");
			}
			memset(ff_buf, 0xFF, (sizeof(char) * (g_pack_info->part[i].block_size)));
			write(fd, ff_buf, g_pack_info->part[i].block_size);
			free(ff_buf);
			ff_buf = NULL;

			continue;
		}

		fd_tmp = open(g_pack_info->part[i].file_name, O_RDONLY);
		if (-1 == fd_tmp) {
			printf("open %s fail.\n", g_pack_info->part[i].file_name);

			close(fd);
			return -1;
		}
		memset(&statbuf, 0x00, sizeof(struct stat));
		fstat(fd_tmp, &statbuf);
		if (statbuf.st_size > g_pack_info->part[i].block_size) {
			printf("file %s is too big.\n", g_pack_info->part[i].file_name);

			ret = -1;
			goto f_end;
		}

		file_buf = (char *)malloc(sizeof(char) * statbuf.st_size);
		if (!file_buf) {
			printf("malloc error.\n");

			ret = -1;
			goto f_end;
		}
		memset(file_buf, 0x00, (sizeof(char) * statbuf.st_size));
		read(fd_tmp, file_buf, statbuf.st_size);
		write(fd, file_buf, statbuf.st_size);

		free(file_buf);
		file_buf = NULL;

		ff_buf = (char *)malloc(sizeof(char) * ((g_pack_info->part[i].block_size) - statbuf.st_size));
		if (!ff_buf) {
			printf("malloc zero_buf error.\n");

			ret = -1;
			goto f_end;
		}
		memset(ff_buf, 0xFF, (sizeof(char) * ((g_pack_info->part[i].block_size) - statbuf.st_size)));
		write(fd, ff_buf, (g_pack_info->part[i].block_size) - statbuf.st_size);
		free(ff_buf);
		ff_buf = NULL;
		close(fd_tmp);
	}

	close(fd);

	return 0;
f_end:
	close(fd_tmp);
	close(fd);

	return ret;
}

int make_update_files(int part_num)
{
	char of_name[32] 	= {0x00};	
	char wf_name[50] 	= {0x00};

	memset(of_name, 0x00, sizeof(of_name)/sizeof(of_name[0]));
	(void)snprintf(of_name, sizeof(of_name)/sizeof(of_name[0]), "%s%s", g_pack_info->fw_name, "_ota_file.bin");
	if (-1 != access(of_name, F_OK)) {
		unlink(of_name);
	}

	snprintf(wf_name, sizeof(wf_name)/sizeof(wf_name[0]), "%s%s", g_pack_info->fw_name, "_whole_file.bin");
	snprintf(wf_name, sizeof(wf_name)/sizeof(wf_name[0]), "%s%s", g_pack_info->fw_name, "_whole_file.bin");
	if (-1 != access(wf_name, F_OK)) {
		unlink(wf_name);
	}

	if (make_ota_file(part_num, of_name)) {
		printf("make ota file fail.\n");

		return -1;
	}

	if (make_whole_flash_file(part_num, wf_name)) {
		printf("make whole flash file fail.\n");

		return -1;
	}

	return 0;
}

int main (int argc, const char * argv[])
 {
	int part_num = 0;

	if (argc < 2) {
		printf("format error.\n");
		printf("./pack partitions.json\n");
		
		return -1;
	}

	part_num = ana_cjson_file();
	if (part_num < 0) {
		printf("analysis cjson file fail.\n");

		goto end;
	}

	if (make_update_files(part_num)) {
		printf("make update files fail.\n");

		goto end;
	}

end:
	free(g_pack_info->part);
	g_pack_info->part 	= NULL;
	free(g_pack_info);
	g_pack_info			= NULL;	

	return 0;
}
