#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

#ifndef WIN32
#define WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <time.h>
#elif UNIX
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <sys/time.h> 
	#include <arpa/inet.h>  // to use inet_add get Client IP & Port
	#include <unistd.h>     // to use write() oder to write data in socket
	#include <string.h>     // to use strlen()
	//#include <pthread.h>
	#include <errno.h>
#endif

#define PORT 8080
#define MAXSIZE_BUFFER 1024
#define TRUE 1
#define SEND_FLAG 0
#define RECV_FLAG 0
#define TIME_OUT 5

//pthread_t server_thread;

struct sockaddr_in server, client;
struct timeval timeout;

int socket_master, new_socket;
int client_socket[5], max_socket, socket_desc, activity, valread;

int opt = TRUE;
int max_client = 5;

char* message;
char  buffer[MAXSIZE_BUFFER];     //declare buffer

 /********************************************
 *               MAIN FUNCTION
 *********************************************/
int main(int argc, char*argv[])
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	printf("===>>>>> Start main!! \n \n");

	for (int i = 0; i < max_client; i++)
	{
		client_socket[i] = 0;
	}

	// create socket
	socket_master = socket(AF_INET, SOCK_STREAM, 0);

	if (socket_master == -1)
		printf("could not create socket! \n");
	else
		printf("soket create successly! \n");

	if (setsockopt(socket_master, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	setsockopt(socket_master, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	setsockopt(socket_master, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

	// socket addr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	//bind
	int bindResult = bind(socket_master, (struct sockaddr*)& server, sizeof(struct sockaddr));
	if (bindResult < 0)
	{
		printf("bind failed, errno: %d! \n", errno);
		exit(EXIT_FAILURE);
	}
	else
		printf("bind successly \n");

	//try to specify maximum of 5 pending connections for the master socket
	listen(socket_master, 5);

	printf("Waiting for incoming connections...\n");

	while (TRUE)
	{
		fd_set readfds;

		FD_ZERO(&readfds);                //init value        
		FD_SET(socket_master, &readfds);  // set read socket_master

										  //set timeout     
		timeout.tv_sec = TIME_OUT;
		timeout.tv_usec = 0;

		max_socket = socket_master;

		//add child sockets to set
		for (int i = 0; i < max_client; i++)
		{
			//socket descriptor
			socket_desc = client_socket[i];

			//if valid socket descriptor then add to read list
			if (socket_desc > 0)
				FD_SET(socket_desc, &readfds);

			//highest file descriptor number, need it for the select function
			if (socket_desc > max_socket)
				max_socket = socket_desc;
		}
		//wait for an activity on one of the sockets , timeout is timeout , so wait indefinitely
		activity = select(max_socket + 1, &readfds, NULL, NULL, &timeout);

		if ((activity < 0) && (errno != EINTR))
		{
			printf("select error");
		}

		//If something happened on the master socket
		if (FD_ISSET(socket_master, &readfds))
		{
			int socket_size = sizeof(struct sockaddr_in);
			if ((new_socket = accept(socket_master, (struct sockaddr *)&client, (socklen_t*)&socket_size))<0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands
#if defined(WIN32) && !defined(UNIX)
			printf("New client comming\n");
#elif defined(UNIX) && !defined(WIN32)
			printf("New client comming , socket fd is %d , from IP is : %s , PORT : %d \n", new_socket, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
#else
					/* Error, both can't be defined or undefined same time */
#endif

			//send new connection greeting message

			message = "Welcome client!\n";
			if ((send(new_socket, message, strlen(message), SEND_FLAG)) != strlen(message))
			{
				perror("send");
			}

			//add new socket to array of sockets
			for (int i = 0; i < max_client; i++)
			{
				if (client_socket[i] == 0)
				{
					client_socket[i] = new_socket;
					printf("Add socket to list as pos: %d\n", i);

					break;
				}
			}
		}

		for (int i = 0; i < max_client; i++)
		{
			socket_desc = client_socket[i];

			if (FD_ISSET(socket_desc, &readfds))
			{
				//Check if it was for closing , and also read the incoming message
				if ((recv(socket_desc, buffer, 1024, RECV_FLAG)) == 0)
				{
					int socket_size = sizeof(struct sockaddr_in);
					getpeername(socket_desc, (struct sockaddr*)&client, (socklen_t*)&socket_size);
					//
#if defined(WIN32) && !defined(UNIX)
					printf("Host disconnected!\n");
#elif defined(UNIX) && !defined(WIN32)
					printf("Host disconnected , ip %s , port %d \n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
#else
					/* Error, both can't be defined or undefined same time */
#endif
					close(socket_desc);  //Close the socket and mark as 0
					client_socket[i] = 0;
				}
				else
				{
					//execute commandline from data read
					message = "\nReceived command line!\n";
					send(socket_desc, message, strlen(message), SEND_FLAG);

					printf("Command line in buffer: %s \n", buffer);

					
					char *cmd[MAXSIZE_BUFFER];
					int cmd_index = 0;
					cmd[cmd_index] = malloc(sizeof(char) * MAXSIZE_BUFFER + 1);

#if defined(WIN32) && !defined(UNIX)
					FILE *fp = _popen(buffer, "r");
#elif defined(UNIX) && !defined(WIN32)
					FILE *fp = popen(buffer, "r");
#else
					/* Error, both can't be defined or undefined same time */
#endif
					
					while (fgets((cmd[cmd_index]), sizeof(cmd[cmd_index]) - 1, fp) != NULL)
					{
						printf("%s", cmd[cmd_index]);
						cmd_index++;
						cmd[cmd_index] = malloc(sizeof(char) * MAXSIZE_BUFFER + 1);
					}

					for (int j = 0; j < cmd_index; j++)
					{
						send(socket_desc, cmd[j], strlen(cmd[j]), SEND_FLAG);
					}

					fclose(fp);

					memset(buffer, 0, sizeof(buffer));					
				}
			}
		}
	}

	/*
	socket_size = sizeof(struct sockaddr_in);
	while((newsocket = accept(socket_master, (struct sockaddr*)&client, (socklen_t *)&socket_size)) > 0)
	{
	char *client_ip = inet_ntoa(client.sin_addr);
	int client_port = ntohs(client.sin_port);

	printf("Accept successly! Client IP:%s port:%d!\n",client_ip,client_port);

	message = "Hello Client , I have received your connection.\n";
	write(newsocket , message , strlen(message));

	//pthread_create( &server_thread, NULL, connection_handler, (void*)newsocket);
	printf("pthread start to handle! \n");
	}

	if(newsocket < 0)
	printf("Accept faile!");
	*/

	return EXIT_SUCCESS;
}


/***********************************************
*   Handle thread
***********************************************/
/*
void* connection_handler(void *arg )
{
printf(" [Line][] TCPServer::connection_handler(void *arg )\n");
//Get the socket descriptor

char* msg = "Connection handler!\n";

int sendResult = write(socket, msg, strlen(msg));
if(sendResult == -1)
{
printf("Send msg Error!\n");
}
else
{
printf("Send msg Successly!\n");
}

if((read(socket, msg, 512)) > 0)
{
printf("read successly!\n");
printf("Received msg from Client: %s", msg);

pthread_join(server_thread, 0);
}
else
printf("read failed! %d\n", errno);


return 0;
}
*/