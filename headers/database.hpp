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
//include "main.cpp"

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
        "HI               INTEGER     NOT NULL,"\
        "LO               INTEGER     NOT NULL,"\
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
    };
    
//destructor
    ~database(){
	get_data();
        sqlite3_close(db);
        cout<<"database closed. \n";
    };

    void add_data(const node_t& board, int ply, int hi, int lo){
        auto hash= board.hash;
        const char *sql;    //maybe find a way to make work without const
        //create SQL statement from string into char * array
        std::ostringstream o;
        
        o<< "REPLACE INTO \"MoveSet\"VALUES ("<<ply<<",'"<<get_board_string(board)<<"',"<<hi<<","<<lo<<","<<hash<<");";
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
            fprintf(stdout, "Records created sucessfully\n");
        }     
    };

    std::string get_board_string(const node_t& board){
        std::ostringstream out;
        int i;
        out<< std::endl;
        for (i=0; i<64; i++){
            switch (board.color[i]){
                case EMPTY:
                    out<< " .";
                    break;
                case LIGHT:
                    out << " " << piece_char[(size_t)board.piece[i]];
                    break;
                case DARK:
                    char ch = (piece_char[(size_t)board.piece[i]]);
                    out << " "<< ch;
                    break;
            }
            if ((i+1)%8 ==0 && i!= 63)
              out << std::endl;
        }
        return out.str();
    };

    int get_data(){
      const char *data= "Callback function called";
      const char *sql= "SELECT * from MoveSet";
      rc= sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
      if (rc!= SQLITE_OK ){
         fprintf(stderr, "SQL error: %s\n", zErrMsg);
         sqlite3_free(zErrMsg);
      }else 
         fprintf(stdout, "Operation done  sucessfully\n");
         return 0;
    };
    
    //callback used for select operation
    static int callback(void *NotUsed, int argc, char **argv, char **azColName){
       int i;
       for(i=0; i<argc; i++){
          printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
       }
       printf("\n");
       return 0;
       };
    int main(){
        return 0;
    };

};
//extern database dbase;
#endif
