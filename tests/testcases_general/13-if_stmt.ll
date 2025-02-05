; ModuleID = 'cminus'
source_filename = "/home/vixic/2024ustc-jianmu-compiler/tests/testcases_general/13-if_stmt.cminus"

declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define void @main() {
label_entry:
  %op2 = icmp ne i32 2, 0
  br i1 %op2, label %label3, label %label4
label3:                                                ; preds = %label_entry
  br label %label4
label4:                                                ; preds = %label_entry, %label3
  ret void
}
