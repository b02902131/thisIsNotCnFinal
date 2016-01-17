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

char *login_table = "\n======================== Welcome to BigyoChat ========================\n1.Login\n2.Create new account\n===================================================================\nMy selection:\n";


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
                            
                        }//end (state:4 substate:1)
                        
                        //state:3 substate:1(Main table)===========================================
                        else if(s1 == 3 && s2 == 1)
                        {
                            
                        }//end (state:3 substate:1)
                        
                        //state:2 substate:2(Create-passward)==========================================
                        else if(s1 == 2 && s2 == 2)
                        {
                            
                        }//end (state:2 substate:2)
                        
                        //state:2 substate:1(Create-account)===========================================
                        else if(s1 == 2 && s2 == 1)
                        {
                            
                        }//end(state:2 substate:1)
                        
                        //state:1 substate:2(login-passward)===========================================
                        else if(s1 == 1 && s2 == 2)
                        {
                            
                        }//end else if(state:1 substate:2)
                        
                        //state:1 substate:1(login-account)============================================
                        else if(s1 == 1 && s2 == 1)
                        {
                            
                        }//(End)state:1 substate:1(login table)========================================
                        
                        //state:0 substate:0(login table)==============================================
                        else if(s1 == 0 && s2 == 0)
                        {
                            
                        }//(End)state:0 substate:0(login table)========================================
        			}//end else(i == svr.listen_fd)
        		}
        	}//end for(i=0;i<=connect_sum;i++)

        }//end of else(select_return != -1)
    }//end while(1)

}


//===================================================================================================//

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
    r = read(reqP->conn_fd, buf, sizeof(buf));
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