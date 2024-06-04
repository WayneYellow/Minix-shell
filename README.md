# This is a basic shell implementation on minix3.3.0
the goal is to understand how a shell works and know some of the basic system calls.

1) Build: using "clang shell.c -o shell" to compile
2) Run: "./shell"
3) Features: my program implements all the requirements in the assignment's details from 1~7 and the 'cd' command.
4) Notice: pipe only support single pipe. It cannot support multiple pipes or a combination of pipes and redirection in one command.
5) Testing: 
    - You can see '>> ' if my shell is running, type any commands you want. Here are some examples:
    - `ls`
    - `clear`
    - `ls -l`
    - `cd /root`
    - `cd ..`
    - `ls -l &`
    - `ls -l > foo`
    - `sort < foo`
    - `ls -l | more`
    - `poweroff`
