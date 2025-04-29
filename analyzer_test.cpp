// analyzer_test.cpp

// Compile with C++20: g++ -std=c++20 -o analyzer_test_cpp20 analyzer_test.cpp -pthread -Wall -Wextra
// Compile with C++23: g++ -std=c++23 -o analyzer_test_cpp23 analyzer_test.cpp -pthread -Wall -Wextra
// Clang: clang++ -std=c++20 -o analyzer_test_cpp20 analyzer_test.cpp -pthread -Wall -Wextra
// Clang: clang++ -std=c++23 -o analyzer_test_cpp23 analyzer_test.cpp -pthread -Wall -Wextra
// For data race detection runtime checks (dynamic analysis): g++ -std=c++23 -fsanitize=thread ...
// To run gcc analyzer: gcc -fanalyzer -std=c++23 -o analyzer_test_cpp23 analyzer_test.cpp -pthread -Wall -Wextra
// To run clang analyzer: clang++ -std=c++23 --analyze -o analyzer_test_cpp23 analyzer_test.cpp -pthread -Wall -Wextra

#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <memory>    // For new/delete
#include <limits>    // For numeric_limits
#include <iterator>  // For std::distance
#include <cstdio>    // For std::printf, std::scanf, FILE*, fopen, fclose, remove
#include <cstring>   // For std::memcpy
#include <future>    // For std::async
#include <numeric>   // For std::accumulate
#include <sstream>   // For string performance example
#include <cmath>     // For std::sqrt, std::abs, NAN, INFINITY

// C++20 specific includes
#if __cplusplus >= 202002L
#include <format>  // C++20
#include <span>    // C++20
#include <ranges>  // C++20
#endif

// C++23 specific includes
#if __cplusplus > 202002L  // Typically __cplusplus will be 202101L or higher for C++23
#include <print>           // C++23
#include <expected>        // C++23
#endif

// --- Helper Struct/Classes ---
struct LargeObject {
  long long data[1024];

  LargeObject() { data[0] = 1; }
};

// Base class for OO Demos
class BaseOO {
public:
  BaseOO() { /* std::cout << "BaseOO Constructor\n"; */
  }          // Quieten output for clarity

  // PROBLEM (OO #1): Base class has virtual functions but non-virtual destructor.
  // Deleting DerivedOO via BaseOO* invokes UB. Analyzer should flag this.
  // virtual ~BaseOO() { std::cout << "BaseOO Virtual Destructor\n"; } // Correct Fix
  ~BaseOO() { std::cout << "BaseOO Non-Virtual Destructor Called\n"; }  // Problematic

  virtual void print() const { std::cout << "BaseOO print\n"; }

  void base_only_method() { std::cout << "Base only method\n"; }
};

class DerivedOO : public BaseOO {
public:
  std::string derived_data;  // Extra data member

  DerivedOO()
  : derived_data("Derived Data") { /* std::cout << "DerivedOO Constructor\n"; */
  }

  ~DerivedOO() { std::cout << "DerivedOO Destructor Called\n"; }  // This won't be called via Base* delete if Base::~BaseOO is not virtual

  void print() const override { std::cout << "DerivedOO print: " << derived_data << "\n"; }

  void derived_only_method() { std::cout << "Derived only method\n"; }
};

// --- Problem Demonstrations ---

// --- Core Language & Memory Issues ---

// 1. Uninitialized Variable
void demo_uninitialized_variable() { /* ... see previous code ... */
  std::cout << "\n--- 1. Uninitialized Variable Demo ---" << std::endl;
  int x;                          // POTENTIAL PROBLEM: Uninitialized
  if (/* condition && */ x > 0) { /* UB */
  }
  std::cout << "Checked uninitialized variable usage." << std::endl;
}

// 2. Potential Null Pointer Dereference
void demo_nullptr_dereference(int* ptr) { /* ... see previous code ... */
  std::cout << "\n--- 2. Null Pointer Dereference Demo ---" << std::endl;
  // *ptr = 10; // PROBLEM: Potential dereference before check.
  if (ptr) {
    std::cout << "Checked potential null pointer dereference." << std::endl;
  } else {
    std::cout << "Null pointer passed for demo." << std::endl;
  }
}

// 3. Out-of-Bounds Access
void demo_out_of_bounds() { /* ... see previous code ... */
  std::cout << "\n--- 3. Out-of-Bounds Access Demo ---" << std::endl;
  std::vector<int> data = {10, 20, 30};
  int index = 5;
  // data[index] = 1; // PROBLEM: Out of bounds access.
  std::cout << "Checked out-of-bounds access (index " << index << " vs size " << data.size() << ")." << std::endl;
}

// 4. Memory Leak
void demo_memory_leak() { /* ... see previous code ... */
  std::cout << "\n--- 4. Memory Leak Demo ---" << std::endl;
  int* leaky_ptr = new int(42);  // PROBLEM: Leaked memory.
  // delete leaky_ptr; // Missing delete.
  std::cout << "Checked memory leak (missing delete)." << std::endl;
}

// 5. Resource Management Issues
void demo_resource_management() {
  std::cout << "\n--- 5. Resource Management Issues Demo ---" << std::endl;

  // PROBLEM: Double delete
  int* ptr_double = new int(1);
  delete ptr_double;
  delete ptr_double;  // UB. Analyzer should flag.

  // PROBLEM: Mismatched new/delete[]
  int* ptr_mismatch1 = new int[10];
  delete ptr_mismatch1;  // UB. Should be delete[]. Analyzer should flag.
  //delete[] ptr_mismatch1;  // Correct cleanup for demo run

  int* ptr_mismatch2 = new int(3);
  delete[] ptr_mismatch2;  // UB. Should be delete. Analyzer should flag.
  delete ptr_mismatch2;    // Correct cleanup for demo run

  // PROBLEM: Leaked file handle (RAII is better: std::ofstream, std::ifstream)
  FILE* fp = std::fopen("temp_analyzer_test_resource.txt", "w");
  if (fp) {
    std::fprintf(fp, "Temporary file.\n");
    // Missing std::fclose(fp) on some path (e.g., early return, exception) is a leak.
    // Analyzers often track resources like file handles.
    if (true) {  // Simulate a path where fclose might be missed
      std::cout << "Opened file handle (potential leak path without RAII)." << std::endl;
      // fclose(fp); // If missing here, it leaks.
    }
    fclose(fp);                                      // Close it for demo run cleanliness
    std::remove("temp_analyzer_test_resource.txt");  // Clean up file
  } else {
    std::cerr << "Warning: Could not open temporary file for resource leak demo." << std::endl;
  }
  std::cout << "Checked double delete, mismatched new/delete, file leak." << std::endl;
}

// --- Numerical Issues ---

// 6. Division By Zero
void demo_division_by_zero(int int_divisor, double double_divisor) {
  std::cout << "\n--- 6. Division By Zero Demo ---" << std::endl;

  // Integer division
  // int int_result = 100 / int_divisor; // PROBLEM: Potential integer division by zero (UB).
  if (int_divisor != 0) {
    std::cout << "Integer division ok." << std::endl;
  } else {
    std::cout << "Integer division by zero skipped." << std::endl;
  }

  // Floating point division
  // double fp_result = 1.0 / double_divisor; // PROBLEM?: Results in +/- INF, which might be intended or an error.
  // Analyzers might flag division by potential zero float depending on context/settings.
  if (double_divisor != 0.0) {
    double fp_result = 1.0 / double_divisor;
    std::cout << "Floating point division result: " << fp_result << std::endl;
  } else {
    double fp_result_inf = 1.0 / double_divisor;  // Results in INF
    std::cout << "Floating point division by zero result: " << fp_result_inf << std::endl;
  }
}

// 7. Other Numerical Issues
void demo_numerical_issues() {
  std::cout << "\n--- 7. Numerical Issues Demo ---" << std::endl;

  // Floating point comparison
  double x = 0.1 + 0.1 + 0.1;  // Likely 0.30000000000000004
  double y = 0.3;
  // PROBLEM: Direct comparison of floats is often unreliable. Analyzers might flag '==' with floats.
  if (x == y) { /* Unlikely branch */
  } else {
    std::cout << "Checked floating point comparison (x != y is expected)." << std::endl;
  }
  // Better: check if std::abs(x-y) < epsilon

  // Integer truncation / Loss of precision
  double high_precision = 123.789;
  // PROBLEM: Assigning floating point to integer truncates. Potential loss of data. Analyzer may flag.
  int truncated = high_precision;
  std::cout << "Checked integer truncation: " << high_precision << " -> " << truncated << std::endl;
  long long large_ll = 3'000'000'000LL;
  // PROBLEM: Assigning larger integer type to smaller is potential truncation/overflow.
  int small_int = large_ll;  // Value likely changes.
  std::cout << "Checked large->small integer conversion: " << large_ll << " -> " << small_int << std::endl;

  // Bit shifting issues
  int val = 1;
  int shift_amount = 35;  // PROBLEM: Shifting >= width of type (assuming 32-bit int) is UB.
  // int shifted = val << shift_amount; // UB
  int negative_shift = -5;  // PROBLEM: Shifting by negative amount is UB.
  // int shifted_neg = val << negative_shift; // UB
  std::cout << "Checked invalid bit shifts (commented out UB)." << std::endl;

  // Unsigned integer wrap-around
  unsigned int u_val = 0;
  u_val--;  // Well-defined (wraps to UINT_MAX), but sometimes a logic error. Analyzers might flag contextually.
  std::cout << "Checked unsigned integer wrap-around: 0u - 1u = " << u_val << std::endl;

  // Potential NaN/Inf generation
  double negative_val = -1.0;
  // PROBLEM: sqrt of negative number results in NaN. Analyzer might flag if input can be negative.
  double result_nan = std::sqrt(negative_val);
  std::cout << "Checked potential NaN from sqrt(-1): " << result_nan << std::endl;
  double zero = 0.0;
  // double result_inf_log = std::log(zero); // Results in -INF. PROBLEM if unexpected.
  // std::cout << "Checked potential Inf from log(0): " << result_inf_log << std::endl;
}

// 8. Integer Overflow
void demo_integer_overflow() { /* ... see previous code ... */
  std::cout << "\n--- 8. Integer Overflow Demo ---" << std::endl;
  int max_val = std::numeric_limits<int>::max();
  // int potentially_overflowing = max_val + 1; // PROBLEM: Signed overflow is UB.
  std::cout << "Checked signed integer overflow (commented out UB)." << std::endl;
}

// --- Concurrency Issues ---

// 9. Data Race
long long demo9_shared_counter = 0;

void unsafe_increment9() {
  for (int i = 0; i < 10000; ++i) {
    demo9_shared_counter++;  // PROBLEM: Unprotected RMW access = Data Race
  }
}

void demo_data_race() { /* ... see previous code, using demo9_shared_counter & unsafe_increment9 ... */
  std::cout << "\n--- 9. Data Race Demo ---" << std::endl;
  std::thread t1(unsafe_increment9);
  std::thread t2(unsafe_increment9);
  t1.join();
  t2.join();
  std::cout << "Checked data race (result likely != 20000: " << demo9_shared_counter << ")." << std::endl;
}

// 10. Deadlock
std::mutex demo10_mutex1;
std::mutex demo10_mutex2;

void deadlock_thread_func1_10() { /* ... see previous code locking 1 then 2 ... */
  std::lock_guard<std::mutex> lock1(demo10_mutex1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  std::lock_guard<std::mutex> lock2(demo10_mutex2);  // PROBLEM: Waits for Thread 2
}

void deadlock_thread_func2_10() { /* ... see previous code locking 2 then 1 ... */
  std::lock_guard<std::mutex> lock2(demo10_mutex2);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  std::lock_guard<std::mutex> lock1(demo10_mutex1);  // PROBLEM: Waits for Thread 1
}

void demo_deadlock() { /* ... see previous code ... */
  std::cout << "\n--- 10. Deadlock Demo ---" << std::endl;
  std::cout << "(Deadlock demo threads started - may hang!)" << std::endl;
  std::thread t1(deadlock_thread_func1_10);
  std::thread t2(deadlock_thread_func2_10);
  // Note: joining might hang here if deadlock occurs.
  t1.join();
  t2.join();  // If we get here, deadlock didn't happen this run.
  std::cout << "Deadlock demo threads joined (if successful)." << std::endl;
}

// --- API Usage & Control Flow ---

// 11. API Misuse
void demo_api_misuse() { /* ... see previous code ... */
  std::cout << "\n--- 11. API Misuse Demo ---" << std::endl;
  std::printf("Mismatch format: %d\n", "hello");  // PROBLEM: printf format mismatch
  char buffer[] = "123456789";
  std::memcpy(buffer + 2, buffer, 5);  // PROBLEM: Overlapping memcpy
  std::cout << "Checked API misuse (printf format, memcpy overlap)." << std::endl;
}

// 12. Unchecked Return Values
void demo_unchecked_return() { /* ... see previous code ... */
  std::cout << "\n--- 12. Unchecked Return Values Demo ---" << std::endl;
  int value;
  std::scanf("%d", &value);  // PROBLEM: scanf return ignored
  std::mutex mtx;
  mtx.try_lock();                                    // PROBLEM: try_lock return ignored
  std::async(std::launch::async, [] { return 5; });  // PROBLEM: async future ignored
  std::cout << "Checked unchecked return values (scanf, try_lock, async)." << std::endl;
  // Clear stdin buffer after potential bad input
  std::scanf("%*[^\n]");
  std::scanf("%*c");
}

// 13. Control Flow Issues
void demo_control_flow() { /* ... see previous code ... */
  std::cout << "\n--- 13. Control Flow Demo ---" << std::endl;
  int i = 1;
  // identical code
  if (i > 2) {
    i = 2;  // unreachable
    std::cout << "If and else are identical\n";
  } else {
    i = 2;
    std::cout << "If and else are identical\n";
  }

  std::vector<int> empty_vec_13;
  for (size_t i = 0; i > empty_vec_13.size(); ++i) { /* PROBLEM: Unreachable loop body */
  }
  std::cout << "Checked control flow issues (unreachable loop)." << std::endl;
}

// 14. Unreachable Code
int demo_unreachable_code(int input) { /* ... see previous code ... */
  std::cout << "\n--- 14. Unreachable Code Demo ---" << std::endl;
  return -1;
  std::cout << "Unreachable line.";  // PROBLEM: Code after return
  if (false) {
    std::cout << "Unreachable block.";
  }  // PROBLEM: False condition
  std::cout << "Checked unreachable code." << std::endl;
  return 0;  // Added return to satisfy function signature after commenting out loop
}

// --- Logic & Style Issues ---

// 15. Logic Errors
void demo_logic_errors() { /* ... see previous code ... */
  std::cout << "\n--- 15. Logic Errors Demo ---" << std::endl;
  int a = 0, b = 1;
  if (a = b) {
  }  // PROBLEM: Assignment in condition
  int flags = 2, mask = 1;
  if ((flags | mask) != 0) {
  }  // PROBLEM?: Bitwise OR (|) instead of logical OR (||) or bit check (&)
  std::cout << "Checked logic errors (assignment in condition, bitwise vs logical)." << std::endl;
}

// 16. Miscellaneous Analyzer Warnings
// Add unused parameter to signature for demo
void demo_misc_analyzer_warnings(int used_param, int unused_param) {
  std::cout << "\n--- 16. Miscellaneous Analyzer Warnings Demo ---" << std::endl;

  // PROBLEM: Magic numbers
  if (used_param > 3600) { /* Magic number 3600 */
  }
  std::cout << "Checked magic numbers." << std::endl;

  // PROBLEM: Unused variable / parameter
  int unused_local_var = 10;  // Never used. Analyzer/Compiler warning.
  // 'unused_param' is also unused. Analyzer/Compiler warning.
  std::cout << "Checked unused variable/parameter ('unused_local_var', 'unused_param')." << std::endl;

  // PROBLEM: Shadowing variable
  int outer_scope_var = 100;
  { int outer_scope_var = 200; /* Inner shadows outer */ }
  std::cout << "Checked variable shadowing." << std::endl;

  // PROBLEM: Const correctness / const_cast misuse
  const int const_val = 50;
  int* non_const_ptr = const_cast<int*>(&const_val);
  // *non_const_ptr = 60; // UB! Modifying const object via const_cast. Analyzer might warn.
  std::cout << "Checked const_cast misuse (commented out UB)." << std::endl;
}

// 17. Nesting Issues
void demo_nesting(int level) { /* ... see previous code ... */
  std::cout << "\n--- 17. Nesting Issues Demo ---" << std::endl;
  if (level > 0) {
    if (level > 1) {
      if (level > 2) {
        if (level > 3) {
          if (level > 4) { /* Deep */
          }
        }
      }
    }
  }
  std::cout << "Checked deep nesting (level " << level << ")." << std::endl;
}

// 18. Performance Issues
void process_large_object_by_value(LargeObject obj) { /* ... */
}  // PROBLEM: Pass by value

void demo_performance() { /* ... see previous code ... */
  std::cout << "\n--- 18. Performance Issues Demo ---" << std::endl;
  LargeObject obj;
  process_large_object_by_value(obj);  // Pass by value
  std::string res;
  std::vector<std::string> parts = {"a", "b", "c"};
  for (const auto& p : parts) {
    res = res + p;  // PROBLEM: String concat in loop
  }
  for (int i = 0; i < 3; ++i) {
    std::cout << i << std::endl;  // PROBLEM: Excessive endl flush
  }
  std::cout << "Checked performance issues (pass-by-value, string concat, endl)." << std::endl;
}

// --- Object Oriented Issues ---

// 19. Object Oriented Issues
void demo_oo_issues() {
  std::cout << "\n--- 19. Object Oriented Issues Demo ---" << std::endl;

  std::cout << "Testing missing virtual destructor:" << std::endl;
  BaseOO* base_ptr = new DerivedOO();  // Create derived obj
  // PROBLEM (OO #1 triggered): Deleting via base ptr w/o virtual base dtor.
  // DerivedOO::~DerivedOO will NOT be called. UB / Resource leak. Analyzer flags this.
  delete base_ptr;
  std::cout << "---" << std::endl;

  std::cout << "Testing object slicing:" << std::endl;
  DerivedOO derived_obj;
  // PROBLEM (OO #2): Assigning derived to base by value slices off derived parts.
  BaseOO base_obj = derived_obj;  // derived_data member is lost. Analyzer might flag.
  base_obj.print();               // Calls BaseOO::print, not DerivedOO::print
  std::cout << "Checked object slicing." << std::endl;
}

// --- C++20/23 Features (Existing, combined section) ---
void demo_cpp_latest_features() {
#if __cplusplus >= 202002L
  std::cout << std::format("\n--- C++20 Features Demo ({}) ---", __cplusplus) << std::endl;
  // Span with potential bad size
  int arr[] = {1, 2};
  std::span<int> risky_span(arr, 5);  // PROBLEM: Span larger than buffer.
  // Range transform with potential issue
  std::vector<int> numbers = {1, 2, 0, 4};
  auto transformation = [](int n) -> std::optional<int> { if (n == 0){ return std::nullopt;
} return n*n; };
  auto results = numbers | std::views::transform(transformation);  // PROBLEM?: Need to check optional later
  std::cout << "Checked C++20 span bounds, range optional result." << std::endl;
#endif

#if __cplusplus > 202002L
  std::print("\n--- C++23 Features Demo ({}) ---\n", __cplusplus);
  auto exp_res = std::expected<int, std::string>(std::unexpected("Error"));
  // *exp_res = 1; // PROBLEM: Accessing unexpected value via operator* is UB.
  std::print("Checked C++23 expected access.\n");
#endif

#if __cplusplus < 202002L
  std::cout << "\n--- No C++20/23 Features Available ({}) ---" << __cplusplus << std::endl;
#endif
}

// --- Main Function ---
int main() {
  std::cout << "===== Starting Extended Static Analyzer Test Code =====" << std::endl;
  std::cout << "Compiled with C++ Standard: " << __cplusplus << std::endl;

  // --- Call Demo Functions ---
  demo_uninitialized_variable();
  demo_nullptr_dereference(nullptr);
  demo_out_of_bounds();
  demo_memory_leak();
  demo_resource_management();

  demo_division_by_zero(0, 0.0);  // Pass zero divisors
  demo_numerical_issues();
  demo_integer_overflow();

  // Concurrency demos (run cautiously)
  demo_data_race();
  // demo_deadlock(); // UNCOMMENT CAUTIOUSLY - INTENDED TO HANG

  // API, Control Flow
  demo_api_misuse();
  demo_unchecked_return();  // Requires user input - type 'abc' then Enter
  demo_control_flow();
  demo_unreachable_code(5);

  // Logic, Style, Misc
  demo_logic_errors();
  demo_misc_analyzer_warnings(4000, 99);  // Pass args (one used, one unused)
  demo_nesting(5);                        // Trigger deep nesting check
  demo_performance();

  // Object Oriented
  demo_oo_issues();

  // C++20 / C++23 Features
  demo_cpp_latest_features();

  std::cout << "\n===== Finished Extended Static Analyzer Test Code =====" << std::endl;
  return 0;
}