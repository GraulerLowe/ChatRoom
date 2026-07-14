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
        string msg;

        while (corriendo) {
            getline(cin, msg);

            if (msg == "/salir") {
                corriendo = false;
                shutdown(clientSocket, SHUT_RDWR);
                break;
            }

            // Armar el JSON del mensaje con identificador, usuario y contenido
            json message;
            message["identificador"] = generarIDUnico();
            message["usuario"] = nombre;
            message["mensaje"] = msg;
            string message_serializado = message.dump();

            // Enviar el JSON serializado al servidor
            // fix: usar message_serializado (el string), no message (el objeto json)
            if (send(clientSocket, message_serializado.c_str(), message_serializado.length(), 0) == -1) {
                cerr << "Error al enviar mensaje" << endl;
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
