Documenting the Existing Release Pipeline
-----------------------------------------

### Overview of the Release Pipeline

Tmux uses GitHub for version control and version releases and is built using GNU Autotools.If issues are found in releases, users may report the issues to GitHub Issues and can also reach out to the mailing list for assistance. 

Details of each release pipeline phase can be found in the following sections. 

### Integration Phase

#### GitHub

The tmux project now uses GitHub exclusively for its code repository and release management. This platform allows for collaborative development, where developers can contribute code, report issues, and suggest improvements. Changes to the source code are committed directly to the GitHub repository. This setup facilitates easier tracking of contributions and version history, making it more accessible for developers worldwide. GitHub's integrated features, like pull requests and issue tracking, enhance the development workflow, ensuring efficient and organized project management for tmux.

#### Contributions and Pull Requests

In the updated workflow of the tmux project on GitHub, users encountering issues or having suggestions can open an issue, adhering to the guidelines in 'CONTRIBUTING.md'. Contributions to tmux are made through GitHub pull requests.

Once a pull request is created, it can be reviewed by any member of the tmux organization, not limited to specific maintainers. This approach encourages a more collaborative and open review process. The changes are then committed directly to the GitHub repository.

This direct use of GitHub streamlines the contribution process, enabling a more transparent and immediate integration of changes. Pull requests are merged upon approval, reflecting real-time updates and contributions in the tmux contributor dashboard. This setup allows for a broader range of contributors to actively participate and be recognized in the project.

#### GitHub Actions Workflows For Integration Phase

GitHub Actions is a popular CI/CD platform that automates the build, test, and deploy process of software workflows.

The tmux project currently contains multiple GitHub Action workflows. Below are explanations for each.

The '[lock.yml](https://github.com/tmux/tmux/blob/master/.github/workflows/lock.yml)' file contains the workflow that is triggered daily at midnight and automatically locks issues after 30 days of inactivity and PRs after 60 days of inactivity.

The clang-linter.yml file contains the workflow to run clang-tidy linter on every push and pull request and if the linter fails with errors then it will comment on the PR that caused the failure informing about the linting errors. It first installs the required dependencies to run the workflow and sets up Autotools using the configure script. Then it generates compile_commands.json which will be used by the linter to look for errors in multiple files. Then finally the clang-tidy linter is run on the code and if the linter returns an error, the workflow uses actions/github-script to comment on the PR informing the about the error.

The test.yml file is configured to execute CI tests for each push or pull request, ensuring a more efficient and automated testing process. The tests complete within approximately 2 minutes, providing quick feedback. If a test fails, it displays a clear error message detailing the discrepancy between the actual and expected output. Additionally, the workflow covers dependency installation and program building, flagging any errors in these stages too.

The doc_review_reminder.yml contains a workflow that automatically creates an issue every six months as a reminder for documentation reviewers. This scheduled reminder is crucial for maintaining up-to-date and accurate documentation. By periodically prompting reviewers, the workflow ensures that the documentation is regularly examined and updated, reflecting any changes or improvements in the project.

Build Phase

#### GNU Autotools

Tmux uses GNU Autotools, an abstraction-based build toolchain. It expands macros to generate platform-specific Makefiles. Macros are like placeholders in the Makefile. In the "configure" step, Autotools will read from system environment and user configurations to expand the macros, in order to generate the final Makefile. Similar to CMake, it's an abstraction layer upon Makefiles. The key difference is there is a "configure" step.

Diagram of the key Autotools build process for tmux

#####  Important Files in the Autotools Build Process

Makefile.am

It contains high-level declarative build instructions written by developers. It is part of source code and meant to be simple to maintain

e.g., CLEANFILES = tmux.1.mdoc tmux.1.man cmd-parse.c

This will in the end translate to a 'clean' target that removes these files along with the usual build artifacts.

Makefile.in

It is generated from 'Makefile.am'. For tmux, it generated 1776 lines from 242 lines. It is detailed and looks very similar to 'Makefile', but includes placeholders for configuration options that can vary between systems.

e.g., CC = @CC@

@CC@ is a placeholder. It will be replaced by the actual compiler command ('gcc', 'clang', etc.)

Makefile

'configure' fills in system-specific details (paths, compiler flags, etc.) into the placeholders in 'Makefile.in' to produce the actual 'Makefile'

configure.ac

It is a M4 macro file used to define the requirements and settings that the configure script will check for. It is part of source code.

e.g.

The ACPROG_CC macro is used to find a suitable C compiler for the project. It checks C compilers in a predefined order and selects the first one it finds available.

aclocal.m4

All necessary macro definitions are consolidated into this.

configure

It is a shell script. It will test the target system's environment (compilers, libraries, header files, etc.) to generate the Makefile from Makefile.in. In tmux, it takes in 988 lines of configure.ac and 1426 lines of aclocal.m4 to generate11612 lines of configure shell script.

##### Build Workflow

To build tmux from source files, the following commands need to be run on the terminal:  

'sh autogen.sh' './configure && make'.

Below are more details about what occurs during each step.

Step 1: sh autogen.sh

It does three important tasks

1\. aclocal

'aclocal' prepares the project for configuration. It scans 'configure.ac' and 'Makefile.am' for macro invocations to generate an 'aclocal.m4' file. This .m4 file includes macros required by 'configure.ac'. This is to ensure all necessary macro definitions are available and organized for generating the 'configure' script.

2\. autoconf

'autoconf' is executed by 'autoreconf' in the shell script. It generates the 'configure' script from 'configure.ac'. Developers write the 'configure.ac' file in M4 macro language to define the requirements and settings.

3\. automake

'automake' generates 'Makefile.in' template from 'Makefile.am'. 'Makefile.in' is then used by 'configure' to produce the final 'Makefile'. The developers get to work on a high-level and abstract 'Makefile.am' file, and the automake process will ensure that the build process adheres to GNU standards, making software more portable and easier to build across different Unix-like systems.

Step 2: ./configure 

This script adjusts the package configuration for the local system, checking for libraries, header files, and functions that are needed. It generates the Makefile from the Makefile.in template according to the local system's configuration.

Step 3: make

This command builds the project using the generated Makefile.

### Deployment Phase

The continuous delivery pipeline is called tagged-release. It gets triggered by any push to the master branch that contains changes. 

The deployment phase involves managing the version number. The version number is stored in configure.ac, defined under "AC_INIT([tmux], next-<major.minor.patch>)". It is treated as a single source of truth. In development, the version number is "next-" as a prefix. In the release pipeline, it is expected to make a release without the "next-" prefix, bump up the version, then add the "next-" prefix back for developing the next version. For example, if the current version is "next-3.2.10" before release, the pipeline releases tmux with version "3.2.10" then changes the version to "next-3.2.11" in configure.ac.

After the workflow is triggered, the first thing it does is to set up the environment. It runs on ubuntu-latest and checks out the latest copy of tmux repository. Then, it uses sed to locate the specific line containing the version number, then drops the "next-" prefix.

Then, it installs necessary packages like bison, autotools-dev and build-essential. Most importantly, it downloads the two dependencies that tmux is built on - libevent and ncurses. It utilizes existing scripts to configure, build and make the release tarball for tmux. 

The CHANGES file will also be automatically updated to contain the commit messages of the commits included in the release.

Once the above steps are completed, a tag will be created and pushed to GitHub for release. A tarball created using command 'make dist' will then be attached to the release notes of the release. Along with the release, a GitHub issue will be created for users and developers to ask questions about the latest release.

After this, the workflow runs a script to bump up the version, adding one to patch number by default. It writes this new version back to configure.ac, with the "next-" prefix attached back. 

Finally, after a release is made, the released tarball can be found on the [release page](https://github.com/tmux/tmux/releases). Users may install tmux by downloading the source tarball or getting from version control following the steps outlined on the [tmux installation wiki](https://github.com/tmux/tmux/wiki/Installing).

### Monitoring Phase

Users can turn on logging manually to monitor tmux. They can report issues through GitHub Issues or the mailing list.

#### Logging

Users can run tmux with -v or -vv for verbose logging. Log messages will be saved into tmux-client-PID.log and tmux-server-PID.log files in the current directory, where PID is the PID of the server or client process.  If -v is specified twice, an additional tmux-out-PID.log file is generated with a copy of everything written to the terminal.

Logging framework is implemented in a single `log.c` file.

#### Reporting

Users can report bugs on GitHub Issues or email the tmux-users Google group. The tmux-users group seems to be relatively active, with the project owner often responding to emails within one or two days. If an issue is reported on GitHub Issues, it will be automatically locked after 30 days of inactivity.