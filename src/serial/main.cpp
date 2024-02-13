#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <map>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>

std::map<std::string, std::string> KV_DATASTORE;

int create_server_socket(int port)
{
  int sockfd;
  struct sockaddr_in serv_addr;

  // Initialize serv_addr to zeros
  bzero((char *)&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("ERROR opening socket");
    exit(1);
  }
  int option = 1;

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

  std::cout << "Socket created. FD: " << sockfd << std::endl;

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("ERROR on binding");
    exit(1);
  }

  listen(sockfd, 5);

  std::cout << "Server listening on port " << port << std::endl;

  return sockfd;
}

void handle_client(int c_fd)
{
  char buffer[256];
  std::string message;

  //loop to create message string from client
  while (1)
  {
    bzero(buffer, 256);
    int n = read(c_fd, buffer, 255);
    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }
    message += std::string(buffer, n);
    std::cout << message << std::endl;
    if (message.find("END") != std::string::npos)
    {
      break;
    }
  }

  std::stringstream ss(message);
  std::string command;

  //loop to parse message string and execute commands
  while (ss >> command)
  {
    if (command == "READ")
    {
      std::string key;
      ss >> key;
      auto it = KV_DATASTORE.find(key);
      if (it != KV_DATASTORE.end())
      {
        std::string value = it->second;
        write(c_fd, (value + "\n").c_str(), value.size() + 1);
      }
      else
      {
        write(c_fd, "NULL\n", 5);
      }
    }
    else if (command == "WRITE")
    {
      std::string key, value;
      ss >> key;
      ss >> value;
      std::cout << "This is the value substr:" << value.substr(1, value.size() - 1) << std::endl;
      KV_DATASTORE[key] = value.substr(1, value.size() - 1);
      write(c_fd, "FIN\n", 4);
    }
    else if (command == "COUNT")
    {
      int count = KV_DATASTORE.size();
      write(c_fd, (std::to_string(count) + "\n").c_str(), std::to_string(count).size() + 1);
    }
    else if (command == "DELETE")
    {
      std::string key;
      ss >> key;
      auto it = KV_DATASTORE.find(key);
      if (it != KV_DATASTORE.end())
      {
        KV_DATASTORE.erase(key);
        write(c_fd, "FIN\n", 4);
      }
      else
      {
        write(c_fd, "NULL\n", 5);
      }
    }
    else if (command == "END")
    {
      close(c_fd);
      break;
    }
  }
}

int main(int argc, char **argv)
{

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  int sockfd = create_server_socket(atoi(argv[1]));

  struct sockaddr_in cli_addr;

  socklen_t clilen = sizeof(cli_addr);

  while (1)
  {
    int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
    {
      perror("ERROR on accept");
      exit(1);
    }
    std::cout << "New connection accepted. Socket FD: " << newsockfd << std::endl;
    handle_client(newsockfd);
  }

  close(sockfd);
  return 0;
}
