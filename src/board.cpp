////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Philip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
/*
 *  board.cpp
 */


#include "board.hpp"
#include "here.hpp"
#include <string.h>
#include <algorithm>
#include <iostream>

// Make sure hashing works. Note
// that the test is not thread safe.
//#define CHECK_HASH 1

// init_board() sets the board to the initial game state.


// Construct a board from a string
node_t::node_t(std::string s) {
  int spot = 0;
  for(auto i=s.begin();i != s.end();++i) {
    const char c = *i;
    bool incr = false;
    if (c == '.')
    {
      incr = true;
      color[spot] = 6;
      piece[spot] = 6;
    }
    else if(islower(c))
    {
      incr = true;
      color[spot] = 1;
      switch (c)
      {
        case 'k':
          piece[spot] = 5;
          break;
        case 'q':
          piece[spot] = 4;
          break;
        case 'r':
          piece[spot] = 3;
          break;
        case 'b':
          piece[spot] = 2;
          break;
        case 'n':
          piece[spot] = 1;
          break;
        case 'p':
          piece[spot] = 0;
          break;
        default:
          incr = false;
          break;
      }
    }
    else if(isupper(c))
    {
      incr = true;
      color[spot] = 0;
      switch (c)
      {
        case 'K':
          piece[spot] = 5;
          break;
        case 'Q':
          piece[spot] = 4;
          break;
        case 'R':
          piece[spot] = 3;
          break;
        case 'B':
          piece[spot] = 2;
          break;
        case 'N':
          piece[spot] = 1;
          break;
        case 'P':
          piece[spot] = 0;
          break;
        default:
          incr = false;
          break;
      }
    }
    if(incr && spot < 63)
      spot++;
  }
}

void init_board(node_t& board)
{
    int i;

    for (i = 0; i < 64; ++i) {
        board.color[i] = init_color[i];
        board.piece[i] = init_piece[i];
    }
    board.side = LIGHT;
    board.castle = 15;
    board.ep = -1;
    board.fifty = 0;
    board.move_num = 0;
    board.ply = 0;
    board.hist_dat.clear();
    // init_hash() must be called before this function
    board.hash = set_hash(board);  
    board.follow_capt = false;
}

bool board_equals(const node_t& b1,const node_t& b2) {
    return memcmp(&b1,&b2,sizeof(base_node_t)) == 0;
    int n = b1.hist_dat.size();
    if(n != b2.hist_dat.size())
        return false;
    for(int i=0;i<n;i++)
        if(b1.hist_dat[i] != b2.hist_dat[i])
            return false;
    return true;
}


// init_hash() initializes the random numbers used by set_hash().

void init_hash()
{
    int i, j, k;

    for (i = 0; i < 2; ++i)
        for (j = 0; j < 6; ++j)
            for (k = 0; k < 64; ++k)
                hash_piece[i][j][k] = hash_rand();
    hash_side = hash_rand();
    for (i = 0; i < 64; ++i)
        hash_ep[i] = hash_rand();
}
   
static int hash_index = 0;

hash_t hash_rand()
{
    return rnum[hash_index++];
}


/* set_hash() uses the Zobrist method of generating a unique number (hash)
   for the current chess position. Of course, there are many more chess
   positions than there are 32 bit numbers, so the numbers generated are
   not really unique, but they're unique enough for our purposes (to detect
   repetitions of the position). 
   The way it works is to XOR random numbers that correspond to features of
   the position, e.g., if there's a black knight on B8_CHESS, hash is XORed with
   hash_piece[BLACK][KNIGHT][B8_CHESS]. All of the pieces are XORed together,
   hash_side is XORed if it's black's chess_move, and the en passant square is
   XORed if there is one. (A chess technicality is that one position can't
   be a repetition of another if the en passant state is different.) */

hash_t set_hash(node_t& board)
{
    int i;

    hash_t hash(0);   
    for (i = 0; i < 64; ++i)
        if (board.color[i] != EMPTY)
            hash ^= hash_piece[(size_t)board.color[i]][(size_t)board.piece[i]][i];
    if (board.side == DARK)
        hash ^= hash_side;
    if (board.ep != -1)
        hash ^= hash_ep[board.ep];

    return hash;
}

/*  
    This is the function for updating the hash instead of recomputing it based on a chess_move.

    For instance, if a pawn on a chessboard square is replaced by a rook from another square, 
    the resulting position would be produced by XORing the existing hash like this:
       0) XOR out the pawn at the destination square
       1) XOR in the rook at the destination square
       2) XOR out the rook from the source square
    
    Also because the chess_move changes the side of the player, we will XOR the hash_side.
    This function is called at the beginning of makemove()
*/
hash_t update_hash(node_t& board, chess_move& m)
{
  /* m.from contains the location of the 'rook'
     m.to contains the location of the 'pawn'
     hash_piece[][][] is indexed by piece [color][type][square] */
  hash_t hash(board.hash);
  if (board.color[m.getTo()] != EMPTY)
    hash ^= hash_piece[(size_t)board.color[m.getTo()]][(size_t)board.piece[m.getTo()]][m.getTo()];    // XOR out the 'pawn' from the destination square (or skip if empty)
  hash ^= hash_piece[(size_t)board.color[m.getFrom()]][(size_t)board.piece[m.getFrom()]][m.getTo()];  // XOR in the 'rook' at the destination square
  hash ^= hash_piece[(size_t)board.color[m.getFrom()]][(size_t)board.piece[m.getFrom()]][m.getFrom()];  // XOR out the 'rook' from the source square
  
  hash ^= hash_side;
  
  return hash;
}


/* in_check() returns TRUE if side s is in check and FALSE
   otherwise. It just scans the board to find side s's king
   and calls attack() to see if it's being attacked. */

bool in_check(const node_t& board, int s)
{
    int i;

    for (i = 0; i < 64; ++i)
        if (board.piece[i] == KING && board.color[i] == s)
            return attack(board, i, s ^ 1);
   // assert(false);
    return true;  /* shouldn't get here */
}


/* attack() returns TRUE if square sq is being attacked by side
   s and FALSE otherwise. */

bool attack(const node_t& board, int sq, int s)
{
    int i, j, n;

    for (i = 0; i < 64; ++i)
        if (board.color[i] == s) {
            if (board.piece[i] == PAWN) {
                if (s == LIGHT) {
                    if (COL(i) != 0 && i - 9 == sq)
                        return true;
                    if (COL(i) != 7 && i - 7 == sq)
                        return true;
                }
                else {
                    if (COL(i) != 0 && i + 7 == sq)
                        return true;
                    if (COL(i) != 7 && i + 9 == sq)
                        return true;
                }
            }
            else if(board.piece[i] != EMPTY) {
                int bp = board.piece[i];
                for (j = 0; j < offsets[bp]; ++j) {
                    for (n = i;;) {
                        n = mailbox[mailbox64[n] + offset[bp][j]];
                        if (n == -1)
                            break;
                        if (n == sq)
                            return true;
                        if (board.color[n] != EMPTY)
                            break;
                        if (!slide[bp])
                            break;
                    }
                }
            }
        }
    return false;
}

bool workq_sort(const chess_move& m1,const chess_move& m2) {
    return m1.score > m2.score;
}

/* gen() generates pseudo-legal moves for the current position.
   It scans the board to find friendly pieces and then determines
   what squares they attack. When it finds a piece/square
   combination, it calls gen_push to put the chess_move on the "chess_move
   stack." */

void gen(std::vector<chess_move>& workq, const node_t& board)
{
  int i, j, n;
  do_data();

  // so far, we have no moves for the current ply

  for (i = 0; i < 64; ++i)
      if (board.color[i] == board.side) {
          if (board.piece[i] == PAWN) {
              if (board.side == LIGHT) {
                  if (COL(i) != 0 && board.color[i - 9] == DARK)
                      gen_push(workq, board, i, i - 9, 17);
                  if (COL(i) != 7 && board.color[i - 7] == DARK)
                      gen_push(workq, board, i, i - 7, 17);
                  if (board.color[i - 8] == EMPTY) {
                      gen_push(workq, board, i, i - 8, 16);
                      if (i >= 48 && board.color[i - 16] == EMPTY)
                          gen_push(workq, board, i, i - 16, 24);
                  }
              }
              else {
                  if (COL(i) != 0 && board.color[i + 7] == LIGHT)
                      gen_push(workq, board, i, i + 7, 17);
                  if (COL(i) != 7 && board.color[i + 9] == LIGHT)
                      gen_push(workq, board, i, i + 9, 17);
                  if (board.color[i + 8] == EMPTY) {
                      gen_push(workq, board, i, i + 8, 16);
                      if (i <= 15 && board.color[i + 16] == EMPTY)
                          gen_push(workq, board, i, i + 16, 24);
                  }
              }
          }
          else if(board.piece[i] != EMPTY)
              for (j = 0; j < offsets[(size_t)board.piece[i]]; ++j)
                  for (n = i;;) {
                      n = mailbox[mailbox64[n] + offset[(size_t)board.piece[i]][j]];
                      if (n == -1)
                          break;
                      if (board.color[n] != EMPTY) {
                          if (board.color[n] == (board.side ^ 1))
                              gen_push(workq, board, i, n, 1);
                          break;
                      }
                      gen_push(workq, board, i, n, 0);
                      if (!slide[(size_t)board.piece[i]])
                          break;
                  }
      }


  // generate castle moves
  if (board.side == LIGHT) {
      if (board.castle & 1)
          gen_push(workq, board, E1_CHESS, G1_CHESS, 2);
      if (board.castle & 2)
          gen_push(workq, board, E1_CHESS, C1_CHESS, 2);
  }
  else {
      if (board.castle & 4)
          gen_push(workq, board, E8_CHESS, G8_CHESS, 2);
      if (board.castle & 8)
          gen_push(workq, board, E8_CHESS, C8_CHESS, 2);
  }
  
  // generate en passant moves
  if (board.ep != -1) {
      if (board.side == LIGHT) {
          if (COL(board.ep) != 0 && board.color[board.ep + 7] == LIGHT && board.piece[board.ep + 7] == PAWN)
              gen_push(workq, board, board.ep + 7, board.ep, 21);
          if (COL(board.ep) != 7 && board.color[board.ep + 9] == LIGHT && board.piece[board.ep + 9] == PAWN)
              gen_push(workq, board, board.ep + 9, board.ep, 21);
      }
      else {
          if (COL(board.ep) != 0 && board.color[board.ep - 9] == DARK && board.piece[board.ep - 9] == PAWN)
              gen_push(workq, board, board.ep - 9, board.ep, 21);
          if (COL(board.ep) != 7 && board.color[board.ep - 7] == DARK && board.piece[board.ep - 7] == PAWN)
              gen_push(workq, board, board.ep - 7, board.ep, 21);
      }
  }
  std::sort(workq.begin(),workq.end(),workq_sort);
}

uint8_t score_piece(uint8_t p) {
    switch(p) {
        case QUEEN:
            return 10;
        case ROOK:
            return 5;
        case KNIGHT:
        case BISHOP:
            return 3;
        case KING:
            return 50;
        case PAWN:
            return 1;
        case EMPTY:
            return 0;
        default:
            std::cout << "p=(" << int(p) << ")" << std::endl;
            abort();
    }
    return 1;
}

/* gen_push() puts a chess_move on the chess_move stack, unless it's a
   pawn promotion that needs to be handled by gen_promote().
   It also assigns a score to the chess_move for alpha-beta chess_move
   ordering. If the chess_move is a capture, it uses MVV/LVA
   (Most Valuable Victim/Least Valuable Attacker). Otherwise,
   it uses the chess_move's history heuristic value. Note that
   1,000,000 is added to a capture chess_move's score, so it
   always gets ordered above a "normal" chess_move. */

void gen_push(std::vector<chess_move>& workq, const node_t& board, int from, int to, int bits)
{
    chess_move g;
    uint8_t score;

    // test for capture
    if (bits & 1) {
        score = 100*score_piece(board.piece[to])
                  -10*score_piece(board.piece[from]);
    } else {
        score = score_piece(board.piece[from]);
    }
    g.score = score;
    
    if (bits & 16) {
        if (board.side == LIGHT) {
            if (to <= H8_CHESS) {
                gen_promote(workq, from, to, bits);
                return;
            }
        }
        else {
            if (to >= A1_CHESS) {
                gen_promote(workq, from, to, bits);
                return;
            }
        }
    }
    g.setBytes(from, to, 0, bits);

    workq.push_back(g);
}


/* gen_promote() is just like gen_push(), only it puts 4 moves
   on the chess_move stack, one for each possible promotion piece */

void gen_promote(std::vector<chess_move>& workq, int from, int to, int bits)
{
    int i;
    chess_move g;
    
    for (i = KNIGHT; i <= QUEEN; ++i) {
        g.setBytes(from, to, i, (bits | 32));
        workq.push_back(g);
    }
}


/* makemove() makes a chess_move. If the chess_move is illegal, it
   undoes whatever it did and returns FALSE. Otherwise, it
   returns TRUE. */

bool makemove(node_t& board,chess_move& m)
{
    bool needs_set_hash = false;
    if(board.ep != -1)
        board.hash ^= hash_ep[board.ep];
    board.hash = update_hash(board, m);
    /* test to see if a castle chess_move is legal and chess_move the rook
       (the king is moved with the usual chess_move code later) */
    if (m.getBits() & 2) {
        int from, to;
        needs_set_hash = true;
        // It's not legal to castle if
        // the king is not on the from
        // square. SRB
        if(board.piece[m.getFrom()] != KING || board.color[m.getFrom()] != board.side)
            return false;

        if (in_check(board, board.side))
            return false;
        switch (m.getTo()) {
            case 62:
                if (board.color[F1_CHESS] != EMPTY || board.color[G1_CHESS] != EMPTY ||
                        attack(board, F1_CHESS, board.side ^ 1) || attack(board, G1_CHESS, board.side ^ 1))
                    return false;
                from = H1_CHESS;
                to = F1_CHESS;
                break;
            case 58:
                if (board.color[B1_CHESS] != EMPTY || board.color[C1_CHESS] != EMPTY || board.color[D1_CHESS] != EMPTY ||
                        attack(board, C1_CHESS, board.side ^ 1) || attack(board, D1_CHESS, board.side ^ 1))
                    return false;
                from = A1_CHESS;
                to = D1_CHESS;
                break;
            case 6:
                if (board.color[F8_CHESS] != EMPTY || board.color[G8_CHESS] != EMPTY ||
                        attack(board, F8_CHESS, board.side ^ 1) || attack(board, G8_CHESS, board.side ^ 1))
                    return false;
                from = H8_CHESS;
                to = F8_CHESS;
                break;
            case 2:
                if (board.color[B8_CHESS] != EMPTY || board.color[C8_CHESS] != EMPTY || board.color[D8_CHESS] != EMPTY ||
                        attack(board, C8_CHESS, board.side ^ 1) || attack(board, D8_CHESS, board.side ^ 1))
                    return false;
                from = A8_CHESS;
                to = D8_CHESS;
                break;
            default:  /* shouldn't get here */
                from = -1;
                to = -1;
                break;
        }
        board.color[to] = board.color[from];
        board.piece[to] = board.piece[from];
        board.color[from] = EMPTY;
        board.piece[from] = EMPTY;
    }

    board.hist_dat.push_back(board.hash);
    board.ply++;

    /* update the castle, en passant, and
       fifty-chess_move-draw variables */
    board.castle &= castle_mask[m.getFrom()] & castle_mask[m.getTo()];
    if (m.getBits() & 8) {
        if (board.side == LIGHT)
        {
            board.ep = m.getTo() + 8;
            board.hash ^= hash_ep[board.ep];
        }
        else
        {
            board.ep = m.getTo() - 8;
            board.hash ^= hash_ep[board.ep];
        }
    }
    else
        board.ep = -1;
    if (m.getBits() & 17){
        board.fifty = 0;
        board.move_num++;
        board.hist_dat.clear();
    }else{ 
        board.fifty++;
        board.move_num++;
    }
    assert(board.hist_dat.size() == board.fifty);
    /* move the piece */
    board.color[m.getTo()] = board.side;
    if (m.getBits() & 32)
    {
        board.piece[m.getTo()] = m.getPromote();
        needs_set_hash = true;
    }
    else
        board.piece[m.getTo()] = board.piece[m.getFrom()];
    board.color[m.getFrom()] = EMPTY;
    board.piece[m.getFrom()] = EMPTY;

    /* erase the pawn if this is an en passant chess_move */
    if (m.getBits() & 4) {
        if (board.side == LIGHT) {
            board.color[m.getTo() + 8] = EMPTY;
            board.piece[m.getTo() + 8] = EMPTY;
            needs_set_hash = true;
        }
        else {
            board.color[m.getTo() - 8] = EMPTY;
            board.piece[m.getTo() - 8] = EMPTY;
            needs_set_hash = true;
        }
    }

    /* switch sides and test for legality (if we can capture
       the other guy's king, it's an illegal position and
       we need to return FALSE) */
    board.side ^= 1;
    
    if (in_check(board, board.side ^ 1)) {
        return false;
    }
    if (needs_set_hash)
    {
      board.hash = set_hash(board);
#if CHECK_HASH
      count2++;
#endif
    }
// This could debugs update_hash(), making sure it's consistent
// with set_hash()
#if CHECK_HASH
    hash_t sh = set_hash(board);
    if (board.hash == sh)
      count++;
    if (board.hash != sh) {
      std::cerr << "board.hash == " << board.hash << " :: set_hash == " << sh << std::endl;
      std::cerr << "Number of times board.hash == sh: " << count << std::endl;
      std::cerr << "Number of times set_hash was called: " << count2 << std::endl;
      abort();
    }
#endif
    //board.hash = set_hash(board);
    //assert(board.hash != 0);
    return true;
}
