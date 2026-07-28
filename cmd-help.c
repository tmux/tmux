/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael R
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

enum cmd_help_category {
  CMD_HELP_SESSIONS,
  CMD_HELP_WINDOWS,
  CMD_HELP_KEYS,
  CMD_HELP_OPTIONS,
  CMD_HELP_BUFFERS,
  CMD_HELP_AUTOMATION,
  CMD_HELP_NCATEGORIES
};

struct cmd_help_data {
  const char *name;
  enum cmd_help_category category;
  const char *description;
};

struct cmd_help_example {
  const char *name;
  const char *task;
  const char *command;
};

static const char *cmd_help_category_names[] = {
    "Sessions & Server",     "Windows & Panes", "Keys & Interaction",
    "Options & Environment", "Buffers",         "Automation"};

#define H(name, category, description) {name, CMD_HELP_##category, description}

/*
 * Keep descriptions and examples together as the source of truth for command
 * discovery. cmd_help_complete() verifies both whenever the catalog is shown
 */
static const struct cmd_help_data cmd_help_data[] = {
    H("attach-session", SESSIONS, "Attach to an existing session"),
    H("detach-client", SESSIONS, "Detach one or more clients"),
    H("has-session", SESSIONS, "Check whether a session exists"),
    H("kill-server", SESSIONS, "Stop the tmux server"),
    H("kill-session", SESSIONS, "Destroy one or more sessions"),
    H("list-clients", SESSIONS, "List connected clients"),
    H("list-sessions", SESSIONS, "List sessions and their state"),
    H("lock-client", SESSIONS, "Lock a client"),
    H("lock-server", SESSIONS, "Lock every client"),
    H("lock-session", SESSIONS, "Lock every client attached to a session"),
    H("new-session", SESSIONS, "Create a new session"),
    H("refresh-client", SESSIONS, "Refresh or resize a client"),
    H("rename-session", SESSIONS, "Rename a session"),
    H("server-access", SESSIONS, "Manage access to the tmux server"),
    H("show-messages", SESSIONS, "Show server and client messages"),
    H("start-server", SESSIONS, "Start the tmux server"),
    H("suspend-client", SESSIONS, "Suspend a client"),
    H("switch-client", SESSIONS, "Switch a client to another session"),

    H("break-pane", WINDOWS, "Move a pane into its own window"),
    H("choose-client", WINDOWS, "Choose a client interactively"),
    H("choose-tree", WINDOWS, "Browse sessions and windows interactively"),
    H("clear-history", WINDOWS, "Clear a pane's history"),
    H("clock-mode", WINDOWS, "Show a large clock in a pane"),
    H("copy-mode", WINDOWS, "Enter copy mode"),
    H("display-panes", WINDOWS, "Display pane numbers"),
    H("find-window", WINDOWS, "Find windows by content or name"),
    H("join-pane", WINDOWS, "Move a pane into another window"),
    H("kill-pane", WINDOWS, "Destroy one or more panes"),
    H("kill-window", WINDOWS, "Destroy one or more windows"),
    H("last-pane", WINDOWS, "Select the previously active pane"),
    H("last-window", WINDOWS, "Select the previously active window"),
    H("link-window", WINDOWS, "Link a window into another session"),
    H("list-panes", WINDOWS, "List panes and their state"),
    H("list-windows", WINDOWS, "List windows and their state"),
    H("move-pane", WINDOWS, "Move or float a pane"),
    H("move-window", WINDOWS, "Move a window between sessions"),
    H("new-pane", WINDOWS, "Create a pane"),
    H("new-window", WINDOWS, "Create a window"),
    H("next-layout", WINDOWS, "Select the next layout"),
    H("next-window", WINDOWS, "Select the next window"),
    H("previous-layout", WINDOWS, "Select the previous layout"),
    H("previous-window", WINDOWS, "Select the previous window"),
    H("rename-window", WINDOWS, "Rename a window"),
    H("resize-pane", WINDOWS, "Resize or zoom a pane"),
    H("resize-window", WINDOWS, "Resize a window"),
    H("respawn-pane", WINDOWS, "Restart a pane's command"),
    H("respawn-window", WINDOWS, "Restart every pane in a window"),
    H("rotate-window", WINDOWS, "Rotate panes in a window"),
    H("select-layout", WINDOWS, "Choose or apply a layout"),
    H("select-pane", WINDOWS, "Select or mark a pane"),
    H("select-window", WINDOWS, "Select a window"),
    H("split-window", WINDOWS, "Create a new pane by splitting a window"),
    H("swap-pane", WINDOWS, "Swap two panes"),
    H("swap-window", WINDOWS, "Swap two windows"),
    H("unlink-window", WINDOWS, "Unlink a window from a session"),

    H("bind-key", KEYS, "Bind a key to one or more commands"),
    H("clear-prompt-history", KEYS, "Clear interactive prompt history"),
    H("command-prompt", KEYS, "Open an interactive command prompt"),
    H("confirm-before", KEYS, "Ask for confirmation before a command"),
    H("customize-mode", KEYS, "Browse and edit options and key bindings"),
    H("display-menu", KEYS, "Display an interactive menu"),
    H("display-message", KEYS, "Display a message or expand formats"),
    H("display-popup", KEYS, "Display a popup running a command"),
    H("help", KEYS, "Show command help and examples"),
    H("list-commands", KEYS, "List commands or show command syntax"),
    H("list-keys", KEYS, "List key bindings"),
    H("send-keys", KEYS, "Send keys to a pane or client"),
    H("send-prefix", KEYS, "Send the prefix key to a pane"),
    H("show-prompt-history", KEYS, "Show interactive prompt history"),
    H("switch-mode", KEYS, "Switch the active pane mode"),
    H("unbind-key", KEYS, "Remove key bindings"),

    H("set-environment", OPTIONS, "Set or remove an environment variable"),
    H("set-hook", OPTIONS, "Set or remove a hook"),
    H("set-option", OPTIONS, "Set a server, session, or pane option"),
    H("set-window-option", OPTIONS, "Set a window option"),
    H("show-environment", OPTIONS, "Show environment variables"),
    H("show-hooks", OPTIONS, "Show hooks"),
    H("show-options", OPTIONS, "Show server, session, or pane options"),
    H("show-window-options", OPTIONS, "Show window options"),

    H("choose-buffer", BUFFERS, "Choose a paste buffer interactively"),
    H("delete-buffer", BUFFERS, "Delete a paste buffer"),
    H("list-buffers", BUFFERS, "List paste buffers"),
    H("load-buffer", BUFFERS, "Load a file into a paste buffer"),
    H("paste-buffer", BUFFERS, "Paste a buffer into a pane"),
    H("save-buffer", BUFFERS, "Save a paste buffer to a file"),
    H("set-buffer", BUFFERS, "Create or modify a paste buffer"),
    H("show-buffer", BUFFERS, "Print a paste buffer"),

    H("capture-pane", AUTOMATION, "Capture pane content"),
    H("if-shell", AUTOMATION, "Run a command conditionally"),
    H("pipe-pane", AUTOMATION, "Pipe pane input or output to a command"),
    H("run-shell", AUTOMATION, "Run a shell command"),
    H("source-file", AUTOMATION, "Load commands from a configuration file"),
    H("wait-for", AUTOMATION, "Coordinate clients with named channels")};

#undef H

static const struct cmd_help_example cmd_help_examples[] = {
    /* Sessions and server. */
    {"attach-session", "Attach to a named session",
     "tmux attach-session -t work"},
    {"detach-client", "Detach clients from a session",
     "tmux detach-client -s work"},
    {"has-session", "Check whether work exists", "tmux has-session -t work"},
    {"kill-server", "Stop the tmux server", "tmux kill-server"},
    {"kill-session", "Remove an old session", "tmux kill-session -t old-work"},
    {"list-clients", "List attached clients", "tmux list-clients"},
    {"list-sessions", "List sessions", "tmux list-sessions"},
    {"lock-client", "Lock the current client", "tmux lock-client"},
    {"lock-server", "Lock every client", "tmux lock-server"},
    {"lock-session", "Lock clients on work", "tmux lock-session -t work"},
    {"new-session", "Create a named session", "tmux new-session -s work"},
    {"new-session", "Create it in the background",
     "tmux new-session -d -s work"},
    {"refresh-client", "Refresh the current client", "tmux refresh-client"},
    {"rename-session", "Rename a session",
     "tmux rename-session -t work project"},
    {"server-access", "List server access rules", "tmux server-access -l"},
    {"show-messages", "Show server messages", "tmux show-messages"},
    {"start-server", "Start the server", "tmux start-server"},
    {"suspend-client", "Suspend the current client", "tmux suspend-client"},
    {"switch-client", "Switch to another session",
     "tmux switch-client -t work"},

    /* Windows and panes. */
    {"break-pane", "Move a pane to a new window",
     "tmux break-pane -t work:0.1"},
    {"choose-client", "Choose an attached client", "tmux choose-client"},
    {"choose-tree", "Browse sessions and windows", "tmux choose-tree"},
    {"clear-history", "Clear one pane's history",
     "tmux clear-history -t work:0.0"},
    {"clock-mode", "Show a clock", "tmux clock-mode"},
    {"copy-mode", "Enter copy mode", "tmux copy-mode"},
    {"display-panes", "Show pane numbers", "tmux display-panes"},
    {"find-window", "Find a window named editor", "tmux find-window editor"},
    {"join-pane", "Move a pane into another window",
     "tmux join-pane -s work:1.0 -t work:0.0"},
    {"kill-pane", "Remove a pane", "tmux kill-pane -t work:0.1"},
    {"kill-window", "Remove a named window", "tmux kill-window -t work:logs"},
    {"last-pane", "Return to the previous pane", "tmux last-pane"},
    {"last-window", "Return to the previous window", "tmux last-window"},
    {"link-window", "Link a shared window into work",
     "tmux link-window -s shared:0 -t work:"},
    {"list-panes", "Print pane IDs and commands",
     "tmux list-panes -F '#{pane_id} #{pane_current_command}'"},
    {"list-windows", "List windows", "tmux list-windows"},
    {"move-pane", "Move a pane between windows",
     "tmux move-pane -s work:1.0 -t work:0.0"},
    {"move-window", "Move a window into work",
     "tmux move-window -s scratch:1 -t work:"},
    {"new-pane", "Create a pane on the right", "tmux new-pane -h"},
    {"new-window", "Create a named window", "tmux new-window -n editor"},
    {"next-layout", "Cycle to the next layout", "tmux next-layout"},
    {"next-window", "Select the next window", "tmux next-window"},
    {"previous-layout", "Return to the previous layout",
     "tmux previous-layout"},
    {"previous-window", "Select the previous window", "tmux previous-window"},
    {"rename-window", "Rename a window", "tmux rename-window -t work:0 editor"},
    {"resize-pane", "Toggle pane zoom", "tmux resize-pane -Z"},
    {"resize-pane", "Grow right by five cells", "tmux resize-pane -R 5"},
    {"resize-window", "Set the window size", "tmux resize-window -x 120 -y 40"},
    {"respawn-pane", "Restart a pane", "tmux respawn-pane -k -t work:0.0"},
    {"respawn-window", "Restart a window", "tmux respawn-window -k -t work:0"},
    {"rotate-window", "Rotate panes downward", "tmux rotate-window -D"},
    {"select-layout", "Apply the tiled layout", "tmux select-layout tiled"},
    {"select-pane", "Select the pane above", "tmux select-pane -U"},
    {"select-window", "Select a named window",
     "tmux select-window -t work:editor"},
    {"split-window", "Split left and right", "tmux split-window -h"},
    {"split-window", "30% pane below", "tmux split-window -v -p 30"},
    {"swap-pane", "Swap two panes", "tmux swap-pane -s work:0.0 -t work:0.1"},
    {"swap-window", "Swap two windows", "tmux swap-window -s work:1 -t work:2"},
    {"unlink-window", "Unlink a shared window",
     "tmux unlink-window -t work:shared"},

    /* Keys and interaction. */
    {"bind-key", "Bind prefix-r to reload configuration",
     "tmux bind-key r source-file ~/.tmux.conf"},
    {"clear-prompt-history", "Clear all prompt history",
     "tmux clear-prompt-history"},
    {"command-prompt", "Open a command prompt", "tmux command-prompt"},
    {"confirm-before", "Confirm before killing a pane",
     "tmux confirm-before -p 'Kill pane? (y/n)' kill-pane"},
    {"customize-mode", "Browse options and bindings", "tmux customize-mode"},
    {"display-menu", "Show a one-item actions menu",
     "tmux display-menu -T Actions 'New window' n new-window"},
    {"display-message", "Print the current target",
     "tmux display-message -p '#{session_name}:#{window_index}'"},
    {"display-popup", "Open a shell popup", "tmux display-popup -E \"$SHELL\""},
    {"help", "Show help for list-panes", "tmux help list-panes"},
    {"list-commands", "Browse all commands", "tmux list-commands"},
    {"list-commands", "Inspect one command", "tmux help split-window"},
    {"list-keys", "List prefix-table bindings", "tmux list-keys -T prefix"},
    {"send-keys", "Run text in a pane",
     "tmux send-keys -t work:0.0 'make test' Enter"},
    {"send-prefix", "Send the prefix through to a pane",
     "tmux send-prefix -t work:0.0"},
    {"show-prompt-history", "Show prompt history", "tmux show-prompt-history"},
    {"switch-mode", "Browse available pane modes", "tmux switch-mode"},
    {"unbind-key", "Remove the prefix-r binding",
     "tmux unbind-key -T prefix r"},

    /* Options and environment. */
    {"set-environment", "Set a global environment variable",
     "tmux set-environment -g EDITOR nvim"},
    {"set-hook", "Run a command after window creation",
     "tmux set-hook -g after-new-window 'display-message created'"},
    {"set-option", "Enable mouse support", "tmux set-option -g mouse on"},
    {"set-window-option", "Disable automatic window names",
     "tmux set-window-option -g automatic-rename off"},
    {"show-environment", "Show the global PATH",
     "tmux show-environment -g PATH"},
    {"show-hooks", "Show global hooks", "tmux show-hooks -g"},
    {"show-options", "Show one global option",
     "tmux show-options -g status-style"},
    {"show-window-options", "Show the global mode keys",
     "tmux show-window-options -g mode-keys"},

    /* Buffers. */
    {"choose-buffer", "Choose a buffer interactively", "tmux choose-buffer"},
    {"delete-buffer", "Delete a named buffer", "tmux delete-buffer -b notes"},
    {"list-buffers", "List paste buffers", "tmux list-buffers"},
    {"load-buffer", "Load a file into a buffer",
     "tmux load-buffer -b notes ./notes.txt"},
    {"paste-buffer", "Paste a named buffer", "tmux paste-buffer -b notes"},
    {"save-buffer", "Save a buffer to a file",
     "tmux save-buffer -b notes ./notes.txt"},
    {"set-buffer", "Create a named buffer",
     "tmux set-buffer -b greeting 'hello world'"},
    {"show-buffer", "Print a named buffer", "tmux show-buffer -b greeting"},

    /* Automation. */
    {"capture-pane", "Print visible pane content", "tmux capture-pane -p"},
    {"capture-pane", "Include pane history", "tmux capture-pane -p -S -"},
    {"if-shell", "Run a command only when a file exists",
     "tmux if-shell 'test -f .env' 'display-message found'"},
    {"pipe-pane", "Append pane output to a file",
     "tmux pipe-pane -o 'cat >>~/tmux-pane.log'"},
    {"run-shell", "Run a command in the background",
     "tmux run-shell -b 'make test'"},
    {"source-file", "Reload the user configuration",
     "tmux source-file ~/.tmux.conf"},
    {"wait-for", "Signal a channel", "tmux wait-for -S ready"},
    {"wait-for", "Wait for a channel", "tmux wait-for ready"}};

static const struct cmd_help_data *cmd_help_find(const char *name) {
  u_int i;

  for (i = 0; i < nitems(cmd_help_data); i++) {
    if (strcmp(cmd_help_data[i].name, name) == 0)
      return (&cmd_help_data[i]);
  }
  return (NULL);
}

const char *cmd_help_description(const char *name) {
  const struct cmd_help_data *data = cmd_help_find(name);

  if (data == NULL)
    return (NULL);
  return (data->description);
}

int cmd_help_complete(void) {
  const struct cmd_entry **entryp;
  const struct cmd_entry *entry;
  char *cause;
  u_int i, j, k;
  int found;

  for (entryp = cmd_table; *entryp != NULL; entryp++) {
    if (cmd_help_find((*entryp)->name) == NULL)
      return (0);
  }
  for (i = 0; i < nitems(cmd_help_data); i++) {
    entry = cmd_find(cmd_help_data[i].name, &cause);
    if (entry == NULL) {
      free(cause);
      return (0);
    }
    found = 0;
    for (k = 0; k < nitems(cmd_help_examples); k++) {
      if (strcmp(cmd_help_data[i].name, cmd_help_examples[k].name) == 0) {
        found = 1;
        break;
      }
    }
    if (!found)
      return (0);
    for (j = i + 1; j < nitems(cmd_help_data); j++) {
      if (strcmp(cmd_help_data[i].name, cmd_help_data[j].name) == 0)
        return (0);
    }
  }
  for (i = 0; i < nitems(cmd_help_examples); i++) {
    if (cmd_help_find(cmd_help_examples[i].name) == NULL)
      return (0);
  }
  return (1);
}

void cmd_help_print_catalog(struct cmdq_item *item) {
  struct cmd_output_table *table;
  const struct cmd_help_data *data;
  const struct cmd_entry *entry;
  const char *headers[] = {"COMMAND", "ALIAS", "DESCRIPTION"};
  const char *cells[3];
  enum cmd_output_style styles[] = {CMD_OUTPUT_IDENTIFIER, CMD_OUTPUT_DIM,
                                    CMD_OUTPUT_DEFAULT};
  char *alias, *cause;
  u_int category, i;

  if (!cmd_help_complete()) {
    cmdq_error(item, "command help metadata is incomplete");
    return;
  }
  cmd_output_print(item, CMD_OUTPUT_DIM,
                   "See 'tmux help <command>' for usage and examples.");
  cmdq_print(item, "%s", "");
  for (category = 0; category < CMD_HELP_NCATEGORIES; category++) {
    table = cmd_output_table_create(item, cmd_help_category_names[category], 3,
                                    headers);
    for (i = 0; i < nitems(cmd_help_data); i++) {
      data = &cmd_help_data[i];
      if (data->category != category)
        continue;
      entry = cmd_find(data->name, &cause);
      if (entry == NULL) {
        free(cause);
        continue;
      }
      if (entry->alias == NULL)
        alias = xstrdup("");
      else
        xasprintf(&alias, "(%s)", entry->alias);
      cells[0] = entry->name;
      cells[1] = alias;
      cells[2] = data->description;
      cmd_output_table_add(table, cells, styles);
      free(alias);
    }
    cmd_output_table_print(table);
    cmd_output_table_free(table);
  }
}

enum cmd_retval cmd_help_print(struct cmdq_item *item, const char *name) {
  const struct cmd_help_data *data;
  const struct cmd_help_example *example;
  const struct cmd_entry *entry;
  struct cmd_output_table *table;
  const char *headers[] = {"TASK", "COMMAND"};
  const char *cells[2];
  enum cmd_output_style styles[] = {CMD_OUTPUT_DEFAULT, CMD_OUTPUT_IDENTIFIER};
  char *cause, *text;
  u_int i, nexamples = 0;

  if (name == NULL) {
    cmd_help_print_catalog(item);
    return (CMD_RETURN_NORMAL);
  }
  /*
   * Use normal command lookup so aliases and canonical names produce the same
   * help text.
   */
  entry = cmd_find(name, &cause);
  if (entry == NULL) {
    cmdq_error(item, "%s", cause);
    free(cause);
    return (CMD_RETURN_ERROR);
  }
  data = cmd_help_find(entry->name);
  if (data == NULL) {
    cmdq_error(item, "no help available for %s", entry->name);
    return (CMD_RETURN_ERROR);
  }

  cmd_output_print(item, CMD_OUTPUT_HEADING, "NAME");
  xasprintf(&text, "%s - %s", entry->name, data->description);
  cmd_output_print_wrapped(item, CMD_OUTPUT_IDENTIFIER, 2, text);
  free(text);
  cmdq_print(item, "%s", "");

  cmd_output_print(item, CMD_OUTPUT_HEADING, "USAGE");
  xasprintf(&text, "tmux %s%s%s", entry->name,
            entry->usage == NULL || *entry->usage == '\0' ? "" : " ",
            entry->usage == NULL ? "" : entry->usage);
  cmd_output_print_wrapped(item, CMD_OUTPUT_IDENTIFIER, 2, text);
  free(text);
  cmdq_print(item, "%s", "");

  if (entry->alias != NULL) {
    cmd_output_print(item, CMD_OUTPUT_HEADING, "ALIAS");
    cmd_output_print(item, CMD_OUTPUT_DIM, "  %s", entry->alias);
    cmdq_print(item, "%s", "");
  }

  for (i = 0; i < nitems(cmd_help_examples); i++) {
    if (strcmp(cmd_help_examples[i].name, entry->name) == 0)
      nexamples++;
  }
  if (nexamples != 0) {
    table = cmd_output_table_create(item, "EXAMPLES", 2, headers);
    for (i = 0; i < nitems(cmd_help_examples); i++) {
      example = &cmd_help_examples[i];
      if (strcmp(example->name, entry->name) != 0)
        continue;
      cells[0] = example->task;
      cells[1] = example->command;
      cmd_output_table_add(table, cells, styles);
    }
    cmd_output_table_print(table);
    cmd_output_table_free(table);
  }
  cmd_output_print(item, CMD_OUTPUT_DIM,
                   "See 'man tmux' for the complete reference.");
  return (CMD_RETURN_NORMAL);
}

static enum cmd_retval cmd_help_exec(struct cmd *self, struct cmdq_item *item) {
  return (cmd_help_print(item, args_string(cmd_get_args(self), 0)));
}

const struct cmd_entry cmd_help_entry = {.name = "help",
                                         .alias = NULL,

                                         .args = {"", 0, 1, NULL},
                                         .usage = "[command]",

                                         .flags =
                                             CMD_STARTSERVER | CMD_AFTERHOOK,
                                         .exec = cmd_help_exec};
