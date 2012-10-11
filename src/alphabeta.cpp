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
#include "zkey.hpp"


#include "wait_any.hpp"

/*
   Alpha Beta search function. Uses OpenMP parallelization by the 
   'Young Brothers Wait' algorithm which searches the eldest brother (i.e. chess_move)
   serially to determine alpha-beta bounds and then searches the rest of the
   brothers in parallel.

   In order for this to be effective, chess_move ordering is crucial. In theory,
   if the best chess_move is ordered first, then it will produce a cutoff which leads
   to smaller search spaces which can be searched faster than standard minimax.
   However we don't know what a "good chess_move" is until we have searched it, which
   is what iterative deepening is for.

   The algorithm maintains two values, alpha and beta, which represent the minimum 
   score that the maximizing player is assured of and the maximum score that the minimizing 
   player is assured of, respectively. Initially alpha is negative infinity and beta is 
   positive infinity. As the recursion progresses the "window" becomes smaller. 
   When beta becomes less than alpha, it means that the current position cannot 
   be the result of best play by both players and hence need not be explored further.
 */
 
void *search_ab_pt(void *vptr)
{
    search_info *info = (search_info *)vptr;
    assert(info != 0);
    assert(info->self.valid());
    {
        assert(info->depth == info->board.depth);
        info->result = search_ab(info);
        info->set_done();
        mpi_task_array[0].add(1);
        smart_ptr<search_info> eg = info->self;
        info->self=0;
    }
    return NULL;
}

score_t search_ab(search_info *info)
{
    // Unmarshall the info struct
    node_t board = info->board;
    int depth = info->depth;
    score_t alpha = info->alpha;
    score_t beta = info->beta;
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
    if (board.ply && reps(board)) {
        DECL_SCORE(z,0,board.hash);
        return z;
    }

    // fifty chess_move draw rule
    if (board.fifty >= 100) {
        DECL_SCORE(z,0,board.hash);
        return z;
    }

    score_t max_val = bad_min_score;
    score_t zlo,zhi;
    if(get_transposition_value(board,zlo,zhi)) {
        if(zlo >= beta) {
            return zlo;
        }
        if(alpha >= zhi) {
            return zhi;
        }
        alpha = max(zlo,alpha);
        beta  = min(zhi,beta);
    }
    if(alpha >= beta) {
        return alpha;
    }

    std::vector<chess_move> workq;
    chess_move max_move;
    max_move = INVALID_MOVE; 

    gen(workq, board); // Generate the moves

#ifdef PV_ON
    sort_pv(workq, board.ply); // Part of iterative deepening
#endif

    const int worksq = workq.size();
    std::vector<smart_ptr<task> > tasks;
    pthread_cond_t shared_cond;
    pthread_mutex_t shared_mut;
    pthread_mutex_init(&shared_mut,0);
    pthread_cond_init(&shared_cond,0);

    int j=0;
    score_t val;

    /**
     * This loop will execute once and it will do it
     * sequentially. This should produce good cut-offs.
     * It is a 'Younger Brothers Wait' strategy.
     **/
    for(;j < worksq;j++) {
        if(alpha >= beta)
            continue;
        chess_move g = workq[j];
        node_t p_board = board;

        if (!makemove(p_board, g)) { // Make the chess_move, if it isn't 
            continue;                    // legal, then go to the next one
        }

        assert(depth >= 1);
        p_board.depth = depth-1;
        
        search_info* info = new search_info;
        info->board = p_board;
        info->alpha = -beta;
        info->beta = -alpha;
        info->depth = depth-1;
        if(depth == 1 && capture(board,g)) {
            val = -qeval(info);
        } else
            val = -search_ab(info);
        delete info;

        if (val > max_val)
        {
            max_val = val;
            max_move = g;
            if (val > alpha)
            {
                alpha = val;
#ifdef PV_ON
                pv[board.ply].set(g);
#endif
            }
        }
        j++;
        break;
    }

    // loop through the moves
    for (; j < worksq; j++) {
        chess_move g = workq[j];
        smart_ptr<search_info> info = new search_info(board);

        if (makemove(info->board, g)) {
            bool parallel;

            smart_ptr<task> t = parallel_task(depth, &parallel);

            t->info = info;
            t->info->board.depth = info->depth = depth-1;
            assert(depth >= 0);
            t->info->alpha = -beta;
            t->info->beta = -alpha;
            t->info->result = -beta;
            t->info->mv = g;
            
            t->info->coord.active=true;
            t->info->coord.mut=&shared_mut;
            t->info->coord.cond=&shared_cond;
            if(depth == 1 && capture(board,g))
                t->pfunc = qeval_f;
            else
                t->pfunc = search_ab_f;
            t->start();
            tasks.push_back(t);
            if (parallel && (j < worksq - 1))
                continue;
            if (!parallel)
                t->join(); // Serial task
        }
        int total_tasks=tasks.size();
        for(size_t n=0;n<total_tasks;n++) {
            //smart_ptr<search_info> info = tasks[n]->info;
            //tasks[n]->join();
            //val = -tasks[n]->info->result;
            smart_ptr<task> done_task=wait_any(tasks);
            smart_ptr<search_info> info=done_task->info;
            val=-info->result;
            done_task->info->coord.active=false;
            done_task->info=0;

            if (val == bad_max_score)
                continue;

            if (val > max_val) {
                max_val = val;
                max_move = info->mv;
                if (val > alpha)
                {
                    alpha = val;
#ifdef PV_ON
                    pv[board.ply].set(info->mv);
#endif
                    if(alpha >= beta) {
                        break;
                    }
                    
                }
            }
        }
        for (std::vector< smart_ptr<task> >::iterator task = tasks.begin(); task != tasks.end(); ++task)
        {
            pthread_mutex_lock(&(*task)->info->mut);
            (*task)->info->coord.active=false;
            pthread_mutex_unlock(&(*task)->info->mut);
            (*task)->info = 0;
        }
        tasks.clear();
        if(alpha >= beta) {
            break;
        }
    }
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
        ScopedLock s(mutex);
        move_to_make = max_move;
    }

    // fifty chess_move draw rule
    if (board.fifty >= 100) {
        DECL_SCORE(z,0,board.hash);
        return z;
    }

    set_transposition_value(board,
        max(zlo,max_val >= beta  ? max_val : bad_min_score),
        min(zhi,max_val < alpha ? max_val : bad_max_score));


    assert(max_val != bad_min_score);
    return max_val;
}
