//////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2012 Steve Brandt and Phillip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

//#undef NDEBUG

#include "parallel_support.hpp"
#include "database.hpp"
#include "main.hpp"
#include "search.hpp"
#include <assert.h>
#include "parallel.hpp"
#include "zkey.hpp"
#include <atomic>

database dbase;

void search_ab_pt(boost::shared_ptr<search_info> info)
{
    info->result = search_ab(info);
    task_counter.add(1);
}

#define WHEN 1
struct When {
//#ifdef HPX_SUPPORT
#if 0
    typedef HPX_STD_TUPLE<int, hpx::lcos::future<score_t> > result_type;
    std::vector<hpx::lcos::future<score_t> > vec;
    When(std::vector<boost::shared_ptr<task> >& tasks) {
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
    When(std::vector<boost::shared_ptr<task> >& tasks) {}
    int any() { return 0; }
#endif
};

bool db_on = true;

// TODO: The verify routine should take the string
// representation of a board, not a const ref.
void verify(std::string strBoard,int side,const int depth,int castle,int ep,const score_t alpha0,const score_t beta0) {
  bool exact = false;
  bool db_save = db_on;
  db_on = false;
  score_t alpha = alpha0, beta = beta0;
  alpha = max(ADD_SCORE(alpha,-1),bad_min_score);
  beta  = min(ADD_SCORE(beta,1),bad_max_score);

  boost::shared_ptr<search_info> info{new search_info};
  node_t board(strBoard);
  board.castle = castle;
  board.ep = ep;
  board.side = side;
  info->board = board;
  //int n = board.hist_dat.size();
  //info->board.clear();
  //assert(n == board.hist_dat.size());
  info->alpha = alpha;
  info->beta = beta;
  info->depth = depth;
  info->board.depth = info->depth;
  db_on = false;
  score_t g = search_ab(info);
  db_on = true;
  if(info->draw)
    return;
  if (!(alpha < g && g < beta))
    std::cout<<"(g,a,b,depth)=" << g << "," << alpha << "," << beta << "," << depth << std::endl;
  assert(alpha < g && g < beta);
  db_on = db_save;
}

score_t search_ab(boost::shared_ptr<search_info> proc_info)
{
    if(proc_info->get_abort())
        return bad_min_score;
    // Unmarshall the info struct
    node_t board = proc_info->board;
    const int depth = proc_info->depth;
    if(depth != board.depth) {
      std::cout << "depth=" << depth << "board.depth=" << board.depth << std::endl;
    }
    assert(depth == board.depth);
    score_t alpha = proc_info->alpha;
    score_t beta = proc_info->beta;
    assert(depth >= 0);
    
    std::ostringstream strB;
    print_board(board, strB, true);
    std::string strBoard = strB.str();
    
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
        DECL_SCORE(z,0,board.hash);
        proc_info->draw = true;
        return z;
    }

    // fifty chess_move draw rule
    if (board.fifty >= 100) {
        DECL_SCORE(z,0,board.hash);
        proc_info->draw = true;
        return z;
    }


    score_t max_val = bad_min_score;
    score_t p_board = board.p_board;
    score_t zlo = bad_min_score,zhi = bad_max_score;
    bool white =board.side == LIGHT;
    bool entry_found = false;
    int excess =0;
    bool exact = false;
    if (white && board.root_side == LIGHT && db_on && board.ply > 0 && !proc_info->quiescent){
      entry_found = dbase.get_transposition_value (board, zlo, zhi, white,p_board,excess,exact,board.depth);
      int pe = proc_info->excess;
      if (excess > proc_info->excess){
        proc_info->excess = excess;
        //if (!board.follow_capt && search_method == MTDF)
        board.follow_capt = true;
      }
      else{
        //board.follow_depth = 0;
      }
      if(entry_found && excess > 0) {
        assert(depth == board.depth);
        std::cout << "excess = " << (excess+depth) << std::endl;
        zhi = bad_max_score;
        verify(strBoard,board.side,depth+excess,board.castle,board.ep,zlo,bad_max_score);
      }
    }
      
    if (entry_found){
      return zlo;
    }
   
    if(!entry_found) {
      entry_found = get_transposition_value (board, zlo, zhi);

      if(!entry_found && db_on && board.side == LIGHT){
        entry_found = dbase.get_transposition_value(board,zlo,zhi,white,p_board,excess,true,depth);
        verify(strBoard,board.side,depth,board.castle,board.ep,zlo,zhi);
      }
    }

    if (entry_found) {
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
        //proc_info->stop=false;
        //deeper= false;
        return alpha;
    }

    const score_t alpha0 = alpha;

    std::vector<chess_move> workq;
    std::vector<chess_move> max_move;
    

    gen(workq, board); // Generate the moves

#ifdef PV_ON
    if(!proc_info->use_srand)
      proc_info->incr = rand();
    proc_info->use_srand = false;
    srand(proc_info->incr);
    sort_pv(workq, board.depth); // Part of iterative deepening
#endif

    const int worksq = workq.size();
    std::vector<boost::shared_ptr<task> > tasks;

    int j=0;
    score_t val;
    bool aborted = false;
    bool children_aborted = false;
    // loop through the moves
    //for (; j < worksq; j++) 
    while(j < worksq) {
        while(j < worksq) {
            chess_move g = workq[j++];

            boost::shared_ptr<search_info> child_info{new search_info(board)};
            bool parallel;
            if (!aborted && !proc_info->get_abort() && makemove(child_info->board, g)) {
            
                parallel = j > 0 && !capture(board,g);
                boost::shared_ptr<task> t = parallel_task(depth, &parallel);

                t->info = child_info;
                int d = depth - 1;
                if(!test_alphabeta && d == 0 && capture(board,g)) {
                  d = 1;
                  /*
                  if(!proc_info->quiescent) {
                    t->info->alpha = bad_min_score;
                    t->info->beta = bad_max_score;
                  }
                  */
                  t->info->quiescent = true;
                } else if(proc_info->quiescent) {
                  t->info->quiescent = true;
                }
                t->info->board.depth = child_info->depth = d;
                assert(depth >= 0);
                t->info->alpha = -beta;
                t->info->beta = -alpha;
                t->info->mv = g;
                t->pfunc = search_ab_f;
                t->start();
                tasks.push_back(t);

                // Control branching
                if (!parallel)
                    break;
                /*
                else if (beta >= max_score*.9)
                    continue;
                    */
                else if (tasks.size() < 5)
                    continue;
                else
                    break;
            }
        }

        When when(tasks);
        size_t const count = tasks.size();
        for(size_t n_=0;n_<count;n_++) {
            int n = when.any();
            boost::shared_ptr<task> child_task = tasks[n];
            //assert(child_task.valid());
            child_task->join();
            
            boost::shared_ptr<search_info> child_info = child_task->info;
            
            tasks.erase(tasks.begin()+n);

            if(!children_aborted && (aborted || child_info->get_abort())) {
                for(unsigned int m = 0;m < tasks.size();m++) {
                     tasks[m]->info->set_abort(true);
                }
                children_aborted = true;
            }

            //child_task->join();
            if(child_info->get_abort()) 
                continue;
            
            if(child_info->draw)
              proc_info->draw = true;

            val = -child_info->result;
            proc_info->log << " " << child_info->mv << "=" << val;

            bool found = false; 
            if (child_info->excess > proc_info->excess){
              proc_info->excess = child_info->excess;
              found = true;
              if (!board.follow_capt){
                board.follow_capt = true;
              }
            }
            
            if (val > max_val || found ) {
                max_move.clear();
                max_move.push_back(child_info->mv); 
                if (val > max_val)
                  max_val = val;
                if (val > alpha)
                {
                    alpha = val;
#ifdef PV_ON
                    if(!child_info->get_abort())
                        ;//pv[board.search_depth - 1].set(child_info->mv);
#endif
                    if(alpha >= beta) {
                      //aborted = true;
                      j += worksq;
                      continue;
                    }
                }
            } else if(val == max_val && proc_info->excess == 0) {
              max_move.push_back(child_info->mv);
            }
        }
        if(alpha >= beta) {
            j += worksq;
            break;
        }
    
    }
    // no legal moves? then we're in checkmate or stalemate
    if (max_move.size()==0) {
        if (in_check(board, board.side))
        {
            DECL_SCORE(s,max_score,board.hash);
            return s;
       }
        else
        {
            DECL_SCORE(z,0,board.hash);
            return z;
        }
    }
    if(db_on) {
      if (board.ply == 0 || board.depth>0) {
        assert(max_move.size() != 0);
        ScopedLock s(cmutex);
        move_to_make = max_move.at(rand() % max_move.size());
      }
    }
    
    bool store = true;
    if(proc_info->draw) {
      store = false;
    }
    if(proc_info->quiescent)
      store = false;

    score_t lo, hi;
    if(proc_info->excess) {
      lo = max_val;
      hi = max_score;
      //std::cout<<"Max depth: "<<proc_info->excess+depth<<std::endl;
      store = false;
    } else if(alpha0 < max_val && max_val < beta) {
      lo = max_val-1;
      hi = max_val;
    } else if(max_val <= alpha0) {
      hi = max_val;
      lo = zlo;
      if(lo == zlo)
        store = false;
    } else if (max_val >= beta){
      lo = max_val;
      hi = zhi;
      if(hi == zhi)
        store = false;
    } else {
      store = false;
      lo = hi = 0;
    }
    if(store && lo > hi) {
      std::cout << "lo=" << lo << " hi=" << hi << std::endl;
      abort();
    }

    if(store && db_on) {
      verify(strBoard,board.side,depth,board.castle,board.ep,lo,hi);
    }
    if(store) {
      //if(board.depth > 1)
        dbase.add_data(board,lo,hi,white,proc_info->excess);
      set_transposition_value(board,lo,hi);
    }
    return max_val;
}
