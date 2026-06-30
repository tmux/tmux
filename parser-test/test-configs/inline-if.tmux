# Inline %if form.
# Expected: inline true branch selected, then following command in same sequence is processed.
display-message "parser inline if: before" ; %if 1 display-message "parser inline if: branch" %endif ; display-message "parser inline if: after"
