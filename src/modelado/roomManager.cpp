#include <iostream>
#include <set>
#include <string>
#include <sys/socket.h>
#include "json.hpp"
#include "rooms.cpp"

using namespace std;
using json = nlohmann::json;

class RoomManager {

private:
  string accion;
  string usuario;
};
