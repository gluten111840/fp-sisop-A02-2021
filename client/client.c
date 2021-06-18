#include <stdio.h>		//printf
#include <string.h>		//strlen
#include <sys/socket.h> //socket
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>
#include <wait.h>
#include <ctype.h>
#include <stdlib.h>

#define SIZE_BUF 100

int handleLog(int server, char uname[SIZE_BUF], char pswrd[SIZE_BUF]);

void recieveInput(const char *title, char str[])
{
	printf("\e[0m%s\n> \e[36m", title);
	scanf("%s", str);
	str[strlen(str)] = '\0';
	printf("\e[0m");
}

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in server;
	char uname[1024], pswrd[1024], unpass[30000];
	char message[1024], server_reply[1024];
	int tunggu;
	//Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(!getuid())
	{
		if (sock == -1)
		{
			printf("Could not create socket");
		}
		puts("Socket created");

		server.sin_addr.s_addr = inet_addr("127.0.0.1");
		server.sin_family = AF_INET;
		server.sin_port = htons(7000);

		//Connect to remote server
		if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
		{
			perror("connect failed. Error");
			return 1;
		}

		puts("\e[33mConnected.\e[0m\n");

		printf("Welcome to the A02DB monitor. Commands end with 'enter'.\n");
		printf("Your A02DB connection is running\n");
		printf("Server version: 1.0.0-A02DB mendingdb.org binary distribution\n\n");
		printf("Copyright (c) 2021, A02 Berdua Doang.\n\n");
		printf("Press ctrl + c to force stop.\n\n");

		/* TO DO
		TAMBAHIN CODE BUAT KIRIM AUTENTIKASI KE SERVER DISINI
		BIAR SERVER BISA BEDAIN SUDO ATAU ENGGA
		*/

		if (server_reply == "User not found")
		{
			printf("Login failed!\n");
			exit(1);
		}
		int root = 0;
		//keep communicating with server
		int commandTrue = 0;
		while (1)
		{
			printf("A02DB[root] >> ");
			fgets(message, sizeof(message), stdin);

			//Send some data
			if (send(sock, message, 1024, 0) < 0)
			{
				puts("Send failed");
				return 1;
			}

			//Receive a reply from the server
			if (recv(sock, server_reply, 1024, 0) < 0)
			{
				puts("recv failed");
				break;
			}

			puts("Server reply :");
			puts(server_reply);
			memset(&server_reply, '\0', sizeof(server_reply));
			
		}
	}
	else
	{
		if (sock == -1)
		{
			printf("Could not create socket");
		}
		puts("Socket created");

		server.sin_addr.s_addr = inet_addr("127.0.0.1");
		server.sin_family = AF_INET;
		server.sin_port = htons(7000);

		//Connect to remote server
		if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
		{
			perror("connect failed. Error");
			return 1;
		}

		puts("\e[33mConnected.\e[0m\n");

		printf("Welcome to the A02DB monitor. Commands end with 'enter'.\n");
		printf("Your A02DB connection is running\n");
		printf("Server version: 1.0.0-A02DB mendingdb.org binary distribution\n\n");
		printf("Copyright (c) 2021, A02 Berdua Doang.\n\n");
		printf("Press ctrl + c to force stop.\n\n");

		/* TO DO
		TAMBAHIN CODE BUAT KIRIM AUTENTIKASI KE SERVER DISINI
		BIAR SERVER BISA BEDAIN SUDO ATAU ENGGA
		*/

		strcpy(uname, argv[2]);
		strcpy(pswrd, argv[4]);
		sprintf(unpass, "%s:::%s:::\n", uname, pswrd);
		send(sock, unpass, sizeof(unpass), 0);
		while((wait(&tunggu)) > 0);
		recv(sock, server_reply, 1024, 0);
		if (strcmp(server_reply, "User not found")==0)
		{
			printf("Login failed!\n");
			exit(1);
		}
		int root = 0;
		//keep communicating with server
		int commandTrue = 0;
		while (1)
		{
			printf("A02DB[%s] >> ", argv[2]);
			fgets(message, sizeof(message), stdin);

			//Send some data
			if (send(sock, message, 1024, 0) < 0)
			{
				puts("Send failed");
				return 1;
			}

			//Receive a reply from the server
			if (recv(sock, server_reply, 1024, 0) < 0)
			{
				puts("recv failed");
				break;
			}

			puts("Server reply :");
			puts(server_reply);
			memset(&server_reply, '\0', sizeof(server_reply));
			
		}
	}
	close(sock);
	return 0;
}

