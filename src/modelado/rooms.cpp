#include <iostream>
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

    const set<int>& obtenerUsuarios() const;

};
