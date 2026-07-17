#include <iostream>
#include <set>
#include "servidor.cpp"
#include "json.hpp"

using namespace std;

class Room {

public:
  string nombre;
  string contraseña;
  set<string> usuarios;
      
  Room(string n, string c) {
    nombre = n;
    contraseña = c;
    }
  
};

