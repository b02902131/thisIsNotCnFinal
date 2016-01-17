all: server client

server: server.c
	gcc server.c -D CHAT_SERVER -o chat_server

client: client.c
	gcc client.c -o client

clean:
	rm -f chat_server 

put:
	scp ./server.c linux7:~/cn/cn_final/
	scp ./client.c linux7:~/cn/cn_final/
	scp ./Makefile linux7:~/cn/cn_final/
