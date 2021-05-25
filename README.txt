In this project, I implemented a socket based grade server that can handle requests using "threadpool".
The grade server stores grades and will be used by two kinds of clients:
1. Student - can only read grade.
2. Teaching assitant - can read grade, update grade and add a new student to the students' database.

In order to use server, you must have a VM with linux installed.
You can change the students.txt and assitants.txt files to whatever IDs and password that you want as long as they follow the format: <id> <password>.
The id shall not acceed 10 digits and the password shall not acceed 256 characters - each id should be unique and not appear in both students.txt and assitants.txt!

In order to start using the server, one shall need to open two terminals.
On the first terminal we're going to compile the grade_server.c file with the command gcc -g -pthread grade_server.c -o GradeServer.
Then, we want to run the server by using the command ./GradeServer <port number>. For example ./GradeServer 1234
Now our server is working and ready to handle requests!
On the second terminal we're going to compile the grade_client.c file with the command gcc -g -pthread grade_client.c -o GradeClient.
Next, we'll connect to the server by using the command ./GradeClient <host name> <port number>.
In our case, host name = localhost and port number should match the server's port number.

  That's it! we're all set up!
  
  In order to login the server, the client's input should be: Login <id> <password>.
  In order to read grade, the client's input should be: 1. ReadGrade - in case he/she is a student
                                                        2. ReadGrade <id> - in case he/she is a teaching assistant.
  In order to update a student's grade -  the client's input should be: Update <id> <grade> - the action is allowed only for teaching assitants.
  Note: If there was nosuch id in the server then it will be added.
  In order to get a list of the grades, the client's input should be: GradeList - the action is allowed only for teaching assitants.
  In order to logout, the client's input should be: Logout.
  In order to disconnect from the server, the client's input should be: "Exit"
  
  Note: if the action is not allowed, the server will return "Action not allowed"
        if the user's input is not valid the server will return "Invalid input"
  
Have a nice connection!
