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
    Json_schema_keyword *alternate_schema;
    Json_schema_keyword *unevaluated_properties, *unevaluated_items;
    char keyword_name[64];
    uint value;
    uint priority;
    bool allowed;
    uint index;

    virtual bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                          const uchar *k_end= NULL, bool validate_whole= false)
    {
      return false;
    }
    virtual bool handle_keyword(THD *thd, json_engine_t *je,
                                const char* key_start,
                                const char* key_end,
                                List<Json_schema_keyword> *all_keywords)
    {
      return false;
    }
    void set_alternate_schema(Json_schema_keyword *schema)
    {
      alternate_schema= schema;
    }
    Json_schema_keyword(char *schema_name, int len)
    {
      alternate_schema= unevaluated_properties= unevaluated_items= NULL;
      value= 0;
      strncpy(keyword_name, (const char*)schema_name, len);
      keyword_name[len]='\0';
      allowed= true;
      index= 0;
    }
    virtual void set_dependents(Json_schema_keyword *schema1,
                              Json_schema_keyword *schema2)
    {
      return;
    }
    virtual ~Json_schema_keyword() = default;
    virtual bool fall_back_on_alternate_schema(const json_engine_t *je,
                                               const uchar* k_start= NULL,
                                               const uchar* k_end= NULL);
    virtual bool validate_as_alternate(const json_engine_t *je,
                                               const uchar* k_start= NULL,
                                               const uchar* k_end= NULL) { return false; }
    virtual void set_unevaluated_item(Json_schema_keyword *schema) { unevaluated_items= schema; }
    virtual void set_unevaluated_properties(Json_schema_keyword *schema) { unevaluated_properties= schema; }
    virtual bool validate_as_unevaluated(const json_engine_t *je, const uchar* k_start= NULL, const uchar* k_end= NULL) { return false; }
};

/*
  Additional and unvaluated keywords anf items handle
  keywords and validate schema in same way, so it makes sense
  to have a base clasa for them.
*/
class Json_schema_additional_and_unevaluated : public Json_schema_keyword
{
  public:
    List<Json_schema_keyword> schema_list;
    Json_schema_additional_and_unevaluated(char *schema_name, int len,
                                         bool allowed_val):
    Json_schema_keyword(schema_name, len)
    {
      allowed= allowed_val;
    }
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    bool validate_as_alternate(const json_engine_t *je, const uchar *k_start,
                           const uchar *k_end) override;
};


class Json_schema_annotation : public Json_schema_keyword
{
  public:
    Json_schema_annotation(char *schema_name, int len):Json_schema_keyword(schema_name, len){}
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
};

class Json_schema_format : public Json_schema_keyword
{
  public:
    Json_schema_format(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
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
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_type(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    { type= 0; }
};

class Json_schema_const : public Json_schema_keyword
{
  private:
    char *const_json_value;

  public:
    enum json_value_types type;
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_const(char *schema_name, int len):Json_schema_keyword(schema_name, len)
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
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_enum(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
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
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_maximum(char *schema_name, int len):Json_schema_keyword(schema_name, len)
    {}
};

class Json_schema_minimum : public Json_schema_keyword
{
  private:
    double minimum;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_minimum(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_multiple_of : public Json_schema_keyword
{
  private:
    double multiple_of;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_multiple_of(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_ex_maximum : public Json_schema_keyword
{
  private:
    double ex_max;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_ex_maximum(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_ex_minimum : public Json_schema_keyword
{
  private:
    double ex_min;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_ex_minimum(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_max_len : public Json_schema_keyword
{
  private:
    uint max_len;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_max_len(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_min_len : public Json_schema_keyword
{
  private:
    uint min_len;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_min_len(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_pattern : public Json_schema_keyword
{
  private:
    Regexp_processor_pcre re;
    Item *pattern;
    Item_string *str;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_pattern(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
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
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_max_items(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_min_items : public Json_schema_keyword
{
  private:
    uint min_items;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_min_items(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

enum array_property_flag
{ HAS_NO_ARRAY_FLAG= 0, HAS_MAX_CONTAINS=8, HAS_MIN_CONTAINS= 16};

class Json_schema_max_contains : public Json_schema_keyword
{
  public:
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_max_contains(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_min_contains : public Json_schema_keyword
{
  public:
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_min_contains(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};
/*
  The value of max_contains and min_contains is only
  relevant when contains keyword is present.
  Hence the pointers to access them directly.
*/
class Json_schema_contains : public Json_schema_keyword
{
  public:
    List <Json_schema_keyword> contains;
    Json_schema_keyword *max_contains;
    Json_schema_keyword *min_contains;

    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_contains(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      max_contains= NULL;
      min_contains= NULL;
    }
    void set_dependents(Json_schema_keyword *min, Json_schema_keyword *max)
    { min_contains= min; max_contains= max;}
};

class Json_schema_unique_items : public Json_schema_keyword
{
  private:
    bool is_unique;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_unique_items(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      is_unique= false;
    }
};


class Json_schema_prefix_items : public Json_schema_keyword
{
  public:
    List <List_schema_keyword> prefix_items;
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_prefix_items(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      priority= 1;
    }
};

class Json_schema_unevaluated_items : public Json_schema_additional_and_unevaluated
{
  public:
    Json_schema_unevaluated_items(char *schema_name, int len, bool allowed_val):
    Json_schema_additional_and_unevaluated(schema_name, len, allowed_val)
    { priority= 4; allowed= allowed_val;}
};

class Json_schema_additional_items : public Json_schema_additional_and_unevaluated
{
  public:
    Json_schema_additional_items(char *schema_name, int len, bool allowed_val):
    Json_schema_additional_and_unevaluated(schema_name, len, allowed_val)
    { priority= 3; allowed= allowed_val;}
};

class Json_schema_items : public Json_schema_keyword
{
  private:
    Json_schema_prefix_items *validate_like_prefix;
    List<Json_schema_keyword> validate_like_schema;
  public:
    Json_schema_items(char *schema_name, int len, bool allowed_val):
    Json_schema_keyword(schema_name, len)
    {
      validate_like_prefix= NULL;
      priority= 2;
      allowed= allowed_val;
    }
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    bool validate_single_index(json_engine_t *je,
                                        List<Json_schema_keyword> *curr_schema);
    bool validate_as_alternate(const json_engine_t *je, const uchar *k_start,
                           const uchar *k_end) override;
};


class Json_schema_property_names : public Json_schema_keyword
{
  protected:
    List <Json_schema_keyword> property_names;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_property_names(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
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
    bool is_hash_inited;

  public:
    Json_schema_properties(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      is_hash_inited= false;
      priority= 1;
    }
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    ~Json_schema_properties()
    {
      if (is_hash_inited)
        my_hash_free(&properties);
    }
    bool validate_as_alternate(const json_engine_t *je, const uchar *k_start,
                           const uchar *k_end) override;
    bool validate_as_unevaluated(const json_engine_t *je, const uchar* k_start= NULL, const uchar* k_end= NULL) override;
};


class Json_schema_additional_properties : public Json_schema_additional_and_unevaluated
{
  public:
    Json_schema_additional_properties(char *schema_name, int len, bool allowed_val):
    Json_schema_additional_and_unevaluated(schema_name, len, allowed_val)
    { priority= 3; allowed= allowed_val;}
};

class Json_schema_unevaluated_properties : public Json_schema_additional_and_unevaluated
{
  public:
    Json_schema_unevaluated_properties(char *schema_name, int len, bool allowed_val):
    Json_schema_additional_and_unevaluated(schema_name, len, allowed_val)
    { priority= 4; allowed= allowed_val;}
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
};

typedef struct pattern_to_property : public Sql_alloc
{
  Regexp_processor_pcre re;
  Item *pattern;
  List<Json_schema_keyword> *curr_schema;
}st_pattern_to_property;

class Json_schema_pattern_properties : public Json_schema_keyword
{
  private:
    Item_string *str;
    List<st_pattern_to_property> pattern_properties;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_pattern_properties(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      priority= 2;
    }
    ~Json_schema_pattern_properties()
    {
      str= NULL;
      if (!pattern_properties.is_empty())
      {
        st_pattern_to_property *curr_pattern_to_property= NULL;
        List_iterator<st_pattern_to_property> it(pattern_properties);
        while((curr_pattern_to_property= it++))
        {
          curr_pattern_to_property->re.cleanup();
          curr_pattern_to_property->pattern= NULL;
          delete curr_pattern_to_property;
          curr_pattern_to_property= nullptr;
        }
      }
    }
    bool validate_as_alternate(const json_engine_t *je, const uchar *k_start,
                           const uchar *k_end) override;
};


class Json_schema_max_prop : public Json_schema_keyword
{
  private:
    uint max_prop;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_max_prop(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

class Json_schema_min_prop : public Json_schema_keyword
{
  private:
    uint min_prop;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_min_prop(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}  
};

class Json_schema_required : public Json_schema_keyword
{
  private:
    List <String> required_properties;

  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_required(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
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
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_dependent_prop(char *schema_name, int len):
    Json_schema_keyword(schema_name, len){}
};

enum logic { HAS_NOT= 2, HAS_ONE_OF= 4, HAS_ANY_OF= 8, HAS_ALL_OF= 16 };
class Json_schema_logic : public Json_schema_keyword
{
  protected:
    Json_schema_keyword *unevaluated_items;
    Json_schema_keyword *unevaluated_properties;
    uint logic;
    List <List_schema_keyword> schema_items;
    List<Json_schema_keyword> schema_list;
  public:
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    Json_schema_logic(char *schema_name, int len):
    Json_schema_keyword(schema_name, len)
    {
      unevaluated_items= NULL;
      unevaluated_properties= NULL;
      logic= 0;
    }
    virtual bool validate_count(uint* count, uint* total) { return false; }
    void set_dependents(Json_schema_keyword *schema1,
                      Json_schema_keyword *schema2)
    {
      unevaluated_items= schema1;
      unevaluated_properties= schema2;
    }
};

class Json_schema_not : public Json_schema_logic
{
  public:
    Json_schema_not(char *schema_name, int len):Json_schema_logic(schema_name, len)
    {
      logic|= HAS_NOT;
    }
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    bool validate_count(uint *count, uint *total) override { return *count !=0; }
    
};

class Json_schema_one_of : public Json_schema_logic
{
  public:
    Json_schema_one_of(char *schema_name, int len):Json_schema_logic(schema_name, len)
    {
      logic|= HAS_ONE_OF;
    }
     bool validate_count(uint *count, uint *total) override { return !(*count == 1); }
};

class Json_schema_any_of : public Json_schema_logic
{
  public:
    Json_schema_any_of(char *schema_name, int len):Json_schema_logic(schema_name, len)
    {
      logic|= HAS_ANY_OF;
    }
     bool validate_count(uint *count, uint *total) override { return *count == 0; }
};

class Json_schema_all_of : public Json_schema_logic
{
  public:
    Json_schema_all_of(char *schema_name, int len):Json_schema_logic(schema_name, len)
    {
      logic|= HAS_ALL_OF;
    }
     bool validate_count(uint *count, uint *total) override { return *count != *total; }
};

class Json_schema_conditional : public Json_schema_keyword
{
  public:
    List<Json_schema_keyword> conditions_schema;
    Json_schema_keyword *if_cond, *then_cond, *else_cond;
    Json_schema_conditional(char *schema_name, int len):Json_schema_keyword(schema_name, len)
    {
       if_cond= NULL; then_cond= NULL; else_cond= NULL;
    }
    bool validate(const json_engine_t *je, const uchar *k_start= NULL,
                  const uchar *k_end= NULL, bool validate_whole= false) override;
    bool handle_keyword(THD *thd, json_engine_t *je,
                        const char* key_start,
                        const char* key_end,
                        List<Json_schema_keyword> *all_keywords) override;
    void set_dependents(Json_schema_keyword *schema1, Json_schema_keyword* schema2) override
    {
      then_cond= schema1;
      else_cond= schema2;
    }
};

class Json_schema_if : public Json_schema_conditional
{
  public:
    Json_schema_if(char *schema_name, int len):Json_schema_conditional(schema_name, len){}
};

class Json_schema_else : public Json_schema_conditional
{

  public:
    Json_schema_else(char *schema_name, int len):Json_schema_conditional(schema_name, len){}
};

class Json_schema_then : public Json_schema_conditional
{

  public:
    Json_schema_then(char *schema_name, int len):Json_schema_conditional(schema_name, len){}
};


bool create_object_and_handle_keyword(THD *thd, json_engine_t *je,
                                      List<Json_schema_keyword> *keyword_list,                           
                                      List<Json_schema_keyword> *all_keywords);
uchar* get_key_name_for_property(const char *key_name, size_t *length,
                    my_bool /* unused */);

#endif
