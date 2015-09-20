////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Philip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
#ifndef NODE_H
#define NODE_H
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include "score.hpp"
#include "defs.hpp"
#include "hash.hpp"

/**
 * IMPORTANT NOTE:
 * The board_equal() method compares the base_node_t
 * data bitwise to determine whether two boards are
 * equal. Don't put pointers here, and don't add anything
 * that would violate the needs of board_equal().
 */
struct base_node_t { 
    hash_t hash=0;
    char color[64];
    char piece[64];
    int depth=0;
    int excess_depth = 0;
    int side=-1;
    int castle=0;
    int ep=0;
    
    /* the number of moves since a capture or pawn chess_move, used to handle the fifty-chess_move-draw rule */
    int fifty=0;
    //counts number of moves
};
struct node_t : public base_node_t {
    int move_num=0;
    score_t p_board;
    int ply=0;
    bool follow_capt = false;
    bool root_side=-1;
    score_t follow_score;
    int follow_depth = 2;
    int search_depth = 0;
    std::vector<hash_t> hist_dat;

    void clear() {
      hist_dat.clear();
      fifty = 0;
      follow_capt = false;
      ply = 0;
      move_num = 0;
    }
    node_t() {}
    // Make it possible to construct a board from
    // a string representation.
    node_t(std::string s);
};

#endif
