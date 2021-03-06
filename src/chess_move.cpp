#include <sstream>
#include "defs.hpp"
#include "data.hpp"
#include "chess_move.hpp"

using namespace std;

string chess_move::str() const
{
    ostringstream ostr;
    ostr << (char)('a' + COL(getFrom())) << 8 - ROW(getFrom()) <<
            (char)('a' + COL(getTo()))   << 8 - ROW(getTo());
    if (getBits() & 32)
        switch (getPromote())
        { 
          case KNIGHT:
            ostr << "n";
            break;
          case BISHOP:
            ostr << "b";
            break;
          case ROOK:
            ostr << "r";
            break;
          default: 
            ostr << "q";
            break;
        }
    return ostr.str();
}
string chess_move::pgn() const
{
    ostringstream ostr;
    ostr << piece_char[getFrom()];
    ostr << (char)('a' + COL(getFrom())) << 8 - ROW(getFrom()) <<
            (char)('a' + COL(getTo()))   << 8 - ROW(getTo());
    if (getBits() & 32)
        switch (getPromote())
        { 
          case KNIGHT:
            ostr << "=N";
            break;
          case BISHOP:
            ostr << "=B";
            break;
          case ROOK:
            ostr << "=R";
            break;
          default: 
            ostr << "=Q";
            break;
        }
    return ostr.str();
}

