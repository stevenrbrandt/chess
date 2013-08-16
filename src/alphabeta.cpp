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
#include <atomic>

#undef NDEBUG

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
    dtor<search_info> hold = info->self;
    assert(info->self.valid());
    assert(info->depth == info->board.depth);
    info->result = search_ab(info);
    info->set_done();
    mpi_task_array[0].add(1);
    return NULL;
}

//#define WHEN 1
struct When {
#ifdef HPX_SUPPORT
    typedef HPX_STD_TUPLE<int, hpx::lcos::future<score_t> > result_type;
    std::vector<hpx::lcos::future<score_t> > vec;
    When(std::vector<smart_ptr<task> >& tasks) {
        for(unsigned int i=0;i<tasks.size();i++) {
            hpx_task *hpx = dynamic_cast<hpx_task*>(tasks[i].ptr());
            if(hpx != NULL) {
                #ifdef WHEN
                vec.push_back(hpx->result);
                #endif
            }
        }
    }
    int any() {
        if(vec.size()==0) {
            return 0;
        }
        result_type r = hpx::wait_any(vec);
        int i = HPX_STD_GET(0,r);
        vec.erase(vec.begin()+i);
        return i;
    }
#else
    When(std::vector<smart_ptr<task> >& tasks) {}
    int any() { return 0; }
#endif
};

score_t search_ab(search_info *proc_info)
{
    if(proc_info->abort_flag)
        return bad_min_score;
    // Unmarshall the info struct
    node_t board = proc_info->board;
    int depth = proc_info->depth;
    score_t alpha = proc_info->alpha;
    score_t beta = proc_info->beta;
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
        
        search_info* child_info = new search_info;
        child_info->board = p_board;
        child_info->alpha = -beta;
        child_info->beta = -alpha;
        child_info->depth = depth-1;
        if(depth == 1 && capture(board,g)) {
            val = -qeval(child_info);
        } else
            val = -search_ab(child_info);
        delete child_info;

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

    bool aborted = false;
    bool children_aborted = false;
    // loop through the moves
    //for (; j < worksq; j++) {
    while(j < worksq) {
        while(j < worksq) {
            chess_move g = workq[j++];

            smart_ptr<search_info> child_info = new search_info(board);

            bool parallel;
            if (!aborted && !proc_info->abort_flag && makemove(child_info->board, g)) {
                child_info.inc();

                parallel = true;
                smart_ptr<task> t = parallel_task(depth, &parallel);

                t->info = child_info;
                t->info->board.depth = child_info->depth = depth-1;
                assert(depth >= 0);
                t->info->alpha = -beta;
                t->info->beta = -alpha;
                t->info->result = -beta;
                t->info->mv = g;
                if(depth == 1 && capture(board,g))
                    t->pfunc = qeval_f;
                else
                    t->pfunc = search_ab_f;
                t->start();
                tasks.push_back(t);

                // Control branching
                if (parallel && beta >= max_score*.9)
                    continue;
                else if (parallel && tasks.size() < 3)
                    continue;
                else
                    break;
            } else {
                child_info.dec();
                assert(!child_info.valid());
            }
        }
        When when(tasks);
        size_t const count = tasks.size();
        assert(count > 0);
        for(size_t n_=0;n_<count;n_++) {
            int n = when.any();
            smart_ptr<task> child_task = tasks[n];
            assert(child_task.valid());
            child_task->join();
            smart_ptr<search_info> child_info = child_task->info;

            dtor<task> d_child_task(child_task);
            dtor<search_info> d_child_info(child_info);
            tasks.erase(tasks.begin()+n);

            if(!children_aborted && (aborted || child_info->abort_flag)) {
                for(unsigned int m = 0;m < tasks.size();m++) {
                    tasks[m]->info->abort_flag = true;
                }
                children_aborted = true;
            }

            child_task->join();
            if(child_info->abort_flag) 
                continue;
            val = -child_info->result;

            if (val > max_val) {
                max_val = val;
                max_move = child_info->mv;
                if (val > alpha)
                {
                    alpha = val;
#ifdef PV_ON
                    if(!child_info->abort_flag)
                        pv[board.ply].set(child_info->mv);
#endif
                    if(alpha >= beta) {
                        aborted = true;
                        continue;
                    }
                }
            }
        }
        if(alpha >= beta) {
            break;
        }
    }

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


    return max_val;
}
