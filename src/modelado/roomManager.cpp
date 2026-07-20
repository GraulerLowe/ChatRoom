#include <string>
#include <sys/socket.h>
#include "rooms.cpp"


// Esta clase se encargara de la creacion de metodos; crear, unir, obtenerRoom.

using namespace std;

class RoomManager {

private:

    unordered_map<string, Room> salas;

public:

    bool crearSala(
        const string& nombre,
        const string& contraseña,
        int creador
    );

    bool unirSala(
        const string& nombre,
        const string& contraseña,
        int usuario
    );

    Room* obtenerSala(
        const string& nombre
    );
};
