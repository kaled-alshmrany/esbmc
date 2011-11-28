/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <assert.h>

#include <arith_tools.h>

#include "boolbv.h"

/*******************************************************************\

Function: boolbvt::convert_byte_update

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void boolbvt::convert_byte_update(const exprt &expr, bvt &bv)
{
  if(expr.operands().size()!=3)
    throw "byte_update takes three operands";
    
  const exprt &op0=expr.op0();
  const exprt &op1=expr.op1();
  const exprt &op2=expr.op2();

  bool little_endian;
  
  if(expr.id()=="byte_update_little_endian")
    little_endian=true;
  else if(expr.id()=="byte_update_big_endian")
    little_endian=false;
  else
    assert(false);

  convert_bv(op0, bv);

  bvt op2_bv;

  convert_bv(op2, op2_bv);

  unsigned update_width=op2_bv.size();

  // see if the byte number is constant

  mp_integer index;
  if(!to_integer(op1, index))
  {
    mp_integer offset;
    
    if(little_endian)
      offset=index*update_width;
    else
      offset=(mp_integer(bv.size()/update_width)-index-1)*update_width;

    if(mp_integer(bv.size())<offset+update_width || offset<0)
    {
      // out of bounds
    }
    else
    {
      for(unsigned i=0; i<update_width; i++)
        bv[integer2long(offset+i)]=op2_bv[i];
    }

    return;
  }

  // byte_update with variable not yet implemented...
}
