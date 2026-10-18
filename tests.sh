fail_count=0
total_count=0

emitResult() {
  if [ $3 -ne $2 ]; then
    echo -e "\e[0;31mFail '$1'\e[0m"
    ((fail_count++))
  else
    echo -e "\e[0;32mSuccess '$1'\e[0m"
  fi
}

checkExitCode() {
  ((total_count++))
  build/stra tests/$1.stra --output test.out --run &>/dev/null
  emitResult $1 $2 $?
}

checkExitCode if 0
checkExitCode for 0
checkExitCode switch 0
checkExitCode defer 0
checkExitCode function 0
checkExitCode struct 0
checkExitCode enum 0
checkExitCode union 0
checkExitCode namespace 0
checkExitCode pointer 0
checkExitCode slice 0
checkExitCode int_cast 0
checkExitCode float_cast 0
checkExitCode slice_cast 0
checkExitCode struct_self_pointer 0
checkExitCode typeid 0
checkExitCode nested_lookup 0

checkExitCode linking/import 0
checkExitCode linking/link_name 0
checkExitCode linking/mutual 0

checkExitCode comptime/sizeof 0
checkExitCode comptime/alignof 0
checkExitCode comptime/comptime 0
checkExitCode comptime/builtin_global 0
checkExitCode comptime/recursion 0

rm test.out
if [ $fail_count -ne 0 ]; then
  echo -e "\e[0;31m$fail_count/$total_count Test(s) failed\e[0m"
fi
