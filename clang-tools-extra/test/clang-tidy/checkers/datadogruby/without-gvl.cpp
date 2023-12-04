// RUN: %check_clang_tidy -expect-clang-tidy-error %s datadogruby-without-gvl %t

#define ANNOTATE clang::annotate
#define NOGVL_SAFE [[ANNOTATE("datadog_ruby_nogvl_safe")]]
#define GVL_GUARD [[ANNOTATE("datadog_ruby_gvl_guard")]]

// Triggers
// CHECK-MESSAGES: :[[@LINE+1]]:6: error: Function is being called from a NoGVL safe context in 'f7_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
bool ruby_check();
// CHECK-MESSAGES: :[[@LINE+1]]:6: error: Function is being called from a NoGVL safe context in 'f4_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
// CHECK-MESSAGES: :[[@LINE+1]]:6: error: Function is being called from a NoGVL safe context in 'f5_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
// CHECK-MESSAGES: :[[@LINE+1]]:6: error: Function is being called from a NoGVL safe context in 'f7_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
// CHECK-MESSAGES: :[[@LINE+1]]:6: error: Function is being called from a NoGVL safe context in 'rb_thread_call_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
void ruby_xmalloc();
void rb_thread_call_with_gvl(void (*func)(void));
void rb_thread_call_without_gvl(void (*func)(void));

GVL_GUARD bool has_gvl();

NOGVL_SAFE int f1_without_gvl() {
  if (has_gvl()) {
    ruby_xmalloc();
    return 0;
  } else {
    return 1;
  }
  // CHECK-MESSAGES: :[[@LINE+1]]:3: error: Call to NoGVL unsafe function 'ruby_xmalloc' in a NoGVL safe context [datadogruby-without-gvl]
  ruby_xmalloc();
}

NOGVL_SAFE int f2_without_gvl() {
  if (has_gvl()) {
    return 1;
  } else {
    rb_thread_call_with_gvl(ruby_xmalloc);
    return 0;
  }
}

NOGVL_SAFE int f3_without_gvl() {
  GVL_GUARD bool gvl = true;
  if (gvl) {
    ruby_xmalloc();
    return 0;
  } else {
    return 1;
  }
}

NOGVL_SAFE int f4_without_gvl() {
  if (has_gvl()) {
    ruby_xmalloc();
    return 1;
  } else {
    // CHECK-MESSAGES: :[[@LINE+1]]:6: error: Call to NoGVL unsafe function 'ruby_xmalloc' in a NoGVL safe context [datadogruby-without-gvl]
    ruby_xmalloc();
    return 0;
  }
}

NOGVL_SAFE int f5_without_gvl() {
  // CHECK-MESSAGES: :[[@LINE+1]]:3: error: Call to NoGVL unsafe function 'ruby_xmalloc' in a NoGVL safe context [datadogruby-without-gvl]
  ruby_xmalloc();
  return 0;
}

// CHECK-MESSAGES: :[[@LINE+1]]:5: error: Function is being called from a NoGVL safe context in 'this_runs_without_gvl' but not marked as NoGVL safe [datadogruby-without-gvl]
int f6_without_gvl() {
  ruby_xmalloc();
  return 0;
}

NOGVL_SAFE int f7_without_gvl() {
  // CHECK-MESSAGES: :[[@LINE+1]]:7: error: Call to NoGVL unsafe function 'ruby_check' in a NoGVL safe context [datadogruby-without-gvl]
  if (ruby_check()) {
    // CHECK-MESSAGES: :[[@LINE+1]]:5: error: Call to NoGVL unsafe function 'ruby_xmalloc' in a NoGVL safe context [datadogruby-without-gvl]
    ruby_xmalloc();
  }
  return 0;
}

NOGVL_SAFE void this_runs_without_gvl() {
  f1_without_gvl();
  f2_without_gvl();
  f3_without_gvl();
  f4_without_gvl();
  f5_without_gvl();
  // CHECK-MESSAGES: :[[@LINE+1]]:6: error: Call to NoGVL unsafe function 'f6_without_gvl' in a NoGVL safe context [datadogruby-without-gvl]
  f6_without_gvl();
  f7_without_gvl();
}

int main() {
  rb_thread_call_without_gvl(this_runs_without_gvl);
  // CHECK-MESSAGES: :[[@LINE+1]]:30: error: Calling a NoGVL unsafe function 'ruby_xmalloc' while transitioning to not holding the GVL [datadogruby-without-gvl]
  rb_thread_call_without_gvl(ruby_xmalloc);
  return 0;
}

