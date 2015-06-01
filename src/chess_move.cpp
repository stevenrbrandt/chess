#include <sstream>
#include "defs.hpp"
#include "chess_move.hpp"

using namespace std;

string chess_move::str()
{
    ostringstream ostr;

    if (getBits() & 32)
    {   
        ostr << COL(getFrom()) << "a" << 8 - ROW(getFrom()) <<
                COL(getTo())   << "a" << 8 - ROW(getTo());
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
    } else
        ostr << COL(getFrom()) << "a" << 8 - ROW(getFrom()) <<
                COL(getTo())   << "a" << 8 - ROW(getTo());
    return ostr.str();
}
const char* chess_move::c_str()
{
    return this->str().c_str();
}
