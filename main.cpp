#include <iostream>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <sys/epoll.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <unistd.h>
  #include <stdio.h>
#include <stdlib.h>
  #include <list>
  #include <time.h>
#include<strings.h>
#include<cstring>
     #include <sys/sendfile.h>

using namespace std;

char *host = 0, *port = 0, *dir = 0;

  // Default buffer size
  #define BUF_SIZE 1024

  // Default timeout - http://linux.die.net/man/2/epoll_wait
  #define EPOLL_RUN_TIMEOUT -1

  // Count of connections that we are planning to handle (just hint to kernel)
  #define EPOLL_SIZE 10000

  // First welcome message from server
  #define STR_WELCOME "Welcome to seChat! You ID is: Client #%d"

  // Format of message population
  #define STR_MESSAGE "Client #%d>> %s"

  // Warning message if you alone in server
  #define STR_NOONE_CONNECTED "Noone connected to server except you!"

  // Commad to exit
  #define CMD_EXIT "EXIT"

  // Macros - exit in any error (eval < 0) case
  #define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

  // Macros - same as above, but save the result(res) of expression(eval)
  #define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

  // Preliminary declaration of functions
  int setnonblocking(int sockfd);
  void debug_epoll_event(epoll_event ev);
  int handle_message(int new_fd);
  int print_incoming(int fd);

  void extract_path_from_http_get_request(std::string& path, const char* buf, ssize_t len)
  {
      std::string request(buf, len);
      std::string s1(" ");
      std::string s2("?");

      // "GET "
      std::size_t pos1 = 4;

      std::size_t pos2 = request.find(s2, 4);
      if (pos2 == std::string::npos)
      {
          pos2 = request.find(s1, 4);
      }

      path = request.substr(4, pos2 - 4);
  }


int setnonblocking(int sockfd)
{
   CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK));
   return 0;
}



// To store client's socket list
   list<int> clients_list;

   // for debug mode
   int DEBUG_MODE = -1;

   int main(int argc, char *argv[])
   {
       int opt;
       while ((opt = getopt(argc, argv, "h:p:d:")) != -1)
       {
           switch(opt)
           {
               case 'h':
                   host = optarg;
                   break;
               case 'p':
                   port = optarg;https://ru.wikipedia.org/wiki/Itoa_(%D0%A1%D0%B8)
                   break;
               case 'd':
                   dir = optarg;
                   break;
               default:
                   printf("Usage: %s -h <host> -p <port> -d <folder>\n", argv[0]);
                   exit(1);
           }
       }
       // *** Define debug mode
       //     any additional parameres on startup
       //     i.e. like './server f' or './server debug'
       //     we will switch to switch to debug mode(very simple anmd useful)


       if(argc > 1) DEBUG_MODE = 1;
       int  pid;
if( ( pid = fork() ) == 0 )
{
        if(DEBUG_MODE){
           printf("Debug mode is ON!\n");
           printf("MAIN: argc = %d\n", argc);
           for(int i=0; i<argc; i++)
               printf(" argv[%d] = %s\n", i, argv[i]);
       }else printf("Debug mode is OFF!\n");

       // *** Define values
       //     main server listener
       int listener;

       // define ip & ports for server(addr)
       //     and incoming client ip & ports(their_addr)
       struct sockaddr_in addr, their_addr;
       //     configure ip & port for listen
       addr.sin_family = PF_INET;
       addr.sin_port = htons(atoi(port));
       addr.sin_addr.s_addr = inet_addr(host);

       //     size of address
       socklen_t socklen;
       socklen = sizeof(struct sockaddr_in);

       //     event template for epoll_ctl(ev)
       //     storage array for incoming events from epoll_wait(events)
       //        and maximum events count could be EPOLL_SIZE
       static struct epoll_event ev, events[EPOLL_SIZE];
       //     watch just incoming(EPOLLIN)
       //     and Edge Trigged(EPOLLET) events
       ev.events = EPOLLIN | EPOLLET;

       //     chat message buffer
       char message[BUF_SIZE];

       //     epoll descriptor to watch events
       int epfd;

       //     to calculate the execution time of a program
       clock_t tStart;

       // other values:
       //     new client descriptor(client)
       //     to keep the results of different functions(res)
       //     to keep incoming epoll_wait's events count(epoll_events_count)
       int client, res, epoll_events_count;


       // *** Setup server listener
       //     create listener with PF_INET(IPv4) and
       //     SOCK_STREAM(sequenced, reliable, two-way, connection-based byte stream)
       CHK2(listener, socket(PF_INET, SOCK_STREAM, 0));
       printf("Main listener(fd=%d) created! \n",listener);

       //    setup nonblocking socket
       setnonblocking(listener);

       //    bind listener to address(addr)
       CHK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)));
       printf("Listener binded to: %s\n", host);

       //    start to listen connections
       CHK(listen(listener, 1));
       printf("Start to listen: %s!\n", host);

       // *** Setup epoll
       //     create epoll descriptor
       //     and backup store for EPOLL_SIZE of socket events
       CHK2(epfd,epoll_create(EPOLL_SIZE));
       printf("Epoll(fd=%d) created!\n", epfd);

       //     set listener to event template
       ev.data.fd = listener;

       //     add listener to epoll
       CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev));
       printf("Main listener(%d) added to epoll!\n", epfd);

       // *** Main cycle(epoll_wait)
       while(1)
       {
           CHK2(epoll_events_count,epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT));
           if(DEBUG_MODE) printf("Epoll events count: %d\n", epoll_events_count);
           // setup tStart time
           tStart = clock();

           for(int i = 0; i < epoll_events_count ; i++)
           {
                   if(DEBUG_MODE){
                           printf("events[%d].data.fd = %d\n", i, events[i].data.fd);
                           //debug_epoll_event(events[i]);

                   }
                   // EPOLLIN event for listener(new client connection)
                   if(events[i].data.fd == listener)
                   {
                           CHK2(client,accept(listener, (struct sockaddr *) &their_addr, &socklen));
                           if(DEBUG_MODE) printf("connection from:%s:%d, socket assigned to:%d \n",
                                             inet_ntoa(their_addr.sin_addr),
                                             ntohs(their_addr.sin_port),
                                             client);
                           // setup nonblocking socket
                           setnonblocking(client);

                           // set new client to event template
                           ev.data.fd = client;

                           // add new client to epoll
                           CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev));

                           // save new descriptor to further use
                           clients_list.push_back(client); // add new connection to list of clients
                           if(DEBUG_MODE) printf("Add new client(fd = %d) to epoll and now clients_list.size = %d\n",
                                               client,
                                               clients_list.size());

                           // send initial welcome message to client
                            bzero(message, BUF_SIZE);
                           /*res = sprintf(message, STR_WELCOME, client);
                           CHK2(res, send(client, message, BUF_SIZE, 0));*/

                   }else { // EPOLLIN event for others(new incoming message from client)
                           CHK2(res,handle_message(events[i].data.fd));
                   }
           }
           // print epoll events handling statistics
           printf("Statistics: %d events handled at: %.2f second(s)\n",
                                           epoll_events_count,
                                           (double)(clock() - tStart)/CLOCKS_PER_SEC);
       }

       close(listener);
       close(epfd);
}
       return 0;
   }

   // *** Handle incoming message from clients
   int handle_message(int client)
   {
       // get row message from client(buf)
       //     and format message to populate(message)
       char buf[BUF_SIZE], message[BUF_SIZE];
       bzero(buf, BUF_SIZE);
       bzero(message, BUF_SIZE);

       // to keep different results
       int len;

       // try to get new raw message from client
       if(DEBUG_MODE) printf("Try to read from fd(%d)\n", client);
       CHK2(len,recv(client, buf, BUF_SIZE, 0));
       std::string path;
       extract_path_from_http_get_request(path, buf, len);

       std::string full_path = std::string(dir) + path;
       char reply[1024];

       // zero size of len mean the client closed connection
       if(len == 0){
           CHK(close(client));
           clients_list.remove(client);
           if(DEBUG_MODE) printf("Client with fd: %d closed! And now clients_list.size = %d\n", client, clients_list.size());
       // populate message around the world
       }else{

           /*if(clients_list.size() == 1) { // this means that noone connected to server except YOU!
                   CHK(send(client, STR_NOONE_CONNECTED, strlen(STR_NOONE_CONNECTED), 0));
                   return len;*/
           }

           // format message to populate
           sprintf(message, STR_MESSAGE, client, buf);

           // populate message around the world ;-)...
           list<int>::iterator it;
           for(it = clients_list.begin(); it != clients_list.end(); it++){
              //if(*it != client){ // ... except youself of course
                   //CHK(send(*it, message, BUF_SIZE, 0));
                   if (access(full_path.c_str(), F_OK) != -1)
                   {
                       int fd = open(full_path.c_str(), O_RDONLY);
                       int sz = lseek(fd, 0, SEEK_END);;

                       sprintf(reply, "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-length: %d\r\n"
                                      "Connection: close\r\n"
                                      "\r\n", sz);

                       ssize_t send_ret = send(*it, reply, strlen(reply), MSG_NOSIGNAL);

                       off_t offset = 0;
                       while (offset < sz)
                       {
                           offset = sendfile(*it, fd, &offset, sz - offset);
                       }

                       close(fd);
                   }
                   else
                   {
                       strcpy(reply, "HTTP/1.1 404 Not Found\r\n"
                                     "Content-Type: text/html\r\n"
                                     "Content-length: 107\r\n"
                                     "Connection: close\r\n"
                                     "\r\n");

                       ssize_t send_ret = send(*it, reply, strlen(reply), MSG_NOSIGNAL);
                       strcpy(reply, "<html>\n<head>\n<title>Not Found</title>\n</head>\r\n");
                       send_ret = send(*it, reply, strlen(reply), MSG_NOSIGNAL);
                       strcpy(reply, "<body>\n<p>404 Request file not found.</p>\n</body>\n</html>\r\n");
                       send_ret = send(*it, reply, strlen(reply), MSG_NOSIGNAL);
                   }
           }
           /*if(DEBUG_MODE) printf("Client(%d) received message successfully:'%s', a total of %d bytes data...\n",
                client,
                buf,
                len);
       }*/

       return len;
   }
