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
using pseudo = std::vector<char*>;

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
      get_data();
      sqlite3_close(db);
      cout<<"database closed. \n";
    }
    int depth;

 score_t score_board (const node_t& board) {
    evaluator ev;
    DECL_SCORE(s, ev.eval(board, chosen_evaluator), board.hash)
    return s;
  }

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

    void add_data(const node_t& board, score_t lo, score_t hi, bool white){
     // cout<<"This is lo and hi"<<lo<<' '<<hi<<endl;
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
    }
//look for boards that are the same, look for boards >= to current depth, most importantly score greater than the current score
    int get_data(){
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

    bool get_database_value(const node_t& board, score_t& zlo, score_t& zhi, bool white){
      bool gotten = false;
      const char *sql;
      std::ostringstream current, out;
      print_board(board, current, true);
      std::string curr = current.str();
      out<< "SELECT LO, HI, PLY FROM "<<( white ? "white" :"black")<<" WHERE \"BOARD\"=\""<<curr<<"\" AND \"PLY\"<"<<board.depth<<";";
      std::string result = out.str();
      sql=result.c_str();
      int nrow, ncolumn;
      char ** azResult=NULL;//index:5=ply,6=board, 7=hi, 8=lo
      rc= sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
      if(nrow > 0) {
        //for (int i=0; i<(nrow+1)*ncolumn;i++)
          //cout<<"azResult["<<i<<"] ="<<azResult[i]<<"\n";
        cout << "nrow=" << nrow << " ncol=" << ncolumn <<"       depth="<<azResult[ncolumn*nrow+2]<<" board.depth="<<board.depth<< endl;
        /*stringstream strValue;
        strValue <<*azResult[5];
        //std::string s= *azResult[5];
        int num;//= atoi(s.c_str());
        strValue>>num;
        if (num == board.depth)*/
        if (score_board(board) < atoi(azResult[ncolumn*nrow+0]));{
        zlo = atoi(azResult[ncolumn*nrow+0]);
        zhi = atoi(azResult[ncolumn*nrow+1]);
        cout << "zlo=" << zlo << " zhi=" << zhi << endl;
        gotten=true;}
      }else{
        zlo = bad_min_score;
        zhi = bad_max_score;
      }
      sqlite3_free_table(azResult);

   return gotten;
    }

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

    pseudo search_board(const node_t& board,std::ostringstream& out, const char *select, const char *value, std::string& search, bool white){
    const char *sql;
    pseudo v_score;
    //std::vector<args> a;
      out<< "SELECT "<<select<<" FROM "<<( white ? "white" : "black") <<" WHERE \""<<value<<"\"=\""<<search<<"\" AND \"PLY\"="<<board.depth<<";";
    std::string result = out.str();
        sql = result.c_str();
    rc = sqlite3_exec(db,sql,callback,&v_score ,&zErrMsg);
    //std::cout << "a.front() is" << &a.front() <<"\n";
    //std::cout << "my vector contains:"<<a<<std::endl;
    //for (unsigned i=0; i<a.size(); i++)
      //std::cout<< ' '<< a.at(i).argv;
    //std::cout<<'\n';
    if (rc!= SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }else{
            //std::cerr << va.size()<<"returned.\n";
      }
      //fprintf(stdout, "Looked through board successfully\n");
        return v_score;
    }

  bool get_transposition_value(const node_t& board, score_t& lower, score_t& upper, bool white){
    bool gotten = false;
    std::ostringstream current;
    print_board(board,current,true);
    std::string curr = current.str();
    const char *select = "HI, LO";
    const char *b = "BOARD";
    int depth = board.depth;
    std::ostringstream search;
    pseudo v_score = search_board(board, search, select, b, curr, white);
    if (! v_score.empty()){
      if (score_board(board) < atoi(v_score[1])){
       int zhi = atoi(v_score[0]);
       int zlow= atoi(v_score[1]);
       lower = zlow;
       upper = zhi;
       gotten = true;
       }
     }
    else {
      lower = bad_min_score;
      upper = bad_max_score;
    }
  return gotten;
  }

    //callback used for select operation
    static int callback(void *Used, int argc, char **argvalue, char **azColName){
       pseudo* v_score=static_cast<pseudo*>(Used);
        v_score->clear();
        v_score->push_back(argvalue[argc-2]);
        v_score->push_back(argvalue[argc-1]);
        for (unsigned int i=0; i<v_score->size();i++){
          if (!v_score->empty()){
            std::cout<< i <<"v_score " <<(*v_score)[i]<<endl;
            }
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
