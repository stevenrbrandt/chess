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

using namespace std;

class database {
  public:
    sqlite3 *db;
    char *zErrMsg=0;
    int rc;
    
    
    database(){
        const char *sql;
        rc = sqlite3_open("test.db", &db);
        if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
         exit(0);
        }else{
          fprintf(stderr, "Opened database successfully\n");
        }
        /* Create SQL statement */
        sql = "CREATE TABLE MoveSet("  \
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
//	deleteAll();
    };
    
//destructor
    ~database(){
      get_data();
      sqlite3_close(db);
      cout<<"database closed. \n";
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

    void add_data(const node_t& board, score_t lo, score_t hi){
      int ply = board.ply;
      auto hash= board.hash;
      const char *sql;    //maybe find a way to make work without const
      //create SQL statement from string into char * array
      std::ostringstream bsm;
      print_board(board,bsm,true);
      std::string bs = bsm.str();

      std::ostringstream o;

      o<< "INSERT OR REPLACE INTO \"MoveSet\"VALUES ("<<ply<<",'"<<bs<<"',"<<hi<<","<<lo<<","<<hash<<");";
      std::string result = o.str();
      sql=result.c_str();
      //char *sql = "REPLACE INTO \"MOVELIST\"VALUES (69,0);";
      //"INSERT INTO \"MOVELIST\"VALUES (66,0);";
      //execute SQL statement
      rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
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

    bool get_database_value(const node_t& board, score_t& zlo, score_t& zhi){
      bool gotten = false;
      const char *sql;
      std::ostringstream current, out;
      print_board(board, current, true);
      std::string curr = current.str();
      out<< "SELECT LO, HI FROM MoveSet WHERE \"BOARD\"=\""<<curr<<"\" AND \"PLY\"="<<board.depth<<";";
      std::string result = out.str();
      sql=result.c_str();
      int nrow, ncolumn;
      char ** azResult=NULL;//index:5=ply,6=board, 7=hi, 8=lo
      rc= sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
      if(nrow > 0) {
        for (int i=0; i<(nrow+1)*ncolumn;i++)
          cout<<"azResult["<<i<<"] ="<<azResult[i]<<"\n";
        cout << "nrow=" << nrow << " ncol=" << ncolumn << endl;
        stringstream strValue; /*
                                  strValue <<*azResult[5]; 
        //std::string s= *azResult[5];
        int num;//= atoi(s.c_str());
        strValue>>num;
        if (num == board.depth)*/
        zlo = atoi(azResult[ncolumn+0]);
        zhi = atoi(azResult[ncolumn+1]);
        cout << "zlo=" << zlo << " zhi=" << zhi << endl;
        gotten=true;
      }
      sqlite3_free_table(azResult);
      return gotten;
    }

      int search_board(const node_t& board,std::ostringstream& out, const char *select, const char *value, std::string& search){
        const char *sql;
        char *data= (char *)calloc(1, sizeof (*data));
        out<< "SELECT "<<select<<" FROM MoveSet WHERE \""<<value<<"\"=\""<<search<<"\";";
        std::string result = out.str();
        sql = result.c_str();
        rc = sqlite3_exec(db,sql,callback, (void*)data,&zErrMsg);
        cout<<data<<"/n";
        //cout<<"This is callback"<< callback << "\n";
        if (rc!= SQLITE_OK){
          fprintf(stderr, "SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }else
          //fprintf(stdout, "Looked through board successfully\n");
          return rc;


      }

      bool get_transposition_value(const node_t& board, score_t& lower, score_t& upper){
        bool gotten = false;
        std::ostringstream current;
        print_board(board,current,true);
        std::string curr = current.str();
        const char *hash = "HASH, HI";
        const char *b = "BOARD";
        std::ostringstream search;
        search_board(board,search,  hash, b, curr);
        std::string buscar = search.str();
        //cout<< "Value "<< buscar<<"\n";
        //fprintf(stdout, "Value: %s", search);
        /*if (z->depth >= 0 && board_equals(board, z->board)){
          lower = z->lower;
          upper = z->upper;
          gotten = true
          } else {
          lower = bad_min_score;
          upper = bad_max_score;
          }*/
        return gotten;
      }

      //callback used for select operation
      static int callback(void *NotUsed, int argc, char **argv, char **azColName){
        int i;
        std::ostringstream out;
        char ** res = (char **) NotUsed;
        //*res=NULL(sizeof(char *));
        // cout<<res<<"test\n";
        *res =(char *)calloc(strlen(argv[2]),sizeof(char *));
        strcpy(*res, argv[2]);
        return 0;
        for(i=0; i<argc; i++){
          out<< argv[i]<<"\n";
          // printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
        printf("\n");
        return 0;
      };
      int main(){
        return 0;
      }

    };
//extern database dbase;
#endif
