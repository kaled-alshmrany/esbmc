/*******************************************************************\

Module: Symbolic Execution - Stack tracking

Authors: Rafael Menezes and Lucas Cordeiro

Date: April 2020

\*******************************************************************/

#include <cassert>
#include <goto-symex/goto_symex.h>
#include <util/expr_util.h>
#include <util/irep2.h>

void goto_symext::process_stack_size(expr2tc &expr)
{
  const code_decl2t &decl_code = to_code_decl2t(expr);
  cur_state->top().frame_info.add_decl(decl_code);
  if(stack_limit != 0)
  {
    type_poolt s_type(true);
    BigInt f_size(cur_state->top().frame_info.total);
    BigInt s_size(stack_limit);
    constant_int2tc function_irep2(s_type.get_uint64(), f_size);
    constant_int2tc limit_irep2(s_type.get_uint64(), s_size);

    lessthanequal2tc check(function_irep2, limit_irep2);
    claim(check, "Stack limit was violated");
  }
}
