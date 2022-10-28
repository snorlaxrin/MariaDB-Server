/* Copyright (c) 2016, 2022, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include "mariadb.h"
#include "sql_class.h"
#include "sql_parse.h" // For check_stack_overrun
#include <m_string.h>
#include "json_schema.h"
#include "json_schema_helper.h"

bool Json_schema_annotation::validate(const json_engine_t *je)
{
  /* Nothing to validate. They're only annotations. */
  return false;
}

bool Json_schema_annotation::handle_keyword(THD *thd, json_engine_t *je, 
                                            const char* key_start,
                                            const char* key_end,
                                            List<Json_schema_keyword>
                                                 *all_keywords)
{
  bool is_invalid_value_type= false, res= false;
  int key_len= (int)(key_end-key_start);

  if (json_key_equals(key_start, { STRING_WITH_LEN("title") },
                      key_len) ||
      json_key_equals(key_start, { STRING_WITH_LEN("description") },
                      key_len) ||
      json_key_equals(key_start, { STRING_WITH_LEN("$comment") },
                      key_len) ||
      json_key_equals(key_start, { STRING_WITH_LEN("$schema") },
                      key_len))
  {
    if (je->value_type != JSON_VALUE_STRING)
      is_invalid_value_type= true;
  }
  else if (json_key_equals(key_start, { STRING_WITH_LEN("deprecated") },
                           key_len) ||
           json_key_equals(key_start, { STRING_WITH_LEN("readOnly") },
                           key_len) ||
           json_key_equals(key_start, { STRING_WITH_LEN("writeOnly") },
                           key_len))
  {
    if (je->value_type != JSON_VALUE_TRUE &&
        je->value_type != JSON_VALUE_FALSE)
      is_invalid_value_type= true;
  }
  else if (json_key_equals(key_start, { STRING_WITH_LEN("example") }, key_len))
  {
    if (je->value_type != JSON_VALUE_ARRAY)
      is_invalid_value_type= true;
    if (json_skip_level(je))
      return true;
  }
  else if (json_key_equals(key_start, { STRING_WITH_LEN("default") }, key_len))
    return false;
  else
    return true;

  if (is_invalid_value_type)
  {
    res= true;
    String keyword(0);
    keyword.append((const char*)key_start, (int)(key_end-key_start));
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), keyword.ptr());
  }
  return res;
}

bool Json_schema_format::validate(const json_engine_t *je)
{
  /* Nothing to validate. They're only annotations. */
  return false;
}

bool Json_schema_format::handle_keyword(THD *thd, json_engine_t *je,              
                                        const char* key_start,
                                        const char* key_end,
                                        List<Json_schema_keyword>
                                             *all_keywords)
{
  if (je->value_type != JSON_VALUE_STRING)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "format");
  }
  return false;
}

bool Json_schema_type::validate(const json_engine_t *je)
{
  return !((1 << je->value_type) & type);
}

bool Json_schema_type::handle_keyword(THD *thd, json_engine_t *je,   
                                      const char* key_start,
                                      const char* key_end,
                                      List<Json_schema_keyword>
                                           *all_keywords)
{
  if (je->value_type == JSON_VALUE_ARRAY)
  {
    int level= je->stack_p;
    while (json_scan_next(je)==0 && je->stack_p >= level)
    {
      if (json_read_value(je))
        return true;
      json_assign_type(&type, je);
    }
    return false;
  }
  else if (je->value_type == JSON_VALUE_STRING)
  {
    return json_assign_type(&type, je);
  }
  else
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "type");
    return true;
  }
}

bool Json_schema_const::validate(const json_engine_t *je)
{
  json_engine_t curr_je;
  curr_je= *je;
  const char *start= (char*)curr_je.value;
  const char *end= (char*)curr_je.value+curr_je.value_len;
  json_engine_t temp_je= *je;
  json_engine_t temp_je_2;
  String a_res("", 0, curr_je.s.cs);
  int err;

  if (type != curr_je.value_type)
   return true;

  if (curr_je.value_type <= JSON_VALUE_NUMBER)
  {
    if (!json_value_scalar(&temp_je))
    {
      if (json_skip_level(&temp_je))
      {
        curr_je= temp_je;
        return true;
      }
      end= (char*)temp_je.s.c_str;
    }
    String val((char*)temp_je.value, end-start, temp_je.s.cs);

    json_scan_start(&temp_je_2, temp_je.s.cs, (const uchar *) val.ptr(),
                    (const uchar *) val.end());

    if (temp_je.value_type != JSON_VALUE_STRING)
    {
      if (json_read_value(&temp_je_2))
      {
        curr_je= temp_je;
        return true;
      }
      json_get_normalized_string(&temp_je_2, &a_res, &err);
      if (err)
       return true;
    }
    else
      a_res.append(val.ptr(), val.length(), temp_je.s.cs);

    if (a_res.length() == strlen(const_json_value) &&
        !strncmp((const char*)const_json_value, a_res.ptr(),
                  a_res.length()))
      return false;
    return true;
  }
  else
    return curr_je.value_type != type;
}

bool Json_schema_const::handle_keyword(THD *thd, json_engine_t *je,
                                       const char* key_start,
                                       const char* key_end,
                                       List<Json_schema_keyword>
                                            *all_keywords)
{
  const char *start= (char*)je->value, *end= (char*)je->value+je->value_len;
  json_engine_t temp_je;
  String a_res("", 0, je->s.cs);
  int err;

  type= je->value_type;

  if (!json_value_scalar(je))
  {
    if (json_skip_level(je))
      return true;
    end= (char*)je->s.c_str;
  }

  String val((char*)je->value, end-start, je->s.cs);

  json_scan_start(&temp_je, je->s.cs, (const uchar *) val.ptr(),
                 (const uchar *) val.end());
  if (je->value_type != JSON_VALUE_STRING)
  {
    if (json_read_value(&temp_je))
      return true;
    json_get_normalized_string(&temp_je, &a_res, &err);
    if (err)
      return true;
  }
  else
    a_res.append(val.ptr(), val.length(), je->s.cs);

  this->const_json_value= (char*)alloc_root(thd->mem_root,
                                            a_res.length()+1);
  if (!const_json_value)
    return true;

  const_json_value[a_res.length()]= '\0';
  strncpy(const_json_value, (const char*)a_res.ptr(), a_res.length());

  return false;
}

bool Json_schema_enum::validate(const json_engine_t *je)
{
  json_engine_t temp_je;
  temp_je= *je;

  String norm_str((char*)"",0, je->s.cs);

  String a_res("", 0, je->s.cs);
  int err= 1;

  if (temp_je.value_type > JSON_VALUE_NUMBER)
  {
    if (temp_je.value_type == JSON_VALUE_TRUE)
      return !(enum_scalar & HAS_TRUE_VAL);
    if (temp_je.value_type == JSON_VALUE_FALSE)
      return !(enum_scalar & HAS_FALSE_VAL);
    if (temp_je.value_type == JSON_VALUE_NULL)
      return !(enum_scalar & HAS_NULL_VAL);
  }
  json_get_normalized_string(&temp_je, &a_res, &err);
  if (err)
    return true;

  norm_str.append((const char*)a_res.ptr(), a_res.length(), je->s.cs);

  if (my_hash_search(&this->enum_values, (const uchar*)(norm_str.ptr()),
                       strlen((const char*)(norm_str.ptr()))))
    return false;
  else
    return true;
}

bool Json_schema_enum::handle_keyword(THD *thd, json_engine_t *je,
                                      const char* key_start,
                                      const char* key_end,
                                      List<Json_schema_keyword>
                                           *all_keywords)
{
  if (my_hash_init(PSI_INSTRUMENT_ME,
                   &this->enum_values,
                   je->s.cs, 1024, 0, 0, (my_hash_get_key) get_key_name,
                   NULL, 0))
    return true;

  if (je->value_type == JSON_VALUE_ARRAY)
  {
    int curr_level= je->stack_p;
    while(json_scan_next(je) == 0 && curr_level <= je->stack_p)
    {
      if (json_read_value(je))
        return true;
      if (je->value_type > JSON_VALUE_NUMBER)
      {
        if (je->value_type == JSON_VALUE_TRUE)
          enum_scalar|= HAS_TRUE_VAL;
        else if (je->value_type == JSON_VALUE_FALSE)
          enum_scalar|= HAS_FALSE_VAL;
        else if (je->value_type == JSON_VALUE_NULL)
          enum_scalar|= HAS_NULL_VAL;
      }

      char *norm_str;
      int err= 1;
      String a_res("", 0, je->s.cs);

      json_get_normalized_string(je, &a_res, &err);
      if (err)
        return true;

      norm_str= (char*)alloc_root(thd->mem_root,
                                  a_res.length()+1);
      if (!norm_str)
        return true;
      else
      {
        norm_str[a_res.length()]= '\0';
        strncpy(norm_str, (const char*)a_res.ptr(), a_res.length());
        if (my_hash_insert(&this->enum_values, (uchar*)norm_str))
         return true;
      }
    }
    return false;
  }
  else
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "enum");
    return true;
  }
}

bool Json_schema_maximum::validate(const json_engine_t *je)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
    return false;

  double val= je->s.cs->strntod((char *) je->value,
                                  je->value_len, &end, &err);
  return (val <= maximum) ? false : true;
}

bool Json_schema_maximum::handle_keyword(THD *thd, json_engine_t *je, 
                                         const char* key_start,
                                         const char* key_end,
                                         List<Json_schema_keyword>
                                              *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maximum");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  maximum= val;

  return false;
}

bool Json_schema_minimum::validate(const json_engine_t *je)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
    return false;

  double val= je->s.cs->strntod((char *) je->value,
                                  je->value_len, &end, &err);
  return val >= minimum ? false : true;
}

bool Json_schema_minimum::handle_keyword(THD *thd, json_engine_t *je,
                                         const char* key_start,
                                         const char* key_end,
                                         List<Json_schema_keyword>
                                              *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minimum");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  minimum= val;

  return false;
}

bool Json_schema_ex_minimum::validate(const json_engine_t *je)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
    return false;

  double val= je->s.cs->strntod((char *) je->value,
                                je->value_len, &end, &err);
  return (val > ex_min) ? false : true;
}

bool Json_schema_ex_minimum::handle_keyword(THD *thd, json_engine_t *je,
                                            const char* key_start,
                                            const char* key_end,
                                            List<Json_schema_keyword>
                                                 *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "exclusiveMinimum");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  ex_min= val;

  return false;
}

bool Json_schema_ex_maximum::validate(const json_engine_t *je)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
    return false;

  double val= je->s.cs->strntod((char *) je->value,
                                  je->value_len, &end, &err);
  return (val < ex_max) ? false : true;
}

bool Json_schema_ex_maximum::handle_keyword(THD *thd, json_engine_t *je,
                                            const char* key_start,
                                            const char* key_end,
                                            List<Json_schema_keyword>
                                                 *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "exclusiveMaximum");
    return true;
  }
  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  ex_max= val;

  return false;
}

bool Json_schema_multiple_of::validate(const json_engine_t *je)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
    return false;

  double val= je->s.cs->strntod((char *) je->value,
                                  je->value_len, &end, &err);
  double temp= val / this->multiple_of;
  bool res= (temp - (long long int)temp) == 0;

  return !res;
}

bool Json_schema_multiple_of::handle_keyword(THD *thd, json_engine_t *je,
                                             const char* key_start,
                                             const char* key_end,
                                             List<Json_schema_keyword>
                                                  *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "multipleOf");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "multipleOf");
  multiple_of= val;

  return false;
}


bool Json_schema_max_len::validate(const json_engine_t *je)
{
  if (je->value_type != JSON_VALUE_STRING)
    return false;
  return (uint)(je->value_len) <= max_len ? false : true;
}

bool Json_schema_max_len::handle_keyword(THD *thd, json_engine_t *je,
                                         const char* key_start,
                                         const char* key_end,
                                         List<Json_schema_keyword>
                                             *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxLength");
    return true;
  }
  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
   my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxLength");
  max_len= (int)val;

  return false;
}

bool Json_schema_min_len::validate(const json_engine_t *je)
{
  if (je->value_type != JSON_VALUE_STRING)
    return false;
  return (uint)(je->value_len) >= min_len ? false : true;
}

bool Json_schema_min_len::handle_keyword(THD *thd, json_engine_t *je,
                                         const char* key_start,
                                         const char* key_end,
                                         List<Json_schema_keyword>
                                              *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minLength");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minLength");
  min_len= (int)val;

  return false;
}

bool Json_schema_pattern::validate(const json_engine_t *je)
{
  bool pattern_matches= false;

  if (je->value_type != JSON_VALUE_STRING)
    return false;

  str->str_value.set_or_copy_aligned((const char*)je->value,
                                     (size_t)je->value_len, je->s.cs);

  if (re.recompile(pattern))
    return true;
  if (re.exec(str, 0, 0))
    return true;
  pattern_matches= re.match();

  return pattern_matches ? false : true;
}

bool Json_schema_pattern::handle_keyword(THD *thd, json_engine_t *je,
                                         const char* key_start,
                                         const char* key_end,
                                         List<Json_schema_keyword>
                                              *all_keywords)
{
  if (je->value_type != JSON_VALUE_STRING)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "pattern");
    return true;
  }

  my_repertoire_t repertoire= my_charset_repertoire(je->s.cs);
  pattern= thd->make_string_literal((const char*)je->value,
                                             je->value_len, repertoire);
  str= (Item_string*)current_thd->make_string_literal((const char*)"",
                                               0, repertoire);
  re.init(je->s.cs, 0);

  return false;
}

bool Json_schema_max_items::validate(const json_engine_t *je)
{
  uint count= 0;
  json_engine_t curr_je;

  curr_je= *je;

  if (curr_je.value_type != JSON_VALUE_ARRAY)
    return false;

  int level= curr_je.stack_p;
  while(json_scan_next(&curr_je)==0 && level <= curr_je.stack_p)
  {
    if (json_read_value(&curr_je))
      return true;
    count++;
    if (!json_value_scalar(&curr_je))
    {
      if (json_skip_level(&curr_je))
        return true;
    }
  }
  return count > max_items ? true : false;
}

bool Json_schema_max_items::handle_keyword(THD *thd, json_engine_t *je,
                                           const char* key_start,
                                           const char* key_end,
                                           List<Json_schema_keyword>
                                                *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxItems");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
   my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxItems");
  max_items= (int)val;

  return false;
}

bool Json_schema_min_items::validate(const json_engine_t *je)
{
  uint count= 0;
  json_engine_t  curr_je;

  curr_je= *je;

  if (curr_je.value_type != JSON_VALUE_ARRAY)
    return false;

  int level= curr_je.stack_p;
  while(json_scan_next(&curr_je)==0 && level <= curr_je.stack_p)
  {
    if (json_read_value(&curr_je))
      return true;
    count++;
    if (!json_value_scalar(&curr_je))
    {
      if (json_skip_level(&curr_je))
        return true;
    }
  }
  return count < min_items ? true : false;
}

bool Json_schema_min_items::handle_keyword(THD *thd, json_engine_t *je,
                                           const char* key_start,
                                           const char* key_end,
                                           List<Json_schema_keyword>
                                                *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minItems");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxLength");
  min_items= (int)val;

  return false;
}

bool Json_schema_contains::validate(const json_engine_t *je)
{
  int level= je->stack_p;
  uint contains=0;
  json_engine_t curr_je;

  curr_je= *je;

  if (curr_je.value_type != JSON_VALUE_ARRAY)
    return false;

  if (contains_type == JSON_VALUE_UNINITIALIZED)
    return false;

  while(json_scan_next(&curr_je)==0 && level <= curr_je.stack_p)
  {
    if (json_read_value(&curr_je))
      return true;
    if ((1 << curr_je.value_type) & contains_type)
      contains++;
    if (!json_value_scalar(&curr_je))
    {
      if (json_skip_level(&curr_je))
        return true;
    }
  }
  if ((contains_flag & HAS_MAX_CONTAINS ?
       contains <= max_contains : true) &&
      (contains_flag & HAS_MIN_CONTAINS ?
       contains >= min_contains : true))
    return false;
  return true;
}

bool Json_schema_contains::handle_keyword(THD *thd, json_engine_t *je,
                                          const char* key_start,
                                          const char* key_end,
                                          List<Json_schema_keyword>
                                               *all_keywords)
{
  int key_len= (int)(key_end-key_start);
  if (json_key_equals((const char*)key_start,
                      { STRING_WITH_LEN("contains") }, key_len))
  {
    const uchar *k_end, *k_start;
    if (je->value_type != JSON_VALUE_OBJECT)
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "contains");
      return true;
    }
    if (json_scan_next(je))
      return true;

    k_start= je->s.c_str;
    do
    {
      k_end= je->s.c_str;
    } while (json_read_keyname_chr(je) == 0);

    if (json_read_value(je))
     return true;
    if (je->value_type != JSON_VALUE_STRING ||
        !json_key_equals((const char*)k_start,
                        { STRING_WITH_LEN("type") }, (int)(k_end-k_start)))
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "contains");
      return true;
    }
    else
    {
      if (json_assign_type(&contains_type, je))
        return true;
    }
  }
  else if (json_key_equals((const char*)key_start,
                      { STRING_WITH_LEN("maxContains") }, key_len))
  {
    int err= 0;
    char *end;

    if (je->value_type != JSON_VALUE_NUMBER)
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxContains");
      return true;
    }

    double val= je->s.cs->strntod((char *) je->value,
                                   je->value_len, &end, &err);
    max_contains= (int)val;
    contains_flag|= HAS_MAX_CONTAINS;
  }
  else if (json_key_equals((const char*)key_start,
                      { STRING_WITH_LEN("minContains") }, key_len))
  {
    int err= 0;
    char *end;

    if (je->value_type != JSON_VALUE_NUMBER)
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minContains");
      return true;
    }

    double val= je->s.cs->strntod((char *) je->value,
                                   je->value_len, &end, &err);
    min_contains= (int)val;
    contains_flag|= HAS_MIN_CONTAINS;
  }
  return false;
}

bool Json_schema_items_details::validate(const json_engine_t *je)
{
  int level= je->stack_p;
  json_engine_t curr_je= *je;
  List_iterator <List<Json_schema_keyword>> it1 (prefix_items);
  List<Json_schema_keyword> *curr_prefix;

  if (curr_je.value_type != JSON_VALUE_ARRAY)
    return false;

  while(json_scan_next(&curr_je)==0 && curr_je.stack_p >= level)
  {
    if (json_read_value(&curr_je))
      return true;
    if (item_type != JSON_VALUE_UNINITIALIZED)
    {
      if (!((1 << curr_je.value_type) & item_type))
        return true;
    }
    if (!(curr_prefix=it1++))
    {
      if (!allow_extra_items)
        return true;
      if (!json_value_scalar(&curr_je))
      {
        if (json_skip_level(&curr_je))
         return true;
      }
    }
    if (curr_prefix)
    {
      List_iterator<Json_schema_keyword> it2(*curr_prefix);
      Json_schema_keyword *curr_keyword= NULL;
      while ((curr_keyword=it2++))
      {
        if (curr_keyword->validate(&curr_je))
          return true;
      }
    }
  }
  return false;
}

bool Json_schema_items_details::handle_keyword(THD *thd, json_engine_t *je,
                                               const char* key_start,
                                               const char* key_end,
                                               List<Json_schema_keyword>
                                                   *all_keywords)
{
  int key_len= (int)(key_end-key_start);

  if (json_key_equals((const char*)key_start,
                      { STRING_WITH_LEN("prefixItems") }, (key_len)))
  {
    if (je->value_type != JSON_VALUE_ARRAY)
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "prefixItems");
      return true;
    }

    int level= je->stack_p;
    while(json_scan_next(je)==0 && je->stack_p >= level)
    {
      json_engine_t temp_je;
      char *begin, *end;
      int len;

      if (json_read_value(je))
        return true;
      if (je->value_type != JSON_VALUE_OBJECT)
      {
       my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "items");
      }
      begin= (char*)je->value;

      if (json_skip_level(je))
        return true;

      end= (char*)je->s.c_str;
      len= (int)(end-begin);

      json_scan_start(&temp_je, je->s.cs, (const uchar *) begin,
                      (const uchar *)begin+len);
      List<Json_schema_keyword> *keyword_list=
                        new (thd->mem_root) List<Json_schema_keyword>;

      if (!keyword_list)
        return true;
      if (create_object_and_handle_keyword(thd, &temp_je, keyword_list,
                                           all_keywords))
        return true;

      prefix_items.push_back(keyword_list);
    }
  }
  else if (json_key_equals((const char*)key_start,
                      { STRING_WITH_LEN("items") }, key_len))
  {
    if (je->value_type == JSON_VALUE_FALSE)
      allow_extra_items= false;
    else if (je->value_type == JSON_VALUE_TRUE)
      allow_extra_items= true;
    else if (je->value_type == JSON_VALUE_OBJECT)
    {
      int k_len;
      const uchar *k_end, *k_start;

      if (json_scan_next(je))
        return true;

      k_start= je->s.c_str;
      do
      {
        k_end= je->s.c_str;
      } while (json_read_keyname_chr(je) == 0);

      k_len= (int)(k_end-k_start);
      if (json_key_equals((const char*)k_start,
                      { STRING_WITH_LEN("type") }, k_len))
      {
        if (json_read_value(je))
          return true;
        if (json_assign_type(&item_type, je))
          return true;
      }
    }
    else
    {
      my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "items");
      return true;
    }
  }
  else
    return true;
  return false;
}

bool Json_schema_unique_items::validate(const json_engine_t *je)
{
  HASH unique_items;
  List <char> norm_str_list;
  json_engine_t curr_je= *je;
  int res= true, level= curr_je.stack_p;

  if (curr_je.value_type != JSON_VALUE_ARRAY)
    return false;

  if (my_hash_init(PSI_INSTRUMENT_ME, &unique_items, curr_je.s.cs,
                   1024, 0, 0, (my_hash_get_key) get_key_name, NULL, 0))
    return true;

  while(json_scan_next(&curr_je)==0 && level <= curr_je.stack_p)
  {
    int has_none= 0, has_true= 2, has_false= 4, has_null= 8, err= 1;
    char *norm_str;
    String a_res("", 0, curr_je.s.cs);

    if (json_read_value(&curr_je))
      goto end;

    json_get_normalized_string(&curr_je, &a_res, &err);

    if (err)
      goto end;

    norm_str= (char*)malloc(a_res.length()+1);
    if (!norm_str)
      goto end;

    norm_str[a_res.length()]= '\0';
    strncpy(norm_str, (const char*)a_res.ptr(), a_res.length());
    norm_str_list.push_back(norm_str);

    if (curr_je.value_type == JSON_VALUE_TRUE)
    {
      if (has_none & has_true)
        goto end;
      has_none= has_none | has_true;
    }
    else if (curr_je.value_type == JSON_VALUE_FALSE)
    {
      if (has_none & has_false)
        goto end;
      has_none= has_none | has_false;
    }
    else if (curr_je.value_type == JSON_VALUE_NULL)
    {
      if (has_none & has_null)
        goto end;
      has_none= has_none | has_null;
    }
    else
    {
      if (!my_hash_search(&unique_items, (uchar*)norm_str,
                          strlen(((const char*)norm_str))))
      {
        if (my_hash_insert(&unique_items, (uchar*)norm_str))
          goto end;
      }
      else
        goto end;
    }
    a_res.set("", 0, curr_je.s.cs);
  }
  res= false;
  end:
  if (!norm_str_list.is_empty())
  {
    List_iterator<char> it(norm_str_list);
    char *curr_norm_str;
    while ((curr_norm_str= it++))
      free(curr_norm_str);
    norm_str_list.empty();
  }
  my_hash_free(&unique_items);
  return res;
}

bool Json_schema_unique_items::handle_keyword(THD *thd, json_engine_t *je,
                                              const char* key_start,
                                              const char* key_end,
                                              List<Json_schema_keyword>
                                                  *all_keywords)
{
  if (je->value_type == JSON_VALUE_TRUE)
    is_unique= true;
  else if (je->value_type == JSON_VALUE_FALSE)
    is_unique= false;
  else
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "uniqueItems");
    return true;
  }
  return false;
}

bool Json_schema_max_prop::validate(const json_engine_t *je)
{
  uint properties_count= 0;
  json_engine_t curr_je= *je;
  int curr_level= je->stack_p;

  if (curr_je.value_type != JSON_VALUE_OBJECT)
    return false;

  while (json_scan_next(&curr_je)== 0 && je->stack_p >= curr_level)
  {
    switch (curr_je.state)
    {
      case JST_KEY:
      {
        do
        {
        } while (json_read_keyname_chr(&curr_je) == 0);
        properties_count++;

        if (json_read_value(&curr_je))
          return true;

        if (!json_value_scalar(&curr_je))
        {
          if (json_skip_level(&curr_je))
            return true;
        }
      }
    }
  }
  return properties_count > max_prop ? true : false;
}

bool Json_schema_max_prop::handle_keyword(THD *thd, json_engine_t *je,
                                          const char* key_start,
                                          const char* key_end,
                                          List<Json_schema_keyword>
                                               *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxProperties");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "maxProperties");
  max_prop= (int)val;

  return false;
}

bool Json_schema_min_prop::validate(const json_engine_t *je)
{
  uint properties_count= 0;
  int curr_level= je->stack_p;
  json_engine_t curr_je= *je;

  if (curr_je.value_type != JSON_VALUE_OBJECT)
    return false;

  while (json_scan_next(&curr_je)== 0 && je->stack_p >= curr_level)
  {
    switch (curr_je.state)
    {
      case JST_KEY:
      {
        do
        {
        } while (json_read_keyname_chr(&curr_je) == 0);
        properties_count++;

        if (json_read_value(&curr_je))
          return true;

        if (!json_value_scalar(&curr_je))
        {
          if (json_skip_level(&curr_je))
            return true;
        }
      }
    }
  }
  return properties_count < min_prop ? true : false;
}

bool Json_schema_min_prop::handle_keyword(THD *thd, json_engine_t *je,
                                          const char* key_start,
                                          const char* key_end,
                                          List<Json_schema_keyword>
                                              *all_keywords)
{
  int err= 0;
  char *end;

  if (je->value_type != JSON_VALUE_NUMBER)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minProperties");
    return true;
  }

  double val= je->s.cs->strntod((char *) je->value,
                                 je->value_len, &end, &err);
  if (val < 0)
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "minProperties");
  min_prop= (int)val;

  return false;
}

bool Json_schema_required::validate(const json_engine_t *je)
{
  json_engine_t curr_je= *je;
  List<char> malloc_mem_list;
  HASH required;
  int res= true, curr_level= curr_je.stack_p;
  List_iterator<String> it(required_properties);
  String *curr_str;

  if (curr_je.value_type != JSON_VALUE_OBJECT)
    return false;

  if(my_hash_init(PSI_INSTRUMENT_ME, &required,
               curr_je.s.cs, 1024, 0, 0, (my_hash_get_key) get_key_name,
               NULL, 0))
      return true;
  while (json_scan_next(&curr_je)== 0 && curr_je.stack_p >= curr_level)
  {
    switch (curr_je.state)
    {
      case JST_KEY:
      {
        const uchar *key_end, *key_start;
        int key_len;
        char *str;

        key_start= curr_je.s.c_str;
        do
        {
          key_end= curr_je.s.c_str;
        } while (json_read_keyname_chr(&curr_je) == 0);

        key_len= (int)(key_end-key_start);
        str= (char*)malloc((size_t)(key_len)+1);
        strncpy(str, (const char*)key_start, key_len);
        str[key_len]='\0';

        if (my_hash_insert(&required, (const uchar*)str))
          goto error;
        malloc_mem_list.push_back(str);
      }
    }
  }
  while ((curr_str= it++))
  {
    if (!my_hash_search(&required, (const uchar*)curr_str->ptr(),
                        curr_str->length()))
      goto error;
  }
  res= false;
  error:
  if (!malloc_mem_list.is_empty())
  {
    List_iterator<char> it(malloc_mem_list);
    char *curr_ptr;
    while ((curr_ptr= it++))
      free(curr_ptr);
    malloc_mem_list.empty();
  }
  my_hash_free(&required);
  return res;
}

bool Json_schema_required::handle_keyword(THD *thd, json_engine_t *je,
                                          const char* key_start,
                                          const char* key_end,
                                          List<Json_schema_keyword>
                                               *all_keywords)
{
  int level= je->stack_p;

  if (je->value_type != JSON_VALUE_ARRAY)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "required");
    return true;
  }
  while(json_scan_next(je)==0 && level <= je->stack_p)
  {
    if (json_read_value(je))
      return true;
    else
    {
      String *str= new (thd->mem_root)String((char*)je->value,
                                                    je->value_len, je->s.cs);
      this->required_properties.push_back(str);
    }
  }
  return false;
}

bool Json_schema_dependent_prop::validate(const json_engine_t *je)
{
  json_engine_t curr_je= *je;
  HASH properties;
  bool res= true;
  int curr_level= curr_je.stack_p;
  List <char> malloc_mem_list;
  List_iterator<st_dependent_keywords> it(dependent_required);
  st_dependent_keywords *curr_keyword= NULL;

  if (curr_je.value_type != JSON_VALUE_OBJECT)
    return false;

  if (my_hash_init(PSI_INSTRUMENT_ME, &properties,
                 curr_je.s.cs, 1024, 0, 0, (my_hash_get_key) get_key_name,
                 NULL, 0))
    return true;

  while (json_scan_next(&curr_je)== 0 && curr_je.stack_p >= curr_level)
  {
    switch (curr_je.state)
    {
      case JST_KEY:
      {
        const uchar *key_end, *key_start;
        int key_len;
        char *str;

        key_start= curr_je.s.c_str;
        do
        {
          key_end= curr_je.s.c_str;
        } while (json_read_keyname_chr(&curr_je) == 0);

        key_len= (int)(key_end-key_start);
        str= (char*)malloc((size_t)(key_len)+1);
        strncpy(str, (const char*)key_start, key_len);
        str[(int)(key_end-key_start)]='\0';

        if (my_hash_insert(&properties, (const uchar*)str))
          goto error;
        malloc_mem_list.push_back(str);
      }
    }
  }
  while ((curr_keyword= it++))
  {
    if (my_hash_search(&properties,
                       (const uchar*)curr_keyword->property->ptr(),
                          curr_keyword->property->length()))
    {
      List_iterator<String> it2(curr_keyword->dependents);
      String *curr_depended_keyword;
      while ((curr_depended_keyword= it2++))
      {
        if (!my_hash_search(&properties,
                            (const uchar*)curr_depended_keyword->ptr(),
                            curr_depended_keyword->length()))
        {
          goto error;
        }
      }
    }
  }
  res= false;

  error:
  my_hash_free(&properties);
  if (!malloc_mem_list.is_empty())
  {
    List_iterator<char> it(malloc_mem_list);
    char *curr_ptr;
    while ((curr_ptr= it++))
      free(curr_ptr);
    malloc_mem_list.empty();
  }
  return res;
}

bool Json_schema_dependent_prop::handle_keyword(THD *thd, json_engine_t *je,
                                                const char* key_start,
                                                const char* key_end,
                                                List<Json_schema_keyword>
                                                    *all_keywords)
{
  if (je->value_type == JSON_VALUE_OBJECT)
  {
    int level1= je->stack_p;
    while (json_scan_next(je)==0 && level1 <= je->stack_p)
    {
      switch(je->state)
      {
        case JST_KEY:
        {
          const uchar *k_end, *k_start;
          int k_len;

          k_start= je->s.c_str;
          do
          {
            k_end= je->s.c_str;
          } while (json_read_keyname_chr(je) == 0);

          k_len= (int)(k_end-k_start);
          if (json_read_value(je))
            return true;

          if (je->value_type == JSON_VALUE_ARRAY)
          {
            st_dependent_keywords *curr_dependent_keywords=
                    (st_dependent_keywords *) alloc_root(thd->mem_root,
                                                sizeof(st_dependent_keywords));

            if (curr_dependent_keywords)
            {
              curr_dependent_keywords->property=
                          new (thd->mem_root)String((char*)k_start,
                                                     k_len, je->s.cs);
              curr_dependent_keywords->dependents.empty();
              int level2= je->stack_p;
              while (json_scan_next(je)==0 && level2 <= je->stack_p)
              {
                if (json_read_value(je) || je->value_type != JSON_VALUE_STRING)
                {
                  my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0),
                           "dependentRequired");
                  return true;
                }
                else
                {
                  String *str=
                    new (thd->mem_root)String((char*)je->value,
                                                  je->value_len, je->s.cs);
                  curr_dependent_keywords->dependents.push_back(str);
                }
              }
              dependent_required.push_back(curr_dependent_keywords);
            }
            else
            {
              return true;
            }
          }
          else
          {
            my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0),
                     "dependentRequired");
            return true;
          }
        }
      }
    }
  }
  else
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "dependentRequired");
    return true;
  }
  return false;
}

bool Json_schema_properties::validate(const json_engine_t *je)
{
  json_engine_t curr_je= *je;

  if (curr_je.value_type != JSON_VALUE_OBJECT)
    return false;

  int level= curr_je.stack_p;
  while (json_scan_next(&curr_je)==0 && level <= curr_je.stack_p)
  {
    switch(curr_je.state)
    {
      case JST_KEY:
      {
        const uchar *k_end, *k_start= curr_je.s.c_str;
        do
        {
          k_end= curr_je.s.c_str;
        } while (json_read_keyname_chr(&curr_je) == 0);

        if (json_read_value(&curr_je))
          return true;

        st_property *curr_property= NULL;
        if ((curr_property=
                      (st_property*)my_hash_search(&properties,
                                                   (const uchar*)k_start,
                                                    (size_t)(k_end-k_start))))
        {
          List_iterator<Json_schema_keyword> it(*(curr_property->curr_schema));
          Json_schema_keyword *curr_keyword= NULL;

          while ((curr_keyword=it++))
          {
            if (curr_keyword->validate(&curr_je))
            {
              return true;
            }
          }
        }
        else
        {
          if (json_value_scalar(&curr_je))
          {
            if (json_skip_level(&curr_je))
              return true;
          }
        }
      }
    }
  }

  return false;
}

bool Json_schema_properties::handle_keyword(THD *thd, json_engine_t *je,
                                            const char* key_start,
                                            const char* key_end,
                                            List<Json_schema_keyword>
                                                 *all_keywords)
{
  if (je->value_type != JSON_VALUE_OBJECT)
  {
    my_error(ER_JSON_INVALID_VALUE_FOR_KEYWORD, MYF(0), "properties");
    return true;
  }
  if (my_hash_init(PSI_INSTRUMENT_ME,
                     &this->properties,
                     je->s.cs, 1024, 0, 0,
                     (my_hash_get_key) get_key_name_for_property,
                     NULL, 0))
      return true;

  int level= je->stack_p;
  while (json_scan_next(je)==0 && level <= je->stack_p)
  {
    switch(je->state)
    {
      case JST_KEY:
      {
        const uchar *k_end, *k_start= je->s.c_str;
        do
        {
          k_end= je->s.c_str;
        } while (json_read_keyname_chr(je) == 0);

        if (json_read_value(je))
          return true;

        st_property *curr_property=
                         (st_property*)alloc_root(thd->mem_root,
                                                   sizeof(st_property));
        if (curr_property)
        {
          curr_property->key_name= (char*)alloc_root(thd->mem_root,
                                                   (size_t)(k_end-k_start)+1);
          curr_property->curr_schema=
                       new (thd->mem_root) List<Json_schema_keyword>;
          if (curr_property->key_name)
          {
            curr_property->key_name[(int)(k_end-k_start)]= '\0';
            strncpy((char*)curr_property->key_name, (const char*)k_start,
                    (size_t)(k_end-k_start));
            if (create_object_and_handle_keyword(thd, je,
                                             curr_property->curr_schema,
                                             all_keywords))
              return true;
            if (my_hash_insert(&properties, (const uchar*)curr_property))
              return true;
          }
        }
      }
    }
  }
  return false;
}



Json_schema_keyword* create_object(THD *thd, json_engine_t *je,
                                   Json_schema_keyword *curr_keyword,
                                   const uchar* key_start, const uchar* key_end)
{
  int key_len= (int)(key_end-key_start);
  const char *key_name= (const char*)key_start;

  if (json_key_equals(key_name, { STRING_WITH_LEN("type") },
                      key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_type();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("const") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_const();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("enum") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_enum();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("maximum") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_maximum();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("minimum") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_minimum();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("exclusiveMaximum") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_ex_maximum();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("exclusiveMinimum") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_ex_minimum();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("multipleOf") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_multiple_of();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("maxLength") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_max_len();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("minLength") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_min_len();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("pattern") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_pattern();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("maxItems") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_max_items();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("minItems") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_min_items();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("contains") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("maxContains") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("minContains") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_contains();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("prefixItems") },
                           key_len) ||
          json_key_equals(key_name, { STRING_WITH_LEN("items") },
                          key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_items_details();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("uniqueItems") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_unique_items();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("properties") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_properties();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("maxProperties") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_max_prop();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("minProperties") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_min_prop();
  }
  else if(json_key_equals(key_name, { STRING_WITH_LEN("required") },
                          key_len))
  {
   curr_keyword= new (thd->mem_root) Json_schema_required();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("dependentRequired") },
                           key_len))
  {
   curr_keyword= new (thd->mem_root) Json_schema_dependent_prop();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("title") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("description") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("$comment") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("$schema") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("deprecated") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("readOnly") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("writeOnly") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("example") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("default") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_annotation();
  }
  else if (json_key_equals(key_name, { STRING_WITH_LEN("date-time") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("date") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("time") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("duration") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("email") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("idn-email") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("hostname") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("idn-hostname") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("ipv4") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("ipv6") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("uri") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("uri-reference") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("iri") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("iri-reference") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("uuid") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("json-pointer") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("relative-json-pointer") },
                           key_len) ||
           json_key_equals(key_name, { STRING_WITH_LEN("regex") },
                           key_len))
  {
    curr_keyword= new (thd->mem_root) Json_schema_format();
  }
  else
  {
    curr_keyword= new (thd->mem_root) Json_schema_keyword();
  }
  return curr_keyword;
}

bool create_object_and_handle_keyword(THD *thd, json_engine_t *je,
                                      List<Json_schema_keyword> *keyword_list,
                                      List<Json_schema_keyword> *all_keywords)
{
  int level= je->stack_p;
  Json_schema_keyword *contains= NULL, *items_details= NULL;

  DBUG_EXECUTE_IF("json_check_min_stack_requirement",
                  {
                    long arbitrary_var;
                    long stack_used_up=
                         (available_stack_size(thd->thread_stack,
                                               &arbitrary_var));
                    ALLOCATE_MEM_ON_STACK(my_thread_stack_size-stack_used_up-STACK_MIN_SIZE);
                  });
  if (check_stack_overrun(thd, STACK_MIN_SIZE , NULL))
    return 1;

  while (json_scan_next(je)== 0 && je->stack_p >= level)
  {
    switch(je->state)
    {
      case JST_KEY:
      {
        const uchar *key_end, *key_start;
        int key_len;

        key_start= je->s.c_str;
        do
        {
          key_end= je->s.c_str;
        } while (json_read_keyname_chr(je) == 0);

        if (json_read_value(je))
         return true;

        key_len= (int)(key_end-key_start);;
        Json_schema_keyword *curr_keyword= NULL;
        if (json_key_equals((const char*)key_start,
                              { STRING_WITH_LEN("contains") }, key_len) ||
            json_key_equals((const char*)key_start,
                              { STRING_WITH_LEN("maxContains") }, key_len) ||
            json_key_equals((const char*)key_start,
                              { STRING_WITH_LEN("minContains") }, key_len))
        {
          if (!contains)
          {
            curr_keyword= create_object(thd, je, curr_keyword, key_start,
                                        key_end);
            if (keyword_list)
              keyword_list->push_back(curr_keyword);
            if (all_keywords)
              all_keywords->push_back(curr_keyword);
            contains= curr_keyword;
          }
          else
          {
            curr_keyword= contains;
          }
        }
        else if (json_key_equals((const char*)key_start,
                              { STRING_WITH_LEN("items") }, key_len) ||
            json_key_equals((const char*)key_start,
                              { STRING_WITH_LEN("prefixItems") }, key_len))
        {
          if (!items_details)
          {
            curr_keyword= create_object(thd, je, curr_keyword, key_start,
                                        key_end);
            if (keyword_list)
              keyword_list->push_back(curr_keyword);
            if (all_keywords)
              all_keywords->push_back(curr_keyword);
            items_details= curr_keyword;
          }
          else
          {
           curr_keyword= items_details;
          }
        }
        else
        {
          curr_keyword= create_object(thd, je, curr_keyword, key_start,
                                      key_end);
          if (keyword_list)
            keyword_list->push_back(curr_keyword);
          if (all_keywords)
            all_keywords->push_back(curr_keyword);
        }
        curr_keyword->handle_keyword(thd, je,
                                     (const char*)key_start,
                                     (const char*)key_end, all_keywords);
        break;
      }
    }
  }
  return false;
}

uchar* get_key_name_for_property(const char *key_name, size_t *length,
                    my_bool /* unused */)
{
  st_property * curr_property= (st_property*)(key_name);

  *length= strlen(curr_property->key_name);
  return (uchar*) curr_property->key_name;
}
