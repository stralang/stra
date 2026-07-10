#if LLVM_VERSION_MAJOR >= 22
// `opt -O1 --print-pipeline-passes`
#define LLVM_OPT_AVAILABLE 1
#define LLVM_OPT_MINIMAL                                                       \
  "memprof-remove-attributes,annotation2metadata,forceattrs,inferattrs,coro-"  \
  "early,function<eager-inv>(ee-instrument<>,lower-expect,simplifycfg<bonus-"  \
  "inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-" \
  "to-arithmetic;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-"     \
  "hoist-loads-stores-with-cond-faulting;no-sink-common-insts;speculate-"      \
  "blocks;simplify-cond-branch;no-speculate-unpredictables>,sroa<modify-cfg>," \
  "early-cse<>),openmp-opt,ipsccp,called-value-propagation,globalopt,"         \
  "function<eager-inv>(mem2reg,instcombine<max-iterations=1;no-verify-"        \
  "fixpoint>,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;"       \
  "switch-range-to-icmp;no-switch-to-arithmetic;no-switch-to-lookup;keep-"     \
  "loops;no-hoist-common-insts;no-hoist-loads-stores-with-cond-faulting;no-"   \
  "sink-common-insts;speculate-blocks;simplify-cond-branch;no-speculate-"      \
  "unpredictables>),always-inline,require<globals-aa>,function(invalidate<aa>" \
  "),require<profile-summary>,cgscc(devirt<4>(inline,function-attrs<skip-non-" \
  "recursive-function-attrs>,function<eager-inv;no-rerun>(sroa<modify-cfg>,"   \
  "early-cse<memssa>,simplifycfg<bonus-inst-threshold=1;no-forward-switch-"    \
  "cond;switch-range-to-icmp;no-switch-to-arithmetic;no-switch-to-lookup;"     \
  "keep-loops;no-hoist-common-insts;no-hoist-loads-stores-with-cond-faulting;" \
  "no-sink-common-insts;speculate-blocks;simplify-cond-branch;no-speculate-"   \
  "unpredictables>,instcombine<max-iterations=1;no-verify-fixpoint>,libcalls-" \
  "shrinkwrap,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;"      \
  "switch-range-to-icmp;no-switch-to-arithmetic;no-switch-to-lookup;keep-"     \
  "loops;no-hoist-common-insts;no-hoist-loads-stores-with-cond-faulting;no-"   \
  "sink-common-insts;speculate-blocks;simplify-cond-branch;no-speculate-"      \
  "unpredictables>,reassociate,loop-mssa(loop-instsimplify,loop-simplifycfg,"  \
  "licm<no-allowspeculation>,loop-rotate<header-duplication;no-prepare-for-"   \
  "lto>,licm<allowspeculation>,simple-loop-unswitch<no-nontrivial;trivial>),"  \
  "simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-" \
  "icmp;no-switch-to-arithmetic;no-switch-to-lookup;keep-loops;no-hoist-"      \
  "common-insts;no-hoist-loads-stores-with-cond-faulting;no-sink-common-"      \
  "insts;speculate-blocks;simplify-cond-branch;no-speculate-unpredictables>,"  \
  "instcombine<max-iterations=1;no-verify-fixpoint>,loop(loop-idiom,indvars,"  \
  "loop-deletion,loop-unroll-full),sroa<modify-cfg>,memcpyopt,sccp,bdce,"      \
  "instcombine<max-iterations=1;no-verify-fixpoint>,coro-elide,adce,"          \
  "simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-" \
  "icmp;no-switch-to-arithmetic;no-switch-to-lookup;keep-loops;no-hoist-"      \
  "common-insts;no-hoist-loads-stores-with-cond-faulting;no-sink-common-"      \
  "insts;speculate-blocks;simplify-cond-branch;no-speculate-unpredictables>,"  \
  "instcombine<max-iterations=1;no-verify-fixpoint>),function-attrs,function(" \
  "require<should-not-run-function-passes>),coro-split,coro-annotation-elide)" \
  "),deadargelim,coro-cleanup,globalopt,globaldce,elim-avail-extern,rpo-"      \
  "function-attrs,recompute-globalsaa,function<eager-inv>(drop-unnecessary-"   \
  "assumes,float2int,lower-constant-intrinsics,loop(loop-rotate<header-"       \
  "duplication;no-prepare-for-lto>,loop-deletion),loop-distribute,inject-tli-" \
  "mappings,loop-vectorize<no-interleave-forced-only;vectorize-forced-only;>," \
  "drop-unnecessary-assumes,infer-alignment,loop-load-elim,instcombine<max-"   \
  "iterations=1;no-verify-fixpoint>,simplifycfg<bonus-inst-threshold=1;"       \
  "forward-switch-cond;switch-range-to-icmp;switch-to-arithmetic;switch-to-"   \
  "lookup;no-keep-loops;hoist-common-insts;no-hoist-loads-stores-with-cond-"   \
  "faulting;sink-common-insts;speculate-blocks;simplify-cond-branch;no-"       \
  "speculate-unpredictables>,vector-combine,instcombine<max-iterations=1;no-"  \
  "verify-fixpoint>,loop-unroll<O1>,transform-warning,sroa<preserve-cfg>,"     \
  "infer-alignment,instcombine<max-iterations=1;no-verify-fixpoint>,loop-"     \
  "mssa(licm<allowspeculation>),alignment-from-assumptions,loop-sink,"         \
  "instsimplify,div-rem-pairs,tailcallelim,simplifycfg<bonus-inst-threshold="  \
  "1;no-forward-switch-cond;switch-range-to-icmp;switch-to-arithmetic;no-"     \
  "switch-to-lookup;keep-loops;no-hoist-common-insts;hoist-loads-stores-with-" \
  "cond-faulting;no-sink-common-insts;speculate-blocks;simplify-cond-branch;"  \
  "speculate-unpredictables>),alloc-token,globaldce,constmerge,cg-profile,"    \
  "rel-lookup-table-converter,function(annotation-remarks),verify"
#endif
