#ifndef JSON_SCHEMA_INCLUDED
#define JSON_SCHEMA_INCLUDED

/* Copyright (c) 2016, 2021, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* This file defines all json schema classes. */

#include "sql_class.h"
#include "sql_type_json.h"
#include "json_schema_helper.h"

class Json_schema_keyword : public Sql_alloc
{
  public:
    virtual bool validate(const json_engine_t *je) {return false;}
    virtual bool handle_keyword(THD *thd, json_engine_t *je,
                                const char* key_start,
                                const char* key_end,
                                List<Json_schema_keyword> *all_keywords)
    {
      return false;
    }
    virtual ~Json_schema_keyword() = default;
};

class Json_schema_annotation : public Json_schema_keyword
{
  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_format : public Json_schema_keyword
{
  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

typedef List<Json_schema_keyword> List_schema_keyword;

class Json_schema_type : public Json_schema_keyword
{
  private:
    uint type;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_type()
    {
      type= 0;
    }
};

class Json_schema_const : public Json_schema_keyword
{
  private:
    char *const_json_value;

  public:
    enum json_value_types type;
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_const()
    {
      const_json_value= NULL;
    }
};

enum enum_scalar_values {
                         HAS_NO_VAL= 0, HAS_TRUE_VAL= 2,
                         HAS_FALSE_VAL= 4, HAS_NULL_VAL= 8
                        };
class Json_schema_enum : public  Json_schema_keyword
{
  private:
    HASH enum_values;
    uint enum_scalar;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_enum()
    {
      enum_scalar= HAS_NO_VAL;
    }
    ~Json_schema_enum()
    {
      my_hash_free(&enum_values);
    }

};

class Json_schema_maximum : public Json_schema_keyword
{
  private:
    double maximum;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_minimum : public Json_schema_keyword
{
  private:
    double minimum;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_multiple_of : public Json_schema_keyword
{
  private:
    double multiple_of;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_ex_maximum : public Json_schema_keyword
{
  private:
    double ex_max;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_ex_minimum : public Json_schema_keyword
{
  private:
    double ex_min;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_max_len : public Json_schema_keyword
{
  private:
    uint max_len;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_min_len : public Json_schema_keyword
{
  private:
    uint min_len;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_pattern : public Json_schema_keyword
{
  private:
    Regexp_processor_pcre re;
    Item *pattern;
    Item_string *str;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_pattern()
    {
      str= NULL;
      pattern= NULL;
    }
    ~Json_schema_pattern() { re.cleanup(); }
};

class Json_schema_max_items : public Json_schema_keyword
{
  private:
    uint max_items;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_min_items : public Json_schema_keyword
{
  private:
    uint min_items;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

enum array_property_flag
{ HAS_NO_ARRAY_FLAG= 0, HAS_MAX_CONTAINS=8, HAS_MIN_CONTAINS= 16};

/*
  contains, maxContains and mincontains are all kept in same class
  instead of separate classes like other keywords. This is because
  minContains and maxContains are depenedent on value of contains
  keywords.
*/
class Json_schema_contains : public Json_schema_keyword
{
  private:
    uint contains_type;
    uint max_contains;
    uint min_contains;
    uint contains_flag;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_contains()
    {
      contains_flag= HAS_NO_ARRAY_FLAG;
      contains_type= 0;
    }
};

class Json_schema_unique_items : public Json_schema_keyword
{
  private:
    bool is_unique;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_unique_items()
    {
      is_unique= false;
    }
};

/*
  prefixItems and allow_extra_items are all kept in same class
  because if result of schema validation if extra items
  are disallowed depends on the prefix items. Exception is made here,
  just like contains, minContains and maxContains.
*/
class Json_schema_items_details : public Json_schema_keyword
{
  private:
    List <List_schema_keyword> prefix_items;
    bool allow_extra_items;
    uint item_type;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_items_details()
    {
      item_type= 0;
      allow_extra_items= true;
    }
};

typedef struct property
{
  List<Json_schema_keyword> *curr_schema;
  char *key_name;
} st_property;

class Json_schema_properties : public Json_schema_keyword
{
  private:
    HASH properties;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    ~Json_schema_properties()
    {
      my_hash_free(&properties);
    }
    
};

class Json_schema_max_prop : public Json_schema_keyword
{
  private:
    uint max_prop;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_min_prop : public Json_schema_keyword
{
  private:
    uint min_prop;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_required : public Json_schema_keyword
{
  private:
    List <String> required_properties;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

typedef struct dependent_keyowrds
{
  String *property;
  List <String> dependents;
} st_dependent_keywords;

class Json_schema_dependent_prop : public Json_schema_keyword
{
  private:
    List<st_dependent_keywords> dependent_required;

  public:
    bool validate(const json_engine_t *je) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

bool create_object_and_handle_keyword(THD *thd, json_engine_t *je,
                                      List<Json_schema_keyword> *keyword_list,                           
                                      List<Json_schema_keyword> *all_keywords);
uchar* get_key_name_for_property(const char *key_name, size_t *length,
                    my_bool /* unused */);

#endif
