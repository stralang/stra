; ModuleID = 'if.stra'
source_filename = "if.stra"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

define i32 @main() {
defines:
  %return_staging = alloca i32, align 4
  %main6output = alloca i32, align 4
  br label %entry

entry:                                            ; preds = %defines
  store i32 12, ptr %main6output, align 4
  %0 = load i32, ptr %main6output, align 4
  %1 = icmp eq i32 %0, 12
  br i1 %1, label %if_then, label %else_body

if_then:                                          ; preds = %entry
  store i32 1, ptr %main6output, align 4
  br label %if_merge

else_body:                                        ; preds = %entry
  %2 = load i32, ptr %main6output, align 4
  %3 = icmp eq i32 %2, 10
  br i1 %3, label %if_then1, label %else_body2

if_merge:                                         ; preds = %if_then
  %4 = load i32, ptr %main6output, align 4
  %5 = icmp eq i32 %4, 1
  %6 = xor i1 %5, true
  %7 = zext i1 %6 to i32
  store i32 %7, ptr %return_staging, align 4
  %8 = load i32, ptr %return_staging, align 4
  ret i32 %8

if_then1:                                         ; preds = %else_body
  store i32 2, ptr %main6output, align 4
  br label %if_merge3

else_body2:                                       ; preds = %else_body2, %else_body
  store i32 3, ptr %main6output, align 4
  br label %else_body2

if_merge3:                                        ; preds = %if_merge3, %if_then1
  br label %if_merge3
}
