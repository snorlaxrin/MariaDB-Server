#pragma once

/* Copyright (c) 2020, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1335  USA */

#include "sql_class.h"
#include "item.h"
#include "item_row.h"

class Item_period: public Item_row
{
public:
  Item_period(THD *thd, const ref_initializer_list<Item> &list)
    : Item_row(thd, list){}

  Type type() const override { return PERIOD_ITEM; }
};

class Item_func_overlaps: public Item_bool_func // TODO Item_bool_func2
{
public:
  Item_func_overlaps(THD *thd, Item_period *left, Item_period *right)
  : Item_bool_func(thd, left, right)
  {

  }

  longlong val_int()
  {

  }
};
