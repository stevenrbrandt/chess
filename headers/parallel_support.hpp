////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Steve Brandt and Philip LeBlanc
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file BOOST_LICENSE_1_0.rst or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////
#ifndef PARALLEL_SUPPORT_HPP
#define PARALLEL_SUPPORT_HPP

#include <sys/time.h>
#include "node.hpp"
#include "score.hpp"
#include "chess_move.hpp"
#include <boost/atomic.hpp>
#include "parallel.hpp"
#include <boost/shared_ptr.hpp>
#include <future>
#include <sstream>

extern bool par_enabled;
int chx_threads_per_proc();

struct task;

struct search_info {
private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
      // TODO: Write a real serializer
      abort();
    }
public:
    std::ostringstream log;
    boost::atomic<bool>  abort_flag_;
    boost::atomic<bool> *abort_flag;
    bool get_abort() { return *abort_flag; }
    void set_abort(bool b) { *abort_flag = b; }
    void set_abort_ref(search_info *s) {
        abort_flag = s->abort_flag;
    }
    bool use_srand = false;
    node_t board;
    bool par_done;
    chess_move mv;
    score_t result;
    int depth;
    int incr=0;
    score_t alpha;
    score_t beta;
    score_t printed_board=0;
    int excess;
    bool quiescent;
    bool draw;
    search_info(const node_t& board_) : abort_flag_(false), abort_flag(&abort_flag_), board(board_), par_done(false), 
            result(bad_min_score),excess(0),quiescent(false), draw(false) {
            }

    search_info() : abort_flag_(false), abort_flag(&abort_flag_), par_done(false), result(bad_min_score), excess(0),quiescent(false),draw(false) {
    }

    ~search_info() {
    }

#ifdef HPX_SUPPORT
    friend hpx::serialization::access;
#endif
};

void print_board(const node_t& board, std::ostream& out,bool trimmed=false);

#define INFO(X) o << " " << #X << "=" << info->X;
inline
std::ostream& operator<<(std::ostream& o,boost::shared_ptr<search_info> info) {
  print_board(info->board,o,true);
  print_board(info->board,o,false);
  INFO(board.hash);
  INFO(board.excess_depth);
  INFO(board.side);
  INFO(board.castle);
  INFO(board.ep);
  INFO(board.move_num);
  INFO(board.follow_capt);
  INFO(board.root_side);
  INFO(board.follow_score);
  INFO(board.follow_depth);
  INFO(board.search_depth);
  INFO(board.ply);
  INFO(board.p_board);
  INFO(board.fifty);
  INFO(board.hist_dat.size());
  node_t& board = info->board;
  o << " hist={";
  for(int i=0;i<board.hist_dat.size();i++) {
    if(i > 0) o << ",";
    o << board.hist_dat.at(i);
  }
  o << "}" << std::endl;
  INFO(mv.str());
  INFO(par_done);
  INFO(result);
  INFO(depth);
  INFO(alpha);
  INFO(beta);
  INFO(printed_board);
  INFO(excess);
  INFO(quiescent);
  INFO(draw);
  INFO(incr);
  INFO(abort_flag);
  INFO(abort_flag_);
  INFO(log.str());
  return o;
}

void search_pt(boost::shared_ptr<search_info>);
void search_ab_pt(boost::shared_ptr<search_info>);
void qeval_pt(boost::shared_ptr<search_info>);

score_t search(boost::shared_ptr<search_info>);
score_t search_ab(boost::shared_ptr<search_info>);
score_t qeval(boost::shared_ptr<search_info>);

enum pfunc_v { no_f, search_f, search_ab_f, qeval_f };

struct task {
    boost::shared_ptr<search_info> info;

    pfunc_v pfunc;
    task() : pfunc(no_f) {
    }
    virtual ~task() {
        info = 0;
    }

    virtual void start() = 0;

    virtual void join() = 0;
};

struct serial_task : public task {
    bool joined;
    serial_task(): joined(false) {}
    ~serial_task() {
        info = 0;
    }

    virtual void start() { }

    virtual void join() {
        if (joined)
            return;
        if(info.get() == nullptr)
            return;
        if(pfunc == search_f)
            info->result = search(info);
        else if(pfunc == search_ab_f)
            info->result = search_ab(info);
        else if(pfunc == qeval_f)
            info->result = qeval(info);
        else
            abort();
        joined = true;
    }
};
class pcounter {
    boost::atomic<int> count;
    int max_count;
public:
    pcounter() : count(0), max_count(1) {
    }
    pcounter(const pcounter& pc) : count(pc.count.load()), max_count(pc.max_count) {
    }
    void set_max(int n) {
        count = max_count = n;
    }
    int get() { return max_count; }
    int add(int n) {
        count += n;
        assert(count <= max_count);
        int m = count;
        return m;
    }
    int dec() {
        int n = count--;
        if(n <= 0)
            count++;
        return n;
    }
};

extern pcounter task_counter;

struct thread_task : public task {
    bool joined;
    std::shared_future<void> th;
    thread_task() : joined(true) {}
    ~thread_task() {
        info = 0;
    }
    virtual void start() {
        joined = false;
        //assert(info.valid());
        if(pfunc == search_f) {
            th = std::async(std::launch::async,search_pt,info);
        } else if(pfunc == search_ab_f) {
            th = std::async(std::launch::async,search_ab_pt,info);
        } else if(pfunc == qeval_f) {
            th = std::async(std::launch::async,qeval_pt,info);
        } else {
            abort();
        }
    }
    virtual void join() {
        if(joined)
            return;
        th.get();
        joined = true;
    }
};

#ifdef HPX_SUPPORT
#include "hpx_support.hpp"
#include <hpx/include/iostreams.hpp>
#include <time.h>
struct hpx_task : public task {
    hpx::lcos::shared_future<void> result;
    hpx_task()  {
    }

    virtual void start();

    virtual void join() {
        result.get();
    }
};
#endif

#endif
