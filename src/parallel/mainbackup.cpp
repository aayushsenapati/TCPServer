#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <pthread.h>
#include <map>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>

 

struct to_thread
{
  std::map<std::string, std::string>* KV_DATASTORE;
  pthread_mutex_t* lock;
  pthread_mutex_t* read_counter_lock;
  int c_fd;
  int * read_count;
};




void *handle_client(void *arg)
{
  struct to_thread * data = (struct to_thread *)arg;
  char buffer[256];
  std::string message;

  std::cout << "New client connected. Socket FD: " << data->c_fd << std::endl;

  while (1)
  {
    bzero(buffer, 256);
    int n = read((data->c_fd), buffer, 255);
    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }
    message += std::string(buffer, n);
    std::cout<<message<<std::endl;
    if (message.find("END") != std::string::npos)
    {
      break;
    }
  }

  std::stringstream ss(message);
  std::string command;

 
  while (ss >> command)
  {
    if (command == "READ")
    {



      pthread_mutex_lock(data->read_counter_lock);
      if(*(data->read_count)==0)
      {
        pthread_mutex_lock(data->lock);
      }
      *(data->read_count)+=1;
      pthread_mutex_unlock(data->read_counter_lock);



      std::string key;
      ss >> key;
      auto it = (*(data->KV_DATASTORE)).find(key);
      if (it != (*(data->KV_DATASTORE)).end())
      {
        std::string value = it->second;
        write((data->c_fd), (value+"\n").c_str(), value.size()+1);

      }
      else
      {
        write((data->c_fd), "NULL\n", 5);
      }

      pthread_mutex_lock(data->read_counter_lock);
      *(data->read_count)-=1;

      if(*(data->read_count)==0)
      {
        pthread_mutex_unlock(data->lock);
      }

      pthread_mutex_unlock(data->read_counter_lock);
      

    }
    else if (command == "WRITE")
    {
      std::string key, value;
      ss >> key;
      ss >> value;
      std::cout<<"This is the value substr:" <<value.substr(1,value.size()-1)<<std::endl;
      (*(data->KV_DATASTORE))[key] = value.substr(1, value.size() - 1);
      write((data->c_fd), "FIN\n", 4);
    }
    else if (command == "COUNT")
    {
      int count = (*(data->KV_DATASTORE)).size();
      write((data->c_fd), (std::to_string(count)+"\n").c_str(), std::to_string(count).size()+1);
    }
    else if (command == "DELETE")
    {
      std::string key;
      ss >> key;
      auto it = (*(data->KV_DATASTORE)).find(key);
      if (it != (*(data->KV_DATASTORE)).end())
      {
        (*(data->KV_DATASTORE)).erase(key);
        write((data->c_fd), "FIN\n", 4);
      }
      else
      {
        write((data->c_fd), "NULL\n", 5);
      }
    }
    else if (command == "END")
    {
      break;
    }
  }


  close((data->c_fd));
  delete (struct to_thread *)arg;
  return NULL;
}

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
  int option=1;

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

int main(int argc, char **argv)
{


  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  std::map<std::string, std::string> KV_DATASTORE;
  pthread_mutex_t lock;
  pthread_mutex_t read_counter_lock;

  int read_count=0;
  

  if (pthread_mutex_init(&lock, NULL) != 0)
  {
    printf("\n mutex init failed\n");
    return 1;
  }
  if (pthread_mutex_init(&read_counter_lock, NULL) != 0)
  {
    printf("\n mutex init failed\n");
    return 1;
  }

  int sockfd = create_server_socket(atoi(argv[1]));

  struct sockaddr_in cli_addr;

  socklen_t clilen = sizeof(cli_addr);

  pthread_t thread_id;

  while (1)
  {

    int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    
    struct to_thread * data =new(struct to_thread);
    data->KV_DATASTORE = &KV_DATASTORE;
    data->lock = &lock;
    data->read_counter_lock = &read_counter_lock;
    data->c_fd = newsockfd;
    data->read_count=&read_count;
    
    if (newsockfd < 0)
    {
      perror("ERROR on accept");
      exit(1);
    }
     std::cout << "New connection accepted. Socket FD: " << newsockfd << std::endl;

    if (pthread_create(&thread_id, NULL, handle_client, (void *)data) < 0)
    {
      perror("could not create thread");
      return 1;
    }
  }

  close(sockfd);
  pthread_mutex_destroy(&lock);
  return 0;
}