#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>
#include <string.h>
#include "node.hpp"
#include "defs.hpp"
#include "data.hpp"
#include "main.hpp"
#include "board.hpp"
#include "eval.hpp"

int think(std::vector<gen_t>& workq, node_t& board);
int search(node_t board, int depth);
int reps(node_t& board);
#endif
