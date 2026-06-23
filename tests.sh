fail_count=0
total_count=0

check() {
  ((total_count++))
  build/stra tests/$1.stra --output test.out --run >/dev/null
  if [ $? -ne $2 ]; then
    echo -e "\e[0;31mFail '$1'\e[0m"
    ((fail_count++))
  else
    echo -e "\e[0;32mSuccess '$1'\e[0m"
  fi
}

check if 0
check for 0
check int_cast 0
check float_cast 0
check slice_cast 0
check sizeof 0
check alignof 0

rm test.out
if [ $fail_count -ne 0 ]; then
  echo -e "\e[0;31m$fail_count/$total_count Test(s) failed\e[0m"
fi
