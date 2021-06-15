#include<stdio.h>
#include<string.h>	//strlen
#include<stdlib.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<pthread.h> //for threading , link with lpthread
#include<wait.h>

//the thread function
void *connection_handler(void *);

int main(int argc, char *argv[])
{
	// DAEMON
    /* Our process ID and Session ID */
    pid_t pid, sid;
        
    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
                
    /* Open any logs here */        
                
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }
        
    /* Change the current working directory */
    chdir("home/bayuekap/Documents/modul4/fp/database/");
    
    /* Change the file mode mask */
    umask(0); 

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
        
    /* Daemon-specific initialization goes here */
        
    /* The Big Loop */
    while (1) {

		int socket_desc , client_sock , c , *new_sock;
		struct sockaddr_in server , client;
		
		//Create socket
		socket_desc = socket(AF_INET , SOCK_STREAM , 0);
		if (socket_desc == -1)
		{
			printf("Could not create socket");
		}
		puts("Socket created");
		
		//Prepare the sockaddr_in structure
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = INADDR_ANY;
		server.sin_port = htons( 7000 );
		
		//Bind
		if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
		{
			//print the error message
			perror("bind failed. Error");
			return 1;
		}
		puts("bind done");
		
		//Listen
		listen(socket_desc , 3);
		
		//Accept and incoming connection
		puts("Waiting for incoming connections...");
		c = sizeof(struct sockaddr_in);
		
		
		//Accept and incoming connection
		puts("Waiting for incoming connections...");
		c = sizeof(struct sockaddr_in);
		while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
		{
			puts("Connection accepted");
			
			pthread_t sniffer_thread;
			new_sock = malloc(1);
			*new_sock = client_sock;
			
			if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
			{
				perror("could not create thread");
				return 1;
			}
			
			//Now join the thread , so that we dont terminate before the thread
			pthread_join( sniffer_thread , NULL);
			puts("Handler assigned");
		}
		
		if (client_sock < 0)
		{
			perror("accept failed");
			return 1;
		}
        sleep(30); /* wait 30 seconds */
        break;
    }
    exit(EXIT_SUCCESS);
	
	return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char *message, client_message[2000];
	
	//Send some messages to the client
	message = "Greetings! I am your connection handler\n";
	write(sock , message , strlen(message));
	
	message = "Now type something and i shall repeat what you type \n";
	write(sock , message , strlen(message));
	
	//Receive a message from client
	while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 )
	{
		//Send the message back to client
		write(sock , client_message , strlen(client_message));
	}
	
	if(read_size == 0)
	{
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1)
	{
		perror("recv failed");
	}
		
	//Free the socket pointer
    close(sock);
	free(socket_desc);
	
	return 0;
}