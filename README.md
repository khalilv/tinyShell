# tinyShell
Mock shell assignment done in C. Can input commands directly into this program and the shell will execute them. Can pass a file into the program and the shell will run all the commands then terminate. Can also add a **FIFO** as an input and the shell will run FIFO mode which will used the named pipe to execute the commands (Note in FIFO mode only piped commands are valid to execute). 

Implemented the internal commands **history, limit** int, **checklim and chdir.** **History** shows the last 100 commands executed. **chdir** changes the working directory. Works with absolute and relative paths. **limit** int sets the soft limit of RLIM_DATA to the integer entered by the user. **checklim** displays the current hard and soft limits of RLIM_DATA. 

Dealt with **signal handling**. When CTRL^C is raised the shell will display a message asking the user if they would like to terminate the program. If yes, shell will terminate. If no, shell will continue. When CTRL^Z is raised, the shell will ignore this command and keep running. 


