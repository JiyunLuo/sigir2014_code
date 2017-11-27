
The code for each year's TREC Session TREC is located in corresponding folders.

In each folder, there are two files:
1) optimal.cpp
2) optimal.sh 

The optimal.cpp file contains the code for the win-win framework. 

There are three essential lines in the optimal.sh file:
  Line 1: it compiles the optimal.cpp file to an executabile file on  a Linux system.
        In order for this line to correctly execute, you need have Indri-5.0 installed in your server. Our codes get the best performance         under Indri-5.0 version.
  Line 2: it runs the executible file and output a TREC Sessin Track run.
        To get this line run, you will need to have the TREC Session Track input files and its qrel files ready. Also you need to have the         ClueWeb09/12 CatB index ready.
  Line 3: it runs the evaluation of the generated TREC run.
        To get this line run, you will need the evaluation scripts from the correct year's TREC Session Track evaluation scripts ready.
