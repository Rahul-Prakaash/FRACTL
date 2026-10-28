; ModuleID = 'tree3d'
source_filename = "tree3d"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%instance = type { double, double, double, double, double, i64, i32 }

@out_path = private unnamed_addr constant [17 x i8] c"build/tree3d.f3d\00", align 1
@instances = private unnamed_addr constant [1 x %instance] [%instance { double 0.000000e+00, double 0.000000e+00, double 0.000000e+00, double 0.000000e+00, double 1.000000e+00, i64 42, i32 0 }]
@name.Tree = private unnamed_addr constant [5 x i8] c"Tree\00", align 1
@colors.Tree = private unnamed_addr constant [24 x i8] c"#4a2f1c,#3f8a2a,#7cbd45\00", align 1

declare void @turtle3_init(ptr)

declare void @turtle3_begin(double, double, double, double, double, double, double, ptr, ptr)

declare void @turtle3_forward(double)

declare void @turtle3_blade(double)

declare void @turtle3_move(double)

declare void @turtle3_yaw(double)

declare void @turtle3_pitch(double)

declare void @turtle3_roll(double)

declare void @turtle3_thin(double)

declare void @turtle3_push()

declare void @turtle3_pop()

declare void @turtle3_color()

declare void @turtle3_end()

declare void @turtle3_finish()

declare void @fractl_seed(i64)

declare double @fractl_rand(i32)

define internal void @Tree.A(i32 %d) {
entry:
  %done = icmp eq i32 %d, 0
  br i1 %done, label %leaf, label %grow

leaf:                                             ; preds = %entry
  ret void

grow:                                             ; preds = %entry
  %next = sub i32 %d, 1
  %level = sub i32 7, %d
  %r = call double @fractl_rand(i32 %level)
  %x = fmul double %r, 1.000000e+00
  %0 = fcmp olt double %x, 6.500000e-01
  br i1 %0, label %p0, label %t1

p0:                                               ; preds = %grow
  call void @turtle3_push()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @Tree.F(i32 %next)
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_color()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @turtle3_pitch(double 2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_yaw(double 1.800000e+02)
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_thin(double 7.800000e-01)
  call void @Tree.A(i32 %next)
  call void @turtle3_pop()
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_push()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @Tree.F(i32 %next)
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_thin(double 7.800000e-01)
  call void @Tree.A(i32 %next)
  call void @turtle3_pop()
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_push()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @Tree.F(i32 %next)
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_color()
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_thin(double 7.800000e-01)
  call void @Tree.A(i32 %next)
  call void @turtle3_pop()
  ret void

t1:                                               ; preds = %grow
  br label %p1

p1:                                               ; preds = %t1
  call void @turtle3_push()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @Tree.F(i32 %next)
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_pitch(double -2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_color()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_thin(double 7.800000e-01)
  call void @Tree.A(i32 %next)
  call void @turtle3_pop()
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_roll(double -2.250000e+01)
  call void @turtle3_push()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @turtle3_pitch(double 2.250000e+01)
  call void @Tree.F(i32 %next)
  call void @turtle3_push()
  call void @turtle3_color()
  call void @turtle3_color()
  call void @turtle3_pitch(double 2.250000e+01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_blade(double 6.000000e-01)
  call void @turtle3_pop()
  call void @turtle3_thin(double 7.800000e-01)
  call void @Tree.A(i32 %next)
  call void @turtle3_pop()
  ret void
}

define internal void @Tree.F(i32 %d) {
entry:
  %done = icmp eq i32 %d, 0
  br i1 %done, label %leaf, label %grow

leaf:                                             ; preds = %entry
  call void @turtle3_forward(double 6.000000e-01)
  ret void

grow:                                             ; preds = %entry
  %next = sub i32 %d, 1
  %level = sub i32 7, %d
  %r = call double @fractl_rand(i32 %level)
  %x = fmul double %r, 1.000000e+00
  %0 = fcmp olt double %x, 5.000000e-01
  br i1 %0, label %p0, label %t1

p0:                                               ; preds = %grow
  call void @Tree.F(i32 %next)
  ret void

t1:                                               ; preds = %grow
  br label %p1

p1:                                               ; preds = %t1
  call void @Tree.F(i32 %next)
  call void @Tree.F(i32 %next)
  ret void
}

define void @fractl_main() {
entry:
  call void @turtle3_init(ptr @out_path)
  br label %instance

instance:                                         ; preds = %next, %entry
  %i = phi i64 [ 0, %entry ], [ %i.next, %next ]
  %row = getelementptr inbounds [1 x %instance], ptr @instances, i64 0, i64 %i
  %0 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 0
  %x = load double, ptr %0, align 8
  %1 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 1
  %y = load double, ptr %1, align 8
  %2 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 2
  %z = load double, ptr %2, align 8
  %3 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 3
  %yaw = load double, ptr %3, align 8
  %4 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 4
  %scale = load double, ptr %4, align 8
  %5 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 5
  %seed = load i64, ptr %5, align 8
  %6 = getelementptr inbounds nuw %instance, ptr %row, i32 0, i32 6
  %fractal = load i32, ptr %6, align 4
  switch i32 %fractal, label %next [
    i32 0, label %grow.Tree
  ]

grow.Tree:                                        ; preds = %instance
  call void @turtle3_begin(double %x, double %y, double %z, double %yaw, double %scale, double 1.400000e+00, double 7.800000e-01, ptr @name.Tree, ptr @colors.Tree)
  call void @fractl_seed(i64 %seed)
  call void @Tree.axiom()
  call void @turtle3_end()
  br label %next

next:                                             ; preds = %grow.Tree, %instance
  %i.next = add i64 %i, 1
  %7 = icmp ult i64 %i.next, 1
  br i1 %7, label %instance, label %done

done:                                             ; preds = %next
  call void @turtle3_finish()
  ret void
}

define internal void @Tree.axiom() {
entry:
  call void @Tree.F(i32 7)
  call void @Tree.A(i32 7)
  ret void
}

define i32 @main() {
entry:
  call void @fractl_main()
  ret i32 0
}
