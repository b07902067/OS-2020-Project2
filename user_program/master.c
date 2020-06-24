// master.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define BUFFER_SIZE 512
#define master_IOCTL_CREATESOCK 0x12345677
#define master_IOCTL_MMAP 0x12345678
#define master_IOCTL_EXIT 0x12345679

int N;
int device_fd, file_fd[128], file_sz[128];
char filename[128][128];
char method; // 'f' if file IO , 'm' if mmap
char buf[BUFFER_SIZE];
char write_buf[410000];
size_t file_size, ret, device_offset = 0;
struct timeval start_time, end_time;
char *from_file, *to_device;
const char special_char = 'E';
int total_file_size = 0;
int size_len = 0;
int PAGE_SIZE = 4096;
int write_size = 0, file_index = 0;

int get_size();
void open_device();
void open_connect();
void close_connect();
void clean_all();

int main(int argc, char *argv[]){
    char *check;
    PAGE_SIZE = getpagesize();
    N = atoi(argv[1]);
    for (int i = 0 ; i < N ; i++) {
        strcpy(filename[i],argv[i+2]);
        if((file_fd[i] = open(argv[i+2] , O_RDWR)) < 0){
            perror("Open file error\n");
            exit(4);
        }
    }
    method = argv[N+2][0];
    
    open_device();
    open_connect();
    
    
    size_len = get_size();
    gettimeofday(&start_time,NULL);
    
    switch(method){
        case 'f' :
            for(int i = 0 ; i < N ; i++){
                while(file_sz[i] > 0){
                    size_t ret = (sizeof(buf) > file_sz[i]) ? file_sz[i] : sizeof(buf);
                    ret = read(file_fd[i] , buf , ret);
                    write(device_fd , buf , ret);
                    file_sz[i] -= ret;
                }
            }
            break;
        case 'm' :
            for(int i = 0 ; i < N ; i++){
                int num_of_page = file_sz[i] / PAGE_SIZE ;
                if(file_sz[i] % PAGE_SIZE != 0) num_of_page ++;
                if(num_of_page <= 100){
                    from_file = mmap(NULL, file_sz[i] , PROT_READ , MAP_SHARED , file_fd[i] , 0);
                    memcpy(write_buf, from_file, file_sz[i]);
                    int wr_offset = 0;
                    while(file_sz[i] > 0){
                        int wr_len = (file_sz[i] < BUFFER_SIZE) ? file_sz[i] : BUFFER_SIZE;
                        write(device_fd, &write_buf[wr_offset], wr_len);
                        file_sz[i] -= wr_len;
                        wr_offset += wr_len;
                    }
                }
                else{
                    int offset = 0;
                    while(file_sz[i] > 0){
                        int rd_len = (file_sz[i] < 409600)? file_sz[i] : 409600;
                        from_file = mmap(NULL, rd_len , PROT_READ , MAP_SHARED , file_fd[i] , offset);
                        memcpy(write_buf, from_file, rd_len);
                        offset += rd_len;
                        int wr_offset = 0;
                        while(wr_offset < rd_len){
                            int wr_len = (file_sz[i] < BUFFER_SIZE) ? file_sz[i] : BUFFER_SIZE;
			    //printf("%d\n", wr_len);
                            write(device_fd, &write_buf[wr_offset], wr_len);
                            file_sz[i] -= wr_len;
                            wr_offset += wr_len;
                        }
                        
                    }
                }
            }
            
            break;
        default :
            perror("Method error\n");
            return -1;
    }
    close_connect();
    gettimeofday(&end_time, NULL);
    double trans_time = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)*0.0001;
    
    
    printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, total_file_size);
    
    
    clean_all();
    return 0;
}


int get_size(){
    int total = 0;
    char size_buf[20];
    for(int i = 0 ; i < N ; i++){
        struct stat st_buf;
        stat(filename[i], &st_buf);
        sprintf(size_buf , "%lld%c\0" , st_buf.st_size , special_char);
        int len = strlen(size_buf);
        write(device_fd, size_buf, len);
        total += len;
        total_file_size += st_buf.st_size;
        file_sz[i] = st_buf.st_size;
    }

    return total;
}


void open_device(){
    if((device_fd = open("/dev/master_device" , O_RDWR)) < 0){
        perror("Open device error\n");
        exit(1);
    }
}


void open_connect(){
    if(ioctl(device_fd , master_IOCTL_CREATESOCK) == -1){
        perror("Open connect error\n");
        exit(2);
    }
}

void close_connect(){
    if(ioctl(device_fd , master_IOCTL_EXIT) == -1){
        perror("Close connect error\n");
        exit(3);
    }
}

void clean_all(){
    close(device_fd);
    for (int i = 0 ; i < N ; i ++) close(file_fd[i]);
}
