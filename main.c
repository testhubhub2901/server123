#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <ev.h>

// TODO: arguments
short port = 12345;
char* ipaddr = "127.0.0.1";
char* dir = "";


const int WORKERS_COUNT = 4;

int main(int argc, char* argv[])
{
	int MasterSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (MasterSocket == -1)
	{
		printf("MasterSocket == -1, %s\n", strerror(errno));
		exit(1);
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_aton(ipaddr, &addr.sin_addr.s_addr) == 0)
	{
		exit(2);
	}

	if (bind(MasterSocket, (struct sockaddr* )&addr, sizeof(addr)) == -1)
	{
		printf("bind return -1, %s\n", strerror(errno));
		exit(3);
	}

	listen(MasterSocket, SOMAXCONN);




	struct ev_io master_watcher;





	// create workers


	close(MasterSocket);
	return 0;
}