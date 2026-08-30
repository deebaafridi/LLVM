; ModuleID = 'example.c'
source_filename = "example.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [14 x i8] c"Result x: %d\0A\00", align 1
@.str.1 = private unnamed_addr constant [14 x i8] c"Result y: %d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 3, i32* %a, align 4
  store i32 7, i32* %b, align 4
  store i32 0, i32* %x, align 4
  store i32 0, i32* %y, align 4
  %0 = load i32, i32* %a, align 4
  %cmp = icmp sgt i32 %0, 0
  br i1 %cmp, label %if.then, label %if.else3

if.then:                                          ; preds = %entry
  %1 = load i32, i32* %b, align 4
  %cmp1 = icmp sgt i32 %1, 5
  br i1 %cmp1, label %if.then2, label %if.else

if.then2:                                         ; preds = %if.then
  %2 = load i32, i32* %a, align 4
  %add = add nsw i32 %2, 10
  store i32 %add, i32* %x, align 4
  br label %if.end

if.else:                                          ; preds = %if.then
  %3 = load i32, i32* %a, align 4
  %sub = sub nsw i32 %3, 5
  store i32 %sub, i32* %x, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then2
  br label %if.end10

if.else3:                                         ; preds = %entry
  %4 = load i32, i32* %b, align 4
  %cmp4 = icmp sgt i32 %4, 5
  br i1 %cmp4, label %if.then5, label %if.else7

if.then5:                                         ; preds = %if.else3
  %5 = load i32, i32* %b, align 4
  %add6 = add nsw i32 %5, 10
  store i32 %add6, i32* %y, align 4
  br label %if.end9

if.else7:                                         ; preds = %if.else3
  %6 = load i32, i32* %b, align 4
  %sub8 = sub nsw i32 %6, 5
  store i32 %sub8, i32* %y, align 4
  br label %if.end9

if.end9:                                          ; preds = %if.else7, %if.then5
  br label %if.end10

if.end10:                                         ; preds = %if.end9, %if.end
  %7 = load i32, i32* %x, align 4
  %call = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([14 x i8], [14 x i8]* @.str, i64 0, i64 0), i32 noundef %7)
  %8 = load i32, i32* %y, align 4
  %call11 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([14 x i8], [14 x i8]* @.str.1, i64 0, i64 0), i32 noundef %8)
  ret i32 0
}

declare dso_local i32 @printf(i8* noundef, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 14.0.6 (https://github.com/llvm/llvm-project.git f28c006a5895fc0e329fe15fead81e37457cb1d1)"}
