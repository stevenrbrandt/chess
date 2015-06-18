#ifndef DATABASE_H
#define DATABASE_H
#include <sstream>
#include <iostream>
#include "hash.hpp"
#include "node.hpp"
#include "data.hpp"
#include "defs.hpp"
#include "chess_move.hpp"
#include "sqlite3.h"
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
    sqlite3 *db;
    char *zErrMsg=0;
    int rc;
    struct args;

    database(){
        const char *sql;
        rc = sqlite3_open("test.db", &db);

        if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
         exit(0);
        }else{
          fprintf(stderr, "Opened database successfully\n");
        }
        rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, &zErrMsg);
        /* Create SQL statement */
        sql = "CREATE TABLE  white("  \
        "PLY              INTEGER     NOT NULL," \
        "BOARD               TEXT     NOT NULL,"\
        "HI               NUMERIC(20) NOT NULL,"\
        "LO               NUMERIC(20) NOT NULL,"\
        "HASH             INTEGER     NOT NULL,"\
        "PRIMARY KEY (PLY, BOARD));";
       //Execute SQL statement
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if( rc != SQLITE_OK ){
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }else{
            fprintf(stdout, "Table created successfully\n");
        }
        sql = "CREATE TABLE black("  \
        "PLY              INTEGER     NOT NULL," \
        "BOARD               TEXT     NOT NULL,"\
        "HI               NUMERIC(20) NOT NULL,"\
        "LO               NUMERIC(20) NOT NULL,"\
        "HASH             INTEGER     NOT NULL,"\
        "PRIMARY KEY (PLY, BOARD));";
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if( rc != SQLITE_OK ){
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }else{
            fprintf(stdout, "Table created successfully\n");
        }
//    deleteAll();
    };

//destructor
    ~database(){
     // get_data();
      sqlite3_close(db);
      cout<<"database closed. \n";
    }
    int depth;

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
    void add_data(const node_t& board, score_t lo, score_t hi, bool white){
      //cout<<"This is lo and hi"<<lo<<' '<<hi<<endl;
      int ply = board.depth;
      auto hash= board.hash;
     const char *sql;    //maybe find a way to make work without const
      //create SQL statement from string into char * array
      std::ostringstream bsm;
      print_board(board,bsm,true);
      std::string bs = bsm.str();

      std::ostringstream o;

      o<< "INSERT OR REPLACE INTO \""<< (white ? "white" : "black") <<"\" VALUES ("<<ply<<",'"<<bs<<"',"<<hi<<","<<lo<<","<<hash<<");";
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
        cout<<"Adding depth of "<<board.depth<<endl;}
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

    pseudo search_board(const node_t& board,std::ostringstream& out, const char *select, const char *value, std::string& search, bool white, score_t s){
    const char *sql;
    pseudo v_score;
    //std::vector<args> a;
    out<< "SELECT "<<select<<" FROM "<<( white ? "white" : "black") <<" WHERE \""<<value<<"\"=\""<<search<<"\" AND \"PLY\""<< (white ? ">": "=") <<board.depth<<" AND \"LO\" > "<< s<<";";
    std::string result = out.str();
        sql = result.c_str();
    rc = sqlite3_exec(db,sql,callback,&v_score ,&zErrMsg);
    if (rc!= SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }else{
      }
      //fprintf(stdout, "Looked through board successfully\n");
        return v_score;
    }

  bool get_transposition_value(const node_t& board, score_t& lower, score_t& upper, bool white,score_t& p_board, int depth, bool& deeper){
    bool gotten = false;
    std::ostringstream current;
    print_board(board,current,true);
    std::string curr = current.str();
    const char *select = "HI, LO, PLY";
    const char *b = "BOARD";
    std::ostringstream search;
    pseudo v_score = search_board(board, search, select, b, curr, white, p_board);
    int additive_ply= depth;
    if (v_score.size() == 3){
      int return_ply = atoi(v_score.at(2).c_str());
      additive_ply= return_ply+depth;
      cout<<"Additive ply of: " <<additive_ply<<endl;
      if (score_board(board) < atoi(v_score.at(1).c_str())){
       lower =  atol(v_score.at(1).c_str());
       upper =  atoi(v_score.at(0).c_str());
       cout<<"upper ="<<upper<<" lower ="<<lower<<endl;
        gotten= true;
       }
     }
    else {
      lower = bad_min_score;
      upper = bad_max_score;
    }
    deeper= false;
    if (additive_ply>depth)
      deeper=true;
    return gotten;
  }

    //callback used for select operation
    static int callback(void *Used, int argc, char **argvalue, char **azColName){
        //cout<<"callback is called"<<endl;
        pseudo* v_score=static_cast<pseudo*>(Used);
        v_score->clear();
        v_score->push_back(argvalue[argc-3]);
        v_score->push_back(argvalue[argc-2]);
        v_score->push_back(argvalue[argc-1]);
        //args (a){argvalue[argc-2],argvalue[argc-1]};
        //std::cout<<a<<std::endl;
        return 0;
       };
    int main(){
        return 0;
    }

};

#endif
