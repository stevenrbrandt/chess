////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Philip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
/*
 *  eval.cpp
 */

#include "eval.hpp"

// the values of the pieces
const int piece_value[6] = {
    100, 300, 300, 500, 900, 0
};

/* The "pcsq" arrays are piece/square tables. They're values
   added to the material value of the piece based on the
   location of the piece. */

const int pawn_pcsq[] = {
      0,   0,   0,   0,   0,   0,   0,   0,
      5,  10,  15,  20,  20,  15,  10,   5,
      4,   8,  12,  16,  16,  12,   8,   4,
      3,   6,   9,  12,  12,   9,   6,   3,
      2,   4,   6,   8,   8,   6,   4,   2,
      1,   2,   3, -10, -10,   3,   2,   1,
      0,   0,   0, -40, -40,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0
};

const int knight_pcsq[] = {
    -10, -10, -10, -10, -10, -10, -10, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10, -30, -10, -10, -10, -10, -30, -10
};

const int bishop_pcsq[] = {
    -10, -10, -10, -10, -10, -10, -10, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10, -10, -20, -10, -10, -20, -10, -10
};

const int king_pcsq[] = {
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -40, -40, -40, -40, -40, -40, -40, -40,
    -20, -20, -20, -20, -20, -20, -20, -20,
      0,  20,  40, -20,   0, -20,  40,  20
};

const int king_endgame_pcsq[] = {
      0,  10,  20,  30,  30,  20,  10,   0,
     10,  20,  30,  40,  40,  30,  20,  10,
     20,  30,  40,  50,  50,  40,  30,  20,
     30,  40,  50,  60,  60,  50,  40,  30,
     30,  40,  50,  60,  60,  50,  40,  30,
     20,  30,  40,  50,  50,  40,  30,  20,
     10,  20,  30,  40,  40,  30,  20,  10,
      0,  10,  20,  30,  30,  20,  10,   0
};

/* The flip array is used to calculate the piece/square
   values for DARK pieces. The piece/square value of a
   LIGHT pawn is pawn_pcsq[sq] and the value of a DARK
   pawn is pawn_pcsq[flip[sq]] */
const int flip[] = {
     56,  57,  58,  59,  60,  61,  62,  63,
     48,  49,  50,  51,  52,  53,  54,  55,
     40,  41,  42,  43,  44,  45,  46,  47,
     32,  33,  34,  35,  36,  37,  38,  39,
     24,  25,  26,  27,  28,  29,  30,  31,
     16,  17,  18,  19,  20,  21,  22,  23,
      8,   9,  10,  11,  12,  13,  14,  15,
      0,   1,   2,   3,   4,   5,   6,   7
};

/* pawn_rank[x][y] is the rank of the least advanced pawn of color x on file
   y - 1. There are "buffer files" on the left and right to avoid special-case
   logic later. If there's no pawn on a rank, we pretend the pawn is
   impossibly far advanced (0 for LIGHT and 7 for DARK). This makes it easy to
   test for pawns on a rank and it simplifies some pawn evaluation code. */
/*
int pawn_rank[2][10];

int piece_mat[2];  // the value of a side's pieces
int pawn_mat[2];  // the value of a side's pawns
*/

int evaluator::eval(const node_t& board, int evaluator)
{
    // Interface function that selects which evaluator to use
    
    if (evaluator == ORIGINAL)
        return eval_orig(board);
    else
        return eval_simple(board);
}

int evaluator::eval_simple(const node_t& board)
{
    // A simple material evaluator
    
    enum material_weight {KING_WEIGHT = 200, QUEEN_WEIGHT = 9, ROOK_WEIGHT = 5, BISHOP_WEIGHT = 3, KNIGHT_WEIGHT = 3, PAWN_WEIGHT = 1};
    
    int score[2];
    int i;
    score[LIGHT]=0;
    score[DARK]=0;
    
    for (i = 0; i < 64; ++i)
    {
        if (board.color[i] == EMPTY)
            continue;
        switch (board.piece[i]) {
            case KING:
                score[(int)board.color[i]]+=KING_WEIGHT;
                break;
            case QUEEN:
                score[(int)board.color[i]]+=QUEEN_WEIGHT;
                break;
            case ROOK:
                score[(int)board.color[i]]+=ROOK_WEIGHT;
                break;
            case BISHOP:
                score[(int)board.color[i]]+=BISHOP_WEIGHT;
                break;
            case KNIGHT:
                score[(int)board.color[i]]+=KNIGHT_WEIGHT;
                break;
            case PAWN:
                score[(int)board.color[i]]+=PAWN_WEIGHT;
                break;
        }
    }
    
    if (board.side == LIGHT)
        return score[LIGHT] - score[DARK];
    return score[DARK] - score[LIGHT];
}

int evaluator::eval_orig(const node_t& board)
{
    int i;
    int f;  // file
    int score[2];  // each side's score

    f = i = 0;
    score[0] = score[1] = 0;

    // this is the first pass: set up pawn_rank, piece_mat, and pawn_mat.
    for (i = 0; i < 10; ++i) {
        pawn_rank[LIGHT][i] = 0;
        pawn_rank[DARK][i] = 7;
    }
    piece_mat[LIGHT] = 0;
    piece_mat[DARK] = 0;
    pawn_mat[LIGHT] = 0;
    pawn_mat[DARK] = 0;
    for (i = 0; i < 64; ++i) {
        if (board.color[i] == EMPTY)
            continue;
        if (board.piece[i] == PAWN) {
            pawn_mat[(int)board.color[i]] += piece_value[PAWN];
            f = COL(i) + 1;  // add 1 because of the extra file in the array
            if (board.color[i] == LIGHT) {
                if (pawn_rank[LIGHT][f] < ROW(i))
                    pawn_rank[LIGHT][f] = ROW(i);
            }
            else {
                if (pawn_rank[DARK][f] > ROW(i))
                    pawn_rank[DARK][f] = ROW(i);
            }
        }
        else
            piece_mat[(int)board.color[i]] += piece_value[(int)board.piece[i]];
    }

    // this is the second pass: evaluate each piece
    score[LIGHT] = piece_mat[LIGHT] + pawn_mat[LIGHT];
    score[DARK] = piece_mat[DARK] + pawn_mat[DARK];
    for (i = 0; i < 64; ++i) {
        if (board.color[i] == EMPTY)
            continue;
        if (board.color[i] == LIGHT) {
            switch (board.piece[i]) {
                case PAWN:
                    score[LIGHT] += eval_light_pawn(i);
                    break;
                case KNIGHT:
                    score[LIGHT] += knight_pcsq[i];
                    break;
                case BISHOP:
                    score[LIGHT] += bishop_pcsq[i];
                    break;
                case ROOK:
                    if (pawn_rank[LIGHT][COL(i) + 1] == 0) {
                        if (pawn_rank[DARK][COL(i) + 1] == 7)
                            score[LIGHT] += ROOK_OPEN_FILE_BONUS;
                        else
                            score[LIGHT] += ROOK_SEMI_OPEN_FILE_BONUS;
                    }
                    if (ROW(i) == 1)
                        score[LIGHT] += ROOK_ON_SEVENTH_BONUS;
                    break;
                case KING:
                    if (piece_mat[DARK] <= 1200)
                        score[LIGHT] += king_endgame_pcsq[i];
                    else
                        score[LIGHT] += eval_light_king(i);
                    break;
            }
        }
        else {
            switch (board.piece[i]) {
                case PAWN:
                    score[DARK] += eval_dark_pawn(i);
                    break;
                case KNIGHT:
                    score[DARK] += knight_pcsq[flip[i]];
                    break;
                case BISHOP:
                    score[DARK] += bishop_pcsq[flip[i]];
                    break;
                case ROOK:
                    if (pawn_rank[DARK][COL(i) + 1] == 7) {
                        if (pawn_rank[LIGHT][COL(i) + 1] == 0)
                            score[DARK] += ROOK_OPEN_FILE_BONUS;
                        else
                            score[DARK] += ROOK_SEMI_OPEN_FILE_BONUS;
                    }
                    if (ROW(i) == 6)
                        score[DARK] += ROOK_ON_SEVENTH_BONUS;
                    break;
                case KING:
                    if (piece_mat[LIGHT] <= 1200)
                        score[DARK] += king_endgame_pcsq[flip[i]];
                    else
                        score[DARK] += eval_dark_king(i);
                    break;
            }
        }
    }

    /* the score[] array is set, now return the score relative
       to the side to chess_move */
    if (board.side == LIGHT)
        return score[LIGHT] - score[DARK];
    return score[DARK] - score[LIGHT];
}

int evaluator::eval_light_pawn(int sq)
{
    int r;  // the value to return
    int f;  // the pawn's file

    r = 0;
    f = COL(sq) + 1;

    r += pawn_pcsq[sq];

    // if there's a pawn behind this one, it's doubled
    if (pawn_rank[LIGHT][f] > ROW(sq))
        r -= DOUBLED_PAWN_PENALTY;

    /* if there aren't any friendly pawns on either side of
       this one, it's isolated */
    if ((pawn_rank[LIGHT][f - 1] == 0) &&
            (pawn_rank[LIGHT][f + 1] == 0))
        r -= ISOLATED_PAWN_PENALTY;

    // if it's not isolated, it might be backwards
    else if ((pawn_rank[LIGHT][f - 1] < ROW(sq)) &&
            (pawn_rank[LIGHT][f + 1] < ROW(sq)))
        r -= BACKWARDS_PAWN_PENALTY;

    // add a bonus if the pawn is passed
    if ((pawn_rank[DARK][f - 1] >= ROW(sq)) &&
            (pawn_rank[DARK][f] >= ROW(sq)) &&
            (pawn_rank[DARK][f + 1] >= ROW(sq)))
        r += (7 - ROW(sq)) * PASSED_PAWN_BONUS;

    return r;
}

int evaluator::eval_dark_pawn(int sq)
{
    int r;  // the value to return
    int f;  // the pawn's file

    r = 0;
    f = COL(sq) + 1;

    r += pawn_pcsq[flip[sq]];

    // if there's a pawn behind this one, it's doubled
    if (pawn_rank[DARK][f] < ROW(sq))
        r -= DOUBLED_PAWN_PENALTY;

    /* if there aren't any friendly pawns on either side of
       this one, it's isolated */
    if ((pawn_rank[DARK][f - 1] == 7) &&
            (pawn_rank[DARK][f + 1] == 7))
        r -= ISOLATED_PAWN_PENALTY;

    // if it's not isolated, it might be backwards
    else if ((pawn_rank[DARK][f - 1] > ROW(sq)) &&
            (pawn_rank[DARK][f + 1] > ROW(sq)))
        r -= BACKWARDS_PAWN_PENALTY;

    // add a bonus if the pawn is passed
    if ((pawn_rank[LIGHT][f - 1] <= ROW(sq)) &&
            (pawn_rank[LIGHT][f] <= ROW(sq)) &&
            (pawn_rank[LIGHT][f + 1] <= ROW(sq)))
        r += ROW(sq) * PASSED_PAWN_BONUS;

    return r;
}

int evaluator::eval_light_king(int sq)
{
    int r;  //the value to return
    int i;

    r = king_pcsq[sq];

    /* if the king is castled, use a special function to evaluate the
       pawns on the appropriate side */
    if (COL(sq) < 3) {
        r += eval_lkp(1);
        r += eval_lkp(2);
        r += eval_lkp(3) / 2;  /* problems with pawns on the c & f files
                                  are not as severe */
    }
    else if (COL(sq) > 4) {
        r += eval_lkp(8);
        r += eval_lkp(7);
        r += eval_lkp(6) / 2;
    }

    /* otherwise, just assess a penalty if there are open files near
       the king */
    else {
        for (i = COL(sq); i <= COL(sq) + 2; ++i)
            if ((pawn_rank[LIGHT][i] == 0) &&
                    (pawn_rank[DARK][i] == 7))
                r -= 10;
    }

    /* scale the king safety value according to the opponent's material;
       the premise is that your king safety can only be bad if the
       opponent has enough pieces to attack you */
    r *= piece_mat[DARK];
    r /= 3100;

    return r;
}

// eval_lkp(f) evaluates the Light King Pawn on file f

int evaluator::evaluator::eval_lkp(int f)
{
    int r = 0;

    if (pawn_rank[LIGHT][f] == 6);  // pawn hasn't moved
    else if (pawn_rank[LIGHT][f] == 5)
        r -= 10;  // pawn moved one square
    else if (pawn_rank[LIGHT][f] != 0)
        r -= 20;  // pawn moved more than one square
    else
        r -= 25;  // no pawn on this file

    if (pawn_rank[DARK][f] == 7)
        r -= 15;  // no enemy pawn
    else if (pawn_rank[DARK][f] == 5)
        r -= 10;  // enemy pawn on the 3rd rank
    else if (pawn_rank[DARK][f] == 4)
        r -= 5;   // enemy pawn on the 4th rank

    return r;
}

int evaluator::eval_dark_king(int sq)
{
    int r;
    int i;

    r = king_pcsq[flip[sq]];
    if (COL(sq) < 3) {
        r += eval_dkp(1);
        r += eval_dkp(2);
        r += eval_dkp(3) / 2;
    }
    else if (COL(sq) > 4) {
        r += eval_dkp(8);
        r += eval_dkp(7);
        r += eval_dkp(6) / 2;
    }
    else {
        for (i = COL(sq); i <= COL(sq) + 2; ++i)
            if ((pawn_rank[LIGHT][i] == 0) &&
                    (pawn_rank[DARK][i] == 7))
                r -= 10;
    }
    r *= piece_mat[LIGHT];
    r /= 3100;
    return r;
}

int evaluator::eval_dkp(int f)
{
    int r = 0;

    if (pawn_rank[DARK][f] == 1);
    else if (pawn_rank[DARK][f] == 2)
        r -= 10;
    else if (pawn_rank[DARK][f] != 7)
        r -= 20;
    else
        r -= 25;

    if (pawn_rank[LIGHT][f] == 0)
        r -= 15;
    else if (pawn_rank[LIGHT][f] == 2)
        r -= 10;
    else if (pawn_rank[LIGHT][f] == 3)
        r -= 5;

    return r;
}
