#include <cstring>
#include <iostream>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "json.hpp"
using namespace std;


using json = nlohmann::json;

json recibirJson(int socket_fd) {
    char buffer[1024] = {0};

    ssize_t bytesLeidos = recv(socket_fd, buffer, 1024, 0);

    if (bytesLeidos <= 0) {
        return json{};
    }

    return json::parse(std::string(buffer, bytesLeidos));
}

int main()
{
    // Crear socket del servidor
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error al crear socket\n";
        return 1;
    }

    // Configurar dirección
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Asociar socket a dirección
    if (bind(serverSocket,
             (struct sockaddr*)&serverAddress,
             sizeof(serverAddress)) == -1)
    {
        cerr << "Error en bind\n";
        close(serverSocket);
        return 1;
    }

    // Escuchar conexiones
    if (listen(serverSocket, 5) == -1) {
        cerr << "Error en listen\n";
        close(serverSocket);
        return 1;
    }

    cout << "Servidor escuchando en puerto 8080\n";

    // Crear instancia de epoll
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        cerr << "Error al crear epoll\n";
        close(serverSocket);
        return 1;
    }

    // Registrar serverSocket en epoll
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = serverSocket;

    if (epoll_ctl(epfd,
                  EPOLL_CTL_ADD,
                  serverSocket,
                  &event) == -1)
    {
        cerr << "Error al agregar serverSocket a epoll\n";
        close(serverSocket);
        close(epfd);
        return 1;
    }

    const int MAX_EVENTS = 10;
    epoll_event events[MAX_EVENTS];

    while (true) {

        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (nfds == -1) {
            cerr << "Error en epoll_wait\n";
            continue;
        }

        for (int i = 0; i < nfds; i++) {

            // Nueva conexión
            if (events[i].data.fd == serverSocket) {

                int clientSocket = accept(serverSocket, nullptr, nullptr);

                if (clientSocket == -1) {
                    cerr << "Error en accept\n";
                    continue;
                }

                cout << "Nueva conexión entrante: "
                     << clientSocket << endl;

                epoll_event clienteEvent{};
                clienteEvent.events = EPOLLIN;
                clienteEvent.data.fd = clientSocket;

                if (epoll_ctl(epfd,
                              EPOLL_CTL_ADD,
                              clientSocket,
                              &clienteEvent) == -1)
                {
                    cerr << "Error al agregar cliente a epoll\n";
                    close(clientSocket);
                    continue;
                }

            }
            // Datos de un cliente
            else {

                int client_fd = events[i].data.fd;

                char buffer[1024];
                int bytes = recv(client_fd,
                                 buffer,
                                 sizeof(buffer) - 1,
                                 0);

                // Cliente cerró conexión
                if (bytes == 0) {

                    cout << "Cliente desconectado: "
                         << client_fd << endl;

                    epoll_ctl(epfd,
                              EPOLL_CTL_DEL,
                              client_fd,
                              nullptr);

                    close(client_fd);
                }
                // Error
                else if (bytes < 0) {

                    cerr << "Error en recv para cliente "
                         << client_fd << endl;

                    epoll_ctl(epfd,
                              EPOLL_CTL_DEL,
                              client_fd,
                              nullptr);

                    close(client_fd);
                }
                // Mensaje recibido
                else {

                    buffer[bytes] = '\0';

                    cout << "Mensaje de "
                         << client_fd
                         << ": "
                         << buffer
                         << endl;
                }
            }
        }
    }

    close(epfd);
    close(serverSocket);

    return 0;
}
