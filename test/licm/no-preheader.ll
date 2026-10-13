; Two outside predecessors mean there is no dedicated loop preheader.
; CHECK-LABEL: define i32 @no_preheader(
; CHECK: loop:
; CHECK: %invariant = add i32 %a, %b
define i32 @no_preheader(i1 %choose, i32 %a, i32 %b) {
entry:
  br i1 %choose, label %left, label %right
left:
  br label %loop
right:
  br label %loop
loop:
  %i = phi i32 [ 0, %left ], [ 1, %right ], [ %next, %loop ]
  %invariant = add i32 %a, %b
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 8
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %invariant
}

