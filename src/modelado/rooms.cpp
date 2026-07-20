#include <set>
#include <string>
#include <sys/socket.h>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
class Room {

private:

    string nombre;
    string contraseña;
    set<int> usuarios;

public:

    Room(string nombre, string contraseña);

    void agregarUsuario(int socket);

    void eliminarUsuario(int socket);

    bool contiene(int socket);

    const set<int> &obtenerUsuarios() const;

  bool verificarContraseña(string pass) {
    return contraseña == pass;
  }

  void broadcast(string mensaje) {
    for (int fd : usuarios) {
        send(fd, mensaje.c_str(), mensaje.length(), 0);
    }
  }

};
