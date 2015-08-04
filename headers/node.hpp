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
    int move_num=0;
    score_t p_board;
    bool follow_capt = false;
    bool root_side=-1;
    score_t follow_score;
    int follow_depth = 2;
    int search_depth = 0;
    int ply=0;
};
struct node_t : public base_node_t {
    std::vector<hash_t> hist_dat;
    void clear() {
      hist_dat.clear();
      fifty = 0;
      follow_capt = false;
      ply = 0;
      move_num = 0;
    }
};

#endif
