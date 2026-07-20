#include <iostream>
#include <string>
#include <thread>
#include <ostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "json.hpp"
#include <atomic>
#include <sstream>

using namespace std;
using json = nlohmann::json;



// Contador global para generar IDs únicos por mensaje
std::atomic<uint64_t> id_counter{0};

uint64_t generarIDUnico() {
    return ++id_counter;
}

int main() {

    // ── 1. DATOS DE CONEXIÓN ──────────────────────────────────────

    int puerto;
    cout << "Bienvenido al ChatRoom" << endl;
    cout << "Esta es la sala principal del chat."<<endl;
    cout << "Ingresa el puerto de servidor: ";
    cin >> puerto;

    if (puerto < 1 || puerto > 65535) {
        cerr << "Puerto inválido. Debe estar entre 1 y 65535" << endl;
        return 1;
    }

    string nombre;
    cout << "Ingresa tu nombre de usuario: ";
    cin >> nombre;
    cin.ignore(); // limpiar el '\n' que queda en el buffer después de cin >> nombre
                  // sin esto, el primer getline() del thread de envío lee una línea vacía

    // ── 2. CREAR SOCKET Y CONECTAR ────────────────────────────────

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cerr << "Error al conectarse" << endl;
        close(clientSocket);
        return 1;
    }

    cout << "✅ Conectado exitosamente al servidor en el puerto " << puerto << endl;
    cout << "Para desconectarte escribe la palabra '/salir' " <<endl;
    cout << "Para crear una sala escribe la palabra '/crear <nombreSala> <contraseña>' " <<endl;
    cout << "Para unirte a una sala escribe la palabra '/unir <nombreSala> <contraseña>' " <<endl;
    cout << "Para consultar a los usuarios activos escribe la palabra '/usuarios' " <<endl;
    // ── 3. ENVIAR JSON DE IDENTIFICACIÓN ─────────────────────────
    // Esto va ANTES de los threads para que el servidor sepa quién
    // somos antes de recibir cualquier mensaje del chat.

    json identify;
    identify["identificador"] = generarIDUnico();
    identify["usuario"] = nombre;
    string identify_serializado = identify.dump();

    if (send(clientSocket, identify_serializado.c_str(), identify_serializado.length(), 0) == -1) {
        cerr << "Error al enviar identificación" << endl;
        close(clientSocket);
        return 1;
    }

    // ── 4. VARIABLE DE CONTROL COMPARTIDA ENTRE THREADS ──────────
    // atomic<bool> para que ambos threads puedan leerla/escribirla
    // sin race conditions. Cuando uno la pone en false, el otro se entera.

    atomic<bool> corriendo{true};

    // ── 5. THREAD DE ENVÍO ────────────────────────────────────────
    // Lee mensajes del usuario por stdin y los manda al servidor.
    // Se detiene cuando el usuario escribe "/salir".

thread t_send([&corriendo, &nombre, clientSocket]() {

    std::string msg;

    while (corriendo) {

        std::getline(std::cin, msg);

        if (msg.empty())
            continue;

        // ---------------- COMANDOS ----------------
        if (msg[0] == '/') {

            std::istringstream iss(msg);

            std::string comando;
            std::string sala;
            std::string contraseña;

            iss >> comando >> sala >> contraseña;

            if (comando == "/salir") {
                corriendo = false;
                shutdown(clientSocket, SHUT_RDWR);
                break;
            }

            else if (comando == "/crear") {

                if (sala.empty() || contraseña.empty()) {
                    cout << "Uso: /crear <nombreSala> <contraseña>\n";
                    continue;
                }

                json crear;
                crear["accion"] = "createRoom";
                crear["sala"] = sala;
                crear["contraseña"] = contraseña;

                string datos = crear.dump();

                if (send(clientSocket,
                         datos.c_str(),
                         datos.length(),
                         0) == -1) {

                    cerr << "Error al crear sala\n";
                    corriendo = false;
                    break;
                }

                continue;
            }

            else if (comando == "/unir") {

                if (sala.empty() || contraseña.empty()) {
                    cout << "Uso: /unir <nombreSala> <contraseña>\n";
                    continue;
                }

                json unir;
                unir["accion"] = "joinRoom";
                unir["sala"] = sala;
                unir["contraseña"] = contraseña;

                string datos = unir.dump();

                if (send(clientSocket,
                         datos.c_str(),
                         datos.length(),
                         0) == -1) {

                    cerr << "Error al unirse a la sala\n";
                    corriendo = false;
                    break;
                }

                continue;
            }

            else if (comando == "/usuarios") {

                json usuarios;
                usuarios["accion"] = "usuarios";

                string datos = usuarios.dump();

                if (send(clientSocket,
                         datos.c_str(),
                         datos.length(),
                         0) == -1) {

                    cerr << "Error al solicitar usuarios\n";
                    corriendo = false;
                    break;
                }

                continue;
            }

            else {

                cout << "Comando desconocido. Escribe /help para ayuda.\n";
                continue;
            }
        }

        // ---------------- MENSAJE NORMAL ----------------

        json mensaje;
        mensaje["accion"] = "mensaje";
        mensaje["identificador"] = generarIDUnico();
        mensaje["usuario"] = nombre;
        mensaje["mensaje"] = msg;

        string datos = mensaje.dump();

        if (send(clientSocket,
                 datos.c_str(),
                 datos.length(),
                 0) == -1) {

            cerr << "Error al enviar mensaje\n";
            corriendo = false;
            break;
        }
    }
});

    thread t_recv([&]() {
      char buf[1024];
      while (corriendo) {
        int message_recv = recv(clientSocket, buf, sizeof(buf) - 1, 0);
        if (message_recv <= 0) {
          // 0 = servidor cerró conexión, <0 = error
          corriendo = false;
          break;
        }
        buf[message_recv] = '\0';
        
        try {
          json message = json::parse(buf);
          if (message.contains("mensaje") && message["mensaje"].is_string()) {
            string mensaje = message["mensaje"];
            string usuario = message["usuario"];
            std::cout << usuario <<":" << mensaje << std::endl;
            }
          
        } catch (const json::parse_error &e) {
          cerr << "Error al parsear el JSON: " << e.what() << endl;
          }
        }
                
      });
    t_send.join();
    t_recv.join();

    // ── 8. CERRAR SOCKET ──────────────────────────────────────────

    close(clientSocket);
    cout << "Conexión cerrada." << endl;

    return 0;
}
