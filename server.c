#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "title.c"

#define ERR_EXIT(a) { perror(a); exit(1); }
#define MAX_DATA 1000
#define max_member 50
#define MAXBUF 1024

typedef struct
{
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct
{
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    
    char account[20];
    char passward[20];
    
    char history[MAX_DATA];
    
    int state;
    int substate;
    
    int correct;
    int is_online;
    
    int login_error;
    
    int send_to_mem_id;
    
} request;

typedef struct
{
    char account[20];
    char passward[20];
    int online;//1:online /0:offline
    int busy;//1:busy /0:not busy
}member;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list


// my function
static void broadcast(char * buf, member * mem, int member_list_len, int connect_sum);
static void setMemOB(int conn_fd, int online, int busy, member * mem, int member_list_len);
static void printMainTable(int conn_fd, member* mem, int member_list_len, int connect_sum);
static void sendUI(int conn_fd, char * ui);
static void changeStateAndSendUI(int conn_fd, int state, int substate, char * ui);
static void changeState(int conn_fd, int state, int substate);
// my function


static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void init_men(member* men);
// initailize a member instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
//static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error

int main(int argc,char *argv[]){
	struct sockaddr_in cliaddr; // for accept()

	int clilen;
    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    char buf_rev[512];
    int buf_len;
    int i,j,ret;

    fd_set  active_fd_set;
    int sock_fd;
    int connect_sum;

    //Parse args.
    if(argc != 2)
    {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    //Load member list from member.txt
    FILE *fp = fopen("member.txt","a+");

    char member_buf[50];	
    int member_list_len;
    char temp_account[20];
    char temp_passward[20];

    member* mem = NULL;
    mem = (member*)malloc(sizeof(member) * max_member);

    if(mem == NULL)
    {
    	ERR_EXIT("out of memory allocating all requests");
    }

    i = -1;
    while(fgets(member_buf,50,fp) != NULL)
    {
        i++;
        sscanf(member_buf,"%s %s",temp_account,temp_passward);
        strcpy(mem[i].account,temp_account);
        strcpy(mem[i].passward,temp_passward);
    }
    member_list_len = i;
    for(i=0;i<max_member;i++){
    	mem[i].online = 0;
    	mem[i].busy = 0;
    }

	// Initialize server
    init_server((unsigned short) atoi(argv[1]));
    //init_server(pport);

    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if(requestP == NULL)
    {
        ERR_EXIT("out of memory allocating all requests");
    }
    for(i=0;i<maxfd;i++)
    {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);
    clilen = sizeof(cliaddr);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    //Initialize select
    FD_ZERO(&active_fd_set);
    FD_SET(svr.listen_fd,&active_fd_set);
    connect_sum = svr.listen_fd;
    while(1)
    {
    	fd_set read_set;
    	int select_return;

    	read_set = active_fd_set;

    	select_return = select(connect_sum+1,&read_set,NULL,NULL,NULL);

    	if(select_return == -1)
        {
            perror("Fail to select");
            return -1;
            
        }//end if(select_ret == -1)

        else
        {
        	for(i=0;i<=connect_sum;i++)
        	{
        		if(FD_ISSET(i,&read_set))
        		{
        			//new connection
        			if(i == svr.listen_fd)
        			{
        				clilen = sizeof(cliaddr);
        				conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);

        				if(conn_fd == -1)
        				{
        					perror("Fail to accept()");
        					return -1;
        				}

        				FD_SET(conn_fd, &active_fd_set);

        				if(conn_fd > connect_sum)
        					connect_sum = conn_fd;

        				init_request(&requestP[conn_fd]);

        				requestP[conn_fd].conn_fd = conn_fd;
        				strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
						fprintf(stderr, "getting a new request... fd %d from %d\n", conn_fd, requestP[conn_fd].conn_fd);

						sprintf(buf, "%s", login_table);
						write(requestP[conn_fd].conn_fd, buf, strlen(buf));

						FD_CLR(svr.listen_fd,&read_set);
        			}//end if(i == svr.listen_fd)

        			//receive request
        			else
        			{
        				conn_fd = i;
                        if(conn_fd < 0)
                        {
                            if (errno == EINTR || errno == EAGAIN)
                                continue;
                            
                            if (errno == ENFILE)
                            {
                                (void)fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                                continue;
                            }
                            
                            ERR_EXIT("accept")
                        }

                        int new_fd = requestP[conn_fd].conn_fd;
                        char buf_rev[512];

                        bzero(buf_rev,512);
                        ret = recv(new_fd,buf_rev,512,0);

                        if(ret < 0)
                        {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }

                        strcpy(requestP[conn_fd].buf,buf_rev);
                        int len = strlen(requestP[conn_fd].buf);
                        printf("return buf:%s",requestP[conn_fd].buf);
                        printf("  buf_len:%d\n",len);

                        int s1 = requestP[conn_fd].state;
                        int s2 = requestP[conn_fd].substate;

                        //state:8 substate:2(File Transfer-Sending finish)=============================
                        if(s1 == 8 && s2 == 2){

                        }//end state:8 substate:2(File Transfer-Sending finish)=============================

                        //state:8 substate:1(File Transfer-Receiving)==================================
                        else if(s1 == 8 && s2 == 1)
                        {

                        }//end (state:8 substate:1)

                        //state:7 substate:1(Offline Message)=====================================
                        else if(s1 == 7 && s2 == 1)
                        {
                            
                        }//end (state:7 substate:1)

                        //state:6 substate:1(Historical Message)==================================
                        else if(s1 == 6 && s2 == 1)
                        {
                            
                        }//end (state:6 substate:1)

                        //state:5 substate:3(File Transfer- Enter file name)===========================
                        else if(s1 == 5 && s2 == 3)
                        {
                            
                        }//end (state:5 substate:3)

                        //state:5 substate:2(File Transfer- Enter friend ID)===========================
                        else if(s1 == 5 && s2 == 2)
                        {
                            
                        }//end (state:5 substate:2)

                        //state:5 substate:1(File transformation)==================================
                        else if(s1 == 5 && s2 == 1)
                        {
                            
                        }//end (state:5 substate:1)
                        
                        //state:4 substate:1(Chat Room)============================================
                        else if(s1 == 4 && s2 == 1)
                        {
                            if(strcmp(requestP[conn_fd].buf,"/Home") == 0 || strcmp(requestP[conn_fd].buf,"/home") == 0)
                            {
                                changeStateAndSendUI(conn_fd,3,1,main_menu);
                                //broad cast
                                sprintf(buf, "[system] %s exits the room.\n\n", requestP[conn_fd].account);
                                broadcast(buf, mem, member_list_len, connect_sum);
                            }
                            else
                            {
                                char str[100];
                                strcpy(str, requestP[conn_fd].buf);

                                int isPM = 0;
                                char *pm_to;
                                if(str[0] == '/'){
                                    isPM = 1;
                                    pm_to = strtok (&str[1]," ");
                                }
                                printf("318: isPM = %d, pm_to = %s\n",isPM, pm_to);
                                printf("319: member_list_len = %d \n", member_list_len);
                                
                                if(isPM == 0)
                                    sprintf(buf,"%s: %s\n",requestP[conn_fd].account, requestP[conn_fd].buf);
                                else 
                                    sprintf(buf,"[toe toe talk] %s: %s\n",requestP[conn_fd].account, &requestP[conn_fd].buf[2+strlen(pm_to)]);

                                if(isPM){
                                    // deal with account doesn't exist
                                    int account_avaible = 0;
                                    for(i=0;i<=member_list_len;i++){
                                        if(strcmp(pm_to, mem[i].account) == 0){
                                            account_avaible = 1;
                                            break;
                                        }
                                    }
                                    if(account_avaible == 0){
                                        sendUI(conn_fd,"this ID doesn't exit\n\n");
                                        break;
                                    }
                                    // deal with PM self
                                    if(strcmp(pm_to, requestP[conn_fd].account) == 0){
                                        sendUI(conn_fd,"Don't talk to youself, idiot!\n\n");
                                        break;
                                    }
                                }

                                time_t curtime;
                                struct tm *loctime;

                                curtime = time(NULL);

                                char str_time[30];
                                sprintf(str_time,"%s\n", asctime(localtime(&curtime)));

                                for(i=0;i<=member_list_len;i++){
                                    printf("335: mem[%d].account = %s\n", i, mem[i].account);
                                    int isReceiver = 0;
                                    if(!isPM) isReceiver = 1;
                                    else if (isPM && (strcmp(pm_to, mem[i].account) == 0)) isReceiver = 1;
                                    if(!isReceiver) continue;
                                    
                                    char fp_open_name[20];
                                    FILE *fp_record;
                                    
                                    if(mem[i].online == 1){
                                        printf("345:\n");
                                        for(j=0;j<=connect_sum;j++){
                                            printf("347: requestP[%d] = %s\n", j, requestP[j].account);
                                            //mem[i].account = requestP[j].account
                                            if(strcmp(mem[i].account, requestP[j].account) == 0){
                                                //Historical message
                                                if(requestP[j].state == 4 && requestP[j].substate == 1)
                                                {
                                                    printf("352: p = %s\n", requestP[j].account);
                                                    sprintf(fp_open_name,"history_%s.txt",requestP[j].account);
                                                    fp_record = fopen(fp_open_name,"a");
                                                    
                                                    fputs(buf,fp_record);
                                                    fputs(str_time,fp_record);
                                                    fclose(fp_record);

                                                    //Do not send msg back to the sender
                                                    if(requestP[j].conn_fd != requestP[conn_fd].conn_fd)
                                                        write(requestP[j].conn_fd,buf,strlen(buf));
                                                    write(requestP[j].conn_fd,str_time,strlen(str_time));
                                                }
                                            }
                                        }//end for j loop
                                    }//end if(mem[i].online == 1) 
                                    else if(mem[i].online == 0){
                                        sprintf(fp_open_name,"offline_%s.txt",mem[i].account);
                                        fp_record = fopen(fp_open_name,"a");

                                        fputs(buf,fp_record);
                                        fputs(str_time,fp_record);
                                        fclose(fp_record);

                                        if(isPM) {
                                            sendUI(conn_fd, "this account is not online\n\n");
                                            break;
                                        }
                                    }
                                }
                                if(isPM){
                                    //if reach here, means PM success
                                    sendUI(conn_fd, str_time);
                                }
                            }// end else
                            break;
                        }//end (state:4 substate:1)
                        
                        //state:3 substate:1(Main table)===========================================
                        else if(s1 == 3 && s2 == 1)
                        {   
                            
                            int main_select = atoi(requestP[conn_fd].buf);
                            if(main_select == 1)
                            {
                                changeState(conn_fd,4,1);
                                printMainTable(conn_fd, mem, member_list_len, connect_sum);
                                sendUI(conn_fd,offline_end);
                                sendUI(conn_fd,chat_title);

                                //broad cast
                                sprintf(buf, "[system] %s gets in the room.\n\n", requestP[conn_fd].account);
                                broadcast(buf, mem, member_list_len, connect_sum);
                                break;
                            }
                            else if(main_select == 2)
                            {
                                changeStateAndSendUI(conn_fd,5,1,file_tran_title);
                            }
                            else if(main_select == 3)
                            {
                                char filename[20];
                                sprintf(filename, "history_%s.txt",requestP[conn_fd].account);

                                FILE *file_p;
                                file_p = fopen(filename,"r");
                                char tmp_history[128];

                                sendUI(conn_fd, history_title);
                                if(!file_p){
                                    sendUI(conn_fd,"\n there is no record. \n\n");
                                }
                                else{
                                    while(fgets(tmp_history,128,file_p) != NULL){
                                        sendUI(conn_fd, tmp_history);
                                    }
                                }
                                sendUI(conn_fd, history_end);
                                changeState(conn_fd,6,1);
                            }
                            else if(main_select == 4)
                            {

                            }
                            else if(main_select == 5)
                            {

                                sendUI(conn_fd, main_exit);
                                setMemOB(conn_fd,0,0,mem,member_list_len);
                                changeState(conn_fd,0,0);

                                close(conn_fd);
                                FD_CLR(conn_fd, &active_fd_set);
                                break;
                            }
                            else
                            {
                                changeStateAndSendUI(conn_fd,3,1,select_err);
                                printMainTable(conn_fd, mem, member_list_len, connect_sum);
                                sendUI(conn_fd, main_menu);
                            }
                            break;
                        }//end (state:3 substate:1)
                        
                        //state:2 substate:2(Create-passward)==========================================
                        else if(s1 == 2 && s2 == 2)
                        {
                            strcpy(requestP[conn_fd].passward,requestP[conn_fd].buf);
                            requestP[conn_fd].correct = 0;
                            if(strlen(requestP[conn_fd].passward) >= 12)
                            {
                                changeStateAndSendUI(conn_fd,2,1,register_pwd_long);
                                sendUI(conn_fd,register_account);
                            }
                            else if(strlen(requestP[conn_fd].passward) == 0)
                            {
                                changeStateAndSendUI(conn_fd,2,1,register_pwd_empty);
                                sendUI(conn_fd,register_account);
                            }
                            else{
                                changeState(conn_fd,3,1);

                                member_list_len++;
                                strcpy(mem[member_list_len].account,requestP[conn_fd].account);
                                strcpy(mem[member_list_len].passward,requestP[conn_fd].passward);
                                mem[member_list_len].online = 1;

                                //TODO: update member_list
                                //TODO: write into file

                                printMainTable(conn_fd, mem, member_list_len, connect_sum);
                                sendUI(conn_fd, main_menu);
                            }
                            break;
                        }//end (state:2 substate:2)

                            
                        
                        //state:2 substate:1(Create-account)===========================================
                        else if(s1 == 2 && s2 == 1)
                        {
                            strcpy(requestP[conn_fd].account,requestP[conn_fd].buf);

                            requestP[conn_fd].correct = 0;

                            //Account is longer than 10 words
                            if(strlen(requestP[conn_fd].account) >= 12)
                            {
                                changeState(conn_fd,2,1);
                                sendUI(conn_fd,register_account_long);
                                sendUI(conn_fd,register_account);
                            }
                            //you do not enter any account
                            else if(strlen(requestP[conn_fd].account) == 0)
                            {
                                changeState(conn_fd,2,1);
                                sendUI(conn_fd, register_account_empty);
                                sendUI(conn_fd, register_account);
                            }
                            //you have enter any account and its length is correct
                            else {
                                //check whether the account has been used
                                for(i=0;i<=member_list_len;i++)
                                {
                                    if(strcmp(requestP[conn_fd].account,mem[i].account) == 0)
                                    {
                                        requestP[conn_fd].correct ++;
                                    }
                                }
                                //Account has been use
                                if(requestP[conn_fd].correct != 0)
                                {
                                    changeState(conn_fd,2,1);
                                    sendUI(conn_fd, register_account_used);
                                    sendUI(conn_fd, register_account);
                                }
                                else 
                                {
                                    changeStateAndSendUI(conn_fd, 2,2, register_pwd);
                                }
                            }
                            break;
                        }//end(state:2 substate:1)
                        
                        //state:1 substate:2(login-passward)===========================================
                        else if(s1 == 1 && s2 == 2)
                        {
                            strcpy(requestP[conn_fd].passward,requestP[conn_fd].buf);

                            requestP[conn_fd].correct = 0;
                            requestP[conn_fd].is_online = 0;

                            for(i=0;i<=member_list_len;i++)
                            {
                                printf("%s\n", mem[i].account);
                                if(strcmp(requestP[conn_fd].account,mem[i].account) == 0 && strcmp(requestP[conn_fd].passward,mem[i].passward) == 0)
                                {
                                    requestP[conn_fd].correct = 2;
                                    if(mem[i].online == 1){
                                        requestP[conn_fd].is_online = 1;
                                    }
                                    else{
                                        requestP[conn_fd].is_online = 0;
                                        mem[i].online = 1;
                                    }
                                    break;
                                }
                            }

                            if(requestP[conn_fd].correct != 2)
                            {
                                requestP[conn_fd].login_error++;
                                printf("login_error:%d\n",requestP[conn_fd].login_error);
                            }
                            //Correct account & no online
                            if(requestP[conn_fd].correct == 2 && requestP[conn_fd].is_online == 0)
                            {
                                changeState(conn_fd,3,1);
                                printMainTable(conn_fd, mem, member_list_len, connect_sum);
                                sendUI(conn_fd, main_menu);
                            }
                            else if(requestP[conn_fd].correct == 2 &&requestP[conn_fd].is_online == 1)
                            {
                                changeStateAndSendUI(conn_fd,1,1,login_online);
                                sendUI(conn_fd, login_account);
                            }
                            else
                            {
                                changeState(conn_fd,0,0);
                                sendUI(conn_fd,login_error);
                                sendUI(conn_fd,login_table);
                            }
                            break;
                        }//end else if(state:1 substate:2)
                        
                        //state:1 substate:1(login-account)============================================
                        else if(s1 == 1 && s2 == 1)
                        {
                            strcpy(requestP[conn_fd].account,requestP[conn_fd].buf);
                            if(strlen(requestP[conn_fd].account) == 0)
                            {
                                sendUI(conn_fd, login_account_empty);
                                sendUI(conn_fd, login_account);
                            }
                            else 
                            {
                                changeStateAndSendUI(conn_fd,1,2,login_pwd);
                            }
                        }//(End)state:1 substate:1(login table)========================================
                        
                        //state:0 substate:0(login table)==============================================
                        else if(s1 == 0 && s2 == 0)
                        {
                            int login_table_input = atoi(requestP[conn_fd].buf);

                            if(login_table_input != 1 && login_table_input != 2 && login_table_input != 3){
                                changeStateAndSendUI(conn_fd, 0, 0, select_err);
                                sendUI(conn_fd, login_table);   
                            }
                            else if(login_table_input == 1){
                                requestP[conn_fd].login_error = 0;
                                changeStateAndSendUI(conn_fd,1,1,login_account);
                            }
                            else if(login_table_input == 2){
                                changeStateAndSendUI(conn_fd,2,1,register_account);
                            }
                            else if(login_table_input == 3){
                                sendUI(conn_fd, main_exit);
                                setMemOB(conn_fd,0,0,mem,member_list_len);
                                changeState(conn_fd,0,0);
                                close(conn_fd);
                                FD_CLR(conn_fd, &active_fd_set);
                            }
                            break;
                        }//(End)state:0 substate:0(login table)========================================
        			}//end else(i == svr.listen_fd)
        		}
        	}//end for(i=0;i<=connect_sum;i++)

        }//end of else(select_return != -1)
    }//end while(1)

}


//my function ============================================================================================//
static void broadcast(char * buf, member * mem, int member_list_len, int connect_sum){
    int i,j;
    for(i=0;i<=member_list_len;i++){
        if(mem[i].online == 1){
            for(j=0;j<=connect_sum;j++){
                if(strcmp(mem[i].account, requestP[j].account) == 0){
                    if(requestP[j].state == 4){
                        sendUI(j, buf);
                    }
                }
            }
        }
    }
}

static void setMemOB(int conn_fd, int online, int busy, member * mem, int member_list_len){
    int i;
    for(i=0;i<=member_list_len;i++){
        if(strcmp(requestP[conn_fd].account,mem[i].account) == 0){
            mem[i].online = online;
            mem[i].busy = busy;
        }
    }
}

static void printMainTable(int conn_fd, member * mem, int member_list_len, int connect_sum){
    int i, j;
    char buf[512];
    for(i=0;i<=member_list_len;i++)
        for(j=0;j<=connect_sum;j++){
            if(strcmp(mem[i].account,requestP[j].account) == 0){
                if(mem[i].online == 1 && requestP[j].state == 4 && requestP[j].substate == 1)
                    mem[i].busy = 1;
                else
                    mem[i].busy = 0;
            }
        }
    sendUI(conn_fd, main_title);

    char hello_title[20];
    sprintf(hello_title,"Hello,%s",requestP[conn_fd].account);
    sendUI(conn_fd, hello_title);
    sendUI(conn_fd, main_friend_title1);
    sendUI(conn_fd, main_friend_title2);
    sendUI(conn_fd, main_friend_title3);

    for(i=0;i<=member_list_len;i++){
        if(mem[i].online == 0){
            if(mem[i].busy == 0){
                sprintf(buf,"                                            %d              %s\n",i+1,mem[i].account);
                write(requestP[conn_fd].conn_fd, buf, strlen(buf));
            }
        }
        else{
            if(mem[i].busy == 0){
                sprintf(buf,"       *                                    %d              %s\n",i+1,mem[i].account);
                write(requestP[conn_fd].conn_fd, buf, strlen(buf));
            }
            else{
                sprintf(buf,"       *                *                   %d              %s\n",i+1,mem[i].account);
                write(requestP[conn_fd].conn_fd, buf, strlen(buf));
            }    
        }//end else(mem[i].online == 0)    
    }//end for-loop

}

static void sendUI(int conn_fd, char * ui){
    char buf[512];
    sprintf(buf,"%s",ui);
    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
}

static void changeStateAndSendUI(int conn_fd, int state, int substate, char * ui){
    requestP[conn_fd].state = state;
    requestP[conn_fd].substate = substate;
    char buf[512];
    sprintf(buf,"%s",ui);
    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
}

static void changeState(int conn_fd, int state, int substate){
    requestP[conn_fd].state = state;
    requestP[conn_fd].substate = substate;
}

//my function ============================================================================================//

#include <fcntl.h>

static void* e_malloc(size_t size);

static void init_men(member* men)
{
    men -> online = 0;
    men -> busy = 0;
}

static void init_request(request* reqP)
{
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->state = 0;
    reqP->substate = 0;
    reqP->send_to_mem_id = 0;
}

static void free_request(request* reqP)
{
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP)
{
    int r;
    char buf[512];
    
    // Read in request from client
    r = read(reqP->conn_fd, buf , sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    // be careful that in Windows, line ends with \015\012
    if (p1 == NULL) {
        p1 = strstr(buf, "\012");
        newline_len = 1;
        if (p1 == NULL) {
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port)
{
    struct sockaddr_in servaddr;
    int tmp;
    
    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;
    
    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0)
    {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0)
    {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size)
{
    void* ptr;
    
    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}