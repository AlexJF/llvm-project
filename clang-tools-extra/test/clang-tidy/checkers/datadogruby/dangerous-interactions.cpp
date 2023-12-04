// RUN: %check_clang_tidy -expect-clang-tidy-error %s datadogruby-dangerous-interactions %t

#define ANNOTATE clang::annotate
#define GC_SAFE [[ANNOTATE("datadog_ruby_gc_safe")]]
#define NOGVL_SAFE [[ANNOTATE("datadog_ruby_nogvl_safe")]]

// Triggers

#define SNEAKY_F1_CALL f1()
#define SNEAKY_F2_CALL f2()

GC_SAFE NOGVL_SAFE void complicatedf(int v1);

// CHECK-MESSAGES: :[[@LINE+1]]:16: error: Function is being called from a GC safe context in 'this_runs_under_gc' but not marked as GC safe [datadogruby-dangerous-interactions]
NOGVL_SAFE int f1() {
  return 1;
}

// CHECK-MESSAGES: :[[@LINE+1]]:13: error: Function is being called from a NoGVL safe context in 'this_runs_without_gvl' but not marked as NoGVL safe [datadogruby-dangerous-interactions]
GC_SAFE int f2() {
  return 1;
}

GC_SAFE void this_runs_under_gc() {
  // CHECK-MESSAGES: :[[@LINE+1]]:3: error: call to GC unsafe function 'f1' in a GC safe context [datadogruby-dangerous-interactions]
  f1();
  // CHECK-MESSAGES: :[[@LINE+1]]:3: error: call to GC unsafe function 'f1' in a GC safe context [datadogruby-dangerous-interactions]
  SNEAKY_F1_CALL;
  SNEAKY_F2_CALL;

  complicatedf(f2());
}

NOGVL_SAFE void this_runs_without_gvl() {
  f1();
  SNEAKY_F1_CALL;
  // CHECK-MESSAGES: :[[@LINE+1]]:3: error: call to NoGVL unsafe function 'f2' in a NoGVL safe context [datadogruby-dangerous-interactions]
  SNEAKY_F2_CALL;

  // CHECK-MESSAGES: :[[@LINE+1]]:16: error: call to NoGVL unsafe function 'f2' in a NoGVL safe context [datadogruby-dangerous-interactions]
  complicatedf(f2());
}

// No Triggers

GC_SAFE void this_runs_under_gc_good() {
  SNEAKY_F2_CALL;

  complicatedf(f2());
}

NOGVL_SAFE void this_runs_without_gvl_good() {
  f1();
  SNEAKY_F1_CALL;
}
