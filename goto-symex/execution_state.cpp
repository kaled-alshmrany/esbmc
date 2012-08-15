/*******************************************************************\

   Module:

   Author: Lucas Cordeiro, lcc08r@ecs.soton.ac.uk

\*******************************************************************/

#include <irep2.h>
#include <migrate.h>
#include "execution_state.h"
#include "reachability_tree.h"
#include <string>
#include <sstream>
#include <vector>
#include <i2string.h>
#include <string2array.h>
#include <std_expr.h>
#include <expr_util.h>
#include "../ansi-c/c_types.h"
#include <simplify_expr.h>
#include "config.h"

unsigned int execution_statet::node_count = 0;

execution_statet::execution_statet(const goto_functionst &goto_functions,
                                   const namespacet &ns,
                                   reachability_treet *art,
                                   symex_targett *_target,
                                   contextt &context,
                                   ex_state_level2t *l2init,
                                   const optionst &options) :
  goto_symext(ns, context, goto_functions, _target, options),
  owning_rt(art),
  state_level2(l2init)
{

  // XXXjmorse - C++s static initialization order trainwreck means
  // we can't initialize the id -> serializer map statically. Instead,
  // manually inspect and initialize. This is not thread safe.
  if (!execution_statet::expr_id_map_initialized) {
    execution_statet::expr_id_map_initialized = true;
    execution_statet::expr_id_map = init_expr_id_map();
  }

  CS_number = 0;
  TS_number = 0;
  node_id = 0;
  guard_execution = "execution_statet::\\guard_exec";
  interleaving_unviable = false;

  goto_functionst::function_mapt::const_iterator it =
    goto_functions.function_map.find("main");
  if (it == goto_functions.function_map.end())
    throw "main symbol not found; please set an entry point";

  const goto_programt *goto_program = &(it->second.body);

  // Initialize initial thread state
  goto_symex_statet state(*state_level2, global_value_set, ns);
  state.initialize((*goto_program).instructions.begin(),
             (*goto_program).instructions.end(),
             goto_program, 0);

  threads_state.push_back(state);
  cur_state = &threads_state.front();
  cur_state->global_guard.make_true();
  cur_state->global_guard.add(get_guard_identifier());

  atomic_numbers.push_back(0);

  if (DFS_traversed.size() <= state.source.thread_nr) {
    DFS_traversed.push_back(false);
  } else {
    DFS_traversed[state.source.thread_nr] = false;
  }

  exprs_read_write.push_back(read_write_set());
  thread_start_data.push_back(expr2tc());

  active_thread = 0;
  last_active_thread = 0;
  node_count = 0;
  nondet_count = 0;
  dynamic_counter = 0;
  DFS_traversed.reserve(1);
  DFS_traversed[0] = false;
}

execution_statet::execution_statet(const execution_statet &ex) :
  goto_symext(ex),
  owning_rt(ex.owning_rt),
  state_level2(ex.state_level2->clone())
{

  *this = ex;

  // Regenerate threads state using new objects state_level2 ref
  threads_state.clear();
  std::vector<goto_symex_statet>::const_iterator it;
  for (it = ex.threads_state.begin(); it != ex.threads_state.end(); it++) {
    goto_symex_statet state(*it, *state_level2, global_value_set);
    threads_state.push_back(state);
  }

  // Reassign which state is currently being worked on.
  cur_state = &threads_state[active_thread];
}

execution_statet&
execution_statet::operator=(const execution_statet &ex)
{
  // Don't copy level2, copy cons it in execution_statet(ref)
  //state_level2 = ex.state_level2;

  threads_state = ex.threads_state;
  atomic_numbers = ex.atomic_numbers;
  DFS_traversed = ex.DFS_traversed;
  exprs_read_write = ex.exprs_read_write;
  thread_start_data = ex.thread_start_data;
  last_global_read_write = ex.last_global_read_write;
  last_active_thread = ex.last_active_thread;
  active_thread = ex.active_thread;
  guard_execution = ex.guard_execution;
  nondet_count = ex.nondet_count;
  dynamic_counter = ex.dynamic_counter;
  node_id = ex.node_id;
  global_value_set = ex.global_value_set;
  interleaving_unviable = ex.interleaving_unviable;
  pre_goto_guard = ex.pre_goto_guard;

  CS_number = ex.CS_number;
  TS_number = ex.TS_number;

  // Vastly irritatingly, we have to iterate through existing level2t objects
  // updating their ex_state references. There isn't an elegant way of updating
  // them, it seems, while keeping the symex stuff ignorant of ex_state.
  // Oooooo, so this is where auto types would be useful...
  for (std::vector<goto_symex_statet>::iterator it = threads_state.begin();
       it != threads_state.end(); it++) {
    for (goto_symex_statet::call_stackt::iterator it2 = it->call_stack.begin();
         it2 != it->call_stack.end(); it2++) {
      for (goto_symex_statet::goto_state_mapt::iterator it3 = it2->goto_state_map.begin();
           it3 != it2->goto_state_map.end(); it3++) {
        for (goto_symex_statet::goto_state_listt::iterator it4 = it3->second.begin();
             it4 != it3->second.begin(); it4++) {
          ex_state_level2t &l2 = dynamic_cast<ex_state_level2t&>(it4->level2);
          l2.owner = this;
        }
      }
    }
  }

  state_level2->owner = this;

  return *this;
}

execution_statet::~execution_statet()
{
  delete state_level2;
};

void
execution_statet::symex_step(reachability_treet &art)
{

  statet &state = get_active_state();
  const goto_programt::instructiont &instruction = *state.source.pc;

  merge_gotos();

  if (config.options.get_option("break-at") != "") {
    unsigned int insn_num = strtol(config.options.get_option("break-at").c_str(), NULL, 10);
    if (instruction.location_number == insn_num) {
      // If you're developing ESBMC on a machine that isn't x86, I'll send you
      // cookies.
#ifndef _WIN32
      __asm__("int $3");
#else
      std::cerr << "Can't trap on windows, sorry" << std::endl;
      abort();
#endif
    }
  }

  if (options.get_bool_option("symex-trace")) {
    const goto_programt p_dummy;
    goto_functions_templatet<goto_programt>::function_mapt::const_iterator it =
      goto_functions.function_map.find(instruction.function);

    const goto_programt &p_real = it->second.body;
    const goto_programt &p = (it == goto_functions.function_map.end()) ? p_dummy : p_real;
    p.output_instruction(ns, "", std::cout, state.source.pc, false, false);
  }

  switch (instruction.type) {
    case END_FUNCTION:
      if (instruction.function == "main") {
        end_thread();
        art.force_cswitch_point();
      } else {
        // Fall through to base class
        goto_symext::symex_step(art);
      }
      break;
    case ATOMIC_BEGIN:
      state.source.pc++;
      increment_active_atomic_number();
      break;
    case ATOMIC_END:
      decrement_active_atomic_number();
      state.source.pc++;

      // Don't context switch if the guard is false. This instruction hasn't
      // actually been executed, so context switching achieves nothing. (We
      // don't do this for the active_atomic_number though, because it's cheap,
      // and should be balanced under all circumstances anyway).
      if (!state.guard.is_false())
        art.force_cswitch_point();

      break;
    case RETURN:
      state.source.pc++;
      if(!state.guard.is_false()) {
        expr2tc thecode = instruction.code, assign;
        if (make_return_assignment(assign, thecode)) {
          goto_symext::symex_assign(assign);
        }

        symex_return();

        if (!is_nil_expr(assign))
          owning_rt->analyse_for_cswitch_after_assign(assign);
      }
      break;
    default:
      goto_symext::symex_step(art);
  }

  return;
}

void
execution_statet::symex_assign(const expr2tc &code)
{
  pre_goto_guard = expr2tc();

  goto_symext::symex_assign(code);

  if (threads_state.size() > 1)
    owning_rt->analyse_for_cswitch_after_assign(code);

  return;
}

void
execution_statet::claim(const expr2tc &expr, const std::string &msg)
{
  pre_goto_guard = expr2tc();

  goto_symext::claim(expr, msg);

  if (threads_state.size() > 1)
    owning_rt->analyse_for_cswitch_after_read(expr);

  return;
}

void
execution_statet::symex_goto(const expr2tc &old_guard)
{
  pre_goto_guard = expr2tc();
  expr2tc pre_goto_state_guard = threads_state[active_thread].guard.as_expr();

  goto_symext::symex_goto(old_guard);

  if (!is_nil_expr(old_guard)) {
    if (threads_state.size() > 1) {
      if (owning_rt->analyse_for_cswitch_after_read(old_guard)) {
        // We're taking a context switch, store the state guard of this GOTO
        // instruction. i.e., a state guard that doesn't depend on the _result_
        // of the GOTO.
        pre_goto_guard = pre_goto_state_guard;
      }
    }
  }

  return;
}

void
execution_statet::assume(const expr2tc &assumption)
{
  pre_goto_guard = expr2tc();

  goto_symext::assume(assumption);

  if (threads_state.size() > 1)
    owning_rt->analyse_for_cswitch_after_read(assumption);

  return;
}

unsigned int &
execution_statet::get_dynamic_counter(void)
{

  return dynamic_counter;
}

unsigned int &
execution_statet::get_nondet_counter(void)
{

  return nondet_count;
}

goto_symex_statet &
execution_statet::get_active_state() {

  return threads_state.at(active_thread);
}

const goto_symex_statet &
execution_statet::get_active_state() const
{
  return threads_state.at(active_thread);
}

unsigned int
execution_statet::get_active_atomic_number()
{

  return atomic_numbers.at(active_thread);
}

void
execution_statet::increment_active_atomic_number()
{

  atomic_numbers.at(active_thread)++;
}

void
execution_statet::decrement_active_atomic_number()
{

  atomic_numbers.at(active_thread)--;
}

expr2tc
execution_statet::get_guard_identifier()
{

  return expr2tc(new symbol2t(type_pool.get_bool(), guard_execution,
                              symbol2t::level1, CS_number, 0, node_id, 0));
}

void
execution_statet::switch_to_thread(unsigned int i)
{

  last_active_thread = active_thread;
  active_thread = i;
  cur_state = &threads_state[active_thread];
}

bool
execution_statet::dfs_explore_thread(unsigned int tid)
{

    if(DFS_traversed.at(tid))
      return false;

    if(threads_state.at(tid).call_stack.empty())
      return false;

    if(threads_state.at(tid).thread_ended)
      return false;

    DFS_traversed.at(tid) = true;
    return true;
}

bool
execution_statet::check_if_ileaves_blocked(void)
{

  if(owning_rt->get_CS_bound() != -1 && CS_number >= owning_rt->get_CS_bound())
    return true;

  if (get_active_atomic_number() > 0)
    return true;

  const goto_programt::instructiont &insn = *get_active_state().source.pc;
  std::list<irep_idt>::const_iterator it = insn.labels.begin();
  for (; it != insn.labels.end(); it++)
    if (*it == "__ESBMC_ATOMIC")
      return true;

  if (owning_rt->directed_interleavings)
    // Don't generate interleavings automatically - instead, the user will
    // inserts intrinsics identifying where they want interleavings to occur,
    // and to what thread.
    return true;

  if(threads_state.size() < 2)
    return true;

  return false;
}

bool
execution_statet::apply_static_por(const expr2tc &expr, unsigned int i) const
{
  bool consider = true;

  if (!is_nil_expr(expr))
  {
    if(i < active_thread)
    {
      if(last_global_read_write.write_set.empty() &&
         exprs_read_write.at(i+1).write_set.empty() &&
         exprs_read_write.at(active_thread).write_set.empty())
      {
        return false;
      }

      consider = false;

      if(last_global_read_write.has_write_intersect(exprs_read_write.at(i+1).write_set))
      {
        consider = true;
      }
      else if(last_global_read_write.has_write_intersect(exprs_read_write.at(i+1).read_set))
      {
        consider = true;
      }
      else if(last_global_read_write.has_read_intersect(exprs_read_write.at(i+1).write_set))
      {
        consider = true;
      }
    }
  }

  return consider;
}

void
execution_statet::end_thread(void)
{

  get_active_state().thread_ended = true;
  // If ending in an atomic block, the switcher fails to switch to another
  // live thread (because it's trying to be atomic). So, disable atomic blocks
  // when the thread ends.
  atomic_numbers[active_thread] = 0;
}

bool
execution_statet::is_cur_state_guard_false(void)
{

  // So, can the assumption actually be true? If enabled, ask the solver.
  if (options.get_bool_option("smt-thread-guard")) {
    expr2tc parent_guard = threads_state[active_thread].guard.as_expr();

    runtime_encoded_equationt *rte = dynamic_cast<runtime_encoded_equationt*>
                                                 (target);

    expr2tc the_question(new equality2t(true_expr, parent_guard));

    try {
      tvt res = rte->ask_solver_question(the_question);
      if (res.is_false())
        return true;
    } catch (runtime_encoded_equationt::dual_unsat_exception &e) {
      // Basically, this means our _assumptions_ here are false as well, so
      // neither true or false guards are possible. Consider this as meaning
      // that the guard is false.
      return true;
    }
  }

  return false;
}

void
execution_statet::execute_guard(void)
{

  node_id = node_count++;
  expr2tc guard_expr = get_guard_identifier();
  exprt new_rhs, const_prop_val;
  expr2tc parent_guard;

  // Parent guard of this context switch - if a assign/claim/assume, just use
  // the current thread guard. However if we just executed a goto, use the
  // pre-goto thread guard that we stored at that time. This is so that the
  // thread we context switch to gets the guard of that context switch happening
  // rather than the guard of either branch of the GOTO.
  if (!is_nil_expr(pre_goto_guard))
    parent_guard = pre_goto_guard;
  else
    parent_guard = threads_state[last_active_thread].guard.as_expr();

  // Rename value, allows its use in other renamed exprs
  state_level2->make_assignment(guard_expr, expr2tc(), expr2tc());

  // Truth of this guard implies the parent is true.
  state_level2->rename(parent_guard);
  do_simplify(parent_guard);
  expr2tc assumpt = expr2tc(new implies2t(guard_expr, parent_guard));

  target->assumption(guardt().as_expr(), assumpt, get_active_state().source);

  guardt old_guard;
  old_guard.add(threads_state[last_active_thread].guard.as_expr());

  // If we simplified the global guard expr to false, write that to thread
  // guards, not the symbolic guard name. This is the only way to bail out of
  // evaulating a particular interleaving early right now.
  if (is_false(parent_guard))
    guard_expr = parent_guard;

  for (unsigned int i = 0; i < threads_state.size(); i++)
  {
    threads_state.at(i).global_guard.make_true();
    threads_state.at(i).global_guard.add(get_guard_identifier());
  }

  // Check to see whether or not the state guard is false, indicating we've
  // found an unviable interleaving. However don't do this if we didn't
  // /actually/ switch between threads, because it's acceptable to have a
  // context switch point in a branch where the guard is false (it just isn't
  // acceptable to permit switching).
  if (last_active_thread != active_thread && is_cur_state_guard_false())
    interleaving_unviable = true;
}

unsigned int
execution_statet::add_thread(const goto_programt *prog)
{

  goto_symex_statet new_state(*state_level2, global_value_set, ns);
  new_state.initialize(prog->instructions.begin(), prog->instructions.end(),
                      prog, threads_state.size());

  new_state.source.thread_nr = threads_state.size();
  new_state.global_guard.make_true();
  new_state.global_guard.add(get_guard_identifier());
  threads_state.push_back(new_state);
  atomic_numbers.push_back(0);

  if (DFS_traversed.size() <= new_state.source.thread_nr) {
    DFS_traversed.push_back(false);
  } else {
    DFS_traversed[new_state.source.thread_nr] = false;
  }

  exprs_read_write.push_back(read_write_set());
  thread_start_data.push_back(expr2tc());

  // We invalidated all threads_state refs, so reset cur_state ptr.
  cur_state = &threads_state[active_thread];

  return threads_state.size() - 1; // thread ID, zero based
}

unsigned int
execution_statet::get_expr_write_globals(const namespacet &ns,
                                         const expr2tc &expr)
{

  if (is_address_of2t(expr) || is_valid_object2t(expr) ||
      is_dynamic_size2t(expr) || is_valid_object2t(expr) ||
      is_zero_string2t(expr) || is_zero_length_string2t(expr)) {
    return 0;
  } else if (is_symbol2t(expr)) {
    expr2tc newexpr = expr;
    get_active_state().get_original_name(newexpr);
    const std::string &name = to_symbol2t(newexpr).thename.as_string();
    const symbolt &symbol = ns.lookup(name);
    if (name == "c::__ESBMC_alloc" || name == "c::__ESBMC_alloc_size")
      return 0;
    else if ((symbol.static_lifetime || symbol.type.is_dynamic_set())) {
      exprs_read_write.at(active_thread).write_set.insert(name);
      return 1;
    } else
      return 0;
  }

  unsigned int globals = 0;

  forall_operands2(it, op_list, expr) {
    globals += get_expr_write_globals(ns, **it);
  }

  return globals;
}

unsigned int
execution_statet::get_expr_read_globals(const namespacet &ns,
  const expr2tc &expr)
{

  if (is_address_of2t(expr) || is_pointer_type(expr->type) ||
      is_valid_object2t(expr) || is_dynamic_size2t(expr) ||
      is_zero_string2t(expr) || is_zero_length_string2t(expr)) {
    return 0;
  } else if (is_symbol2t(expr)) {
    expr2tc newexpr = expr;
    get_active_state().get_original_name(newexpr);
    const std::string &name = to_symbol2t(newexpr).thename.as_string();

    if (name == "goto_symex::\\guard!" +
        i2string(get_active_state().top().level1.thread_id))
      return 0;

    const symbolt *symbol;
    if (ns.lookup(name, symbol))
      return 0;

    if (name == "c::__ESBMC_alloc" || name == "c::__ESBMC_alloc_size")
      return 0;
    else if ((symbol->static_lifetime || symbol->type.is_dynamic_set())) {
      exprs_read_write.at(active_thread).read_set.insert(name);
      return 1;
    } else
      return 0;
  }
  unsigned int globals = 0;

  forall_operands2(it, op_list, expr) {
    globals += get_expr_read_globals(ns, **it);
  }

  return globals;
}

crypto_hash
execution_statet::generate_hash(void) const
{

  state_hashing_level2t *l2 =dynamic_cast<state_hashing_level2t*>(state_level2);
  assert(l2 != NULL);
  crypto_hash state = l2->generate_l2_state_hash();
  std::string str = state.to_string();

  for (std::vector<goto_symex_statet>::const_iterator it = threads_state.begin();
       it != threads_state.end(); it++) {
    goto_programt::const_targett pc = it->source.pc;
    int id = pc->location_number;
    std::stringstream s;
    s << id;
    str += "!" + s.str();
  }

  crypto_hash h = crypto_hash(str);

  return h;
}

static std::string state_to_ignore[8] =
{
  "\\guard", "trds_count", "trds_in_run", "deadlock_wait", "deadlock_mutex",
  "count_lock", "count_wait", "unlocked"
};

std::string
execution_statet::serialise_expr(const exprt &rhs __attribute__((unused)))
{
  // FIXME: some way to disambiguate what's part of a hash / const /whatever,
  // and what's part of an operator

  // The plan: serialise this expression into the identifiers of its operations,
  // replacing symbol names with the hash of their value.
  std::cerr << "Serialise expr is a victim of string migration" << std::endl;
  abort();
#if 0

  std::string str;
  uint64_t val;
  int i;

  if (rhs.id() == exprt::symbol) {

    str = rhs.identifier().as_string();
    for (i = 0; i < 8; i++)
      if (str.find(state_to_ignore[i]) != std::string::npos)
	return "(ignore)";

    // If this is something we've already encountered, use the hash of its
    // value.
    exprt tmp = rhs;
    expr2tc new_tmp;
    migrate_expr(tmp, new_tmp);
    get_active_state().get_original_name(new_tmp);
    tmp = migrate_expr_back(new_tmp);
    if (state_level2->current_hashes.find(tmp.identifier().as_string()) !=
        state_level2->current_hashes.end()) {
      crypto_hash h = state_level2->current_hashes.find(
        tmp.identifier().as_string())->second;
      return "hash(" + h.to_string() + ")";
    }

    /* Otherwise, it's something that's been assumed, or some form of
       nondeterminism. Just return its name. */
    return rhs.identifier().as_string();
  } else if (rhs.id() == exprt::arrayof) {
    /* An array of the same set of values: generate all of them. */
    str = "array(";
    irept array = rhs.type();
    exprt size = (exprt &)array.size_irep();
    str += "sz(" + serialise_expr(size) + "),";
    str += "elem(" + serialise_expr(rhs.op0()) + "))";
  } else if (rhs.id() == exprt::with) {
    exprt rec = rhs;

    if (rec.type().id() == typet::t_array) {
      str = "array(";
      str += "prev(" + serialise_expr(rec.op0()) + "),";
      str += "idx(" + serialise_expr(rec.op1()) + "),";
      str += "val(" + serialise_expr(rec.op2()) + "))";
    } else if (rec.type().id() == typet::t_struct) {
      str = "struct(";
      str += "prev(" + serialise_expr(rec.op0()) + "),";
      str += "member(" + serialise_expr(rec.op1()) + "),";
      str += "val(" + serialise_expr(rec.op2()) + "),";
    } else if (rec.type().id() ==  typet::t_union) {
      /* We don't care about previous assignments to this union, because they're
         overwritten by this one, and leads to undefined side effects anyway.
         So, just serialise the identifier, the member assigned to, and the
         value assigned */
      str = "union_set(";
      str += "union_sym(" + rec.op0().identifier().as_string() + "),";
      str += "field(" + serialise_expr(rec.op1()) + "),";
      str += "val(" + serialise_expr(rec.op2()) + "))";
    } else {
      throw "Unrecognised type of with expression: " +
            rec.op0().type().id().as_string();
    }
  } else if (rhs.id() == exprt::index) {
    str = "index(";
    str += serialise_expr(rhs.op0());
    str += ",idx(" + serialise_expr(rhs.op1()) + ")";
  } else if (rhs.id() == "member_name") {
    str = "component(" + rhs.component_name().as_string() + ")";
  } else if (rhs.id() == exprt::member) {
    str = "member(entity(" + serialise_expr(rhs.op0()) + "),";
    str += "member_name(" + rhs.component_name().as_string() + "))";
  } else if (rhs.id() == "nondet_symbol") {
    /* Just return the identifier: it'll be unique to this particular piece of
       entropy */
    exprt tmp = rhs;
    expr2tc new_tmp;
    migrate_expr(tmp, new_tmp);
    get_active_state().get_original_name(new_tmp);
    tmp = migrate_expr_back(new_tmp);
    str = "nondet_symbol(" + tmp.identifier().as_string() + ")";
  } else if (rhs.id() == exprt::i_if) {
    str = "cond(if(" + serialise_expr(rhs.op0()) + "),";
    str += "then(" + serialise_expr(rhs.op1()) + "),";
    str += "else(" + serialise_expr(rhs.op2()) + "))";
  } else if (rhs.id() == "struct") {
    str = rhs.type().tag().as_string();
    str = "struct(tag(" + str + "),";
    forall_operands(it, rhs) {
      str = str + "(" + serialise_expr(*it) + "),";
    }
    str += ")";
  } else if (rhs.id() == "union") {
    str = rhs.type().tag().as_string();
    str = "union(tag(" + str + "),";
    forall_operands(it, rhs) {
      str = str + "(" + serialise_expr(*it) + "),";
    }
  } else if (rhs.id() == exprt::constant) {
    // It appears constants can be "true", "false", or a bit vector. Parse that,
    // and then print the value as a base 10 integer.

    irep_idt idt_val = rhs.value();
    if (idt_val == exprt::i_true) {
      val = 1;
    } else if (idt_val == exprt::i_false) {
      val = 0;
    } else {
      val = strtol(idt_val.c_str(), NULL, 2);
    }

    std::stringstream tmp;
    tmp << val;
    str = "const(" + tmp.str() + ")";
  } else if (rhs.id() == "pointer_offset") {
    str = "pointer_offset(" + serialise_expr(rhs.op0()) + ")";
  } else if (rhs.id() == "string-constant") {
    exprt tmp;
    string2array(rhs, tmp);
    return serialise_expr(tmp);
  } else if (rhs.id() == "same-object") {
    str = "same-obj((" + serialise_expr(rhs.op0()) + "),(";
    str += serialise_expr(rhs.op1()) + "))";
  } else if (rhs.id() == "byte_update_little_endian") {
    str = "byte_up_le((" + serialise_expr(rhs.op0()) + "),(";
    str += serialise_expr(rhs.op1()) + "))";
  } else if (rhs.id() == "byte_update_big_endian") {
    str = "byte_up_be((" + serialise_expr(rhs.op0()) + "),(";
    str += serialise_expr(rhs.op1()) + "))";
  } else if (rhs.id() == "byte_extract_little_endian") {
    str = "byte_up_le((" + serialise_expr(rhs.op0()) + "),(";
    str += serialise_expr(rhs.op1()) + "),";
    str += serialise_expr(rhs.op2()) + "))";
  } else if (rhs.id() == "byte_extract_big_endian") {
    str = "byte_up_be((" + serialise_expr(rhs.op0()) + "),(";
    str += serialise_expr(rhs.op1()) + "),";
    str += serialise_expr(rhs.op2()) + "))";
  } else if (rhs.id() == "infinity") {
    return "inf";
  } else if (rhs.id() == "nil") {
    return "nil";
  } else {
    execution_statet::expr_id_map_t::const_iterator it;
    it = expr_id_map.find(rhs.id());
    if (it != expr_id_map.end())
      return it->second(*this, rhs);

    std::cout << "Unrecognized expression when generating state hash:\n";
    std::cout << rhs.pretty(0) << std::endl;
    abort();
  }

  return str;
#endif
}

// If we have a normal expression, either arithmatic, binary, comparision,
// or whatever, just take the operator and append its operands.
std::string
serialise_normal_operation(execution_statet &ex_state, const exprt &rhs)
{
  std::string str;

  str = rhs.id().as_string();
  forall_operands(it, rhs) {
    str = str + "(" + ex_state.serialise_expr(*it) + ")";
  }

  return str;
}


crypto_hash
execution_statet::update_hash_for_assignment(const exprt &rhs)
{

  return crypto_hash(serialise_expr(rhs));
}

execution_statet::expr_id_map_t execution_statet::expr_id_map;

execution_statet::expr_id_map_t
execution_statet::init_expr_id_map()
{
  execution_statet::expr_id_map_t m;
  m[exprt::plus] = serialise_normal_operation;
  m[exprt::minus] = serialise_normal_operation;
  m[exprt::mult] = serialise_normal_operation;
  m[exprt::div] = serialise_normal_operation;
  m[exprt::mod] = serialise_normal_operation;
  m[exprt::equality] = serialise_normal_operation;
  m[exprt::implies] = serialise_normal_operation;
  m[exprt::i_and] = serialise_normal_operation;
  m[exprt::i_xor] = serialise_normal_operation;
  m[exprt::i_or] = serialise_normal_operation;
  m[exprt::i_not] = serialise_normal_operation;
  m[exprt::notequal] = serialise_normal_operation;
  m["unary-"] = serialise_normal_operation;
  m["unary+"] = serialise_normal_operation;
  m[exprt::abs] = serialise_normal_operation;
  m[exprt::isnan] = serialise_normal_operation;
  m[exprt::i_ge] = serialise_normal_operation;
  m[exprt::i_gt] = serialise_normal_operation;
  m[exprt::i_le] = serialise_normal_operation;
  m[exprt::i_lt] = serialise_normal_operation;
  m[exprt::i_bitand] = serialise_normal_operation;
  m[exprt::i_bitor] = serialise_normal_operation;
  m[exprt::i_bitxor] = serialise_normal_operation;
  m[exprt::i_bitnand] = serialise_normal_operation;
  m[exprt::i_bitnor] = serialise_normal_operation;
  m[exprt::i_bitnxor] = serialise_normal_operation;
  m[exprt::i_bitnot] = serialise_normal_operation;
  m[exprt::i_shl] = serialise_normal_operation;
  m[exprt::i_lshr] = serialise_normal_operation;
  m[exprt::i_ashr] = serialise_normal_operation;
  m[exprt::typecast] = serialise_normal_operation;
  m[exprt::addrof] = serialise_normal_operation;
  m["pointer_obj"] = serialise_normal_operation;
  m["pointer_object"] = serialise_normal_operation;

  return m;
}

void
execution_statet::print_stack_traces(unsigned int indent) const
{
  std::vector<goto_symex_statet>::const_iterator it;
  std::string spaces = std::string("");
  unsigned int i;

  for (i = 0; i < indent; i++)
    spaces += " ";

  i = 0;
  for (it = threads_state.begin(); it != threads_state.end(); it++) {
    std::cout << spaces << "Thread " << i++ << ":" << std::endl;
    it->print_stack_trace(indent + 2);
    std::cout << std::endl;
  }

  return;
}

bool execution_statet::expr_id_map_initialized = false;

execution_statet::ex_state_level2t::ex_state_level2t(
    execution_statet &ref)
  : renaming::level2t(),
    owner(&ref)
{
}

execution_statet::ex_state_level2t::~ex_state_level2t(void)
{
}

execution_statet::ex_state_level2t *
execution_statet::ex_state_level2t::clone(void) const
{

  return new ex_state_level2t(*this);
}

void
execution_statet::ex_state_level2t::rename(expr2tc &lhs_sym, unsigned count)
{
  renaming::level2t::coveredinbees(lhs_sym, count, owner->node_id);
}

void
execution_statet::ex_state_level2t::rename(expr2tc &identifier)
{
  renaming::level2t::rename(identifier);
}

dfs_execution_statet::~dfs_execution_statet(void)
{

  // Delete target; or if we're encoding at runtime, pop a context.
  if (options.get_bool_option("smt-during-symex"))
    target->pop_ctx();
  else
    delete target;
}

dfs_execution_statet* dfs_execution_statet::clone(void) const
{
  dfs_execution_statet *d;

  d = new dfs_execution_statet(*this);

  // Duplicate target equation; or if we're encoding at runtime, push a context.
  if (options.get_bool_option("smt-during-symex")) {
    d->target = target;
    d->target->push_ctx();
  } else {
    d->target = target->clone();
  }

  return d;
}

dfs_execution_statet::dfs_execution_statet(const dfs_execution_statet &ref)
  :  execution_statet(ref)
{
}

schedule_execution_statet::~schedule_execution_statet(void)
{
  // Don't delete equation. Schedule requires all this data.
}

schedule_execution_statet* schedule_execution_statet::clone(void) const
{
  schedule_execution_statet *s;

  s = new schedule_execution_statet(*this);

  // Don't duplicate target equation.
  s->target = target;
  return s;
}

schedule_execution_statet::schedule_execution_statet(const schedule_execution_statet &ref)
  :  execution_statet(ref),
     ptotal_claims(ref.ptotal_claims),
     premaining_claims(ref.premaining_claims)
{
}

void
schedule_execution_statet::claim(const expr2tc &expr, const std::string &msg)
{
  unsigned int tmp_total, tmp_remaining;

  tmp_total = total_claims;
  tmp_remaining = remaining_claims;

  execution_statet::claim(expr, msg);

  tmp_total = total_claims - tmp_total;
  tmp_remaining = remaining_claims - tmp_remaining;

  *ptotal_claims += tmp_total;
  *premaining_claims += tmp_remaining;
  return;
}

execution_statet::state_hashing_level2t::state_hashing_level2t(
                                         execution_statet &ref)
    : ex_state_level2t(ref)
{
}

execution_statet::state_hashing_level2t::~state_hashing_level2t(void)
{
}

execution_statet::state_hashing_level2t *
execution_statet::state_hashing_level2t::clone(void) const
{

  return new state_hashing_level2t(*this);
}

void
execution_statet::state_hashing_level2t::make_assignment(expr2tc &lhs_sym,
                                       const expr2tc &const_value,
                                       const expr2tc &assigned_value)
{
//  crypto_hash hash;

  renaming::level2t::make_assignment(lhs_sym, const_value, assigned_value);

  // XXX - consider whether to use l1 names instead. Recursion, reentrancy.
#if 0
#warning XXXjmorse - state hashing is a casualty of irep2
  hash = owner->update_hash_for_assignment(assigned_value);
  std::string orig_name =
    owner->get_active_state().get_original_name(l1_ident).as_string();
  current_hashes[orig_name] = hash;
#endif
}

crypto_hash
execution_statet::state_hashing_level2t::generate_l2_state_hash() const
{
  unsigned int total;

  uint8_t *data = (uint8_t*)alloca(current_hashes.size() * CRYPTO_HASH_SIZE * sizeof(uint8_t));

  total = 0;
  for (current_state_hashest::const_iterator it = current_hashes.begin();
        it != current_hashes.end(); it++) {
    int j;

    for (j = 0 ; j < 8; j++)
      if (it->first.as_string().find(state_to_ignore[j]) != std::string::npos)
        continue;

    memcpy(&data[total * CRYPTO_HASH_SIZE], it->second.hash, CRYPTO_HASH_SIZE);
    total++;
  }

  return crypto_hash(data, total * CRYPTO_HASH_SIZE);
}
