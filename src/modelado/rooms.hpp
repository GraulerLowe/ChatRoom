#include <set>
#include <string>
#include <sys/socket.h>
#include <iostream>

using namespace std;

class Room {

private:

    string nombre;
    string contraseña;
    set<int> usuarios;

public:

    Room(string nombre, string contraseña)
    : nombre(nombre), contraseña(contraseña) {}

    void agregarUsuario(int socket) {
      usuarios.insert(socket);
      };

    void eliminarUsuario(int socket) {
      usuarios.erase(socket);
      };

    bool contiene(int socket) {
      auto it = usuarios.find(socket);
       if (it != usuarios.end()) {
        return true;
       }
       return false;
    };

    const set<int> &obtenerUsuarios() const { return usuarios; }
    
  bool verificarContraseña(string pass) {
    return contraseña == pass;
  }

  void broadcast(string mensaje) {
    for (int fd : usuarios) {
        send(fd, mensaje.c_str(), mensaje.length(), 0);
    }
  }

};
