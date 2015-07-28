////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2012 Steve Brandt and Phillip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#include "parallel_support.hpp"
#include "search.hpp"
#include <assert.h>
#include "parallel.hpp"

void search_pt(boost::shared_ptr<search_info> info) {
    info->result = search(info);
    task_counter.add(1);
}

score_t search(boost::shared_ptr<search_info> info)
{
  node_t board = info->board;
  int depth = info->depth;
  assert(depth >= 0);
  // if we are a leaf node, return the value from the eval() function
  if (depth == 0)
  {
    evaluator ev;
    DECL_SCORE(s,ev.eval(board, chosen_evaluator),board.hash);
    return s;
  }
  /* if this isn't the root of the search tree (where we have
     to pick a chess_move and can't simply return 0) then check to
     see if the position is a repeat. if so, we can assume that
     this line is a draw and return 0. */
  if (board.ply && reps(board)==3) {
    DECL_SCORE(s,0,board.hash);
    return s;
  }

  // fifty chess_move draw rule
  if (board.fifty >= 100) {
    DECL_SCORE(z,0,board.hash);
    return z;
  }

  score_t val, max;

  std::vector<chess_move> workq;
  chess_move max_move;
  max_move = INVALID_MOVE;

  gen(workq, board); // Generate the moves

  // DECL_SCORE(minf,-10000,board.hash);
  max = bad_min_score; // Set the max score to -infinity

  // const int worksq = workq.size();
  std::vector<boost::shared_ptr<task> > tasks;

  // loop through the moves
  // We do this twice. The first time we skip
  // quiescent searches, the second time we
  // do the quiescent search. By doing this
  // we get the best value of beta to produce
  // cutoffs within the quiescent search routine.
  // Without doing this, minimax runs extremely
  // slowly.
  for(size_t j=0;j < workq.size(); j++) {
    bool last = (j+1)==workq.size();
    chess_move g = workq[j];
    boost::shared_ptr<search_info> info{new search_info(board)};

    if (makemove(info->board, g)) {  
      DECL_SCORE(z,0,board.hash);
      info->depth = depth-1;
      info->mv = g;
      info->result = z;
      bool parallel=true;
      bool skip = true;
      boost::shared_ptr<task> t = 
        parallel_task(depth, &parallel);
      t->info = info;
      t->pfunc = search_f;
      tasks.push_back(t);
      t->start();
      skip = false;
    }
  }
  for(size_t n=0;n<tasks.size();n++) {
    boost::shared_ptr<task> taskn = tasks[n];
    boost::shared_ptr<search_info> info = taskn->info;
    taskn->join();
    val = -taskn->info->result;

    if (val > max)
    {
      max = val;
      max_move = info->mv;
    }
  }
  tasks.clear();
  assert(tasks.size()==0);

  // no legal moves? then we're in checkmate or stalemate
  if (max_move == INVALID_MOVE) {
    if (in_check(board, board.side))
    {
      DECL_SCORE(s,-10000 + board.ply,board.hash);
      return s;
    }
    else
    {
      DECL_SCORE(z,0,board.hash);
      return z;
    }
  }

  if (board.ply == 0) {
    assert(max_move != INVALID_MOVE);
    ScopedLock s(cmutex);
    move_to_make = max_move;
  }

  return max;
}
