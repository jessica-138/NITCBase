#include "Frontend.h"

#include <cstring>
#include <iostream>

int Frontend::create_table(char relname[ATTR_SIZE], int no_attrs, char attributes[][ATTR_SIZE],
                           int type_attrs[]) {
    return Schema::createRel(relname, no_attrs, attributes, type_attrs);
}

int Frontend::drop_table(char relname[ATTR_SIZE]) {
  return Schema::deleteRel(relname);
}

int Frontend::open_table(char relname[ATTR_SIZE]) {
  return Schema::openRel(relname);   // returns SUCCESS if successful
}

int Frontend::close_table(char relname[ATTR_SIZE]) {
  return Schema::closeRel(relname);  
}

int Frontend::alter_table_rename(char relname_from[ATTR_SIZE], char relname_to[ATTR_SIZE]) {
      // Pass relation rename request to Schema layer
  return Schema::renameRel(relname_from, relname_to);
  return SUCCESS;
}

int Frontend::alter_table_rename_column(char relname[ATTR_SIZE], char attrname_from[ATTR_SIZE],
                                        char attrname_to[ATTR_SIZE]) {
      // Pass attribute rename request to Schema layer
    return Schema::renameAttr(
        relname,
        attrname_from,
        attrname_to
    );
  return SUCCESS;
}

int Frontend::create_index(char relname[ATTR_SIZE], char attrname[ATTR_SIZE]) {
  return Schema::createIndex(relname, attrname);
}

int Frontend::drop_index(char relname[ATTR_SIZE], char attrname[ATTR_SIZE]) {
  return Schema::dropIndex(relname, attrname);
}


int Frontend::insert_into_table_values(char relname[ATTR_SIZE], int attr_count, char attr_values[][ATTR_SIZE]) {
  return Algebra::insert(relname, attr_count, attr_values);
  return SUCCESS;
}


// no where condn
int Frontend::select_from_table(char relname_source[ATTR_SIZE], char relname_target[ATTR_SIZE]) {
  // Algebra::project
  return Algebra::project(relname_source, relname_target);
}

//no where condn
int Frontend::select_attrlist_from_table(char relname_source[ATTR_SIZE], char relname_target[ATTR_SIZE],
  int attr_count, char attr_list[][ATTR_SIZE]) 
{
  // Algebra::project
  return Algebra::project(relname_source, relname_target, attr_count, attr_list);
}


int Frontend::select_from_table_where(char relname_source[ATTR_SIZE], char relname_target[ATTR_SIZE],
  char attribute[ATTR_SIZE], int op, char value[ATTR_SIZE]) {
  // Algebra::select
  return Algebra::select(relname_source, relname_target, attribute, op, value);

}

// selection + projection
int Frontend::select_attrlist_from_table_where(
    char relname_source[ATTR_SIZE],
    char relname_target[ATTR_SIZE],
    int attr_count,
    char attr_list[][ATTR_SIZE],
    char attribute[ATTR_SIZE],
    int op,
    char value[ATTR_SIZE])
{
    char tempRel[ATTR_SIZE] = TEMP;

    // select
    int ret = Algebra::select(relname_source, tempRel, attribute, op, value);

    if (ret != SUCCESS)
        return ret;

    int tempRelId = OpenRelTable::openRel(tempRel);

    if (tempRelId < 0){
        Schema::deleteRel(tempRel);
        return tempRelId;
    }

    // project
    ret = Algebra::project( tempRel, relname_target, attr_count, attr_list);

    OpenRelTable::closeRel(tempRelId);
    Schema::deleteRel(tempRel);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}



int Frontend::select_from_join_where(char relname_source_one[ATTR_SIZE], char relname_source_two[ATTR_SIZE],
                                     char relname_target[ATTR_SIZE],
                                     char join_attr_one[ATTR_SIZE], char join_attr_two[ATTR_SIZE]) {
  // Algebra::join
  return SUCCESS;
}

int Frontend::select_attrlist_from_join_where(char relname_source_one[ATTR_SIZE], char relname_source_two[ATTR_SIZE],
                                              char relname_target[ATTR_SIZE],
                                              char join_attr_one[ATTR_SIZE], char join_attr_two[ATTR_SIZE],
                                              int attr_count, char attr_list[][ATTR_SIZE]) {
  // Algebra::join + project
  return SUCCESS;
}

int Frontend::custom_function(int argc, char argv[][ATTR_SIZE]) {
  // argc gives the size of the argv array
  // argv stores every token delimited by space and comma

  // implement whatever you desire

  return SUCCESS;
}