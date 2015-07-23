///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Philip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
/*
 *  search.cpp
 */
#ifdef HPX_SUPPORT
#include <hpx/hpx_fwd.hpp>
#endif

#include "parallel_support.hpp"
#include "search.hpp"
#include "parallel.hpp"
#include <algorithm>
#include <math.h>
#include <assert.h>
#include "here.hpp"
#include "zkey.hpp"
#include "log_board.hpp"
#include <fstream>
#include <sstream>

Mutex cmutex;
const int num_proc = chx_threads_per_proc();

int min(int a,int b) { return a < b ? a : b; }
int max(int a,int b) { return a > b ? a : b; }

std::vector<safe_move> pv;  // Principle Variation, used in iterative deepening

/**
 * This determines whether we have a capture. It's not sophisticated
 * enough to do en passant.
 */
bool capture(const node_t& board,chess_move& g) {
  //bool b1 = board.color[g.getTo()] != EMPTY
      //&& board.color[g.getTo()] != board.color[g.getFrom()];
  bool b2 = g.getCapture();
  //if(b1 != b2) abort();
  return b2;
}

/**
 * Quiescent evaluator. Originally we wished to avoid the complexity
 * of quiescent chess_move searches, but MTD-f does not seem to work properly
 * without it. This particular search evaluates each chess_move using the
 * evalutaor, unless the chess_move is a capture then it calls itself
 * recursively. The quiescent chess_move search uses alpha-beta to speed
 * itself along, and evaluates non-captures first to get the greatest
 * cutoff.
 **/
score_t qeval(boost::shared_ptr<search_info> info)
{
#ifdef HPX_SUPPORT
    if(file_output_enabled) {
        int n = hpx::get_worker_thread_num();
        if(streams[n] == NULL) {
            std::stringstream outname;
            streams[n] = new std::ofstream();
            outname << "chess_out" << n << ".txt";
            streams[n]->open(outname.str());
            std::cout << "file: " << outname.str() << std::endl;
        }
        log_board(info->board,*streams[n]);
    }
#endif
    node_t board = info->board;
    score_t lower = info->alpha;
    score_t upper = info->beta;
    evaluator ev;
    DECL_SCORE(s,ev.eval(board, chosen_evaluator),board.hash);
    return s;
    s = max(lower,s);
    std::vector<chess_move> workq;
    gen(workq, board); // Generate the moves
    for(size_t j=0;j < workq.size(); j++) {
        chess_move g = workq[j];
        if(g.getCapture())
            continue;
        if(info->get_abort())
            return s;
        node_t p_board = board;
        if(!makemove(p_board,g))
            continue;
        DECL_SCORE(v,ev.eval(p_board,chosen_evaluator),p_board.hash);
        s = max(v,s);
        if(s > upper) {
            return s;
        }
    }
    for(size_t j=0;j < workq.size(); j++) {
        if(!workq[j].getCapture())
            continue;
        if(info->get_abort())
            return s;
        chess_move g = workq[j];
        node_t p_board = board;
        if(!makemove(p_board,g))
            continue;
        boost::shared_ptr<search_info> new_info{new search_info};
        new_info->set_abort_ref(info.get());
        new_info->board = p_board;
        new_info->alpha = -upper;
        new_info->beta = -lower;
        s = max(-qeval(new_info),s);
        if(s > upper) {
            return s;
        }
    }
    return s;
}

void qeval_pt(boost::shared_ptr<search_info> info)
{
  info->result = qeval(info);
  task_counter.add(1);
}

boost::shared_ptr<task> parallel_task(int depth, bool *parallel) {

    if(!*parallel) {
        boost::shared_ptr<task> t{new serial_task};
        return t;
    }
    bool use_parallel = false;
    use_parallel = depth >= 3;
    if(use_parallel) {
        int n = task_counter.dec();
        if(n > 0) {
#ifdef HPX_SUPPORT
            boost::shared_ptr<task> t{new hpx_task};
#else
            boost::shared_ptr<task> t{new thread_task};
#endif
            *parallel = (n > 1);
            return t;
        }
    }
    *parallel = false;
    boost::shared_ptr<task> t{new serial_task};
    return t;
}

score_t target_score;

// think() calls a search function 
int think(node_t& board,bool parallel)
{
  evaluator ev;
  DECL_SCORE(curr, ev.eval(board, chosen_evaluator),board.hash);
  score_t score_plus = ADD_SCORE(curr,1);
  board.root_side = board.side;// == LIGHT;
  
  if (board.root_side == LIGHT)
    search_method = light_search_method;
  else
    search_method = dark_search_method;

  boost::shared_ptr<task> root{new serial_task};
#ifdef PV_ON
  pv.clear();
  pv.resize(depth[board.side]);
  chess_move mvz;
  mvz = INVALID_MOVE;
  for(size_t i=0;i<pv.size();i++) {
    pv[i].set(mvz);
  }
#endif
  for(int i=0;i<table_size;i++) {
    transposition_table[i].depth = -1;
    transposition_table[i].lower = bad_min_score;
    transposition_table[i].upper = bad_max_score;
  }
  board.ply = 0;

  if (search_method == MINIMAX) {
    root->pfunc = search_f;
    
    boost::shared_ptr<search_info> info{new search_info};
    info->board = board;
    info->depth = depth[board.side];
    score_t f = search(info);

    assert(move_to_make != INVALID_MOVE);
    if (bench_mode)
      std::cout << "SCORE=" << f << std::endl;
  } else if (search_method == MTDF) {
    if (board.move_num == 0){
      board.p_board = score_plus;
      target_score = score_plus;
    }
    root->pfunc = search_ab_f;
    DECL_SCORE(alpha,-10000,board.hash);
    DECL_SCORE(beta,10000,board.hash);
    int stepsize = 2;
    int d = depth[board.side] % stepsize;
    if(d == 0)
        d = stepsize;
    board.search_depth = depth[board.side];
    board.depth = d;
    boost::shared_ptr<search_info> info{new search_info};
    info->board = board;
    info->depth = d;
    info->alpha = alpha;
    info->beta = beta;
    score_t f(search_ab(info));

    score_t moving = target_score;

    while(d < depth[board.side]) {
        d+=stepsize;
        int excess = 0;
        board.depth = d;
        std::cout << "iter(" << d << ")" << std::endl;
        f = mtdf(board,f,d,target_score,moving);
        if (f > moving){
          moving = f;
        }
        std::cout<< "  d="<< d << " target=" << target_score << " moving="<< moving << " f="<< f<< std::endl;
        if(board.follow_capt && board.follow_depth <= board.search_depth) {
          board.follow_capt = false;
        }
        boost::shared_ptr<task> new_root{new serial_task};
        root = new_root;
    }
    target_score = ADD_SCORE(f,1);
    if (bench_mode)
      std::cout << "SCORE=" << f << std::endl;
  } else if (search_method == ALPHABETA) {
    if (!board.follow_capt){
      board.p_board = score_plus;
    }
    else{
      board.follow_depth-=1;
      if (board.follow_depth == 0)
        board.follow_capt = false;
    }
    root->pfunc = search_ab_f;
    // Initially alpha is -infinity, beta is infinity
    DECL_SCORE(f,0,0);
    DECL_SCORE(alpha,-10000,board.hash);
    DECL_SCORE(beta,10000,board.hash);
    bool brk = false;  /* Indicates whether we broke away from iterative deepening 
                          and need to call search on the actual ply */ 
    board.search_depth = depth[board.side];
    int low = 2;
    if(depth[board.side] % 2 == 1)
        low = 1;
    int excess = 0;
    boost::shared_ptr<search_info> best_info;
    chess_move best_move;
    for (int i = low; i <= depth[board.side]; i++) // Iterative deepening
    {
      excess = 0; 
      std::cout << "iter(" << i << ")" << std::endl;
      board.depth = i;
      boost::shared_ptr<search_info> info{new search_info};
      info->board = board;
      info->depth = i;
      info->alpha = alpha;
      info->beta = beta;
      info->excess = excess;
      //bool stop = info-> stop; 
      f = search_ab(info);
      if (info->excess > 0) {
        excess = info->excess;
        int reach = excess + info->depth;
        board.follow_capt = true;
        std::cout << "Excess: " << excess << " Reach: " << reach << std::endl;
        if(best_info == nullptr || reach > best_info->excess + best_info->depth) {
          best_info = info;
          best_move = move_to_make;
          std::cout << "  set best info " << f << std::endl;
        }
      }
      if (i > depth[board.side])  // if our ply is greater than the iter_depth, then break
      {
        brk = true;
        break;
      }
    }

    // Get the best stuff
    if(best_info != nullptr) {
      move_to_make = best_move;
      board = best_info->board;
      excess =  best_info->excess;
      std::cout << "best: " << (excess+board.depth) << std::endl;
    }
    std::cout << "p_board = " << board.p_board << std::endl;

    if(board.follow_capt && board.follow_depth <= board.search_depth) {
      board.follow_capt = false;
    }
    if ((f >= board.p_board && (excess+board.depth)>board.follow_depth) || !board.follow_capt) {// || score_plus>=board.p_board){
      if (score_plus>=board.p_board)
        std::cout<<"Greater score"<<std::endl;
      if (board.side == LIGHT){
        board.p_board = f;
        board.follow_capt = true;
        board.follow_depth = board.depth + excess;
      }
      if (board.side == DARK && board.follow_capt){
        board.follow_capt = false;
        board.follow_depth = 2;
      }
    }
    std::cout<<"Follow "<<board.follow_depth<<" Follow capt "<<board.follow_capt<< " Follow score: " << board.p_board << std::endl;
    
    /*
    if (brk) {
      boost::shared_ptr<search_info> info{new search_info};
      info->board = board;
      info->depth = depth[board.side];
      info->alpha = alpha;
      info->beta = beta;
      f=search_ab(info);
    }
    */
    if (bench_mode)
      std::cout << "SCORE=" << f << std::endl;
  }
  return 1;
}

/** MTD-f */
score_t mtdf(node_t& board,score_t f,int depth,const score_t target,score_t moving){
    score_t g = f;
    evaluator ev;
    DECL_SCORE(curr, ev.eval(board, chosen_evaluator),board.hash);
    score_t score_plus = ADD_SCORE(curr,1);
    DECL_SCORE(upper,10000,board.hash);
    DECL_SCORE(lower,-10000,board.hash);
    // Technically, MTD-f uses "zero-width" searches
    // However, Aske Plaat points out that it performs
    // better with a coarser evaluation function. Since
    // this maps readily onto a wider, non-zero width
    // we provide a width setting for optimization.
    const int start_width = 0;//atoi(getenv("START_WIDTH"));
    // Sometimes MTD-f gets stuck and can try many
    // times without finding an answer. If this happens
    // we want to set a threshold for bailing out.
    const int max_tries = 4;//atoi(getenv("MAX_TRIES"));
    // If our first guess isn't right, chances are
    // we want to search a little wider the next try
    // to improve our odds.
    const int grow_width = 1;//atoi(getenv("GROW_WIDTH"));
    int width = start_width;
    const int max_width = start_width+grow_width*max_tries;
    score_t alpha = ADD_SCORE(target,-width), beta = ADD_SCORE(moving,width+1); 
    boost::shared_ptr<search_info> best_info;
    chess_move best_move;
    int excess = 0;
    score_t tmp = board.p_board;
    while(lower < upper) {
        if(width >= max_width) {
            alpha = lower;
            beta = upper;
        } else {
            alpha = max(g == lower ? ADD_SCORE(lower,1) : lower, ADD_SCORE(g,    -(1+width/2)));
            beta  = min(g == upper ? ADD_SCORE(upper,-1) : upper, ADD_SCORE(alpha, (1+width)));
        }
        boost::shared_ptr<search_info> info{new search_info};
        info->board = board;
        info->depth = depth;
        info->alpha = alpha;
        info->beta = beta;
        g = search_ab(info);
        std::cout << " g=" << g << " target="<< target <<std::endl;
        if (g > target){
          target_score = g;
          std::cout<<"!!!!!! Cutoff, found better score: "<<g<<std::endl;
          return g;
        }else if (g == target){
          std::cout << "SCORE=" << f << std::endl;
          std::cout<<"Cutoff, found score: "<<g<<" p_board: "<<board.p_board<<" tmp: "<<tmp<<std::endl;
          return g;
        }
        int excess = 0;
        if (info->excess > 0) {
          excess = info->excess;
          int reach = excess + info->depth;
          board.follow_capt = true;
          std::cout << "Excess: " << excess << " Reach: " << reach << std::endl;
          if(best_info == nullptr || reach > best_info->excess + best_info->depth) {
            best_info = info;
            best_move = move_to_make;
            std::cout << "  set best info " << f << std::endl;
          }
        }   
        if(g < beta) {
          if(g > alpha){
            break;
          }
          upper = g;
        } else {
          lower = g;
        }
        width += grow_width;
    }

    if(best_info != nullptr) {
      move_to_make = best_move;
      board = best_info->board;
      int excess =  best_info->excess;
      std::cout << "best: " << (excess+board.depth) << std::endl;
    }
            
    std::cout << "p_board = " << board.p_board << std::endl;
    if ((g >= board.p_board && (excess+board.depth)>board.follow_depth) || !board.follow_capt) {// || score_plus>=board.p_board)
      if (score_plus>=board.p_board)
        std::cout<<"Greater score"<<std::endl;
      if (board.side == LIGHT){
        board.p_board = g;
        board.follow_capt = true;
        board.follow_depth = board.depth + excess;
      }
      if (board.side == DARK && board.follow_capt){
        board.follow_capt = false;
        board.follow_depth = 2;
      }
    }
    return g;
}



/* reps() return the number of times the current position
   has been repeated. It compares the current value of hash
   to previous values. */

int reps(const node_t& board)
{
  int i;
  int r = 0;

  // TODO: Warning! Why are we checking hist_dat.size() here?
  for (i = 0; i < board.fifty && i < board.hist_dat.size(); ++i) {
    assert(i < board.hist_dat.size());
    assert(board.hash != 0);
    if (board.hist_dat[i] == board.hash)
      ++r;
  }
  return r;
}


template<typename T>
void mqswap(std::vector<T>& vec,int i1,int i2) {
  if(i1 == i2) return;
  T tmp = vec.at(i1);
  vec.at(i1) = vec.at(i2);
  vec.at(i2) = tmp;
}

void sort_pv(std::vector<chess_move>& workq, int index)
{
  int i1=0, i2=workq.size()-1;
  chess_move mv1,mv2;
  // put the last 2 pv searches first
  if(pv.size()>0) {
    mv1 = pv[pv.size()-1].mv;
    for(int i=0;i<workq.size();i++) {
      if(mv1 == workq[i]) {
        mqswap(workq,i,i1);
        i1++;
        break;
      }
    }
  }
  if(pv.size()>1) {
    mv2 = pv[pv.size()-1].mv;
    if(mv2 != mv1) {
      for(int i=i1;i<workq.size();i++) {
        if(mv2 == workq[i]) {
          mqswap(workq,i,i1);
          i1++;
          break;
        }
      }
    }
  }
  // put captures last
  while(i1 < i2) {
    if(!workq[i1].getCapture())
      i1++;
    else if(workq[i2].getCapture())
      i2--;
    else
      mqswap(workq,i1,i2);
  }
}

#define TRANSPOSE_ON 1

inline int iabs(int n) {
  if(n < 0)
    return -n;
  else
    return n;
}

zkey_t transposition_table[table_size];

bool get_transposition_value(const node_t& board,score_t& lower,score_t& upper) {
    bool gotten = false;
#ifdef TRANSPOSE_ON
    int n = iabs((board.hash << 4) ^ board.depth) % table_size;
    zkey_t *z = &transposition_table[n];
    ScopedLock s(z->mut);
    if(z->depth >= 0 && board_equals(board,z->board)) {
        lower = z->lower;
        upper = z->upper;
        gotten = true;
    } else {
        lower = bad_min_score;
        upper = bad_max_score;
    }
#endif
    return gotten;
}

void set_transposition_value(const node_t& board,score_t lower,score_t upper) {
#ifdef TRANSPOSE_ON
    int n = iabs((board.hash << 4) ^ board.depth) % table_size;
    zkey_t *z = &transposition_table[n];
    ScopedLock s(z->mut);
    if(board.depth == 1 || board.depth >= z->depth) {
        z->board = board;
        z->lower = lower;
        z->upper = upper;
        z->depth = board.depth;
    }
#endif
}
