all: server client

server: server.c title.c
	gcc server.c -D CHAT_SERVER -o chat_server

client: client.c
	gcc client.c -o client

clean:
	rm -f chat_server 

put: server
	scp ./title.c linux20:~/cn/cn_final/
	scp ./server.c linux20:~/cn/cn_final/
	scp ./client.c linux20:~/cn/cn_final/
	scp ./Makefile linux20:~/cn/cn_final/
