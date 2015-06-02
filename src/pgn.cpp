#include "pgn.hpp"
#include <iostream>
//#include <string>

using namespace std;

const void pgn_output(const node_t &board, const chess_move &move)
{
    string piece;
    piece = piece_char[(size_t)board.piece[move.getTo()]];
    if (piece == "p")
        piece = "";
    cout << "pgn " << piece << (move.getCapture()?"x":"") << move.pgn() << endl;
}

