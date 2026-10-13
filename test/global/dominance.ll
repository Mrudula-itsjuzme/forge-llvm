; CHECK-LABEL: define i32 @dominating_diamond(
; CHECK: %root = add i32 %a, %b
; CHECK: left:
; CHECK-NOT: add i32 %a, %b
; CHECK: br label %merge
; CHECK: right:
; CHECK-NOT: add i32 %a, %b
; CHECK: br label %merge
; CHECK: merge:
; CHECK: %r = phi i32 [ %root, %left ], [ %root, %right ]
define i32 @dominating_diamond(i1 %cond, i32 %a, i32 %b) {
entry:
  %root = add i32 %a, %b
  br i1 %cond, label %left, label %right
left:
  %l = add i32 %a, %b
  br label %merge
right:
  %rgt = add i32 %b, %a
  br label %merge
merge:
  %r = phi i32 [ %l, %left ], [ %rgt, %right ]
  ret i32 %r
}

; Neither arm dominates the merge. All three adds must remain.
; CHECK-LABEL: define i32 @non_dominating(
; CHECK: left:
; CHECK: %l = add i32 %a, %b
; CHECK: right:
; CHECK: %r = add i32 %a, %b
; CHECK: merge:
; CHECK: %p = phi i32 [ %l, %left ], [ %r, %right ]
; CHECK: %m = add i32 %a, %b
define i32 @non_dominating(i1 %cond, i32 %a, i32 %b) {
entry:
  br i1 %cond, label %left, label %right
left:
  %l = add i32 %a, %b
  br label %merge
right:
  %r = add i32 %a, %b
  br label %merge
merge:
  %p = phi i32 [ %l, %left ], [ %r, %right ]
  %m = add i32 %a, %b
  %out = xor i32 %m, %p
  ret i32 %out
}

; CHECK-LABEL: define i32 @loop_dominance(
; CHECK: %pre = mul i32 %a, %b
; CHECK: loop:
; CHECK-NOT: mul i32 %a, %b
; CHECK: %next = add i32 %i, 1
define i32 @loop_dominance(i32 %a, i32 %b) {
entry:
  %pre = mul i32 %a, %b
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %duplicate = mul i32 %b, %a
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 4
  br i1 %done, label %exit, label %loop
exit:
  %result = xor i32 %pre, %duplicate
  ret i32 %result
}
