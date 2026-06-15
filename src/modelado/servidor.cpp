#include <cstring>
#include <iostream>
#include <ostream>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error al crear socket\n";
        return 1;
    }    

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cerr << "Error en bind\n";
        return 1;
    }
    // listening to the assigned socket
    if (listen(serverSocket, 5) == -1) {
        cerr << "Error en listen\n";
        return 1;
    }

    cout << "Servidor escuchando en puerto 8080\n";
    
    // epoll instance create
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        std::cerr << "Error al crear epoll\n";
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverSocket;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSocket, &event) == -1) {
      cerr << "Error al agregar el serverSocket a epoll\n";
      close(serverSocket);
      return 1;
    }

    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
      int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
      if (nfds == -1) {
        std::cerr<<"Error en el epoll_wait\n";
      }

      for (int i = 0; i < nfds; i++) {
        if (events[i].data.fd == serverSocket) {
          int clientSocket = accept(serverSocket, nullptr, nullptr);
          std::cout << "Nueva conexión entrante...\n" << clientSocket << endl;

          struct epoll_event clienteEvent;
          clienteEvent.events = EPOLLIN;
          clienteEvent.data.fd = clientSocket;
          epoll_ctl(epfd, EPOLL_CTL_ADD, clientSocket, &clienteEvent);
          
        } else {
          
          int client_fd = events[i].data.fd;
          char buffer[1024] = {0};
          int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
          if (bytes <= 0) {
            close(client_fd);
            cout << "Cliente desconectado:" << client_fd;
            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
          } else {
            cout <<"Mensaje de"<<client_fd<< ":"<<buffer;
            }
          }
        }
      }
    // closing the socket.
    close(serverSocket);
    return 0;
}
