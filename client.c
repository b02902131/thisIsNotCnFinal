#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>

#define MAXBUF 1024

int main(int argc, char* argv[])
{

    struct sockaddr_in serv_name;
    int status;
    int sockfd,len;
    char buffer[MAXBUF+1];
    fd_set rfds;

    int retval,maxfd = -1;
    
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [port_number]\n", argv[0]);
        exit(1);
    }
    /* create a socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Socket creation");
        exit(1);
    }
    
    /* server address */
    serv_name.sin_family = AF_INET;
    
    struct hostent *hp;
    char *host_str = "linux7.csie.ntu.edu.tw";
    // char *host_str = "127.0.0.1";
    
    hp = gethostbyname(host_str);
    bcopy(hp->h_addr,&serv_name.sin_addr,hp->h_length);

    serv_name.sin_port = htons(atoi(argv[1]));
    
    /* connect to the server */
    status = connect(sockfd, (struct sockaddr*)&serv_name, sizeof(serv_name));
    
    if (status == -1)
    {
        perror("Connection error");
        exit(1);
    }
    
    while(1)
    {
        FD_ZERO(&rfds);
        FD_SET(0,&rfds);
        maxfd = 0;
        
        FD_SET(sockfd,&rfds);
        if(sockfd > maxfd)
            maxfd = sockfd;

        retval = select(maxfd+1,&rfds,NULL,NULL,NULL);
        
        if(retval < 0)
        {
            printf("select error,quit!\n");
            return 0;
        }
        else
        {
            if(FD_ISSET(sockfd,&rfds))
            {
                bzero(buffer,MAXBUF+1);
                len = recv(sockfd, buffer, MAXBUF, 0);
                
                if(len > 0)
                {
                    printf("%s",buffer);
                    
                    //Sending to file server =======================================================//
                    if(buffer[0] == 's' && buffer[1] == 'e' && buffer[2] == 'n' && buffer[3] == 'd' && buffer[4] == 'i' && buffer[5] == 'n' && buffer[6] == 'g' && buffer[7] == ':')
                    {
                        char file_name[20];
                        sscanf(buffer,"sending:%s\n",file_name);
                        
                        FILE *fp = fopen(file_name,"rb");
                        
                        if(fp == NULL)
                        {
                            printf("File: %s is not found!!!\n",file_name);
                            
                        }//end if(fp == NULL)
                        else
                        {
                            while(1)
                            {
                                char buf_file[MAXBUF];
                                int file_block_length = 0;
                                
                                bzero(buf_file,MAXBUF);
                                
                                file_block_length = fread(buf_file,sizeof(char),MAXBUF,fp);
                                
                                if(file_block_length > 0)
                                {
                                    write(sockfd,buf_file,file_block_length);
                                }
                                
                                if (file_block_length < MAXBUF)
                                {
                                    if (ferror(fp))
                                        printf("Error reading\n");
                                    
                                    char *finish_str = "Finish\n";
                                    write(sockfd,finish_str,strlen(finish_str));
                                    
                                    break;
                                }
                                
                            }//end while(1)

                            fclose(fp);
                            
                            printf("File: %s Tansfer Successful!!!\n\n",file_name);
                            
                        }//else(fp == NULL)
                        
                    }//end if(sending:)==============================================================//
                    
                    //Receive file from server ======================================================//
                    else if(buffer[0] == 'R' && buffer[1] == 'e' && buffer[2] == 'c' && buffer[3] == 'e' && buffer[4] == 'i' && buffer[5] == 'v' && buffer[6] == 'i' && buffer[7] == 'n' && buffer[8] == 'g' && buffer[9] == ':')
                    {
                        char file_name[20];
                        sscanf(buffer,"Receiving:%s\n",file_name);
                        
                        FILE *fp = fopen(file_name,"a");
                        
                        if(fp == NULL)
                        {
                            printf("File: %s is not found!!!\n",file_name);
                            exit(1);
                        }//end if(fp == NULL)
                        
                        char buf_file[MAXBUF];
                        int length = 0;
                        
                        bzero(buf_file,MAXBUF);
                        
                        while((length = read(sockfd,buf_file,MAXBUF)) > 0)
                        {
                            int t;
                            int finish_flag=0;
                            for(t=0;t<length;t++)
                            {
                                if(buf_file[t]=='F' && buf_file[t+1] == 'i' && buf_file[t+2]=='n')
                                {
                                    finish_flag = 1;
                                }
                            }
                            
                            if(finish_flag == 1)
                            {
                                break;
                            }
                            
                            fwrite(buf_file,sizeof(char),length,fp);
                            bzero(buf_file,MAXBUF);
                            
                        }//end while((length = read(sockfd,buf_file,MAXBUF)) > 0)
                        
                        if(length < 0)
                        {
                            printf("\n Read Error \n");
                        }
                        
                        printf("Receive File: %s Successful!!!\n\n",file_name);
                        fclose(fp);
                        
                    }//end else if(Receiving:)=======================================================//
                    
                }//end if(len > 0)
                
                else
                {
                    break;
                }//end else(len > 0)
                
            }//end if(FD_ISSET(sockfd,&rfds))
            
            if(FD_ISSET(0,&rfds))
            {
                bzero(buffer, MAXBUF+1);
                fgets(buffer, MAXBUF, stdin);
                    
                len = send(sockfd, buffer, strlen(buffer)-1 , 0);
                
            }//FD_ISSET = 0
            
        }//end else(retval == -1)
        
    }//end while(1)
    
    close(sockfd);
    return 0;
}
