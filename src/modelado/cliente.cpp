#include <iostream>
#include <ostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "json.hpp"
#include <atomic>

using namespace std;

using json = nlohmann::json;

std::atomic<uint64_t> id_counter{0};

uint64_t generarIDUnico() {
    return ++id_counter; 
}

// Archivo de prueba
int main() {


    int puerto;
    cout<<"Ingresa el puerto de servidor: ";
    cin >> puerto;

    if (puerto < 1 || puerto > 65535) {
        cerr << "Puerto inválido. Debe estar entre 1 y 65535" << std::endl;
        return 1;
    }
    
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    string nombre;
    string mensaje;

    std::cout << "Ingresa tu nombre de usuario: ";
    cin >> nombre;

    // sending connection request
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) ==-1) {
      std::cerr << "Error al conectarse";
      close(clientSocket);
      return 1;
    }

    send(clientSocket, nombre.c_str(), nombre.length(), 0);

    // Json para identificar al cliente conectado
    json identify;
    identify["identificador"] = generarIDUnico();
    identify["usuario"] = nombre;
    std::string indentify_serializado = identify.dump();

    if (send(clientSocket, indentify_serializado.c_str(),
             indentify_serializado.length(), 0) == -1) {
      std::cerr << "Error al enviar el cliente";
    } 
    
    std::cout << "✅ Conectado exitosamente al servidor en el puerto " << puerto << std::endl;
    
    // sending data
    const char* message = "Hello, server!";
    send(clientSocket, message, strlen(message), 0);

    // closing socket
    close(clientSocket);

    return 0;
}
