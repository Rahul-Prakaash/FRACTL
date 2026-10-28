; ModuleID = 'carpet'
source_filename = "carpet"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@svg_path = private unnamed_addr constant [17 x i8] c"build/carpet.svg\00", align 1

declare void @turtle_init(double, double, double, double, double, ptr)

declare void @turtle_forward(double)

declare void @turtle_move(double)

declare void @turtle_turn(double)

declare void @turtle_push()

declare void @turtle_pop()

declare void @turtle_finish()

define internal void @Carpet.F(i32 %d) {
entry:
  %done = icmp eq i32 %d, 0
  br i1 %done, label %leaf, label %grow

leaf:                                             ; preds = %entry
  call void @turtle_forward(double 5.000000e+00)
  ret void

grow:                                             ; preds = %entry
  %next = sub i32 %d, 1
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double 9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double -9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double -9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double -9.000000e+01)
  call void @Carpet.f(i32 %next)
  call void @turtle_turn(double 9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double 9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double 9.000000e+01)
  call void @Carpet.F(i32 %next)
  call void @turtle_turn(double -9.000000e+01)
  call void @Carpet.F(i32 %next)
  ret void
}

define internal void @Carpet.f(i32 %d) {
entry:
  %done = icmp eq i32 %d, 0
  br i1 %done, label %leaf, label %grow

leaf:                                             ; preds = %entry
  call void @turtle_move(double 5.000000e+00)
  ret void

grow:                                             ; preds = %entry
  %next = sub i32 %d, 1
  call void @Carpet.f(i32 %next)
  call void @Carpet.f(i32 %next)
  call void @Carpet.f(i32 %next)
  ret void
}

define void @fractl_main() {
entry:
  call void @turtle_init(double 5.000000e+00, double 9.000000e+01, double 0.000000e+00, double 0.000000e+00, double 0.000000e+00, ptr @svg_path)
  call void @Carpet.axiom()
  call void @turtle_finish()
  ret void
}

define internal void @Carpet.axiom() {
entry:
  call void @Carpet.F(i32 4)
  ret void
}

define i32 @main() {
entry:
  call void @fractl_main()
  ret i32 0
}
