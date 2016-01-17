all: server.c
	gcc server.c -D CHAT_SERVER -o chat_server
clean:
	rm -f chat_server 

put:
	scp ./server.c linux7:~/cn/cn_final/
	scp ./Makefile linux7:~/cn/cn_final/
