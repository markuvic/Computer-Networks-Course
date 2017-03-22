#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>

#define MAXBUFLEN 102400

typedef enum http_code
{
    OK=200,
    NotFound=404,
    BadRequest=400,
}http_code;

int socket_fd;
struct sockaddr_in serv_addr;	//we need these structures to store socket info
const char* file_name="/index.html";
char root_path[1024]={0};
const int send_size=1024;

const char* num_to_month(int num) {

    assert(num>=1&&num<=12);

	switch (num) {
		case 1: return "Jan";
		case 2: return "Feb";
		case 3: return "Mar";
		case 4: return "Apr";
		case 5: return "May";
		case 6: return "Jun";
		case 7: return "Jul";
		case 8: return "Aug";
		case 9: return "Sep";
		case 10: return "Oct";
		case 11: return "Nov";
		case 12: return "Dec";
		default:
			return "";
	}
}

const char* http_code_to_str(http_code code){
    switch(code){
        case BadRequest:
            return "400 Bad Request";
        case OK:
            return "200 OK";
        case NotFound:
            return "404 Not Found";
    }
    return "";
}

/*print client who log in*/
void printLog(struct sockaddr_in socket_client,char action[],char response[],char path[]) {

	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	printf("%s %02d %02d:%02d:%02d %s:%d %s; %s; %s\n",
        num_to_month(timeinfo->tm_mon+1), timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
        inet_ntoa(socket_client.sin_addr),socket_client.sin_port,action,response,path);
}

int double_equal(double a,double b){
    return fabs(a-b)<1e-8;
}

http_code is_valid_http_request(const char const buffer[],int size,char path[]){
    char action[MAXBUFLEN]={0};
    char http_flag[16]={0};
    double http_version;
    if(sscanf(buffer,"%s %s %[A-Za-z]/%lf\r\n\r\n",action,path,http_flag,&http_version)!=4 || strncasecmp(action,"GET",strlen(action))!=0){
        return BadRequest;
    }

    if(path[0]!='/'){
        return BadRequest;
    }

    if(strlen(http_flag)!=4 || strncasecmp(http_flag,"HTTP",4)!=0 || !double_equal(http_version,1.0)){
        return BadRequest;
    }
    if(strncasecmp(path,"/./",3)==0 ||strncasecmp(path,"/../",4)==0){
        return NotFound;
    }

    char tmp[MAXBUFLEN]={0};

    if(strcmp(path,"/")==0){
        strcpy(path,root_path);
        strcpy(&path[strlen(root_path)],file_name);
    }else{
        strcpy(tmp,root_path);
        strcpy(&tmp[strlen(root_path)],path);
        memset(path,0,MAXBUFLEN);
        strcpy(path,tmp);
    }

    struct stat st={0};
    stat(path,&st);

    if(S_ISDIR(st.st_mode)){
        return NotFound;
    }

    if(access(path,F_OK)!=0){
        return NotFound;
    }
    return OK;
}

void* recv_proc(){

    ssize_t recsize;
    char buffer[MAXBUFLEN];	//data buffer
    struct sockaddr_in cli_addr={0};
    socklen_t cli_len;
    http_code check_result;
    char path[MAXBUFLEN];
    char original_action[MAXBUFLEN];
    char original_response[MAXBUFLEN];

	while(1) {
        memset(buffer,0,MAXBUFLEN);
        memset(original_action,0,MAXBUFLEN);
        memset(path,0,MAXBUFLEN);
        memset(original_response,0,MAXBUFLEN);
		cli_len = sizeof(cli_addr);
		if ((recsize = recvfrom(socket_fd, buffer, MAXBUFLEN, 0, (struct sockaddr *)&cli_addr, &cli_len)) <=0) {
			break;
		}
    int i;
		for(i=0;i<MAXBUFLEN;++i){
            if(buffer[i]!='\r'){
                original_action[i]=buffer[i];
            }else{
                break;
            }
		}

        memset(path,0,MAXBUFLEN);
        check_result=is_valid_http_request(buffer,recsize,path);

        char result[MAXBUFLEN]={0};

        if(check_result==OK){
            FILE *fp=fopen(path,"rt");
            if(fp==NULL){
                check_result=NotFound;
                memset(buffer,0,MAXBUFLEN);
            }else{
                fseek(fp,0,SEEK_END);
                long file_size=ftell(fp);
                fseek(fp,0,SEEK_SET);
                fread(buffer,file_size,1,fp);
                fclose(fp);
            }
        }else{
            memset(buffer,0,MAXBUFLEN);
        }
        strcpy(result,"HTTP/1.0 ");
        strcat(result,http_code_to_str(check_result));
        strcpy(original_response,result);
        strcat(result,"\r\n\r\n");
        strcat(result,buffer);

        int sent_size=0;
        int need_sent_size=0;
        int total_size=strlen(result);
        while(sent_size<total_size){

            if(sent_size+send_size > total_size){
                need_sent_size=total_size-sent_size;
            }else{
                need_sent_size=send_size;
            }

            sent_size+=sendto(socket_fd,&result[sent_size],need_sent_size,0,(struct sockaddr *)&cli_addr, cli_len);
        }

        if(check_result!=OK){
            path[0]=0;
        }
        printLog(cli_addr,original_action,original_response,path);
	}

    return NULL;
}

int main(int argc, char *argv[]) {

	//check if the formal of the input is correct
	if (argc != 3) {
		printf("Usage: %s <port> <directory>\n", argv[0]);
		printf("ERROR, no port/directory provided\n");
		exit(EXIT_FAILURE);
	}

    strcpy(root_path,argv[2]);

	//initialize sockect
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	//fill the information of serves
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;	//choose the ipv4
	serv_addr.sin_addr.s_addr = INADDR_ANY; //set the ip address to INADDR_ANY, let the computer get the local address
	serv_addr.sin_port = htons(atoi(argv[1]));  //set the port number from the input

	//bind a udp socket and check if it is correct
	if (bind(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		perror("error bind failed");
		shutdown(socket_fd,SHUT_RDWR);
		exit(EXIT_FAILURE);
	}

	printf("sws is running on UDP port %s and serving %s\npress q to quit ...\n",argv[1],root_path);

    //start reciving
    pthread_t recv_thread;
	pthread_create(&recv_thread,NULL,recv_proc,NULL);

	while(getchar()!='q');
    shutdown(socket_fd,SHUT_RDWR);
    pthread_join(recv_thread,NULL);

	return 0;
}
