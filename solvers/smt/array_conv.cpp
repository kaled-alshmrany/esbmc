// Danger Will Robinson: this is not a C++ class, but in fact a template, and
// is included by array_conv.h directly so that uses of it are instanciated
// correctly.

#include <ansi-c/c_types.h>

template <class subclass>
array_convt<subclass>::array_convt(bool enable_cache, bool int_encoding,
                                   const namespacet &_ns, bool is_cpp,
                                   bool tuple_support, bool bools_in_arrs,
                                   bool can_init_inf_arrs)
  // Declare that we can put bools in arrays, and init unbounded arrays
  // XXX - can't put bools in arrays /just/ yet due to some type hiccups.
  : smt_convt(enable_cache, int_encoding, _ns, is_cpp, tuple_support, true,
              true), array_indexes(), array_values(), array_updates()
{
}

template <class subclass>
array_convt<subclass>::~array_convt()
{
}

template <class subclass>
smt_astt
array_convt<subclass>::convert_array_assign(array_ast<subclass> *src, const expr2tc &dst)
{

  assert(is_symbol2t(dst));

  array_ast<subclass> *destination =
    const_cast<array_ast<subclass>*>(array_downcast(this->convert_ast(dst)));
  const array_ast<subclass> *source = src;

  assign_array_symbol(destination->symname, source);

  // And copy across it's valuation
  destination->array_fields = source->array_fields;
  destination->base_array_id = source->base_array_id;
  destination->array_update_num = destination->array_update_num;

  // Return the resulting ast -- this gets shoved into the AST/expr cache.
  return destination;
}

template <class subclass>
smt_ast *
array_convt<subclass>::fresh_array(smt_sortt ms, const std::string &name)
{
  // No solver representation for this.
  unsigned long domain_width = ms->get_domain_width();
  unsigned long array_size = 1UL << domain_width;
  smt_sortt range_sort =
    this->mk_sort(SMT_SORT_BV, ms->get_range_width(), false);

  array_ast<subclass> *mast = new_ast(ms);
  mast->symname = name;
  assign_array_symbol(name, mast);

  if (is_unbounded_array(mast->sort)) {
    // Don't attempt to initialize. Store the fact that we've allocated a
    // fresh new array.
    mast->base_array_id = array_indexes.size();
    mast->array_update_num = 0;
    std::set<expr2tc> tmp_set;
    array_indexes.push_back(tmp_set);

    std::vector<std::list<struct array_select> > tmp2;
    array_values.push_back(tmp2);

    std::list<struct array_select> tmp25;
    array_values[mast->base_array_id].push_back(tmp25);

    std::vector<struct array_with> tmp3;
    array_updates.push_back(tmp3);

    // Aimless piece of data, just to keep indexes in iarray_updates and
    // array_values in sync.
    struct array_with w;
    w.is_ite = false;
    w.idx = expr2tc();
    array_updates[mast->base_array_id].push_back(w);

    array_subtypes.push_back(ms->get_range_width());

    return mast;
  }

  mast->array_fields.reserve(array_size);

  // Populate that array with a bunch of fresh bvs of the correct sort.
  unsigned long i;
  for (i = 0; i < array_size; i++) {
    smt_astt a = this->mk_fresh(range_sort, "array_fresh_array::");
    mast->array_fields.push_back(a);
  }

  return mast;
}

template <class subclass>
smt_astt 
array_convt<subclass>::mk_select(const array_ast<subclass> *ma, const expr2tc &idx,
                         smt_sortt ressort)
{

  if (is_unbounded_array(ma->sort))
    return mk_unbounded_select(ma, idx, ressort);

  assert(ma->array_fields.size() != 0);

  // If this is a constant index, simple. If not, not.
  if (is_constant_int2t(idx)) {
    const constant_int2t &intref = to_constant_int2t(idx);
    unsigned long intval = intref.constant_value.to_ulong();
    if (intval > ma->array_fields.size())
      // Return a fresh value.
      return this->mk_fresh(ressort, "array_mk_select_badidx::");

    // Otherwise,
    return ma->array_fields[intval];
  }

  // What we have here is a nondeterministic index. Alas, compare with
  // everything.
  smt_astt fresh = this->mk_fresh(ressort, "array_mk_select::");
  smt_astt real_idx = this->convert_ast(idx);
  smt_astt args[2], idxargs[2], impargs[2];
  unsigned long dom_width = ma->sort->get_domain_width();
  smt_sortt bool_sort = this->mk_sort(SMT_SORT_BOOL);

  args[0] = fresh;
  idxargs[0] = real_idx;

  for (unsigned long i = 0; i < ma->array_fields.size(); i++) {
    idxargs[1] = this->mk_smt_bvint(BigInt(i), false, dom_width);
    smt_astt idx_eq = this->mk_func_app(bool_sort, SMT_FUNC_EQ, idxargs, 2);
    args[1] = ma->array_fields[i];
    smt_astt val_eq = this->mk_func_app(bool_sort, SMT_FUNC_EQ, args, 2);

    impargs[0] = idx_eq;
    impargs[1] = val_eq;

    this->assert_ast(this->mk_func_app(bool_sort, SMT_FUNC_IMPLIES, impargs, 2));
  }

  return fresh;
}

template <class subclass>
smt_astt 
array_convt<subclass>::mk_store(const array_ast<subclass>* ma, const expr2tc &idx,
                                smt_astt value, smt_sortt ressort)
{

  if (is_unbounded_array(ma->sort))
    return mk_unbounded_store(ma, idx, value, ressort);

  assert(ma->array_fields.size() != 0);

  array_ast<subclass> *mast =
    new_ast(ressort, ma->array_fields);

  // If this is a constant index, simple. If not, not.
  if (is_constant_int2t(idx)) {
    const constant_int2t &intref = to_constant_int2t(idx);
    unsigned long intval = intref.constant_value.to_ulong();
    if (intval > ma->array_fields.size())
      return ma;

    // Otherwise,
    mast->array_fields[intval] = value;
    return mast;
  }

  // Oh dear. We need to update /all the fields/ :(
  smt_astt real_idx = this->convert_ast(idx);
  smt_astt real_value = value;
  smt_astt iteargs[3], idxargs[2];
  unsigned long dom_width = mast->sort->get_domain_width();
  smt_sortt bool_sort = this->mk_sort(SMT_SORT_BOOL);

  idxargs[0] = real_idx;
  iteargs[1] = real_value;

  for (unsigned long i = 0; i < mast->array_fields.size(); i++) {
    idxargs[1] = this->mk_smt_bvint(BigInt(i), false, dom_width);
    smt_astt idx_eq = this->mk_func_app(bool_sort, SMT_FUNC_EQ, idxargs, 2);

    iteargs[0] = idx_eq;
    iteargs[2] = mast->array_fields[i];

    smt_astt new_val =
      this->mk_func_app(iteargs[1]->sort, SMT_FUNC_ITE, iteargs, 3);
    mast->array_fields[i] = new_val;
  }

  return mast;
}

template <class subclass>
smt_astt 
array_convt<subclass>::mk_unbounded_select(const array_ast<subclass> *ma,
                                   const expr2tc &real_idx,
                                   smt_sortt ressort)
{
  // Record that we've accessed this index.
  array_indexes[ma->base_array_id].insert(real_idx);

  // Generate a new free variable
  smt_astt a = this->mk_fresh(ressort, "mk_unbounded_select");

  struct array_select sel;
  sel.src_array_update_num = ma->array_update_num;
  sel.idx = real_idx;
  sel.val = a;
  // Record this index
  array_values[ma->base_array_id][ma->array_update_num].push_back(sel);

  // Convert index; it might trigger an array_of, or something else, which
  // fiddles with other arrays.
  this->convert_ast(real_idx);

  return a;
}

template <class subclass>
smt_astt 
array_convt<subclass>::mk_unbounded_store(const array_ast<subclass> *ma,
                                  const expr2tc &idx, smt_astt value,
                                  smt_sortt ressort)
{
  // Record that we've accessed this index.
  array_indexes[ma->base_array_id].insert(idx);

  // More nuanced: allocate a new array representation.
  array_ast<subclass> *newarr = new_ast(ressort);
  newarr->base_array_id = ma->base_array_id;
  newarr->array_update_num = array_updates[ma->base_array_id].size();

  // Record update
  struct array_with w;
  w.is_ite = false;
  w.idx = idx;
  w.u.w.src_array_update_num = ma->array_update_num;
  w.u.w.val = value;
  array_updates[ma->base_array_id].push_back(w);

  // Convert index; it might trigger an array_of, or something else, which
  // fiddles with other arrays.
  this->convert_ast(idx);

  // Also file a new select record for this point in time.
  std::list<struct array_select> tmp;
  array_values[ma->base_array_id].push_back(tmp);

  // Result is the new array id goo.
  return newarr;
}

template <class subclass>
smt_ast *
array_convt<subclass>::array_ite(smt_astt _cond,
                         smt_astt _true_arr,
                         smt_astt _false_arr,
                         smt_sortt thesort)
{
  const array_ast<subclass> *cond = array_downcast(_cond);
  const array_ast<subclass> *true_arr = array_downcast(_true_arr);
  const array_ast<subclass> *false_arr = array_downcast(_false_arr);

  if (is_unbounded_array(true_arr->sort))
    return unbounded_array_ite(cond, true_arr, false_arr, thesort);

  // For each element, make an ite.
  assert(true_arr->array_fields.size() != 0 &&
         true_arr->array_fields.size() == false_arr->array_fields.size());
  array_ast<subclass> *mast = new_ast(thesort);
  smt_astt args[3];
  args[0] = cond;
  unsigned long i;
  for (i = 0; i < true_arr->array_fields.size(); i++) {
    // One ite pls.
    args[1] = true_arr->array_fields[i];
    args[2] = false_arr->array_fields[i];
    smt_astt res = this->mk_func_app(args[1]->sort, SMT_FUNC_ITE, args, 3);
    mast->array_fields.push_back(array_downcast(res));
  }

  return mast;
}

template <class subclass>
smt_ast *
array_convt<subclass>::unbounded_array_ite(const array_ast<subclass> *cond,
                                   const array_ast<subclass> *true_arr,
                                   const array_ast<subclass> *false_arr,
                                   smt_sortt thesort)
{
  // Precondition for a lot of goo: that the two arrays are the same, at
  // different points in time.
  assert(true_arr->base_array_id == false_arr->base_array_id &&
         "ITE between two arrays with different bases are unsupported");

  array_ast<subclass> *newarr = new_ast(thesort);
  newarr->base_array_id = true_arr->base_array_id;
  newarr->array_update_num = array_updates[true_arr->base_array_id].size();

  struct array_with w;
  w.is_ite = true;
  w.idx = expr2tc();
  w.u.i.src_array_update_true = true_arr->array_update_num;
  w.u.i.src_array_update_false = false_arr->array_update_num;
  w.u.i.cond = cond;
  array_updates[true_arr->base_array_id].push_back(w);

  // Also file a new select record for this point in time.
  std::list<struct array_select> tmp;
  array_values[true_arr->base_array_id].push_back(tmp);

  return newarr;
}

template <class subclass>
smt_astt 
array_convt<subclass>::convert_array_of(const expr2tc &init_val,
                                unsigned long domain_width)
{
  smt_sortt dom_sort = this->mk_sort(SMT_SORT_BV, domain_width, false);
  smt_sortt idx_sort = this->convert_sort(init_val->type);

  if (!this->int_encoding && is_bool_type(init_val) && this->no_bools_in_arrays)
    idx_sort = this->mk_sort(SMT_SORT_BV, 1, false);

  smt_sortt arr_sort = this->mk_sort(SMT_SORT_ARRAY, dom_sort, idx_sort);

  array_ast<subclass> *mast = new_ast(arr_sort);

  smt_astt init = this->convert_ast(init_val);
  if (!this->int_encoding && is_bool_type(init_val) && this->no_bools_in_arrays)
    init = this->make_bool_bit(init);

  if (is_unbounded_array(arr_sort)) {
    std::string name = this->mk_fresh_name("array_of_unbounded::");
    mast = static_cast<array_ast<subclass>*>(fresh_array(arr_sort, name));
    array_of_vals.insert(std::pair<unsigned, smt_astt >
                                  (mast->base_array_id, init));
  } else {
    unsigned long array_size = 1UL << domain_width;
    for (unsigned long i = 0; i < array_size; i++)
      mast->array_fields.push_back(init);
  }

  return mast;
}

template <class subclass>
expr2tc
array_convt<subclass>::get_array_elem(smt_astt a, uint64_t index,
                                      const type2tc &subtype)
{
  const array_ast<subclass> *mast = array_downcast(a);

  if (mast->base_array_id >= array_valuation.size()) {
    // This is an array that was not previously converted, therefore doesn't
    // appear in the valuation table. Therefore, all its values are free.
    return expr2tc();
  }

  // Fetch all the indexes
  const std::set<expr2tc> &indexes = array_indexes[mast->base_array_id];
  unsigned int i = 0;

  // Basically, we have to do a linear search of all the indexes to find one
  // that matches the index argument.
  std::set<expr2tc>::const_iterator it;
  for (it = indexes.begin(); it != indexes.end(); it++, i++) {
    const expr2tc &e = *it;
    expr2tc e2 = this->get(e);
    const constant_int2t &intval = to_constant_int2t(e2);
    if (intval.constant_value.to_uint64() == index)
      break;
  }

  if (it == indexes.end())
    // Then this index wasn't modelled in any way.
    return expr2tc();

  // We've found an index; pick its value out, convert back to expr.
  // First, what's it's type?
  type2tc src_type = get_uint_type(array_subtypes[mast->base_array_id]);

  const std::vector<smt_astt > &solver_values =
    array_valuation[mast->base_array_id][mast->array_update_num];
  assert(i < solver_values.size());
  return this->get_bv(src_type, solver_values[i]);
}

template <class subclass>
void
array_convt<subclass>::add_array_constraints(void)
{

  for (unsigned int i = 0; i < array_indexes.size(); i++) {
    add_array_constraints(i);
  }

  return;
}

template <class subclass>
void
array_convt<subclass>::add_array_constraints(unsigned int arr)
{
  // Right: we need to tie things up regarding these bitvectors. We have a
  // set of indexes...
  const std::set<expr2tc> &indexes = array_indexes[arr];

  // What we're going to build is a two-dimensional vector ish of each element
  // at each point in time. Expensive, but meh.
  array_valuation.resize(array_valuation.size() + 1);
  std::vector<std::vector<smt_astt > > &real_array_values =
    array_valuation.back();

  // Subtype is thus
  smt_sortt subtype = this->mk_sort(SMT_SORT_BV, array_subtypes[arr], false);

  // Pre-allocate all the storage.
  real_array_values.resize(array_values[arr].size());
  for (unsigned int i = 0; i < real_array_values.size(); i++)
    real_array_values[i].resize(indexes.size());

  // Compute a mapping between indexes and an element in the vector. These
  // are ordered by how std::set orders them, not by history or anything. Or
  // even the element index.
  std::map<expr2tc, unsigned> idx_map;
  for (std::set<expr2tc>::const_iterator it = indexes.begin();
       it != indexes.end(); it++)
    idx_map.insert(std::pair<expr2tc, unsigned>(*it, idx_map.size()));

  assert(idx_map.size() == indexes.size());

  // Initialize the first set of elements.
  std::map<unsigned, const smt_ast*>::const_iterator it =
    array_of_vals.find(arr);
  if (it != array_of_vals.end()) {
    collate_array_values(real_array_values[0], idx_map, array_values[arr][0],
        subtype, it->second);
  } else {
    collate_array_values(real_array_values[0], idx_map, array_values[arr][0],
        subtype);
  }

  add_initial_ackerman_constraints(real_array_values[0], idx_map);

  // Now repeatedly execute transitions between states.
  for (unsigned int i = 0; i < real_array_values.size() - 1; i++)
    execute_array_trans(real_array_values, arr, i, idx_map, subtype);

}

template <class subclass>
void
array_convt<subclass>::execute_array_trans(
                            std::vector<std::vector<smt_astt > > &data,
                                   unsigned int arr,
                                   unsigned int idx,
                                   const std::map<expr2tc, unsigned> &idx_map,
                                   smt_sortt subtype)
{
  // Steps: First, fill the destination vector with either free variables, or
  // the free variables that resulted for selects corresponding to that item.
  // Then apply update or ITE constraints.
  // Then apply equalities between the old and new values.

  std::vector<smt_astt > &dest_data = data[idx+1];
  collate_array_values(dest_data, idx_map, array_values[arr][idx+1], subtype);

  smt_sortt boolsort = this->mk_sort(SMT_SORT_BOOL);

  // Two updates that could have occurred for this array: a simple with, or
  // an ite.
  const array_with &w = array_updates[arr][idx+1];
  if (w.is_ite) {
    // Turn every index element into an ITE representing this operation. Every
    // single element is addressed and updated; no further constraints are
    // needed. Not even the ackerman ones, in fact, because instances of that
    // from previous array updates will feed through to here (speculation).

    unsigned int true_idx = w.u.i.src_array_update_true;
    unsigned int false_idx = w.u.i.src_array_update_false;
    assert(true_idx < idx + 1 && false_idx < idx + 1);
    const std::vector<smt_astt > &true_vals = data[true_idx];
    const std::vector<smt_astt > &false_vals = data[false_idx];
    smt_astt cond = w.u.i.cond;

    // Each index value becomes an ITE between each source value.
    smt_astt args[3], eq[2];
    args[0] = cond;
    for (unsigned int i = 0; i < idx_map.size(); i++) {
      args[1] = true_vals[i];
      args[2] = false_vals[i];
      eq[0] = this->mk_func_app(subtype, SMT_FUNC_ITE, args, 3);
      eq[1] = dest_data[i];
      this->assert_ast(this->mk_func_app(boolsort, SMT_FUNC_EQ, eq, 2));
    }
  } else {
    // Place a constraint on the updated variable; add equality constraints
    // between the older version and this version.

    // So, the updated element,
    std::map<expr2tc, unsigned>::const_iterator it = idx_map.find(w.idx);
    assert(it != idx_map.end());

    const expr2tc &update_idx_expr = it->first;
    smt_astt update_idx_ast = this->convert_ast(update_idx_expr);
    unsigned int updated_idx = it->second;
    smt_astt updated_value = w.u.w.val;

    // Assign in its value.
    dest_data[updated_idx] = updated_value;

    // Check all the values selected out of this instance; if any have the same
    // index, tie the select's fresh variable to the updated value. If there are
    // differing index exprs that evaluate to the same location they'll be
    // caught by code later.
    const std::list<struct array_select> &sels = array_values[arr][idx+1];
    for (typename std::list<struct array_select>::const_iterator it = sels.begin();
         it != sels.end(); it++) {
      if (it->idx == update_idx_expr) {
        smt_astt args[2];
        args[0] = updated_value;
        args[1] = it->val;
        this->assert_ast(this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2));
      }
    }

    // Now look at all those other indexes...
    assert(w.u.w.src_array_update_num < idx+1);
    const std::vector<smt_astt > &source_data =
      data[w.u.w.src_array_update_num];

    smt_astt args[3];
    unsigned int i = 0;
    for (std::map<expr2tc, unsigned>::const_iterator it2 = idx_map.begin();
         it2 != idx_map.end(); it2++, i++) {
      if (it2->second == updated_idx)
        continue;

      // Generate an ITE. If the index is nondeterministically equal to the
      // current index, take the updated value, otherwise the original value.
      // This departs from the CBMC implementation, in that they explicitly
      // use implies and ackerman constraints.
      // FIXME: benchmark the two approaches. For now, this is shorter.
      args[0] = update_idx_ast;
      args[1] = this->convert_ast(it2->first);
      args[0] = this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2);
      args[1] = updated_value;
      args[2] = source_data[i];
      args[0] = this->mk_func_app(subtype, SMT_FUNC_ITE, args, 3);
      args[1] = dest_data[i];
      this->assert_ast(this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2));
      // The latter part of this could be replaced with more complex logic,
      // that only asserts an equality between selected values, and just stores
      // the result of the ITE for all other values. FIXME: try this.
    }
  }
}

template <class subclass>
void
array_convt<subclass>::collate_array_values(std::vector<smt_astt > &vals,
                                    const std::map<expr2tc, unsigned> &idx_map,
                                    const std::list<struct array_select> &idxs,
                                    smt_sortt subtype,
                                    smt_astt init_val)
{
  // So, the value vector should be allocated but not initialized,
  assert(vals.size() == idx_map.size());

  // First, make everything null,
  for (std::vector<smt_astt >::iterator it = vals.begin();
       it != vals.end(); it++)
    *it = NULL;

  // Now assign in all free variables created as a result of selects.
  for (typename std::list<struct array_select>::const_iterator it = idxs.begin();
       it != idxs.end(); it++) {
    std::map<expr2tc, unsigned>::const_iterator it2 = idx_map.find(it->idx);
    assert(it2 != idx_map.end());
    vals[it2->second] = it->val;
  }

  // Initialize everything else to either a free variable or the initial value.
  if (init_val == NULL) {
    // Free variables, except where free variables tied to selects have occurred
    for (std::vector<smt_astt >::iterator it = vals.begin();
         it != vals.end(); it++) {
      if (*it == NULL)
        *it = this->mk_fresh(subtype, "collate_array_vals::");
    }
  } else {
    // We need to assign the initial value in, except where there's already
    // a select/index, in which case we assert that the values are equal.
    smt_sortt boolsort = this->mk_sort(SMT_SORT_BOOL);
    for (std::vector<smt_astt >::iterator it = vals.begin();
         it != vals.end(); it++) {
      if (*it == NULL) {
        *it = init_val;
      } else {
        smt_astt args[2];
        args[0] = *it;
        args[1] = init_val;
        this->assert_ast(this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2));
      }
    }
  }

  // Fin.
}

template <class subclass>
void
array_convt<subclass>::add_initial_ackerman_constraints(
                                  const std::vector<smt_astt > &vals,
                                  const std::map<expr2tc,unsigned> &idx_map)
{
  // Lolquadratic,
  smt_sortt boolsort = this->mk_sort(SMT_SORT_BOOL);
  for (std::map<expr2tc, unsigned>::const_iterator it = idx_map.begin();
       it != idx_map.end(); it++) {
    smt_astt outer_idx = this->convert_ast(it->first);
    for (std::map<expr2tc, unsigned>::const_iterator it2 = idx_map.begin();
         it2 != idx_map.end(); it2++) {
      smt_astt inner_idx = this->convert_ast(it2->first);

      // If they're the same idx, they're the same value.
      smt_astt args[2];
      args[0] = outer_idx;
      args[1] = inner_idx;
      smt_astt idxeq = this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2);

      args[0] = vals[it->second];
      args[1] = vals[it2->second];
      smt_astt valeq = this->mk_func_app(boolsort, SMT_FUNC_EQ, args, 2);

      args[0] = idxeq;
      args[1] = valeq;
      this->assert_ast(this->mk_func_app(boolsort, SMT_FUNC_IMPLIES, args, 2));
    }
  }
}

template <typename subclass>
smt_astt
array_ast<subclass>::eq(smt_convt *ctx __attribute__((unused)),
                        smt_astt other __attribute__((unused))) const
{
  std::cerr << "Array equality encoded -- should have become an array assign?" << std::endl;
  abort();
}

template <typename subclass>
smt_astt
array_ast<subclass>::assign(smt_convt *ctx, const expr2tc &sym) const
{
  array_convt<subclass> *actx = static_cast<array_convt<subclass>*>(ctx);
  array_ast<subclass> *mutable_this = const_cast<array_ast<subclass>*>(this);
  return actx->convert_array_assign(this, sym);
}

template <typename subclass>
smt_astt
array_ast<subclass>::update(smt_convt *ctx, smt_astt value,
                                unsigned int idx,
                                expr2tc idx_expr) const
{
  array_convt<subclass> *actx = static_cast<array_convt<subclass>*>(ctx);
  if (is_nil_expr(idx_expr))
    idx_expr = constant_int2tc(get_uint_type(sort->domain_width), BigInt(idx));

  return actx->mk_store(this, idx_expr, value, sort);
}

template <typename subclass>
smt_astt
array_ast<subclass>::select(smt_convt *ctx, const expr2tc &idx) const
{
  array_convt<subclass> *actx = static_cast<array_convt<subclass>*>(ctx);
  smt_sort *s = ctx->mk_sort(SMT_SORT_BV, sort->data_width, false);
  return actx->mk_select(this, idx, s);
}

