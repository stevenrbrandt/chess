#ifndef NODE_H
#define NODE_H
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include "defs.hpp"
#include "hash.hpp"
#include "FixedVec.hpp"

struct base_node_t { 
    hash_t hash;
    char color[64];
    char piece[64];
    int depth;
    int side;
    int castle;
    int ep;
    
    /* the number of moves since a capture or pawn move, used to handle the fifty-move-draw rule */
    int fifty;
    
    int ply;
    int hply;
    //std::vector<hash_t> hist_dat;
};
struct node_t : public base_node_t {
    FixedVec<hash_t,100> hist_dat;
};

#endif
