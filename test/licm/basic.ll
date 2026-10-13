; CHECK-LABEL: define i32 @hoist_invariant(
; CHECK: entry:
; CHECK: %invariant = add i32 %a, %b
; CHECK: br label %loop
; CHECK: loop:
; CHECK-NOT: %invariant = add
define i32 @hoist_invariant(i32 %a, i32 %b) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %invariant = add i32 %a, %b
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 8
  br i1 %done, label %exit, label %loop
exit:
  %r = add i32 %invariant, %i
  ret i32 %r
}

; A potentially trapping division cannot be speculated into the preheader.
; CHECK-LABEL: define i32 @keep_trapping_division(
; CHECK: entry:
; CHECK-NEXT: br label %loop
; CHECK: loop:
; CHECK: %q = sdiv i32 %a, %b
define i32 @keep_trapping_division(i32 %a, i32 %b) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %q = sdiv i32 %a, %b
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 8
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %q
}

; CHECK-LABEL: define i32 @keep_variant(
; CHECK: loop:
; CHECK: %variant = add i32 %i, %a
define i32 @keep_variant(i32 %a) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %variant = add i32 %i, %a
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 8
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %variant
}

