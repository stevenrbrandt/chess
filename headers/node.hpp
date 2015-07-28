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
    hash_t hash;
    char color[64];
    char piece[64];
    int depth;
    int excess_depth = 0;
    int side;
    int castle;
    int ep;
    
    /* the number of moves since a capture or pawn chess_move, used to handle the fifty-chess_move-draw rule */
    int fifty;
    //counts number of moves
    int move_num;
    score_t p_board;
    bool follow_capt = false;
    bool root_side;
    score_t follow_score;
    int follow_depth = 2;
    int search_depth = 0;
    int ply;
    int hply;
};
struct node_t : public base_node_t {
    std::vector<hash_t> hist_dat;
};

#endif
