#ifndef COMBINE_H
#define COMBINE_H
#include <iostream>

#include <string>
#include "sqlite3.h"
#include <vector>

using namespace std;
using compare = std::vector<std::string>;
sqlite3 *db;
sqlite3 *db2;

char *zErrMsg=0;
int rc;
const char *sql;

static int callbackFinal(void *NotUsed, int argc, char **argvalue, char **azColName){ 
  std::ostringstream o;
  std::cout<< argc << std::endl;
  o<<"INSERT OR REPLACE INTO white (depth,board,hi,lo,hash,sumdepth) VALUES ("<<argvalue[0]<<","<<argvalue[1]<<","<<argvalue[2]<<","<<argvalue[3]<<","<<argvalue[4]<<","<<argvalue[5]<<");"; 
  //updating db from db2 if it has better values at 
  std::string update = o.str();
  sql = update.c_str();
  rc = sqlite3_exec(db,sql,0,0,&zErrMsg);
  if (rc != SQLITE_OK){
    fprintf(stderr, "SQL error callbackFinal: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    }
  return 0;
}

static int callback2(void *Used, int argc, char **argvalue, char **azColName);
static int callback(void *NotUsed, int argc, char **argvalue, char **azColName){
  std::ostringstream select2;
  select2<<"SELECT * FROM white WHERE BOARD="<<argvalue[1]<<" AND"<<"DEPTH ="<<argvalue[0]<<";";
  std::string merge = select2.str();
  sql = merge.c_str();
  compare params;
  rc = sqlite3_exec(db, sql, callback2,&params,&zErrMsg);
  std::ostringstream o;
  o<<"UPDATE white SET"<<azColName[2]<<"="<<argvalue[2]<<", "<<azColName[3]<<"="<<argvalue[3]<<", "<<azColName[4]<<"="<<argvalue[4]<<", "<<azColName[5]<<"="<<argvalue[5]<<" WHERE BOARD ="<<argvalue[1]<<" AND DEPTH ="<<argvalue[0]<<";"; 
  //updating db from db2 if it has better values at 
  std::string update = o.str();
  sql = update.c_str();
  int compare = atoi(argvalue[3]);
  if (params.size() == 0)
    return 0;
  if (params.size() == 5){
    if (compare < atoi(params.at(3).c_str())){
      rc = sqlite3_exec(db,sql,0,0,0);
      //then delete value from db2
    }
    if (compare>=atoi(params.at(3).c_str())){
      //delete value from db2
    }
  }
  rc = sqlite3_exec(db, sql, 0,0,0);
  if (rc != SQLITE_OK){
    fprintf(stderr, "SQL error callback: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    }else{
    }
  return 0;
}

static int callback2(void *Used, int argc, char **argvalue, char **azColName){
  compare* params=static_cast<compare*>(Used);
  for(int i=0;i<argc;i++) {
    params->push_back(argvalue[i]);
  }
  return 0;
  std::ostringstream del;
  del<<"DELETE FROM white WHERE BOARD="<<argvalue[1]<<" AND DEPTH ="<<argvalue[0]<<"AND LO<"<<argvalue[3]<<";";
  std::string db_delete = del.str();
  sql = db_delete.c_str();
  rc = sqlite3_exec(db2, sql, 0,0,0);
  if (rc != SQLITE_OK){
    fprintf(stderr, "SQL error callback: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    }else{
    }
}

static int callback(void *NotUsed, int argc, char **argvalue, char **azColName);
static int callbackFinal(void *NotUsed, int argc, char **argvalue, char **azColName); 

void merge(std::vector<std::string> databases)
{
  std::string current_database = databases.at(0).c_str();
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
      fprintf(stderr, "Opened database successfully\n");
      }

    rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, &zErrMsg);  
    rc = sqlite3_exec(db2,"BEGIN;", 0, 0, &zErrMsg);
    rc = sqlite3_exec(db2, "SELECT * FROM white;",callback,0,&zErrMsg);
    if (rc != SQLITE_OK){
      fprintf(stderr, "SQL error 1st select: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      }else{
      } 
    rc = sqlite3_exec(db2, "SELECT * FROM white;",callbackFinal, 0,&zErrMsg);
    if (rc != SQLITE_OK){
      fprintf(stderr, "SQL error 2nd select: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      }else{
        } 
    }
}

#endif