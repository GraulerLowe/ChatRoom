#include <cstring>
#include <iostream>
#include <algorithm>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "json.hpp"
#include "rooms.hpp"
#include <vector>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;

vector<int>                  clientesConectados;
unordered_map<string, Room>  salas;
unordered_map<int, string>   clientes; // fd → nombre

int main()
{
    // ── 1. CREAR SOCKET ──────────────────────────────────────────
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error al crear socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ── 2. CONFIGURAR DIRECCIÓN ───────────────────────────────────
    sockaddr_in serverAddress{};
    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_port        = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // ── 3. BIND ───────────────────────────────────────────────────
    if (bind(serverSocket,
             (struct sockaddr*)&serverAddress,
             sizeof(serverAddress)) == -1)
    {
        cerr << "Error en bind\n";
        close(serverSocket);
        return 1;
    }

    // ── 4. LISTEN ─────────────────────────────────────────────────
    if (listen(serverSocket, 5) == -1) {
        cerr << "Error en listen\n";
        close(serverSocket);
        return 1;
    }

    cout << "Servidor escuchando en puerto 8080\n";

    // ── 5. EPOLL ──────────────────────────────────────────────────
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        cerr << "Error al crear epoll\n";
        close(serverSocket);
        return 1;
    }

    epoll_event event{};
    event.events  = EPOLLIN;
    event.data.fd = serverSocket;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSocket, &event) == -1) {
        cerr << "Error al agregar serverSocket a epoll\n";
        close(serverSocket);
        close(epfd);
        return 1;
    }

    const int MAX_EVENTS = 10;
    epoll_event events[MAX_EVENTS];

    // ── 6. LOOP PRINCIPAL ─────────────────────────────────────────
    while (true) {

        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            cerr << "Error en epoll_wait\n";
            continue;
        }

        for (int i = 0; i < nfds; i++) {

            // ── CASO A: nueva conexión ────────────────────────────
            if (events[i].data.fd == serverSocket) {

                int clientSocket = accept(serverSocket, nullptr, nullptr);
                if (clientSocket == -1) {
                    cerr << "Error en accept\n";
                    continue;
                }

                cout << "Nueva conexión entrante: fd=" << clientSocket << endl;

                epoll_event clienteEvent{};
                clienteEvent.events  = EPOLLIN;
                clienteEvent.data.fd = clientSocket;

                clientesConectados.push_back(clientSocket);

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, clientSocket, &clienteEvent) == -1) {
                    cerr << "Error al agregar cliente a epoll\n";
                    close(clientSocket);
                    continue;
                }

            }
            // ── CASO B: datos de un cliente ───────────────────────
            else {

                int client_fd = events[i].data.fd;
                char buffer[1024];
                int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                // cliente cerró conexión
                if (bytes == 0) {
                    string usuario = clientes.count(client_fd) ? clientes[client_fd] : "desconocido";
                    cout << usuario << " desconectado (fd=" << client_fd << ")\n";

                    // avisar a todos
                    string aviso = "*** " + usuario + " salió del chat ***\n";
                    for (int fd : clientesConectados) {
                        if (fd == client_fd) continue;
                        send(fd, aviso.c_str(), aviso.length(), 0);
                    }

                    // eliminar de todas las salas
                    for (auto& [nombre, sala] : salas) {
                        sala.eliminarUsuario(client_fd);
                    }

                    clientes.erase(client_fd);
                    clientesConectados.erase(
                        remove(clientesConectados.begin(),
                               clientesConectados.end(), client_fd),
                        clientesConectados.end()
                    );
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                    close(client_fd);
                    continue;
                }

                // error en recv
                else if (bytes < 0) {
                    cerr << "Error en recv para cliente fd=" << client_fd << endl;
                    clientes.erase(client_fd);
                    clientesConectados.erase(
                        remove(clientesConectados.begin(),
                               clientesConectados.end(), client_fd),
                        clientesConectados.end()
                    );
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                    close(client_fd);
                    continue;
                }

                // mensaje recibido
                else {
                    buffer[bytes] = '\0';

                    try {
                        json msg    = json::parse(buffer);
                        string accion = msg.value("accion", "mensaje");

                        // ── primera vez → identify ────────────────
                        if (clientes.find(client_fd) == clientes.end()) {
                            string usuario = msg["usuario"];
                            clientes[client_fd] = usuario;
                            cout << usuario << " se identificó\n";

                            string aviso = "*** " + usuario + " se unió al chat ***\n";
                            for (int fd : clientesConectados) {
                                if (fd == client_fd) continue;
                                send(fd, aviso.c_str(), aviso.length(), 0);
                            }

                        // ── crear sala ────────────────────────────
                        } else if (accion == "/crear") {
                            string nombre_sala = msg["sala"];
                            string pass        = msg["contraseña"];

                            if (salas.count(nombre_sala)) {
                                string err = "*** La sala '" + nombre_sala + "' ya existe ***\n";
                                send(client_fd, err.c_str(), err.length(), 0);
                            } else {
                                salas.emplace(nombre_sala, Room(nombre_sala, pass));
                                salas[nombre_sala].agregarUsuario(client_fd);
                                string ok = "*** Sala '" + nombre_sala + "' creada ***\n";
                                send(client_fd, ok.c_str(), ok.length(), 0);
                            }

                        // ── unirse a sala ─────────────────────────
                        } else if (accion == "/unirse") {
                            string nombre_sala = msg["sala"];
                            string pass        = msg["contraseña"];

                            if (!salas.count(nombre_sala)) {
                                string err = "*** Sala '" + nombre_sala + "' no existe ***\n";
                                send(client_fd, err.c_str(), err.length(), 0);
                            } else if (!salas[nombre_sala].verificarContraseña(pass)) {
                                string err = "*** Contraseña incorrecta ***\n";
                                send(client_fd, err.c_str(), err.length(), 0);
                            } else {
                                salas[nombre_sala].agregarUsuario(client_fd);
                                string ok = "*** Entraste a '" + nombre_sala + "' ***\n";
                                send(client_fd, ok.c_str(), ok.length(), 0);
                            }

                        // ── mensaje a sala ────────────────────────
                        } else if (accion == "/mensaje_sala") {
                            string nombre_sala = msg["sala"];
                            string texto       = msg["mensaje"];
                            string usuario     = clientes[client_fd];
                            string salida      = "[" + nombre_sala + "] " + usuario + ": " + texto + "\n";

                            if (!salas.count(nombre_sala) || !salas[nombre_sala].contiene(client_fd)) {
                                string err = "*** No estás en esa sala ***\n";
                                send(client_fd, err.c_str(), err.length(), 0);
                            } else {
                                salas[nombre_sala].broadcast(salida);
                            }

                        // ── mensaje general ───────────────────────
                        } else {
                            string usuario = clientes[client_fd];
                            string texto   = msg["mensaje"];
                            string salida  = usuario + ": " + texto + "\n";

                            cout << salida;
                            for (int fd : clientesConectados) {
                                if (fd == client_fd) continue;
                                send(fd, salida.c_str(), salida.length(), 0);
                            }
                        }

                    } catch (const json::parse_error& e) {
                        cerr << "JSON inválido de fd=" << client_fd << ": " << e.what() << endl;
                    }
                }
            }
        } // cierra for
    } // cierra while

    close(epfd);
    close(serverSocket);
    return 0;
} 
