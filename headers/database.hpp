#ifndef DATABASE_H
#define DATABASE_H
#include <sstream>
#include <iostream>
#include "hash.hpp"
#include "node.hpp"
#include "data.hpp"
#include "defs.hpp"
#include "chess_move.hpp"
#ifdef SQLITE3_SUPPORT
#include "sqlite3.h"
#endif
#include "main.hpp"
#include "search.hpp"
#include <assert.h>
#include "parallel.hpp"
#include "zkey.hpp"
#include <atomic>
#include <vector>

using namespace std;
//using something = std::vector<std::string>;
using pseudo = std::vector<std::string>;

class database {
  public:
     #ifdef SQLITE3_SUPPORT
    sqlite3 *db;
    #endif
    char *zErrMsg=0;
    int rc;
    struct args;

    database(){
    #ifdef SQLITE3_SUPPORT
        const char *sql;
        rc = sqlite3_open("test1.db", &db);

        if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
         exit(0);
        }else{
          fprintf(stderr, "Opened database successfully\n");
        }
        rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, &zErrMsg);
        /* Create SQL statement */
        sql = "CREATE TABLE  white("  \
        "DEPTH            INTEGER     NOT NULL," \
        "BOARD               TEXT     NOT NULL,"\
        "HI               NUMERIC(20) NOT NULL,"\
        "LO               NUMERIC(20) NOT NULL,"\
        "EVAL                TEXT     NOT NULL,"\
        "HASH             INTEGER     NOT NULL,"\
        "SUMDEPTH         INTEGER     NOT NULL,"\
        "PRIMARY KEY (DEPTH, BOARD));";
       //Execute SQL statement
        //cout << "SQL: "<< sql << endl;
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if( rc != SQLITE_OK ){
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }else{
            fprintf(stdout, "Table created successfully\n");
        }
        sql = "CREATE TABLE black("  \
        "DEPTH            INTEGER     NOT NULL," \
        "BOARD               TEXT     NOT NULL,"\
        "HI               NUMERIC(20) NOT NULL,"\
        "LO               NUMERIC(20) NOT NULL,"\
        "EVAL                TEXT     NOT NULL,"\
        "HASH             INTEGER     NOT NULL,"\
        "SUMDEPTH         INTEGER     NOT NULL,"\
        "PRIMARY KEY (DEPTH, BOARD));";
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if( rc != SQLITE_OK ){
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }else{
            fprintf(stdout, "Table created successfully\n");
        }
//    deleteAll();
    #endif
    };

//destructor
    ~database(){
     #ifdef SQLITE3_SUPPORT
     // get_data();
      sqlite3_close(db);
      cout<<"database closed. \n";
     #endif
    }

 score_t score_board (const node_t& board) {
    evaluator ev;
    DECL_SCORE(s, ev.eval(board, chosen_evaluator), board.hash)
    return s;
  }
/*
  void deleteAll(){
    const char *sql;
    std::ostringstream o;
    o<<"delete from \"MoveSet\"";
    std::string result = o.str();
    sql=result.c_str();
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if ( rc == SQLITE_OK) cout<<"MoveSet sucessfully reset";
        else{ cout<<zErrMsg<<"/n"; sqlite3_free(zErrMsg);}
  }
*/
    void add_data(node_t& board, score_t lo, score_t hi, bool white, int excess){
     #ifdef SQLITE3_SUPPORT
      assert(lo < hi);
      assert(excess == 0);
      white == board.side == LIGHT;
      {
        score_t s_unused;
        int excess_unused;
        score_t zlo, zhi;
        bool found = get_transposition_value(board, zlo, zhi, white,s_unused,excess_unused, true,board.depth+excess);
        if(found) {
          assert(excess == 0);
          if(lo <= zlo && zhi <= hi)
            return;
          if(board.depth > 2)
            cout << "Narrowing ";
          int olo = lo, ohi = hi;
          lo = max(lo,zlo);
          hi = min(hi,zhi);
          if(lo >= hi)
            std::cout << "olo=" << olo << " ohi=" << ohi << " zlo=" << zlo << " zhi=" << zhi << std::endl;
          assert(lo < hi);
          if(lo >= hi)
           hi = lo+1;
        }
      }
      //if(board.depth > 2)
        //cout << "add_data(" << board.hash <<','<<board.depth<<','<<lo<<','<<hi<<','<<chosen_evaluator<<')'<<endl;
      int depth = board.depth;
      auto hash= board.hash;
      const char *sql;    //maybe find a way to make work without const
      //create SQL statement from string into char * array
      std::ostringstream bsm;
      print_board(board,bsm,true);
      std::string bs = bsm.str();

      std::ostringstream o;

      o<< "INSERT OR REPLACE INTO "<< (white ? "white" : "black") <<" (depth, board, hi, lo,eval, hash, sumdepth) VALUES ("<<excess+depth<<",'"<<bs<<"',"<<hi<<","<<lo<<","<<chosen_evaluator<<","<<hash<< ","<<excess+depth<<");";
      std::string result = o.str();
      sql=result.c_str();
      //char *sql = "REPLACE INTO \"MOVELIST\"VALUES (69,0);";
      //"INSERT INTO \"MOVELIST\"VALUES (66,0);";
      //execute SQL statement
      rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
      if ( rc!= SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
      }else{
        //fprintf(stdout, "Records created sucessfully\n");
      }
      if (board.depth>3){
        ;//cout<<"Adding depth of "<<board.depth<<endl;
      }
     #endif
    }
//look for boards that are the same, look for boards >= to current depth, most importantly score greater than the current score
/*    int get_data(){
      const char *data= "Callback function called";
      const char *sql= "SELECT * from MoveSet";
      rc= sqlite3_exec(db, sql, 0, (void*)data, &zErrMsg);
      if (rc!= SQLITE_OK ){
         fprintf(stderr, "SQL error: %s\n", zErrMsg);
         sqlite3_free(zErrMsg);
      }else
         fprintf(stdout, "Operation done  sucessfully\n");
         return 0;
    }
*/
   /* bool get_database_value(const node_t& board, score_t& zlo, score_t& zhi, bool white){
      bool gotten = false;
      const char *sql;
      std::ostringstream current, out;
      print_board(board, current, true);
      std::string curr = current.str();
      out<< "SELECT LO, HI, PLY FROM "<<( white ? "white" :"black")<<" WHERE \"BOARD\"=\""<<curr<<"\" AND \"PLY\"="<<board.depth<<";";
      std::string result = out.str();
      sql=result.c_str();
      int nrow, ncolumn;
      char ** azResult=NULL;//index:5=ply,6=board, 7=hi, 8=lo
      rc= sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
      if(nrow > 0) {
        cout << "nrow=" << nrow << " ncol=" << ncolumn <<"       depth="<<azResult[ncolumn*nrow+2]<<" board.depth="<<board.depth<< endl;
        stringstream strValue;
        strValue <<*azResult[5];
        //std::string s= *azResult[5];
        int num;//= atoi(s.c_str());
        strValue>>num;
        if (num == board.depth)
        if (score_board(board) < atoi(azResult[ncolumn*nrow+0]));{
        zlo = atoi(azResult[ncolumn*nrow+0]);
        zhi = atoi(azResult[ncolumn*nrow+1]);
        cout << "zlo=" << zlo << " zhi=" << zhi << endl;
        gotten=true; 
      }else{
        gotten=true;}
        zhi = bad_max_score;
      }
      sqlite3_free_table(azResult);

   return gotten;
    }*/

  struct args{
    //int argc;
    char* argv1;
    char* argv2;
    args(char* v, char* c): argv1(v),argv2(c){}
    };

  friend std::ostream& operator<<(std::ostream& o, const args& a){
    o<<a.argv1<<"\n";
    o<<a.argv2<<endl;
    return o;
    }

  pseudo search_board(const node_t& board,std::ostringstream& out, const char *select, const char *value, std::string& search, bool white, score_t s,bool exact, int depth){
    pseudo v_score;
#ifdef SQLITE3_SUPPORT
    const char *sql;
    int delta = max(0, board.follow_depth - board.search_depth);
    //int delta = max(0, depth - board.depth);
    //std::vector<args> a;
    if (exact)
      out<< "SELECT "<<select<<" FROM "<<( white ? "white" : "black") <<" WHERE "<<value<<"=\""<<search<<"\" AND EVAL="<<chosen_evaluator<<" AND DEPTH"<< (white ? "=": "=") <<depth<<";";
    else
      out<< "SELECT "<<select<<" FROM "<<( white ? "white" : "black") <<" WHERE "<<value<<"=\""<<search<<"\" AND EVAL="<<chosen_evaluator<<" AND DEPTH > " <<board.depth+delta<<" AND LO>="<< s<< " ORDER BY DEPTH"<<";";
    std::string result = out.str();
    sql = result.c_str();
    rc = sqlite3_exec(db,sql,callback,&v_score ,&zErrMsg);
    if (rc!= SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }
    //fprintf(stdout, "Looked through board successfully\n");
#endif
    return v_score;
  }

  bool get_transposition_value(node_t& board, score_t& lower, score_t& upper, bool white,score_t& p_board,int& excess_depth, bool exact, int depth){
    #ifdef SQLITE3_SUPPORT
    bool gotten = false;
    std::ostringstream current;
    print_board(board,current,true);
    std::string curr = current.str();
    const char *select = "HI, LO, DEPTH";
    const char *b = "BOARD";
    std::ostringstream search;
    pseudo v_score = search_board(board, search, select, b, curr, white, p_board,exact, depth);
    if (v_score.size() == 3){
      //int sum_depth = atoi(v_score.at(3).c_str());
      excess_depth = atoi(v_score.at(2).c_str()) - board.depth;
      //int excess= sum_depth-depth;
      lower =  atol(v_score.at(1).c_str());
      upper =  atol(v_score.at(0).c_str());
      assert(lower < upper);
      //cout<<"upper ="<<upper<<" lower ="<<lower<<endl;
      gotten = true;
    }
    if(!gotten) {
      excess_depth = 0;
      lower = bad_min_score;
      upper = bad_max_score;
    }
    //deeper= false;
    return gotten;
    #else
    excess_depth = 0;
    lower = bad_min_score;
    upper = bad_max_score;
    return false;
    #endif
  }

    //callback used for select operation
    static int callback(void *Used, int argc, char **argvalue, char **azColName){
        //cout<<"callback is called"<<endl;
        pseudo* v_score=static_cast<pseudo*>(Used);
        v_score->clear();
        /*
        v_score->push_back(argvalue[argc-4]);
        v_score->push_back(argvalue[argc-3]);
        v_score->push_back(argvalue[argc-2]);
        v_score->push_back(argvalue[argc-1]);
        */
        for(int i=0;i<argc;i++) {
          v_score->push_back(argvalue[i]);
        }
        //args (a){argvalue[argc-2],argvalue[argc-1]};
        //std::cout<<a<<std::endl;
        return 0;
       };
    int main(){
        return 0;
    }

};

#endif
