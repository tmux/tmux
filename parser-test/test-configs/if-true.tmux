# %if true branch.
# Expected: first branch is selected.
%if 1
display-message "parser if true: selected if branch"
%else
display-message "parser if true: selected else branch - wrong"
%endif
display-message "parser if true: after endif"
