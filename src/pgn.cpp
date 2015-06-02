#include "pgn.hpp"
#include <iostream>
//#include <string>

using namespace std;

void pgn_output(chess_move &move)
{
    cout << "pgn " << move.pgn() << endl;
}
