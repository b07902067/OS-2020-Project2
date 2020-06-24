
// slave.c
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
#define slave_IOCTL_CREATESOCK 0x12345677
#define slave_IOCTL_MMAP 0x12345678
#define slave_IOCTL_EXIT 0x12345679

int  N;
int device_fd, file_fd[128], file_sz[128];
char method;
char master_method;
char master_ip[16];
char buf[BUFFER_SIZE];
char write_buf[410000];
size_t file_size, ret, device_offset = 0;
struct timeval start_time, end_time;
char *from_device, *to_file;
const char special_char = 'E';
int total_file_size = 0, file_index = 0;
int PAGE_SIZE = 4096;
char *dst;
void open_device();
void establish_connect();
void close_connect();
void clean_all();
int get_size_from_read();
int first_read_size = 0;

int main(int argc, char *argv[]){
    PAGE_SIZE = getpagesize();
    N = atoi(argv[1]);
    for (int i = 0 ; i < N ; i++) {
        if((file_fd[i] = open(argv[i+2] , O_RDWR | O_CREAT | O_TRUNC )) < 0){
            perror("Create file error\n");
            exit(4);
        }
    }
    method = argv[N+2][0];
    strcpy(master_ip , argv[N+3]);
    
    open_device();
    establish_connect();
    
    int begin_index = 0, init = 0;
    begin_index = get_size_from_read();
    if(begin_index != -1)  init = 1;
    //printf("begin_index = %d\n",begin_index);
    
    gettimeofday(&start_time,NULL);
    
    switch(method){
        case 'f' :
            while(file_index < N){
                int len;
                if(init == 1) {
                    init = 0;
                    len = first_read_size;
                }
                else{
                    begin_index = 0;
                    len = read(device_fd, buf, BUFFER_SIZE);
                    if(len == 0) continue;
                }
                //printf("%d\n",len);
                while(begin_index < len){
                    if(file_sz[file_index] >= len-begin_index){
                        write(file_fd[file_index], &buf[begin_index], len-begin_index);
                        file_sz[file_index] -= (len-begin_index);
                        if(file_sz[file_index] == 0) file_index ++;
                        break;
                    }
                    else{
                        write(file_fd[file_index], &buf[begin_index], file_sz[file_index]);
                        begin_index += file_sz[file_index];
                        file_sz[file_index] = 0;
                        file_index ++;
                        if(file_index >= N) break;
                    }
                    
                }
            }
            break;
        case 'm' :
            for(int i = 0; i<N; i++){
                //printf("%d\n",file_sz[i]);
            }
            int init_mmap = 1, mmap_type = 1, mmap_cnt = 0;
            int write_buf_offset = 0;
            while(file_index < N){
                int len;
                if(init == 1) {
                    init = 0;
                    len = first_read_size;
                }
                else{
                    begin_index = 0;
                    lseek(device_fd, device_offset, SEEK_SET);
                    len = read(device_fd, buf, BUFFER_SIZE);
                    device_offset += len;
                    //printf("%s", buf);
                    if(len == 0) continue;
                }
                //printf("file_index = %d , len = %d\n",file_index , len);
                while(begin_index < len){
                    //printf("begin_index = %d , file_index = %d, write_buf_offset = %d\n", begin_index, file_index, write_buf_offset);
                    if(init_mmap){
                        if(file_sz[file_index] < 409600) {
                            mmap_type = 1; // small
                            posix_fallocate(file_fd[file_index], mmap_cnt * 409600, file_sz[file_index]);
                            dst = mmap(NULL, file_sz[file_index], PROT_WRITE, MAP_SHARED, file_fd[file_index], mmap_cnt * 409600);
                        }
                        else {
                            mmap_type = 2; // large
                            posix_fallocate(file_fd[file_index], mmap_cnt * 409600 , 409600);
                            dst = mmap(NULL, 409600, PROT_WRITE, MAP_SHARED, file_fd[file_index], mmap_cnt * 409600);
                        }
                        init_mmap = 0;
                    }
                    if(mmap_type == 1){
                        int rd_len = (file_sz[file_index] < len - begin_index) ? file_sz[file_index] : len-begin_index;
                        memcpy(&write_buf[write_buf_offset], &buf[begin_index], rd_len);
                        write_buf_offset += rd_len;
                        file_sz[file_index] -= rd_len;
                        begin_index += rd_len;
                        if(file_sz[file_index] == 0){
                            memcpy(dst, write_buf, write_buf_offset);
                            mmap_cnt = 0;
                            write_buf_offset = 0;
                            file_index ++;
                            init_mmap = 1;
                        }
                    }
                    if(mmap_type == 2){
                        int rd_len = (409600-write_buf_offset < len-begin_index)? 409600-write_buf_offset : len-begin_index;
                        memcpy(&write_buf[write_buf_offset], &buf[begin_index], rd_len);
                        write_buf_offset += rd_len;
                        file_sz[file_index] -= rd_len;
                        begin_index += rd_len;
                        if(write_buf_offset == 409600){
                            memcpy(dst, write_buf, write_buf_offset);
                            init_mmap = 1;
                            mmap_cnt ++;
                            write_buf_offset = 0;
                            if(file_sz[file_index] == 0) {
                                file_index ++;
                                mmap_cnt = 0;
                            }
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



void open_device(){
    if((device_fd = open("/dev/slave_device" , O_RDWR)) < 0){
        perror("Open device error\n");
        exit(1);
    }
}


void establish_connect(){
    if(ioctl(device_fd , slave_IOCTL_CREATESOCK , master_ip) == -1){
        perror("Connect master error\n");
        exit(2);
    }
}


void close_connect(){
    if(ioctl(device_fd , slave_IOCTL_EXIT) == -1){
        perror("Close connect error\n");
        exit(3);
    }
}


void clean_all(){
    close(device_fd);
    for (int i = 0 ; i < N ; i ++) close(file_fd[i]);
}
 
    
int get_size_from_read(){
    int f_index = 0, END = 0;
    int index = 0, ret = 0;
    int total = 0;
    while(!END){
        ret = read(device_fd, buf, BUFFER_SIZE);
        
        //printf("ret = %d\n",ret);
        if(ret == 0) continue;
        index = 0;
        while(index < ret && f_index < N){
            device_offset ++;
            if(buf[index] == special_char){
                total_file_size += file_sz[f_index];
                f_index ++;
                if(f_index >= N) END = 1;
                index ++;
                continue;
            }
            file_sz[f_index] = file_sz[f_index] * 10 + buf[index] - '0';
            index ++;
        }
        total += index;
    }
    //printf("index = %d\n",index);
    first_read_size = ret;
    if(index < ret) return index;
    else return -1;
}


// src = mmap(NULL , len ,  , slave_device_fd, offset)
// dst = mmap(NULL , len , , file_fd[i], offset)
// memcpy(dst, src, len)
