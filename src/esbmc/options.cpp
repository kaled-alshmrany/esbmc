#include <cmdline.h>

#include <fstream>

#include "parseoptions.h"

const struct opt_templ esbmc_options[] = {
{ 0,	"no-inlining",		switc,		""	},
{ 0,  "full-inlining",    switc,    ""  },
{ 0,	"program-only",		switc,		""	},
{ 0,  "dump-guards",   switc,    ""  },
{ 0,	"program-too",		switc,		""	},
{ 0,	"simple-ssa-printing",		switc,		""	},
{ 0,	"function",		string,		""	},
{ 0,	"preprocess",		switc,		""	},
{ 0,	"no-simplify",		switc,		""	},
{ 0,	"unwind",		number,		""	},
{ 0,	"unwindset",		string,		""	},
{ 0,	"no-bounds-check",	switc,		""	},
{ 0,	"bv",		        switc,		""	},
{ 0,	"ir",		        switc,		""	},
{ 0,	"z3",			switc,		""	},
{ 0,	"smt",			switc,		""	},
{ 0,	"outfile",		string,		""	},
{ 0,	"witness-output",	string,		""	},
{ 0,	"witness-detailed",	switc,		""	},
{ 0,	"witness-programfile",	string,		""	},
{ 0,	"no-pointer-check",	switc,		""	},
{ 0,	"document-subgoals",	switc,		""	},
{ 0,	"all-claims",		switc,		""	},
{ 'D',	"",			string,		""	},
{ 'I',	"",			string,		""	},
{ 0,	"depth",		number,		""	},
{ 0,	"no-div-by-zero-check",	switc,		""	},
{ 0,	"no-unwinding-assertions", switc,	""	},
{ 0,	"partial-loops",	switc,		""	},
{ 0,	"int-encoding",		switc,		""	},
{ 0,	"memory-leak-check",	switc,		""	},
{ 0,	"overflow-check",	switc,		""	},
{ 0,	"floatbv",		switc,		""	},
{ 0,	"fixedbv",		switc,		""	},
{ 0,	"no-assertions",	switc,		""	},
{ 0,	"16",			switc,		""	},
{ 0,	"32",			switc,		""	},
{ 0,	"64",			switc,		""	},
{ 0,	"little-endian",	switc,		""	},
{ 0,	"big-endian",		switc,		""	},
{ 0,  "show-parse-tree",  switc,    ""  },
{ 0,	"show-goto-functions",	switc,		""	},
{ 0,	"show-counter-example",	switc,		""	},
{ 0,  "result-only", switc,    ""  },
{ 0,	"show-goto-value-sets",	switc,		""	},
{ 0,  "show-symex-value-sets",  switc,    ""  },
{ 0,	"show-loops",		switc,		""	},
{ 0,	"show-symbol-table",	switc,		""	},
{ 0,	"show-vcc",		switc,		""	},
{ 0,	"show-claims",		switc,		""	},
{ 0,	"claim",		number,		""	},
{ 0,	"atomicity-check",	switc,		""	},
{ 0,	"error-label",		string,		""	},
{ 0,	"verbosity",		number,		""	},
{ 0,	"binary",		string,		""	},
{ 0,	"no-library",		switc,		""	},
{ 0,	"version",		switc,		""	},
{ 0,	"i386-linux",		switc,		""	},
{ 0,	"i386-macos",		switc,		""	},
{ 0,	"i386-win32",		switc,		""	},
{ 0,	"ppc-macos",		switc,		""	},
{ 0,	"unsigned-char",	switc,		""	},
{ 0,	"lock-order-check",	switc,		""	},
{ 0,	"deadlock-check",	switc,		""	},
{ 0,	"no-arch",		switc,		""	},
{ 0,	"no-slice",		switc,		""	},
{ 0,	"control-flow-test",	switc,		""	},
{ 0,	"context-bound",	number,		"-1"	},
{ 0,	"time-slice",   	number,		"1"	},
{ 0,	"k-step",		number,		"1"	},
{ 0,	"max-k-step",   	number,		"50"	},
{ 0,	"no-por",		switc,		""	},
{ 0,	"data-races-check",	switc,		""	},
{ 0,	"DFS",			switc,		""	},
{ 0,	"schedule",		switc,		""	},
{ 0,	"all-runs",		switc,		""	},
{ 0,	"timeout",		string,		""	},
{ 0,	"memlimit",		string,		""	},
{ 0,	"state-hashing",	switc,		""	},
{ 0,	"symex-trace",		switc,		""	},
{ 0,  "ssa-trace",   switc,    ""  },
{ 0,  "extended-try-analysis",    switc,    ""  },
{ 0,	"core-size",		number,		""	},
{ 0,	"smtlib-ileave-num",	number,		""	},
{ 0,	"direct-interleavings",	switc,		""	},
{ 0,	"show-ileave-points",	switc,		""	},
{ 0,	"checkpoint-file",	string,		""	},
{ 0,	"from-checkpoint",	switc,		""	},
{ 0,    "checkpoint-on-cex",    switc,          ""      },
{ 0,	"print-stack-traces",	switc,		""	},
{ 0,	"interactive-ileaves",	switc,		""	},
{ 0,	"round-robin",	switc,		""	},
{ 0,  "falsification", switc, "" },
{ 0,  "incremental-bmc", switc, "" },
{ 0,	"base-case",	switc,		""	},
{ 0,	"forward-condition",	switc,		""	},
{ 0,	"inductive-step",	switc,		""	},
{ 0,	"k-induction", switc, "" },
{ 0,  "k-induction-parallel", switc, "" },
{ 0,  "k-induction-busy-wait", switc, "" },
{ 0,  "constrain-all-states", switc, ""},
{ 0,  "k-induction-nondet-loops", switc, "" },
{ 0,  "dont-ignore-dead-child-process", switc, "" },
{ 0,  "unlimited-k-steps", switc, "" },
{ 0,  "unroll-loops", switc, "" },
{ 0,  "clang-frontend", switc, "" },
{ 0,  "no-bitfields", switc, "" },
{ 0,  "keep-unused", switc, "" },
{ 0,	"break-at",		string,		""	},
{ 0,	"enable-core-dump",     switc,		""	},
{ 0,	"memstats",		switc,		""	},
{ 0,	"dump-z3-assigns",	switc,		""	},
{ 0,	"smt-during-symex",	switc,		""	},
{ 0,	"smt-thread-guard",	switc,		""	},
{ 0,	"smt-symex-guard",	switc,		""	},
{ 0,	"ltl",			switc,		""	},
{ 0,	"ltl-dummy",            switc,		""	},
{ 0,	"symex-ssa-trace",	switc,		""	},
{ 0,    "smtlib-solver-prog",   string,         ""      },
{ 0,    "smtlib",               switc,          ""      },
{ 0,    "output",               string,         ""      },
{ 0,	"double-assign-check",	switc,		""	},
{ 0,    "minisat",              switc,         ""      },
{ 0,    "boolector",            switc,         ""      },
{ 0,    "z3",            switc,         ""      },
{ 0,    "sword",                switc,         ""      },
{ 0,    "stp",                  switc,         ""      },
{ 0,    "mathsat",              switc,         ""      },
{ 0,    "cvc",                  switc,         ""      },
{ 0,	"git-hash",		switc,		""	},
{ 0,	"no-align-check",	switc,		""	},
{ 0,	"quiet",	switc,		""	},
{ 0,	"tuple-node-flattener",	switc,		""	},
{ 0,	"tuple-sym-flattener",	switc,		""	},
{ 0,	"array-flattener",	switc,		""	},
{ 0,	"yices",		switc,		""	},
{ 0,	"force-malloc-success",	switc,		""	},
{ 0,	"list-solvers",	switc,		""	},
{ 0,    "abort-on-recursion", switc,    ""  },
{ 0,	"skip-bmc",	switc,		""	},
{ '?',	"",			switc,		""	},
{ 'h',	"",			switc,		""	},
{ 'I',	"",			string,		""	},
{ 0,	"help",			switc,		""	},
{ 0,	"",			switc,		""	}
};
