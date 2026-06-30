# %if/%elif/%else.
# Expected: elif branch is selected.
%if 0
display-message "parser elif: selected if branch - wrong"
%elif 1
display-message "parser elif: selected elif branch"
%else
display-message "parser elif: selected else branch - wrong"
%endif
display-message "parser elif: after endif"
