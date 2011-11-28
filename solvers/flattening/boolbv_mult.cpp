/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <std_types.h>

#include "boolbv.h"

#ifdef HAVE_FLOATBV
#include "../floatbv/float_utils.h"
#endif

/*******************************************************************\

Function: boolbvt::convert_mult

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void boolbvt::convert_mult(const exprt &expr, bvt &bv)
{
  unsigned width;
  if(boolbv_get_width(expr.type(), width))
    return conversion_failed(expr, bv);

  bv.resize(width);

  const exprt::operandst &operands=expr.operands();
  if(operands.size()==0)
    throw "mult without operands";

  const exprt &op0=expr.op0();

  bool no_overflow=expr.id()=="no-overflow-mult";
  
  if(expr.type().id()=="fixedbv")
  {
    if(op0.type()!=expr.type())
      throw "multiplication with mixed types";
    
    convert_bv(op0, bv);

    if(bv.size()!=width)
      throw "convert_mult: unexpected operand width";

    unsigned fraction_bits=
      to_fixedbv_type(expr.type()).get_fraction_bits();
             
    // do a sign extension by fraction_bits bits
    bv=bv_utils.sign_extension(bv, bv.size()+fraction_bits);
      
    for(exprt::operandst::const_iterator it=operands.begin()+1;
        it!=operands.end(); it++)
    {
      bvt op;

      if(it->type()!=expr.type())
        throw "multiplication with mixed types";

      convert_bv(*it, op);

      if(op.size()!=width)
        throw "convert_mult: unexpected operand width";

      op=bv_utils.sign_extension(op, bv.size());

      bv=bv_utils.signed_multiplier(bv, op);
    }
    
    // cut it down again
    bv.erase(bv.begin(), bv.begin()+fraction_bits);

    return;
  }
  else if(expr.type().id()=="floatbv")
  {
    #ifdef HAVE_FLOATBV
    if(op0.type()!=expr.type())
      throw "multiplication with mixed types";
    
    convert_bv(op0, bv);

    if(bv.size()!=width)
      throw "convert_mult: unexpected operand width";

    float_utilst float_utils(prop);
    float_utils.spec=to_floatbv_type(expr.type());

    for(exprt::operandst::const_iterator it=operands.begin()+1;
        it!=operands.end(); it++)
    {
      bvt op;

      if(it->type()!=expr.type())
        throw "multiplication with mixed types";

      convert_bv(*it, op);

      if(op.size()!=width)
        throw "convert_mult: unexpected operand width";

      bv=float_utils.mul(bv, op);
    }
    
    return;
    #endif
  }
  else if(expr.type().id()=="unsignedbv" ||
          expr.type().id()=="signedbv")
  {
    if(op0.type()!=expr.type())
      throw "multiplication with mixed types";
      
    bv_utilst::representationt rep=
      expr.type().id()=="signedbv"?bv_utilst::SIGNED:
                                   bv_utilst::UNSIGNED;
    
    convert_bv(op0, bv);

    if(bv.size()!=width)
      throw "convert_mult: unexpected operand width";
      
    for(exprt::operandst::const_iterator it=operands.begin()+1;
        it!=operands.end(); it++)
    {
      bvt op;

      if(it->type()!=expr.type())
        throw "multiplication with mixed types";

      convert_bv(*it, op);

      if(op.size()!=width)
        throw "convert_mult: unexpected operand width";

      if(no_overflow)
        bv=bv_utils.multiplier_no_overflow(bv, op, rep);
      else
        bv=bv_utils.multiplier(bv, op, rep);
    }    

    return;
  }
  
  conversion_failed(expr, bv);
}
