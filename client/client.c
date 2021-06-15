#include <stdio.h>	//printf
#include <string.h>	//strlen
#include <sys/socket.h>	//socket
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>

void recieveInput(const char *title, char str[]) {
    printf("\e[0m%s\n> \e[36m", title);
    scanf("%s", str);
    str[strlen(str)] = '\0';
    printf("\e[0m");
}

int main(int argc, char *argv[])
{

	int sock;
	struct sockaddr_in server;
	char uname[1000], pswrd[1000];
	char message[1000] , server_reply[2000];
	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");
	
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( 7000 );

	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}
	
	puts("\e[33mConnected.\e[0m\n");
	
	//keep communicating with server
	int commandTrue = 0;
	while(1)
	{
		// printf("Enter message : ");
		// scanf("%s" , message);
		
		// //Send some data
		// if( send(sock , message , strlen(message) , 0) < 0)
		// {
		// 	puts("Send failed");
		// 	return 1;
		// }
		
		// //Receive a reply from the server
		// if( recv(sock , server_reply , 2000 , 0) < 0)
		// {
		// 	puts("recv failed");
		// 	break;
		// }
		
		// puts("Server reply :");
		// puts(server_reply);
		// commandTrue = 0;
        while(!commandTrue){
			char *cmd[1000];
            // recieveInput("\nInsert Command >> ", cmd);
			cmd[strlen(cmd)];
			printf("\e[0mOurSQL >> \e[36m");
			scanf("%s", cmd);
            for(int b = 0; b < strlen(cmd); b++){
                cmd[b] = tolower(cmd[b]);
            }
            send(sock, cmd, sizeof(cmd), 0);
            if(!strcmp(cmd, "use")) {
                chdir("");
            }
			else if(!strcmp(cmd, "")) {
				
			}
        }

        sleep(2);
        if(commandTrue) break;
	}
	
	close(sock);
	return 0;
}