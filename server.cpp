/* The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <pthread.h>

int max_services = 2;

std::mutex g_pages_mutex; 


void error(const char *msg)
{
    perror(msg);
    exit(1);
    std::cout << msg << std::endl;
}

enum messageType
{
   busy = -1,
   echo,
   doubleEcho,
};

std::string getGreetingMessage(messageType type)
{
   std::string msg = "Welcome! ";
   switch (type)
   {
   case busy:
      return "Sorry all services are busy. Connection close.\n";
      break;
   case echo:
      msg.append("Echo service\n");
      break;
   case doubleEcho:
      msg.append("Double Echo service\n");
      break;
   
   default:
      break;
   }
   return msg;
}

struct ClientInfo
{  
   int _clientFD;
   int _clientID;
   int _status;

   ClientInfo(int clientFD, int clientID, int status)
   : _clientFD(clientFD), _clientID(clientID), _status(status)
   {};

   void printInfo ()
   {
      std::cout 
         << "    Client ID: " << _clientID 
         << " Status: " << _status 
         << std::endl; 
   }
};

std::string getResponseMessage(std::string buffer, int loops)
{  
   std::string cleanBuffer;
   int ind = buffer.find_last_of('\n');
   cleanBuffer = buffer.substr(0, ind);

   std::string msg = cleanBuffer;
   for (int i = 0; i < loops; ++i)
   {
      msg
         .append(" ")
         .append(cleanBuffer);
   }
   msg.push_back('\n');
   return msg;
}

struct ServerCollector 
{
   std::vector<std::shared_ptr<ClientInfo>> clientsInfoContainer;
   std::vector<int> sevrices;

   ServerCollector(int maxClients)
   : clientsInfoContainer(maxClients, nullptr), sevrices(maxClients, 0)
   {}

   void add_client(int ind, std::shared_ptr<ClientInfo> clientInfo)
   {
      clientsInfoContainer[ind] = clientInfo;
      sevrices[ind] = 1;
   }

   void remove_client(std::shared_ptr<ClientInfo> clientInfo)
   {
      
   }
   
   void check_services()
   {
      // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      for (int i = 0; i < clientsInfoContainer.size(); ++i)
      {
         std::lock_guard<std::mutex> guard(g_pages_mutex);
         
         std::shared_ptr<ClientInfo> ciPtr = clientsInfoContainer[i];
         if (ciPtr)
         {
            // ciPtr->printInfo();
            if (ciPtr->_status == 0)
            {
               sevrices[i] = 0;
            }
         }
      }
      // printf("------------------------\n");
   }
};

void *clientService (void *arg)
{
   std::shared_ptr<ClientInfo> clientInfo = *static_cast<std::shared_ptr<ClientInfo>*>(arg);

   int client = clientInfo->_clientFD;
   int clientIndex = clientInfo->_clientID;

   char stopServer[20] = "kill server\n";
   char exit[20] = "exit\n";
   char stop[20] = "\n";
   char buffer[256];
   int n;

   std::string messageStr = getGreetingMessage( static_cast<messageType>(clientIndex) ); 
   const char *message = messageStr.c_str(); 
   int s = send(client, message, strlen(message), 0);

   while (true)
   {
      // std::cout << "Reading ...\n";
      
      bzero(buffer, 256);
      n = read(client, buffer, 255);
      if (n < 0) 
      {
         error("ERROR reading from socket");
         break;
      }

      if (n > 0)
      {
         // std::this_thread::sleep_for(std::chrono::milliseconds(1000));            

         if (strcmp(buffer, stop) == 0)
         {
            std::cout << "close " << client << std::endl;
            close(client);
            
            clientInfo->_status = 0;
            break;
         }
         printf("\tHere is the message from: %d  -  %s", client, buffer);
         
         buffer[n] = '\0'; 

         std::string messageStr = getResponseMessage(buffer, clientIndex);
         const char *message = messageStr.c_str();
         int s = send(client, message, strlen(message), 0);

         if (s != strlen(message))
         {
            std::cout << "Send Problema " << s << std::endl;
         }

         bzero(buffer, 256);
      }
      else
      {         
         close(client);
         clientInfo->_status = 0;
         break;
      }
   }
}


void server_worker(std::shared_ptr<ServerCollector> controllerPtr)
{
   int newsockfd, sockfd;
   int portno = 8888;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;
   char buffer[256];

   sockfd =  socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) 
   {
      error("ERROR opening socket");
   }

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;  
   serv_addr.sin_addr.s_addr = INADDR_ANY;  
   serv_addr.sin_port = htons(portno);

   if ( bind( sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) < 0 ) 
   {
      // error("ERROR on binding");
   }

   listen(sockfd, 5);
   clilen = sizeof(cli_addr);

   pthread_t threads[5];
   while (true)
   {      
      printf("Listening...\n");

      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

      int rc = 1;

      for (int i = 0; i < controllerPtr->sevrices.size(); ++i)
      {  
         // std::cout << "    Index: " << i << " Status: " << controllerPtr->sevrices[i] << std::endl;
         if (controllerPtr->sevrices[i] == 0)
         {
            printf(
                "New connection, socket fd is %d, ip is : %s, port : %d\n" , 
                newsockfd , 
                inet_ntoa(cli_addr.sin_addr) , 
                ntohs(cli_addr.sin_port) 
            ); 

            std::shared_ptr<ClientInfo> clientInfoPtr = std::make_shared<ClientInfo>(newsockfd, i, 1);

            // pthread_create (thread, attr, start_routine, arg) 
            rc = pthread_create(
                  &threads[i], NULL, clientService, 
                  static_cast<void*>(new std::shared_ptr<ClientInfo>(clientInfoPtr))
               );

            if (rc == 0)
            {
               controllerPtr->add_client(i, clientInfoPtr);
               break;
            }
         }
      }
      if (rc) {
         std::string messageStr = getGreetingMessage(messageType::busy); 
         const char *message = messageStr.c_str(); 
         int s = send(newsockfd, message, strlen(message), 0);
         close(newsockfd);
         // std::cout << "Error:unable to create thread," << rc << std::endl;
      }
   }

   close(sockfd);
}


int main(int argc, char *argv[])
{

   std::shared_ptr<ServerCollector> ptr = std::make_shared<ServerCollector>(max_services);

   std::thread workerthread(server_worker, ptr);
   while (true)
   {
      ptr->check_services();
   }

   return 0; 
}

// g++ server.cpp -o server -lpthread
// -lnsl