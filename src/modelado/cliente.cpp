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
    cout << "Esta es la sala principal del chat." << endl;
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
    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_port        = htons(puerto);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cerr << "Error al conectarse" << endl;
        close(clientSocket);
        return 1;
    }

    cout << "✅ Conectado exitosamente al servidor en el puerto " << puerto << endl;
    cout << "Para desconectarte escribe '/salir'" << endl;
    cout << "Para crear una sala escribe '/crear <nombreSala> <contraseña>'" << endl;
    cout << "Para unirte a una sala escribe '/unir <nombreSala> <contraseña>'" << endl;
    cout << "Para consultar usuarios activos escribe '/usuarios'" << endl;

    // ── 3. ENVIAR JSON DE IDENTIFICACIÓN ─────────────────────────
    // Va ANTES de los threads para que el servidor sepa quién somos
    // antes de recibir cualquier mensaje del chat.

    json identify;
    identify["identificador"] = generarIDUnico();
    identify["usuario"]       = nombre;
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

            if (msg.empty())
                continue;

            // ── COMANDOS ─────────────────────────────────────────
            if (msg[0] == '/') {

                istringstream iss(msg);
                string comando, sala, contraseña;
                iss >> comando >> sala >> contraseña;

                // /salir
                if (comando == "/salir") {
                    corriendo = false;
                    shutdown(clientSocket, SHUT_RDWR); // despierta recv() en t_recv
                    break;
                }

                // /crear <nombreSala> <contraseña>
                else if (comando == "/crear") {
                    if (sala.empty() || contraseña.empty()) {
                        cout << "Uso: /crear <nombreSala> <contraseña>\n";
                        continue;
                    }

                    json crear;
                    crear["accion"]    = "/crear"; // debe coincidir con el servidor
                    crear["sala"]      = sala;
                    crear["contraseña"] = contraseña;
                    string datos = crear.dump();

                    if (send(clientSocket, datos.c_str(), datos.length(), 0) == -1) {
                        cerr << "Error al crear sala\n";
                        corriendo = false;
                        break;
                    }
                    continue;
                }

                // /unir <nombreSala> <contraseña>
                else if (comando == "/unir") {
                    if (sala.empty() || contraseña.empty()) {
                        cout << "Uso: /unir <nombreSala> <contraseña>\n";
                        continue;
                    }

                    json unir;
                    unir["accion"]    = "/unirse"; // debe coincidir con el servidor
                    unir["sala"]      = sala;
                    unir["contraseña"] = contraseña;
                    string datos = unir.dump();

                    if (send(clientSocket, datos.c_str(), datos.length(), 0) == -1) {
                        cerr << "Error al unirse a la sala\n";
                        corriendo = false;
                        break;
                    }
                    continue;
                }

                // /usuarios
                else if (comando == "/usuarios") {
                    json usuarios;
                    usuarios["accion"] = "/usuarios";
                    string datos = usuarios.dump();

                    if (send(clientSocket, datos.c_str(), datos.length(), 0) == -1) {
                        cerr << "Error al solicitar usuarios\n";
                        corriendo = false;
                        break;
                    }
                    continue;
                }

                else {
                    cout << "Comando desconocido. Comandos disponibles: /salir, /crear, /unir, /usuarios\n";
                    continue;
                }
            }

            // ── MENSAJE NORMAL ────────────────────────────────────

            json mensaje;
            mensaje["accion"]       = "mensaje";
            mensaje["identificador"] = generarIDUnico();
            mensaje["usuario"]      = nombre;
            mensaje["mensaje"]      = msg;
            string datos = mensaje.dump();

            if (send(clientSocket, datos.c_str(), datos.length(), 0) == -1) {
                cerr << "Error al enviar mensaje\n";
                corriendo = false;
                break;
            }
        }
    });

    // ── 6. THREAD DE RECEPCIÓN ────────────────────────────────────
    // Escucha mensajes entrantes del servidor y los muestra en pantalla.
    // Maneja tanto JSON (mensajes de otros usuarios) como texto plano
    // (avisos y confirmaciones del servidor).

    thread t_recv([&corriendo, clientSocket]() {
        char buf[1024];

        while (corriendo) {
            int n = recv(clientSocket, buf, sizeof(buf) - 1, 0);

            if (n <= 0) {
                // 0 = servidor cerró conexión, <0 = error o shutdown()
                corriendo = false;
                break;
            }

            buf[n] = '\0';

            try {
                // intenta parsear como JSON (mensaje de otro usuario)
                json message = json::parse(buf);

                if (message.contains("mensaje") && message.contains("usuario")) {
                    cout << message["usuario"].get<string>()
                         << ": "
                         << message["mensaje"].get<string>()
                         << endl;
                }

            } catch (const json::parse_error& e) {
                // no es JSON → es texto plano del servidor (avisos, confirmaciones)
                cout << buf << flush;
            }
        }
    });

    // ── 7. ESPERAR THREADS ────────────────────────────────────────

    t_send.join();
    t_recv.join();

    // ── 8. CERRAR SOCKET ──────────────────────────────────────────

    close(clientSocket);
    cout << "Conexión cerrada." << endl;

    return 0;
}
