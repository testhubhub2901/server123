#include <iostream>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ev.h>


ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);
int set_nonblock(int fd);


std::map<int, bool> workers;


void slave_send_to_worker(struct ev_loop *loop, struct ev_io *w, int revents)
{
	// get slave socket
	int slave_socket = w->fd;

	// find a free worker and send slave socket to it
	for(auto it = workers.begin(); it != workers.end(); it++)
	{
		if ((*it).second)
		{
			// found free worker, set it is busy
			(*it).second = false;

			char tmp[1];
			sock_fd_write((*it).first, tmp, sizeof(tmp), slave_socket);

			return;
		}
	}

	// no free workers
}


void do_work(struct ev_loop *loop, struct ev_io *w, int revents)
{
	// get appropriate slave socket and read from it
	int slave_socket;
	char sbuf[10];

	int paired_socket = w->fd;
	sock_fd_read(paired_socket, sbuf, sizeof(sbuf), &slave_socket);
	if (slave_socket == -1)
	{
		exit(4);
	}

	// read from slave socket
	char buf[1024];
	ssize_t read_ret = read(slave_socket, buf, sizeof(buf));
	if (read_ret == -1)
	{
		// process error
		return;
	}

	// process http request


	// write an answer to slave socket


	// write back to paired socket to update worker status
	char tmp[1];
	sock_fd_write(paired_socket, tmp, sizeof(tmp), slave_socket);
}


void set_worker_free(struct ev_loop *loop, struct ev_io *w, int revents)
{
	// get socket of the pair
	int fd = w->fd;
	workers[fd] = true;
}


pid_t create_worker()
{
	int sp[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sp) == -1)
	{
		printf("socketpair error, %s\n", strerror(errno));
		exit(1);
	}

  	// get default loop
  	struct ev_loop* loop = EV_DEFAULT;

	auto pid = fork();

	if (pid)
	{
		// parent, use socket 0
		close(sp[1]);

		// save worker socket and set free status
		workers.insert(std::pair<int, bool>(sp[0], true));

		// to detect the worker finished work with a socket
		struct ev_io half_watcher;
  		ev_init(&half_watcher, set_worker_free);
  		ev_io_set(&half_watcher, sp[0], EV_READ);
  		ev_io_start(loop, &half_watcher);
	}
	else
	{
		// child, use socket 1
		close(sp[0]);

		// we use EVFLAG_FORKCHECK instead of
		// ev_default_fork();

		// create watcher for paired socket
		struct ev_io worker_watcher;
  		ev_init(&worker_watcher, do_work);
  		ev_io_set(&worker_watcher, sp[1], EV_READ);
  		ev_io_start(loop, &worker_watcher);

  		// wait for events
  		ev_loop(loop, 0);
	}

	return pid;
}

void master_accept_connection(struct ev_loop *loop, struct ev_io *w, int revents)
{
	// create slave socket
	int slave_socket = accept(w->fd, 0, 0);
	if (slave_socket == -1)
	{
		printf("accept error, %s\n", strerror(errno));
		exit(3);
	}

	set_nonblock(slave_socket);

	// create watcher for a slave socket
	struct ev_io slave_watcher;
  	ev_init (&slave_watcher, slave_send_to_worker);
  	ev_io_set(&slave_watcher, slave_socket, EV_READ);
  	ev_io_start(loop, &slave_watcher);
}


int main(int argc, char* argv[])
{
	char *host = 0, *port = 0, *dir = 0;

	int opt;
	while ((opt = getopt(argc, argv, "h:p:d:")) != -1)
	{
		switch(opt)
		{
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case 'd':
				dir = optarg;
				break;
			default:
				printf("Usage: %s -h <host> -p <port> -d <folder>\n", argv[0]);
				exit(1);
		}
	}

	if (host == 0 || port == 0 || dir == 0)
	{
		printf("Usage: %s -h <host> -p <port> -d <folder>\n", argv[0]);
		exit(1);
	}


	//printf("%s %s %s\n", host, port, dir);
	//exit(0);


	//--------------------------------------------------------------------//


	// Our event loop
	struct ev_loop *loop = ev_default_loop(EVFLAG_FORKCHECK);


	//---------------- Create 2 workers --------------------//

	if (create_worker() == 0)
	{
		// worker 1 process
		printf("Worker 1 is about to return\n");
		return 0;
	}

	if (create_worker() == 0)
	{
		// worker 2 process
		printf("Worker 2 is about to return\n");
		return 0;
	}

	//------------------------------------------------------//


	// Master socket, think non-blocking
	int master_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (master_socket == -1)
	{
		printf("socket error, %s\n", strerror(errno));
		exit(1);
	}
	set_nonblock(master_socket);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));

	//printf("Port: %d\n", atoi(port));

	if (inet_pton(AF_INET, host, &(addr.sin_addr.s_addr)) != 1)
	{
		printf("inet_aton error\n");
		exit(2);
	}

	if (bind(master_socket, (struct sockaddr* )&addr, sizeof(addr)) == -1)
	{
		printf("bind return -1, %s\n", strerror(errno));
		exit(3);
	}


	listen(master_socket, SOMAXCONN);


	// Master watcher
	struct ev_io master_watcher;
  	ev_init (&master_watcher, master_accept_connection);
  	ev_io_set(&master_watcher, master_socket, EV_READ);
  	ev_io_start(loop, &master_watcher);


  	// Start loop
  	ev_loop(loop, 0);



	close(master_socket);

	return 0;
}


ssize_t
sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
    ssize_t     size;
    struct msghdr   msg;
    struct iovec    iov;
    union {
        struct cmsghdr  cmsghdr;
        char        control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr  *cmsg;

    iov.iov_base = buf;
    iov.iov_len = buflen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd != -1) {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof (int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        printf ("passing fd %d\n", fd);
        *((int *) CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        printf ("not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);

    if (size < 0)
        perror ("sendmsg");
    return size;
}

ssize_t
sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
    ssize_t     size;

    if (fd) {
        struct msghdr   msg;
        struct iovec    iov;
        union {
            struct cmsghdr  cmsghdr;
            char        control[CMSG_SPACE(sizeof (int))];
        } cmsgu;
        struct cmsghdr  *cmsg;

        iov.iov_base = buf;
        iov.iov_len = bufsize;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);
        size = recvmsg (sock, &msg, 0);
        if (size < 0) {
            perror ("recvmsg");
            exit(1);
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level != SOL_SOCKET) {
                fprintf (stderr, "invalid cmsg_level %d\n",
                     cmsg->cmsg_level);
                exit(1);
            }
            if (cmsg->cmsg_type != SCM_RIGHTS) {
                fprintf (stderr, "invalid cmsg_type %d\n",
                     cmsg->cmsg_type);
                exit(1);
            }

            *fd = *((int *) CMSG_DATA(cmsg));
            printf ("received fd %d\n", *fd);
        } else
            *fd = -1;
    } else {
        size = read (sock, buf, bufsize);
        if (size < 0) {
            perror("read");
            exit(1);
        }
    }
    return size;
}

int set_nonblock(int fd)
{
	int flags;
#if defined(O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIONBIO, &flags);
#endif
}
