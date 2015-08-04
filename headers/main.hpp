///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Phillip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
#ifndef MAIN_H
#define MAIN_H
#include <iostream>
#include <string>

#include <sys/timeb.h>
#include "node.hpp"
#include "defs.hpp"
#include "data.hpp"
#include "search.hpp"
#include "chess_move.hpp"

int main(int argc, char *argv[]);
int parse_move(std::vector<chess_move>& workq, const char *s, const node_t& board);
char *move_str(chess_move& m);
//void print_board(const node_t& board, std::ostream& out,bool trimmed=false);
int print_result(std::vector<chess_move>& workq, node_t& board);
void start_benchmark(std::string filename, int ply_level, int num_runs,bool parallel,node_t& n);
int get_ms();
std::string get_log_name();
int chx_main();
#ifdef HPX_SUPPORT
extern database dbase;
extern std::ofstream **streams;
extern bool file_output_enabled;
#endif

#endif
