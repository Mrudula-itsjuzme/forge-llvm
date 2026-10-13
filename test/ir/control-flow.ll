; Expressions in separate blocks are intentionally not merged.
; CHECK-LABEL: define i32 @branches(
; CHECK: then:
; CHECK: %t = add i32 %a, %b
; CHECK: else:
; CHECK: %e = add i32 %a, %b
define i32 @branches(i1 %cond, i32 %a, i32 %b) {
entry:
  br i1 %cond, label %then, label %else
then:
  %t = add i32 %a, %b
  br label %exit
else:
  %e = add i32 %a, %b
  br label %exit
exit:
  %r = phi i32 [ %t, %then ], [ %e, %else ]
  ret i32 %r
}

; CHECK-LABEL: define i32 @loop(
; CHECK: %x = add i32 %i, %n
; CHECK-NOT: %y = add i32 %i, %n
define i32 @loop(i32 %n) {
entry:
  br label %body
body:
  %i = phi i32 [ 0, %entry ], [ %next, %body ]
  %x = add i32 %i, %n
  %y = add i32 %i, %n
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 4
  br i1 %done, label %exit, label %body
exit:
  ret i32 %y
}

