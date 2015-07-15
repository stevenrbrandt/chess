#ifndef COMBINE_H
#define COMBINE_H
#include <iostream>

#include <string>
#ifdef SQLITE3_SUPPORT
#include "sqlite3.h"
#endif
#include "score.hpp"
#include <vector>

using namespace std;
using compare = std::vector<std::string>;
#ifdef SQLITE3_SUPPORT
sqlite3 *db;
sqlite3 *db2;
#endif

char *zErrMsg=0;
int rc;
const char *sql;


static int callback2(void *Used, int argc, char **argvalue, char **azColName);
static int callback(void *NotUsed, int argc, char **argvalue, char **azColName){
#ifdef SQLITE3_SUPPORT
  //get values from destination database if they exist
  std::ostringstream select2;
  select2<<"SELECT * FROM white WHERE DEPTH=\'"<<argvalue[0]<<"\' AND BOARD=\'"<<argvalue[1]<<"\';";
  std::string merge = select2.str();
  sql = merge.c_str();
  compare params;
  rc = sqlite3_exec(db, sql, callback2,&params,&zErrMsg);
  if (rc != SQLITE_OK){
    fprintf(stderr, "SQL error callback: %s\n", zErrMsg);
    std::cout<<sql<<std::endl;
    sqlite3_free(zErrMsg);
    }

  //preparing statement for if no match found then insert into db
  std::ostringstream ins;
  ins<<"INSERT OR REPLACE INTO white (depth, board, hi, lo, hash, sumdepth) VALUES (\'"<<argvalue[0]<<"\',\'"<<argvalue[1]<<"\',\'"<<argvalue[2]<<"\',\'"<<argvalue[3]<<"\',\'"<<argvalue[4]<<"\',\'"<<argvalue[5]<<"\');"; 
  std::string insert = ins.str();
  if (params.size() == 0){
    sql = insert.c_str();
    rc = sqlite3_exec(db, sql, 0,0, &zErrMsg);
    if (rc != SQLITE_OK){
      fprintf(stderr, "SQL error callbackFinal: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }
    return 0;
  }

  if (params.size() == 6){
    if (argvalue[3]== NULL)
      assert(false);
    score_t zlo = max(atoi(argvalue[3]), atol(params.at(3).c_str()));
    score_t zhi = min(atoi(argvalue[2]), atol(params.at(2).c_str())); 
    std::ostringstream o;
    o<<"UPDATE white SET \'"<<azColName[2]<<"\'=\'"<<zhi<<"\', \'"<<azColName[3]<<"\'=\'"<<zlo<<"\', \'"<<azColName[4]<<"\'=\'"<<argvalue[4]<<"\', \'"<<azColName[5]<<"\'=\'"<<argvalue[5]<<"\' WHERE BOARD=\'"<<argvalue[1]<<"\' AND DEPTH=\'"<<argvalue[0]<<"\';"; 
    std::string update = o.str();
    sql = update.c_str();
    rc = sqlite3_exec(db,sql,0,0,&zErrMsg);
    if (rc != SQLITE_OK){
      fprintf(stderr, "SQL error callback params: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }
  }
#endif
  return 0;
}

static int callback2(void *Used, int argc, char **argvalue, char **azColName){
  compare* params=static_cast<compare*>(Used);
  for(int i=0;i<argc;i++) {
    params->push_back(argvalue[i]);
  }
  return 0;
}

static int callback(void *NotUsed, int argc, char **argvalue, char **azColName);

void merge(std::vector<std::string> databases)
{
#ifdef SQLITE3_SUPPORT
  sqlite3_close(db);
  std::string current_database = databases.at(0).c_str();
  std::ostringstream l;
  l<<current_database<<".db";
  std::string open_1 = l.str();
  sql = open_1.c_str();
  rc = sqlite3_open(sql, &db);
    if(rc){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      exit(0);
    }else{
      fprintf(stderr, "Opened test1 database successfully\n");
      }
  for(int i(1); i<databases.size(); ++i)
  {
    std::string merging_database = databases.at(i).c_str();
    std::ostringstream o;
    o<<merging_database<<".db";
    std::string open = o.str();
    sql = open.c_str();
    rc = sqlite3_open(sql, &db2);
    if(rc){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      exit(0);
    }else{
      fprintf(stderr, "Opened database testx successfully\n");
      }

    //rc = sqlite3_exec(db2,"BEGIN;", 0, 0, &zErrMsg);
    rc = sqlite3_exec(db2, "SELECT * FROM white;",callback,0,&zErrMsg);
    if (rc != SQLITE_OK){
      fprintf(stderr, "SQL error 1st select: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    }else{
      fprintf(stderr, "Merged successfuly\n");  
      } 
   }
  // sqlite3_close(db);
   //sqlite3_close(db2);
    
#endif
}

#endif
